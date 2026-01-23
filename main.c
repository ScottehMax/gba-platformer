#include "gba.h"
#include "skelly.h"
#include "ground.h"
#include "ground2.h"
#include "level2.h"

// Use fixed-point math (8 bits fractional) for smooth sub-pixel movement
#define FIXED_SHIFT 8
#define FIXED_ONE (1 << FIXED_SHIFT)

#define GRAVITY (FIXED_ONE / 2)
#define JUMP_STRENGTH (FIXED_ONE * 5)
#define MAX_SPEED (FIXED_ONE * 3)
#define ACCELERATION (FIXED_ONE * 1)
#define FRICTION (FIXED_ONE / 10)
#define AIR_FRICTION (FIXED_ONE / 8)
#define DASH_SPEED (FIXED_ONE * 5)
#define COYOTE_TIME 6  // Frames of grace period after walking off edge

#define PLAYER_RADIUS 8

typedef struct {
    int x;  // Fixed-point
    int y;  // Fixed-point
    int vx; // Fixed-point
    int vy; // Fixed-point
    int onGround;
    int coyoteTime;   // Frames remaining for coyote time jump
    int dashing;
    int dashCooldown;
    int facingRight;  // 1 = right, 0 = left
    u16 prevKeys;     // Previous frame keys for detecting button presses
} Player;

typedef struct {
    int x;  // Camera X in pixels
    int y;  // Camera Y in pixels
} Camera;

void vsync() {
    while ((*(volatile u16*)0x04000006) >= 160);
    while ((*(volatile u16*)0x04000006) < 160);
}

u8 getTileAt(const Level* level, int tileX, int tileY) {
    if (tileX < 0 || tileX >= level->width || tileY < 0 || tileY >= level->height) {
        return 0; // Out of bounds
    }
    return level->tiles[tileY * level->width + tileX];
}

int isTileSolid(u8 tileId) {
    // Tiles 1-8 are solid (ground tiles)
    // Tile 0 is empty/air
    return tileId >= 1 && tileId <= 8;
}

void updateCamera(Camera* camera, Player* player, const Level* level) {
    // Camera follows player with dead zone
    int playerScreenX = (player->x >> FIXED_SHIFT) - camera->x;
    int playerScreenY = (player->y >> FIXED_SHIFT) - camera->y;
    
    // Horizontal camera
    int deadZoneLeft = SCREEN_WIDTH / 3;
    int deadZoneRight = 2 * SCREEN_WIDTH / 3;
    
    if (playerScreenX < deadZoneLeft) {
        camera->x += playerScreenX - deadZoneLeft;
    } else if (playerScreenX > deadZoneRight) {
        camera->x += playerScreenX - deadZoneRight;
    }
    
    // Vertical camera  
    int deadZoneTop = SCREEN_HEIGHT / 3;
    int deadZoneBottom = 2 * SCREEN_HEIGHT / 3;
    
    if (playerScreenY < deadZoneTop) {
        camera->y += playerScreenY - deadZoneTop;
    } else if (playerScreenY > deadZoneBottom) {
        camera->y += playerScreenY - deadZoneBottom;
    }
    
    // Clamp camera to level bounds
    int maxCameraX = level->width * 8 - SCREEN_WIDTH;
    int maxCameraY = level->height * 8 - SCREEN_HEIGHT;
    
    if (camera->x < 0) camera->x = 0;
    if (camera->x > maxCameraX) camera->x = maxCameraX;
    if (camera->y < 0) camera->y = 0;
    if (camera->y > maxCameraY) camera->y = maxCameraY;
}

