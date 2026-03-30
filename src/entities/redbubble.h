#ifndef REDBUBBLE_H
#define REDBUBBLE_H

#include "core/game_types.h"
#include "level/level.h"

#define MAX_RED_BUBBLES 32

typedef struct {
    int x;              // Center position in pixels
    int y;
    int width;          // Hitbox width in pixels
    int height;         // Hitbox height in pixels
    int active;         // 1 if bubble exists, 0 if inactive
} RedBubble;

typedef struct {
    RedBubble bubbles[MAX_RED_BUBBLES];
    int count;
} RedBubbleManager;

/**
 * Initialize red bubble manager
 *
 * @param manager The red bubble manager to initialize
 */
void initRedBubbleManager(RedBubbleManager* manager);

/**
 * Load red bubbles from level objects
 *
 * @param manager The red bubble manager to populate
 * @param level The level containing red bubble objects
 */
void loadRedBubblesFromLevel(RedBubbleManager* manager, const Level* level);

/**
 * Check collision between player and red bubbles, trigger boost if hit
 *
 * @param manager The red bubble manager
 * @param player The player to check collision with
 */
void updateRedBubbles(RedBubbleManager* manager, Player* player);

/**
 * Render all active red bubbles
 *
 * @param manager The red bubble manager
 * @param cameraX Camera X position in pixels
 * @param cameraY Camera Y position in pixels
 */
void renderRedBubbles(const RedBubbleManager* manager, int cameraX, int cameraY);

#endif
