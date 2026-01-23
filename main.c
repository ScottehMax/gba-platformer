#include "gba.h"

// Use fixed-point math (8 bits fractional) for smooth sub-pixel movement
#define FIXED_SHIFT 8
#define FIXED_ONE (1 << FIXED_SHIFT)

#define GRAVITY (FIXED_ONE / 2)
#define JUMP_STRENGTH (FIXED_ONE * 5)
#define MAX_SPEED (FIXED_ONE * 2)
#define ACCELERATION (FIXED_ONE)
#define FRICTION (FIXED_ONE / 4)
#define AIR_FRICTION (FIXED_ONE / 8)

#define PLAYER_RADIUS 8
#define GROUND_HEIGHT 130

typedef struct {
    int x;  // Fixed-point
    int y;  // Fixed-point
    int vx; // Fixed-point
    int vy; // Fixed-point
    int onGround;
} Player;

void vsync() {
    while ((*(volatile u16*)0x04000006) >= 160);
    while ((*(volatile u16*)0x04000006) < 160);
}

void clearScreen(u8 colorIndex) {
    // Create a 32-bit fill value (4 pixels at once)
    u32 fillValue = (colorIndex << 24) | (colorIndex << 16) | (colorIndex << 8) | colorIndex;
    // Mode 4 screen is 240x160 = 38400 bytes = 9600 words (32-bit)
    dmaFill((void*)backBuffer, fillValue, 9600);
}

void updatePlayer(Player* player, u16 keys) {
    // Horizontal movement with acceleration
    if (keys & KEY_LEFT) {
        player->vx -= ACCELERATION;
        if (player->vx < -MAX_SPEED) {
            player->vx = -MAX_SPEED;
        }
    } else if (keys & KEY_RIGHT) {
        player->vx += ACCELERATION;
        if (player->vx > MAX_SPEED) {
            player->vx = MAX_SPEED;
        }
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

    // Jump
    if ((keys & KEY_A) && player->onGround) {
        player->vy = -JUMP_STRENGTH;
        player->onGround = 0;
    }

    // Apply gravity
    if (!player->onGround) {
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

    // Ground collision
    int screenY = player->y >> FIXED_SHIFT;
    if (screenY + PLAYER_RADIUS >= GROUND_HEIGHT) {
        player->y = (GROUND_HEIGHT - PLAYER_RADIUS) << FIXED_SHIFT;
        player->vy = 0;
        player->onGround = 1;
    }
}

void drawGame(Player* player) {
    // Clear sky area only (optimization - don't redraw ground every frame)
    u32 skyFill = (COLOR_SKY << 24) | (COLOR_SKY << 16) | (COLOR_SKY << 8) | COLOR_SKY;
    dmaFill((void*)backBuffer, skyFill, (SCREEN_WIDTH * GROUND_HEIGHT) / 4);

    // Draw ground as solid fill
    u32 groundFill = (COLOR_GREEN << 24) | (COLOR_GREEN << 16) | (COLOR_GREEN << 8) | COLOR_GREEN;
    dmaFill((void*)(backBuffer + SCREEN_WIDTH * GROUND_HEIGHT), groundFill,
            (SCREEN_WIDTH * (SCREEN_HEIGHT - GROUND_HEIGHT)) / 4);

    // Draw player (red circle) - convert from fixed-point to screen coordinates
    int screenX = player->x >> FIXED_SHIFT;
    int screenY = player->y >> FIXED_SHIFT;
    drawCircle(screenX, screenY, PLAYER_RADIUS, COLOR_RED);
}

int main() {
    // Set up video mode (Mode 4 with page flipping)
    REG_DISPCNT = VIDEOMODE_4 | BGMODE_2;

    // Initialize palette
    setPalette(COLOR_BLACK, COLOR(0, 0, 0));
    setPalette(COLOR_RED, COLOR(31, 0, 0));
    setPalette(COLOR_GREEN, COLOR(0, 31, 0));
    setPalette(COLOR_SKY, COLOR(15, 20, 31));

    // Initialize player (in fixed-point coordinates)
    Player player;
    player.x = (SCREEN_WIDTH / 2) << FIXED_SHIFT;
    player.y = (GROUND_HEIGHT - PLAYER_RADIUS) << FIXED_SHIFT;
    player.vx = 0;
    player.vy = 0;
    player.onGround = 1;

    // Game loop
    while (1) {
        vsync();

        u16 keys = getKeys();
        updatePlayer(&player, keys);
        drawGame(&player);

        // Flip to the buffer we just drew
        flipPage();
    }

    return 0;
}