void updatePlayer(Player* player, u16 keys, const Level* level) {
    // Detect button presses (pressed this frame but not last frame)
    u16 pressed = keys & ~player->prevKeys;

    // Dash cooldown
    if (player->dashCooldown > 0) {
        player->dashCooldown--;
    }

    // R button: Dash (8-directional or forward) - only on press, not hold
    if ((pressed & KEY_R) && player->dashCooldown == 0 && player->dashing == 0) {
        player->dashing = 8; // Dash lasts 8 frames
        player->dashCooldown = 30; // 30 frames cooldown

        // Check which directions are held
        int dashX = 0, dashY = 0;
        if (keys & KEY_LEFT) dashX = -1;
        if (keys & KEY_RIGHT) dashX = 1;
        if (keys & KEY_UP) dashY = -1;
        if (keys & KEY_DOWN) dashY = 1;

        // If no direction held, dash forward based on facing
        if (dashX == 0 && dashY == 0) {
            dashX = player->facingRight ? 1 : -1;
        }

        // Apply dash velocity (normalize diagonals to maintain speed)
        if (dashX != 0 && dashY != 0) {
            // Diagonal: multiply by ~0.707 (using 181/256 as approximation)
            player->vx = (dashX * DASH_SPEED * 181) >> 8;
            player->vy = (dashY * DASH_SPEED * 181) >> 8;
        } else {
            player->vx = dashX * DASH_SPEED;
            player->vy = dashY * DASH_SPEED;
        }
    }

    // Countdown dash timer
    if (player->dashing > 0) {
        player->dashing--;
    }

    // Horizontal movement (disabled during dash)
    if (player->dashing == 0) {
        if (keys & KEY_LEFT) {
            player->vx -= ACCELERATION;
            if (player->vx < -MAX_SPEED) {
                player->vx = -MAX_SPEED;
            }
            player->facingRight = 0;  // Face left
        } else if (keys & KEY_RIGHT) {
            player->vx += ACCELERATION;
            if (player->vx > MAX_SPEED) {
                player->vx = MAX_SPEED;
            }
            player->facingRight = 1;  // Face right
        } else {
            // Apply friction when no input
            int friction = player->onGround ? FRICTION : AIR_FRICTION;
            if (player->vx > 0) {
                player->vx -= friction;
                if (player->vx < 0) player->vx = 0;
            } else if (player->vx < 0) {
                player->vx += friction;
                if (player->vx > 0) player->vx = 0;
            }
        }
    }

    // A button: Jump - only on press, not hold
    // Allow jump if grounded OR within coyote time window
    if ((pressed & KEY_A) && (player->onGround || player->coyoteTime > 0)) {
        player->vy = -JUMP_STRENGTH;
        player->onGround = 0;
        player->coyoteTime = 0;  // Consume coyote time on jump
    }

    // Apply gravity (but not during dash, to preserve dash trajectory)
    if (!player->onGround && player->dashing == 0) {
        player->vy += GRAVITY;
    }

    // Swept collision - move along each axis and stop at first collision
    int oldX = player->x;
    int oldY = player->y;
    
    // Horizontal sweep
    player->x += player->vx;
    int screenX = player->x >> FIXED_SHIFT;
    int screenY = player->y >> FIXED_SHIFT;
    
    // Level bounds
    int levelWidthPx = level->width * 8;
    if (screenX < PLAYER_RADIUS) {
        player->x = PLAYER_RADIUS << FIXED_SHIFT;
        player->vx = 0;
    } else if (screenX > levelWidthPx - PLAYER_RADIUS) {
        player->x = (levelWidthPx - PLAYER_RADIUS) << FIXED_SHIFT;
        player->vx = 0;
    } else {
        // Check for tile collision at new X position
        screenX = player->x >> FIXED_SHIFT;
        int tileMinX = (screenX - PLAYER_RADIUS) / 8;
        int tileMaxX = (screenX + PLAYER_RADIUS) / 8;
        int tileMinY = (screenY - PLAYER_RADIUS) / 8;
        int tileMaxY = (screenY + PLAYER_RADIUS) / 8;
        
        for (int ty = tileMinY; ty <= tileMaxY; ty++) {
            for (int tx = tileMinX; tx <= tileMaxX; tx++) {
                u8 tile = getTileAt(level, tx, ty);
                if (!isTileSolid(tile)) continue;
                
                int tileLeft = tx * 8;
                int tileRight = (tx + 1) * 8;
                int tileTop = ty * 8;
                int tileBottom = (ty + 1) * 8;
                
                int playerLeft = screenX - PLAYER_RADIUS;
                int playerRight = screenX + PLAYER_RADIUS;
                int playerTop = screenY - PLAYER_RADIUS;
                int playerBottom = screenY + PLAYER_RADIUS;
                
                if (playerRight > tileLeft && playerLeft < tileRight &&
                    playerBottom > tileTop && playerTop < tileBottom) {
                    // Collision - snap to tile edge instead of reverting
                    if (player->vx > 0) {
                        // Moving right, snap to left edge of tile
                        player->x = (tileLeft - PLAYER_RADIUS) << FIXED_SHIFT;
                    } else {
                        // Moving left, snap to right edge of tile
                        player->x = (tileRight + PLAYER_RADIUS) << FIXED_SHIFT;
                    }
                    player->vx = 0;
                    goto horizontal_done;
                }
            }
        }
    }
    horizontal_done:
    
    // Vertical sweep
    player->y += player->vy;
    screenX = player->x >> FIXED_SHIFT;
    screenY = player->y >> FIXED_SHIFT;
    
    player->onGround = 0;
    
    // Ceiling bounds
    if (screenY - PLAYER_RADIUS < 0) {
        player->y = PLAYER_RADIUS << FIXED_SHIFT;
        player->vy = 0;
    } else {
        // Check for tile collision at new Y position
        int tileMinX = (screenX - PLAYER_RADIUS) / 8;
        int tileMaxX = (screenX + PLAYER_RADIUS) / 8;
        int tileMinY = (screenY - PLAYER_RADIUS) / 8;
        int tileMaxY = (screenY + PLAYER_RADIUS) / 8;
        
        for (int ty = tileMinY; ty <= tileMaxY; ty++) {
            for (int tx = tileMinX; tx <= tileMaxX; tx++) {
                u8 tile = getTileAt(level, tx, ty);
                if (!isTileSolid(tile)) continue;
                
                int tileLeft = tx * 8;
                int tileRight = (tx + 1) * 8;
                int tileTop = ty * 8;
                int tileBottom = (ty + 1) * 8;
                
                int playerLeft = screenX - PLAYER_RADIUS;
                int playerRight = screenX + PLAYER_RADIUS;
                int playerTop = screenY - PLAYER_RADIUS;
                int playerBottom = screenY + PLAYER_RADIUS;
                
                if (playerRight > tileLeft && playerLeft < tileRight &&
                    playerBottom > tileTop && playerTop < tileBottom) {
                    // Collision - snap to tile edge
                    if (player->vy > 0) {
                        // Moving down, snap to top of tile
                        player->y = (tileTop - PLAYER_RADIUS) << FIXED_SHIFT;
                        player->vy = 0;
                        player->onGround = 1;
                        if (player->dashing > 0) player->dashing = 0;
                    } else {
                        // Moving up, snap to bottom of tile
                        player->y = (tileBottom + PLAYER_RADIUS) << FIXED_SHIFT;
                        player->vy = 0;
                    }
                    goto vertical_done;
                }
            }
        }
    }
    vertical_done:
    
    // Ground check for standing still
    if (!player->onGround) {
        screenX = player->x >> FIXED_SHIFT;
        screenY = player->y >> FIXED_SHIFT;
        int feetY = (screenY + PLAYER_RADIUS + 1) / 8;
        int checkX = screenX / 8;

        u8 tile = getTileAt(level, checkX, feetY);
        if (isTileSolid(tile)) {
            int tileTop = feetY * 8;
            if (screenY + PLAYER_RADIUS >= tileTop - 1) {
                player->onGround = 1;
            }
        }
    }

    // Update coyote time
    if (player->onGround) {
        player->coyoteTime = COYOTE_TIME;  // Reset when grounded
    } else if (player->coyoteTime > 0) {
        player->coyoteTime--;  // Count down when airborne
    }

    // Update previous keys for next frame
    player->prevKeys = keys;
}

