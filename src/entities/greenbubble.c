#include "greenbubble.h"
#include "player/player.h"
#include "player/state.h"
#include "core/game_math.h"
#include <string.h>  // For memset

void initGreenBubbleManager(GreenBubbleManager* manager) {
    manager->count = 0;
    memset(manager->bubbles, 0, sizeof(manager->bubbles));
}

void loadGreenBubblesFromLevel(GreenBubbleManager* manager, const Level* level) {
    manager->count = 0;

    // Scan level objects for green bubbles
    for (int i = 0; i < level->objectCount && manager->count < MAX_GREEN_BUBBLES; i++) {
        const LevelObject* obj = &level->objects[i];

        if (obj->type == OBJ_GREEN_BUBBLE) {
            GreenBubble* bubble = &manager->bubbles[manager->count];

            // Green bubbles are 8x8 (standard tile size)
            bubble->x = obj->x + 4;  // Center X (obj->x is top-left)
            bubble->y = obj->y + 4;  // Center Y
            bubble->width = 8;
            bubble->height = 8;
            bubble->active = 1;

            manager->count++;
        }
    }
}

void updateGreenBubbles(GreenBubbleManager* manager, Player* player) {
    // Get player hitbox in pixels
    int playerX = player->x >> FIXED_SHIFT;
    int playerY = player->y >> FIXED_SHIFT;
    int playerLeft = playerX - PLAYER_RADIUS_X;
    int playerRight = playerX + PLAYER_RADIUS_X;
    int playerTop = playerY - PLAYER_RADIUS_Y;
    int playerBottom = playerY + PLAYER_RADIUS_Y;

    // Skip collision entirely while in Boost state (being moved to bubble center)
    if (player->stateMachine.state == ST_BOOST) {
        return;
    }

    // Check collision with each bubble
    for (int i = 0; i < manager->count; i++) {
        GreenBubble* bubble = &manager->bubbles[i];
        if (!bubble->active) continue;

        // Bubble hitbox (centered on bubble center)
        int bubbleLeft = bubble->x - bubble->width / 2;
        int bubbleRight = bubble->x + bubble->width / 2;
        int bubbleTop = bubble->y - bubble->height / 2;
        int bubbleBottom = bubble->y + bubble->height / 2;

        // Check if this is the bubble player is currently using
        if (player->currentBubbleX == bubble->x && player->currentBubbleY == bubble->y) {
            // Check if player has moved away from this bubble
            int overlapping = (playerRight > bubbleLeft && playerLeft < bubbleRight &&
                             playerBottom > bubbleTop && playerTop < bubbleBottom);
            if (!overlapping) {
                // Player has left the bubble - clear tracking (Celeste CallDashEvents line 3437)
                player->currentBubbleX = -1000;
                player->currentBubbleY = -1000;
            } else {
                // Still overlapping current bubble - skip collision
                continue;
            }
        }

        // AABB collision check
        if (playerRight > bubbleLeft && playerLeft < bubbleRight &&
            playerBottom > bubbleTop && playerTop < bubbleBottom) {

            // Player collided with green bubble - trigger green boost
            playerGreenBoost(player, bubble->x, bubble->y);
            return;  // Only trigger one bubble per frame
        }
    }
}

void renderGreenBubbles(const GreenBubbleManager* manager, int cameraX, int cameraY) {
#ifndef DESKTOP_BUILD
    // Render all green bubbles using hardware OBJ sprites (sprites 80-111)
    volatile u16* oam = (volatile u16*)MEM_OAM;

    for (int i = 0; i < manager->count; i++) {
        const GreenBubble* bubble = &manager->bubbles[i];
        if (i >= 32) break;  // Only 32 bubble sprites available

        int spriteIndex = 80 + i;  // Sprites 80-111 for green bubbles
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
        // Tile 4 (filled square), palette bank 13 (green for green bubbles)
        spriteAttrs[2] = 4 | (0 << 10) | (13 << 12);  // Tile 4, priority 0, palette bank 13
    }
#else
    (void)manager;
    (void)cameraX;
    (void)cameraY;
#endif
}
