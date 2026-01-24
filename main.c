#include "gba.h"
#include "skelly.h"
#include "grassy_stone.h"
#include "level3.h"
#include "text.h"

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
#define TRAIL_LENGTH 10

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

    // Dash trail
    int trailX[TRAIL_LENGTH];  // Fixed-point positions
    int trailY[TRAIL_LENGTH];
    int trailFacing[TRAIL_LENGTH];
    int trailIndex;  // Current trail buffer index
    int trailTimer;  // Frames since last trail update
    int trailFadeTimer;  // Frames since dash ended (for gradual fade)
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
    // Tile 0 is empty/air
    // All other tiles (1-55) are solid
    return tileId >= 1;
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
        player->trailFadeTimer = 0; // Reset fade timer for new dash

        // Clear old trail positions
        for (int i = 0; i < TRAIL_LENGTH; i++) {
            player->trailX[i] = -1000 << FIXED_SHIFT;
            player->trailY[i] = -1000 << FIXED_SHIFT;
        }

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
        // Start fade timer when dash ends
        if (player->dashing == 0) {
            player->trailTimer = 0;
            player->trailFadeTimer = 0;
        }
    }

    // Update trail fade after dash ends (10 sprites * 2 frames each = 20 frames)
    if (player->dashing == 0 && player->trailFadeTimer < TRAIL_LENGTH * 2) {
        player->trailFadeTimer++;
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

    // Update dash trail (record every 2 frames for spacing)
    // Continue recording for a bit after dash ends to fill the trail buffer
    if (player->dashing > 0 || player->trailFadeTimer < 10) {
        player->trailTimer++;
        if (player->trailTimer >= 2) {
            player->trailTimer = 0;
            player->trailIndex = (player->trailIndex + 1) % TRAIL_LENGTH;
            player->trailX[player->trailIndex] = player->x;
            player->trailY[player->trailIndex] = player->y;
            player->trailFacing[player->trailIndex] = player->facingRight;
        }
    }

    // Update previous keys for next frame
    player->prevKeys = keys;
}

