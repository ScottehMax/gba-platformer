#include "redbubble.h"
#include "player/player.h"
#include "player/state.h"
#include "core/game_math.h"
#include <string.h>  // For memset

void initRedBubbleManager(RedBubbleManager* manager) {
    manager->count = 0;
    memset(manager->bubbles, 0, sizeof(manager->bubbles));
}

void loadRedBubblesFromLevel(RedBubbleManager* manager, const Level* level) {
    manager->count = 0;

    // Scan level objects for red bubbles
    for (int i = 0; i < level->objectCount && manager->count < MAX_RED_BUBBLES; i++) {
        const LevelObject* obj = &level->objects[i];

        if (obj->type == OBJ_RED_BUBBLE) {
            RedBubble* bubble = &manager->bubbles[manager->count];

            // Red bubbles are 8x8 (standard tile size)
            bubble->x = obj->x + 4;  // Center X (obj->x is top-left)
            bubble->y = obj->y + 4;  // Center Y
            bubble->width = 8;
            bubble->height = 8;
            bubble->active = 1;

            manager->count++;
        }
    }
}

void updateRedBubbles(RedBubbleManager* manager, Player* player) {
    // Only check collision when player is not boosting or dashing from a bubble
    // In Celeste, CurrentBooster tracks the active bubble and prevents re-trigger
    // We simplify by skipping collision during boost and dash states
    if (player->stateMachine.state == ST_BOOST ||
        player->stateMachine.state == ST_DASH ||
        player->stateMachine.state == ST_RED_DASH) {
        return;
    }

    // Get player hitbox in pixels
    int playerX = player->x >> FIXED_SHIFT;
    int playerY = player->y >> FIXED_SHIFT;
    int playerLeft = playerX - PLAYER_RADIUS_X;
    int playerRight = playerX + PLAYER_RADIUS_X;
    int playerTop = playerY - PLAYER_RADIUS_Y;
    int playerBottom = playerY + PLAYER_RADIUS_Y;

    // Check collision with each bubble
    for (int i = 0; i < manager->count; i++) {
        RedBubble* bubble = &manager->bubbles[i];
        if (!bubble->active) continue;

        // Bubble hitbox (centered on bubble center)
        int bubbleLeft = bubble->x - bubble->width / 2;
        int bubbleRight = bubble->x + bubble->width / 2;
        int bubbleTop = bubble->y - bubble->height / 2;
        int bubbleBottom = bubble->y + bubble->height / 2;

        // AABB collision check
        if (playerRight > bubbleLeft && playerLeft < bubbleRight &&
            playerBottom > bubbleTop && playerTop < bubbleBottom) {

            // Player collided with red bubble - trigger red boost
            playerRedBoost(player, bubble->x, bubble->y);
            return;  // Only trigger one bubble per frame
        }
    }
}

void renderRedBubbles(const RedBubbleManager* manager, int cameraX, int cameraY) {
#ifndef DESKTOP_BUILD
    // Render all red bubbles using hardware OBJ sprites (sprites 48-79)
    volatile u16* oam = (volatile u16*)MEM_OAM;

    for (int i = 0; i < manager->count; i++) {
        const RedBubble* bubble = &manager->bubbles[i];
        if (i >= 32) break;  // Only 32 bubble sprites available

        int spriteIndex = 48 + i;  // Sprites 48-79 for red bubbles
        volatile u16* spriteAttrs = &oam[spriteIndex * 4];

        // Check if bubble is active and onscreen
        if (!bubble->active) {
            // Hide inactive bubbles
            spriteAttrs[0] = 160;  // Y offscreen
            continue;
        }

        // Convert to screen coordinates (bubble center to top-left for rendering)
        int screenX = bubble->x - bubble->width / 2 - cameraX;
        int screenY = bubble->y - bubble->height / 2 - cameraY;

        // Check if offscreen (with margin for sprite size)
        if (screenX + bubble->width < 0 || screenX >= 240 ||
            screenY + bubble->height < 0 || screenY >= 160) {
            // Hide offscreen bubbles
            spriteAttrs[0] = 160;  // Y offscreen
            continue;
        }

        // OAM Attribute 0: Y position and shape (square)
        spriteAttrs[0] = (screenY & 0xFF) | (0 << 14);  // Y position, square shape

        // OAM Attribute 1: X position and size (8x8)
        spriteAttrs[1] = (screenX & 0x1FF) | (0 << 14);  // X position, 8x8 size

        // OAM Attribute 2: Tile index and palette
        // Tile 4 (filled square), palette bank 12 (orange for red bubbles)
        spriteAttrs[2] = 4 | (0 << 10) | (12 << 12);  // Tile 4, priority 0, palette bank 12
    }
#else
    (void)manager;
    (void)cameraX;
    (void)cameraY;
#endif
}
