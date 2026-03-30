#include "redbubble.h"
#include "entity_common.h"
#include "player/player.h"
#include "player/state.h"
#include "core/game_math.h"
#include "core/vram_layout.h"
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
    AABB pBox = playerAABB(player);

    // Skip collision entirely while in Boost state (being moved to bubble center)
    if (player->stateMachine.state == ST_BOOST) {
        return;
    }

    // Check collision with each bubble
    for (int i = 0; i < manager->count; i++) {
        RedBubble* bubble = &manager->bubbles[i];
        if (!bubble->active) continue;

        AABB bBox = entityAABBCentered(bubble->x, bubble->y,
                                       bubble->width / 2, bubble->height / 2);

        // Check if this is the bubble player is currently using
        if (player->currentBubbleX == bubble->x && player->currentBubbleY == bubble->y) {
            if (!aabbOverlap(&pBox, &bBox)) {
                // Player has left the bubble - clear tracking (Celeste CallDashEvents line 3437)
                player->currentBubbleX = -1000;
                player->currentBubbleY = -1000;
            } else {
                // Still overlapping current bubble - skip collision
                continue;
            }
        }

        if (aabbOverlap(&pBox, &bBox)) {
            // Player collided with red bubble - trigger red boost
            playerRedBoost(player, bubble->x, bubble->y);
            return;  // Only trigger one bubble per frame
        }
    }
}

void renderRedBubbles(const RedBubbleManager* manager, int cameraX, int cameraY) {
#ifndef DESKTOP_BUILD
    // Render all red bubbles using hardware OBJ sprites
    volatile u16* oam = (volatile u16*)MEM_OAM;

    for (int i = 0; i < manager->count; i++) {
        const RedBubble* bubble = &manager->bubbles[i];
        if (i >= OAM_RED_BUBBLE_COUNT) break;

        int spriteIndex = OAM_RED_BUBBLE_BASE + i;
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
        spriteAttrs[2] = TILE_OBJ_ENTITY | (0 << 10) | (PAL_OBJ_RED_BUBBLE << 12);
    }
#else
    (void)manager;
    (void)cameraX;
    (void)cameraY;
#endif
}