void drawGame(Player* player, Camera* camera) {
    // Draw dash trail (sprites 1-10)
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        // Calculate how many frames since dash ended (for gradual fade)
        int fadeSteps = player->trailFadeTimer / 2;  // Hide one sprite every 2 frames

        // Hide this sprite if it's been faded out (fade from back to front)
        // i=0 is newest (closest to player), i=9 is oldest (farthest)
        // We want to hide oldest first, so hide when i >= (TRAIL_LENGTH - fadeSteps)
        if (player->dashing == 0 && i >= (TRAIL_LENGTH - fadeSteps)) {
            *((volatile u16*)(0x07000000 + (i + 1) * 8)) = 160 << 0;  // Hide sprite
            continue;
        }

        // Only show trail if actively dashing or still fading
        if (player->dashing > 0 || player->trailFadeTimer < TRAIL_LENGTH * 2) {
            // Show every position to maximize visible trail
            int trailIdx = (player->trailIndex - i + TRAIL_LENGTH) % TRAIL_LENGTH;
            int trailScreenX = (player->trailX[trailIdx] >> FIXED_SHIFT) - camera->x - 8;
            int trailScreenY = (player->trailY[trailIdx] >> FIXED_SHIFT) - camera->y - 8;

            // Hide if off screen
            if (trailScreenX < -1000 || trailScreenX > 239 || trailScreenY < -1000 || trailScreenY > 159) {
                *((volatile u16*)(0x07000000 + (i + 1) * 8)) = 160 << 0;  // Hide sprite
            } else {
                // Calculate sprite "age" for palette selection (older = lighter/more transparent)
                // i=0 is newest, i=9 is oldest
                // fadeSteps adds extra age during fadeout for smooth transition
                int spriteAge = i + fadeSteps; // Combine position and fade
                int paletteNum = spriteAge; // 0-9 range for 10 palettes
                if (paletteNum > 9) paletteNum = 9; // Clamp to available palettes (1-10)
                if (paletteNum < 0) paletteNum = 0;

                // Use progressively lighter palettes (1-10) for very gradual fading effect
                *((volatile u16*)(0x07000000 + (i + 1) * 8)) = (trailScreenY & 0xFF) | (1 << 10);  // Semi-transparent mode
                *((volatile u16*)(0x07000000 + (i + 1) * 8 + 2)) = trailScreenX | (1 << 14) | (player->trailFacing[trailIdx] ? 0 : (1 << 12));
                *((volatile u16*)(0x07000000 + (i + 1) * 8 + 4)) = ((paletteNum + 1) << 12);  // tile 0, palette 1-10 based on age
            }
        } else {
            *((volatile u16*)(0x07000000 + (i + 1) * 8)) = 160 << 0;  // Hide sprite
        }
    }

    // Update player sprite position - convert from world to screen coordinates
    int screenX = (player->x >> FIXED_SHIFT) - camera->x - 8;  // Center the 16x16 sprite
    int screenY = (player->y >> FIXED_SHIFT) - camera->y - 8;  // Center the 16x16 sprite

    // Clamp to visible area
    if (screenX < -16) screenX = -16;
    if (screenY < -16) screenY = -16;
    if (screenX > 239) screenX = 239;
    if (screenY > 159) screenY = 159;

    // Update sprite position (16x16, 16-color mode, palette 0, normal/opaque mode)
    // Clear bits 10-11 to ensure normal mode (not semi-transparent)
    *((volatile u16*)0x07000000) = (screenY & 0xFF) | (0 << 10);   // attr0: Y position (8 bits), bits 10-11 = 00 (normal mode)
    *((volatile u16*)0x07000002) = screenX | (1 << 14) | (player->facingRight ? 0 : (1 << 12));   // attr1: X + size 16x16 + H-flip if facing left
    *((volatile u16*)0x07000004) = 0;                      // attr2: tile 0, palette 0
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
    
    // Mode 0 with BG0, BG1 and sprites enabled
    REG_DISPCNT = VIDEOMODE_0 | BG0_ENABLE | (1 << 9) | OBJ_ENABLE | OBJ_1D_MAP;  // BG1_ENABLE = bit 9

    // Enable alpha blending for sprites
    // BLDCNT: Effect=Alpha blend (bit 6), NO global OBJ target (sprites set semi-transparent individually)
    // 2nd target=BG0+BG1+BD (bits 8,9,13) - what semi-transparent sprites blend with
    *((volatile u16*)0x04000050) = (1 << 6) | (1 << 8) | (1 << 9) | (1 << 13);
    // BLDALPHA: Set blend coefficients EVA (sprite) and EVB (background) - must sum to 16 or less
    *((volatile u16*)0x04000052) = (7 << 0) | (9 << 8);  // ~44% trail, ~56% background (more transparent)

    // Set up background palette
    volatile u16* bgPalette = (volatile u16*)0x05000000;

    // Copy Grassy_stone palette to background palette
    for (int i = 0; i < 256; i++) {
        bgPalette[i] = Grassy_stonePal[i];
    }

    // Override palette index 0 with dark blue for sky
    bgPalette[0] = COLOR(3, 6, 15);
    
    // Load font palette to palette slot 1 (colors 16-31) to avoid conflict with game tiles
    for (int i = 0; i < 16; i++) {
        bgPalette[16 + i] = tinypixiePal[i];
    }

    // Create background tiles in VRAM
    volatile u32* bgTiles = (volatile u32*)0x06000000;

    // Tile 0: Sky (all pixels palette index 0)
    for (int i = 0; i < 16; i++) {
        bgTiles[i] = 0x00000000;
    }

    // Copy all Grassy_stone tiles (starting at tile index 1)
    // Grassy_stoneTilesLen is the byte length, divide by 4 to get u32 count
    int tileCount = Grassy_stoneTilesLen / 4;
    for (int i = 0; i < tileCount; i++) {
        bgTiles[16 + i] = Grassy_stoneTiles[i];
    }

    // Set BG0 control register (256 color mode = bit 7, screen base 16, char base 0)
    *((volatile u16*)0x04000008) = 0x0080 | (16 << 8);
    
    // Initialize background text system (BG1 - uses char block 1)
    init_bg_text();

    // Copy sprite palette to VRAM
    volatile u16* spritePalette = (volatile u16*)0x05000200;

    // Palette 0: Normal sprite colors
    for (int i = 0; i < 16; i++) {
        spritePalette[i] = skellyPal[i];
    }

    // Palettes 1-10: Light blue/cyan silhouettes with varying opacity for dash trail fade
    // Create 10 palettes with very gradual color transitions for smooth fade effect
    for (int pal = 0; pal < 10; pal++) {
        for (int i = 0; i < 16; i++) {
            if (i == 0) {
                spritePalette[(pal + 1) * 16 + i] = 0;  // Index 0 is transparent
            } else {
                // Very gradual fade from bright to light blue
                // Palette 1: Most opaque (10, 20, 31)
                // Palette 10: Lightest (2, 6, 16)
                int r = 10 - (pal * 8) / 10;  // 10 -> 2
                int g = 20 - (pal * 14) / 10; // 20 -> 6
                int b = 31 - (pal * 15) / 10; // 31 -> 16
                if (r < 2) r = 2;
                if (g < 6) g = 6;
                if (b < 16) b = 16;
                spritePalette[(pal + 1) * 16 + i] = COLOR(r, g, b);
            }
        }
    }

    // Copy player sprite to VRAM (char block 4 at 0x06010000)
    volatile u32* spriteTiles = (volatile u32*)0x06010000;
    for (int i = 0; i < 32; i++) {  // 16-color mode: 4 tiles, 8 u32s per tile
        spriteTiles[i] = skellyTiles[i];
    }

    // Set up sprite 0 as 16x16, 16-color mode
    *((volatile u16*)0x07000000) = 0;
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
    player.trailIndex = 0;
    player.trailTimer = 0;
    player.trailFadeTimer = TRAIL_LENGTH * 2;  // Start fully faded

    // Initialize trail positions off-screen
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        player.trailX[i] = -1000 << FIXED_SHIFT;
        player.trailY[i] = -1000 << FIXED_SHIFT;
        player.trailFacing[i] = player.facingRight;
    }
    
    // Initialize camera
    Camera camera;
    camera.x = 0;
    camera.y = 0;

    // Background map pointer
    volatile u16* bgMap = (volatile u16*)0x06008000;

    // Frame counter for demo
    int frameCount = 0;
    char posXStr[32] = "X: 0";
    char posYStr[32] = "Y: 0";
    char velXStr[32] = "VX: 0";
    char velYStr[32] = "VY: 0";
    char groundStr[32] = "Ground: No";
    
    // Allocate text slots once
    int titleSlot = draw_bg_text_auto("Debug Info", 1, 1);
    int posXSlot = draw_bg_text_auto(posXStr, 1, 2);
    int posYSlot = draw_bg_text_auto(posYStr, 1, 3);
    int velXSlot = draw_bg_text_auto(velXStr, 1, 4);
    int velYSlot = draw_bg_text_auto(velYStr, 1, 5);
    int groundSlot = draw_bg_text_auto(groundStr, 1, 6);

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
        
        // Update debug info every frame
        // Position X (in pixels)
        int posX = player.x >> FIXED_SHIFT;
        int posY = player.y >> FIXED_SHIFT;
        
        // Simple int to string for X position
        posXStr[0] = 'X';
        posXStr[1] = ':';
        posXStr[2] = ' ';
        int idx = 3;
        if (posX < 0) {
            posXStr[idx++] = '-';
            posX = -posX;
        }
        int digits[5];
        int numDigits = 0;
        if (posX == 0) {
            digits[numDigits++] = 0;
        } else {
            int temp = posX;
            while (temp > 0) {
                digits[numDigits++] = temp % 10;
                temp /= 10;
            }
        }
        for (int i = numDigits - 1; i >= 0; i--) {
            posXStr[idx++] = '0' + digits[i];
        }
        posXStr[idx] = '\0';
        draw_bg_text_slot(posXStr, 1, 2, posXSlot);
        
        // Position Y
        posY = player.y >> FIXED_SHIFT;
        posYStr[0] = 'Y';
        posYStr[1] = ':';
        posYStr[2] = ' ';
        idx = 3;
        if (posY < 0) {
            posYStr[idx++] = '-';
            posY = -posY;
        }
        numDigits = 0;
        if (posY == 0) {
            digits[numDigits++] = 0;
        } else {
            int temp = posY;
            while (temp > 0) {
                digits[numDigits++] = temp % 10;
                temp /= 10;
            }
        }
        for (int i = numDigits - 1; i >= 0; i--) {
            posYStr[idx++] = '0' + digits[i];
        }
        posYStr[idx] = '\0';
        draw_bg_text_slot(posYStr, 1, 3, posYSlot);
        
        // Velocity X (fixed point to int)
        int velX = player.vx >> FIXED_SHIFT;
        velXStr[0] = 'V';
        velXStr[1] = 'X';
        velXStr[2] = ':';
        velXStr[3] = ' ';
        idx = 4;
        if (velX < 0) {
            velXStr[idx++] = '-';
            velX = -velX;
        }
        numDigits = 0;
        if (velX == 0) {
            digits[numDigits++] = 0;
        } else {
            int temp = velX;
            while (temp > 0) {
                digits[numDigits++] = temp % 10;
                temp /= 10;
            }
        }
        for (int i = numDigits - 1; i >= 0; i--) {
            velXStr[idx++] = '0' + digits[i];
        }
        velXStr[idx] = '\0';
        draw_bg_text_slot(velXStr, 1, 4, velXSlot);
        
        // Velocity Y
        int velY = player.vy >> FIXED_SHIFT;
        velYStr[0] = 'V';
        velYStr[1] = 'Y';
        velYStr[2] = ':';
        velYStr[3] = ' ';
        idx = 4;
        if (velY < 0) {
            velYStr[idx++] = '-';
            velY = -velY;
        }
        numDigits = 0;
        if (velY == 0) {
            digits[numDigits++] = 0;
        } else {
            int temp = velY;
            while (temp > 0) {
                digits[numDigits++] = temp % 10;
                temp /= 10;
            }
        }
        for (int i = numDigits - 1; i >= 0; i--) {
            velYStr[idx++] = '0' + digits[i];
        }
        velYStr[idx] = '\0';
        draw_bg_text_slot(velYStr, 1, 5, velYSlot);
        
        // On ground status
        if (player.onGround) {
            draw_bg_text_slot("Ground: Yes", 1, 6, groundSlot);
        } else {
            draw_bg_text_slot("Ground: No", 1, 6, groundSlot);
        }
        
        frameCount++;
    }

    return 0;
}
