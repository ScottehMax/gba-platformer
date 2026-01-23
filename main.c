#include "gba.h"

// 16x16 red circle sprite in tile format (4 tiles of 8x8)
const unsigned char playerSprite[256] = {
    0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0,
    1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,
};

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

#define PLAYER_RADIUS 8
#define GROUND_HEIGHT 130

typedef struct {
    int x;  // Fixed-point
    int y;  // Fixed-point
    int vx; // Fixed-point
    int vy; // Fixed-point
    int onGround;
    int dashing;
    int dashCooldown;
    int facingRight;  // 1 = right, 0 = left
    u16 prevKeys;     // Previous frame keys for detecting button presses
} Player;

void vsync() {
    while ((*(volatile u16*)0x04000006) >= 160);
    while ((*(volatile u16*)0x04000006) < 160);
}

void updatePlayer(Player* player, u16 keys) {
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
    if ((pressed & KEY_A) && player->onGround) {
        player->vy = -JUMP_STRENGTH;
        player->onGround = 0;
    }

    // Apply gravity (but not during dash, to preserve dash trajectory)
    if (!player->onGround && player->dashing == 0) {
        player->vy += GRAVITY;
    }

    // Update position
    player->x += player->vx;
    player->y += player->vy;

    // Keep player on screen horizontally
    int screenX = player->x >> FIXED_SHIFT;
    if (screenX < PLAYER_RADIUS) {
        player->x = PLAYER_RADIUS << FIXED_SHIFT;
        player->vx = 0;
    }
    if (screenX > SCREEN_WIDTH - PLAYER_RADIUS) {
        player->x = (SCREEN_WIDTH - PLAYER_RADIUS) << FIXED_SHIFT;
        player->vx = 0;
    }

    // Ceiling collision
    int screenY = player->y >> FIXED_SHIFT;
    if (screenY - PLAYER_RADIUS < 0) {
        player->y = PLAYER_RADIUS << FIXED_SHIFT;
        player->vy = 0;
    }

    // Ground collision
    if (screenY + PLAYER_RADIUS >= GROUND_HEIGHT) {
        player->y = (GROUND_HEIGHT - PLAYER_RADIUS) << FIXED_SHIFT;
        player->vy = 0;
        player->onGround = 1;

        // End dash when landing
        if (player->dashing > 0) {
            player->dashing = 0;
        }
        // Horizontal velocity is preserved automatically
    } else {
        player->onGround = 0;
    }

    // Update previous keys for next frame
    player->prevKeys = keys;
}

void drawGame(Player* player) {
    // Update player sprite position - convert from fixed-point to screen coordinates
    int screenX = (player->x >> FIXED_SHIFT) - 8;  // Center the 16x16 sprite
    int screenY = (player->y >> FIXED_SHIFT) - 8;  // Center the 16x16 sprite

    // Clamp to visible area
    if (screenX < 0) screenX = 0;
    if (screenY < 0) screenY = 0;
    if (screenX > 239) screenX = 239;
    if (screenY > 159) screenY = 159;

    // Update sprite position (16x16, 256-color)
    *((volatile u16*)0x07000000) = screenY | (1 << 13);   // attr0: Y + 256-color mode
    *((volatile u16*)0x07000002) = screenX | (1 << 14);   // attr1: X + size 16x16
    *((volatile u16*)0x07000004) = 0;                      // attr2: tile 0
}

int main() {
    // Mode 0 with BG0 and sprites enabled
    REG_DISPCNT = VIDEOMODE_0 | BG0_ENABLE | OBJ_ENABLE | OBJ_1D_MAP;

    // Set up background palette
    setPalette(0, COLOR(15, 20, 31));  // Sky color (light blue)
    setPalette(1, COLOR(0, 31, 0));     // Green ground

    // Create background tiles (tile 0 = sky, tile 1 = ground)
    // Each tile is 8x8 pixels in 8bpp mode = 64 bytes = 32 halfwords
    volatile u16* bgTiles = (volatile u16*)0x06000000;

    // Tile 0: Sky (all pixels palette index 0) - 32 halfwords of 0x0000
    for (int i = 0; i < 32; i++) {
        bgTiles[i] = 0x0000;
    }

    // Tile 1: Ground (all pixels palette index 1) - 32 halfwords of 0x0101
    for (int i = 0; i < 32; i++) {
        bgTiles[32 + i] = 0x0101;
    }

    // Set up tile map at screen base block 16
    volatile u16* bgMap = (volatile u16*)0x06008000;
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            if (y >= GROUND_HEIGHT / 8) {
                bgMap[y * 32 + x] = 1;  // Ground tile
            } else {
                bgMap[y * 32 + x] = 0;  // Sky tile
            }
        }
    }

    // Set BG0 control register (256 color mode = bit 7, screen base 16, char base 0)
    *((volatile u16*)0x04000008) = 0x0080 | (16 << 8);

    // Set sprite palette
    *((volatile u16*)0x05000200) = 0x0000;        // Index 0: transparent
    *((volatile u16*)0x05000202) = COLOR(31, 0, 0);  // Index 1: red

    // Copy player sprite to VRAM (char block 4 at 0x06010000)
    // 16x16 sprite = 4 tiles (8x8 each), 256 bytes total
    volatile u16* spriteTiles = (volatile u16*)0x06010000;
    const u16* spriteData = (const u16*)playerSprite;
    for (int i = 0; i < 128; i++) {  // 256 bytes = 128 halfwords
        spriteTiles[i] = spriteData[i];
    }

    // Set up sprite 0 as 16x16
    *((volatile u16*)0x07000000) = (1 << 13);   // attr0: Y=0, 256-color, square
    *((volatile u16*)0x07000002) = (1 << 14);   // attr1: X=0, size=16x16
    *((volatile u16*)0x07000004) = 0;           // attr2: tile 0

    // Hide other sprites
    for (int i = 1; i < 128; i++) {
        *((volatile u16*)(0x07000000 + i * 8)) = 160;
    }

    // Initialize player (in fixed-point coordinates)
    Player player;
    player.x = (SCREEN_WIDTH / 2) << FIXED_SHIFT;
    player.y = (GROUND_HEIGHT - PLAYER_RADIUS) << FIXED_SHIFT;
    player.vx = 0;
    player.vy = 0;
    player.onGround = 1;
    player.dashing = 0;
    player.dashCooldown = 0;
    player.facingRight = 1;  // Start facing right
    player.prevKeys = 0;      // No keys pressed initially

    // Game loop
    while (1) {
        vsync();

        u16 keys = getKeys();
        updatePlayer(&player, keys);
        drawGame(&player);
    }

    return 0;
}