void drawGame(Player* player, Camera* camera) {
    // Update player sprite position - convert from world to screen coordinates
    int screenX = (player->x >> FIXED_SHIFT) - camera->x - 8;  // Center the 16x16 sprite
    int screenY = (player->y >> FIXED_SHIFT) - camera->y - 8;  // Center the 16x16 sprite

    // Clamp to visible area
    if (screenX < -16) screenX = -16;
    if (screenY < -16) screenY = -16;
    if (screenX > 239) screenX = 239;
    if (screenY > 159) screenY = 159;

    // Update sprite position (16x16, 256-color)
    *((volatile u16*)0x07000000) = screenY | (1 << 13);   // attr0: Y + 256-color mode
    *((volatile u16*)0x07000002) = screenX | (1 << 14) | (player->facingRight ? 0 : (1 << 12));   // attr1: X + size 16x16 + H-flip if facing left
    *((volatile u16*)0x07000004) = 0;                      // attr2: tile 0
}

void loadLevelToVRAM(const Level* level) {
    // Set up background tiles from level data
    // The level uses tile IDs 0-8, which map to our existing tiles
    // This function could be extended to dynamically load tiles based on level needs
    
    // Tiles are already set up in main(), so nothing extra needed here
    // In a more advanced implementation, we'd stream tiles as camera scrolls
}

