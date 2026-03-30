#ifndef SPRING_H
#define SPRING_H

#include "core/game_types.h"
#include "level/level.h"

#define MAX_SPRINGS 32

typedef enum {
    SPRING_NORMAL,       // Upward spring (Celeste Player.cs line 1844)
    SPRING_SUPER,        // Super upward spring (Celeste Player.cs line 1878)
    SPRING_WALL_LEFT,    // Left wall spring (Celeste Player.cs line 1919)
    SPRING_WALL_RIGHT    // Right wall spring (Celeste Player.cs line 1919)
} SpringType;

typedef struct {
    SpringType type;
    int x;              // Position in pixels
    int y;
    int width;          // Hitbox width in pixels
    int height;         // Hitbox height in pixels
    int active;         // 1 if spring exists, 0 if inactive
} Spring;

typedef struct {
    Spring springs[MAX_SPRINGS];
    int count;
} SpringManager;

/**
 * Initialize spring manager
 *
 * @param manager The spring manager to initialize
 */
void initSpringManager(SpringManager* manager);

/**
 * Load springs from level objects
 *
 * @param manager The spring manager to populate
 * @param level The level containing spring objects
 */
void loadSpringsFromLevel(SpringManager* manager, const Level* level);

/**
 * Check collision between player and springs, trigger bounce if hit
 *
 * @param manager The spring manager
 * @param player The player to check collision with
 */
void updateSprings(SpringManager* manager, Player* player);

/**
 * Render all active springs
 *
 * @param manager The spring manager
 * @param cameraX Camera X position in pixels
 * @param cameraY Camera Y position in pixels
 */
void renderSprings(const SpringManager* manager, int cameraX, int cameraY);

#endif