int main() {
    // Load level
    const Level* currentLevel = &Tutorial_Level;
    
    // Mode 0 with BG0 and sprites enabled
    REG_DISPCNT = VIDEOMODE_0 | BG0_ENABLE | OBJ_ENABLE | OBJ_1D_MAP;

    // Set up background palette
    volatile u16* bgPalette = (volatile u16*)0x05000000;
    setPalette(0, COLOR(15, 20, 31));  // Sky color (light blue) at index 0
    
    // Copy ground palette to indices 1-4
    bgPalette[1] = groundPal[0];
    bgPalette[2] = groundPal[1];
    bgPalette[3] = groundPal[2];
    bgPalette[4] = groundPal[3];
    
    // Copy ground2 palette to indices 5-6
    bgPalette[5] = ground2Pal[0];
    bgPalette[6] = ground2Pal[1];

    // Create background tiles in VRAM
    volatile u32* bgTiles = (volatile u32*)0x06000000;

    // Tile 0: Sky (all pixels palette index 0)
    for (int i = 0; i < 16; i++) {
        bgTiles[i] = 0x00000000;
    }

    // Tiles 1-4: ground.png - remap palette indices: 0->1, 1->2, 2->3, 3->4
    for (int i = 0; i < 64; i++) {
        u32 tile = groundTiles[i];
        u32 b0 = (tile & 0xFF) + 1;
        u32 b1 = ((tile >> 8) & 0xFF) + 1;
        u32 b2 = ((tile >> 16) & 0xFF) + 1;
        u32 b3 = ((tile >> 24) & 0xFF) + 1;
        bgTiles[16 + i] = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    // Tiles 5-8: ground2.png - remap palette indices: 0->5, 1->6
    for (int i = 0; i < 64; i++) {
        u32 tile = ground2Tiles[i];
        u32 b0 = (tile & 0xFF) == 0 ? 5 : 6;
        u32 b1 = ((tile >> 8) & 0xFF) == 0 ? 5 : 6;
        u32 b2 = ((tile >> 16) & 0xFF) == 0 ? 5 : 6;
        u32 b3 = ((tile >> 24) & 0xFF) == 0 ? 5 : 6;
        bgTiles[80 + i] = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    // Set BG0 control register (256 color mode = bit 7, screen base 16, char base 0)
    *((volatile u16*)0x04000008) = 0x0080 | (16 << 8);

    // Copy sprite palette to VRAM
    volatile u16* spritePalette = (volatile u16*)0x05000200;
    for (int i = 0; i < 256; i++) {
        spritePalette[i] = skellyPal[i];
    }

    // Copy player sprite to VRAM (char block 4 at 0x06010000)
    volatile u32* spriteTiles = (volatile u32*)0x06010000;
    for (int i = 0; i < 64; i++) {
        spriteTiles[i] = skellyTiles[i];
    }

    // Set up sprite 0 as 16x16
    *((volatile u16*)0x07000000) = (1 << 13);
    *((volatile u16*)0x07000002) = (1 << 14);
    *((volatile u16*)0x07000004) = 0;

    // Hide other sprites
    for (int i = 1; i < 128; i++) {
        *((volatile u16*)(0x07000000 + i * 8)) = 160;
    }

    // Initialize player from level spawn point
    Player player;
    player.x = currentLevel->playerSpawnX << FIXED_SHIFT;
    player.y = currentLevel->playerSpawnY << FIXED_SHIFT;
    player.vx = 0;
    player.vy = 0;
    player.onGround = 0;
    player.coyoteTime = 0;
    player.dashing = 0;
    player.dashCooldown = 0;
    player.facingRight = 1;
    player.prevKeys = 0;
    
    // Initialize camera
    Camera camera;
    camera.x = 0;
    camera.y = 0;

    // Background map pointer
    volatile u16* bgMap = (volatile u16*)0x06008000;

    // Game loop
    while (1) {
        vsync();

        u16 keys = getKeys();
        updatePlayer(&player, keys, currentLevel);
        updateCamera(&camera, &player, currentLevel);
        
        // Update background map based on camera position
        // Calculate which tiles are visible
        int startTileX = camera.x / 8;
        int startTileY = camera.y / 8;
        
        // GBA background is 32x32 tiles, we render from level data
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                int levelTileX = startTileX + x;
                int levelTileY = startTileY + y;
                
                u8 tileId = getTileAt(currentLevel, levelTileX, levelTileY);
                bgMap[y * 32 + x] = tileId;
            }
        }
        
        // Set background scroll registers
        *((volatile u16*)0x04000010) = camera.x % 8;  // BG0HOFS
        *((volatile u16*)0x04000012) = camera.y % 8;  // BG0VOFS
        
        drawGame(&player, &camera);
    }

    return 0;
}
