#ifndef PLAYER_H
#define PLAYER_H

#include "core/game_types.h"
#include "core/game_math.h"
#include "collision/collision.h"

static inline void clearPlayerDashTrail(Player* player) {
    player->trailIndex = 0;
    player->trailTimer = 0;
    player->trailFadeTimer = TRAIL_LENGTH * 8;
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        player->trailX[i] = -1000 << FIXED_SHIFT;
        player->trailY[i] = -1000 << FIXED_SHIFT;
        player->trailFacing[i] = player->facingRight;
    }
}

static inline void hidePlayerDashTrailPositions(Player* player) {
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        player->trailX[i] = -1000 << FIXED_SHIFT;
        player->trailY[i] = -1000 << FIXED_SHIFT;
    }
}

/**
 * Initialize a player at the level spawn point
 *
 * @param player The player to initialize
 * @param level The level containing spawn point
 */
void initPlayer(Player* player, const Level* level);

/**
 * Update player physics, input, and state
 *
 * @param player The player to update
 * @param keys Current frame key input
 * @param level The level for collision detection
 */
void updatePlayer(Player* player, u16 keys, const Level* level);

/**
 * Refill player's dashes to maximum (Celeste Player.cs line 2002)
 *
 * @param player The player to refill
 * @return 1 if dashes were refilled, 0 if already at max
 */
int refillDash(Player* player);

/**
 * Refill player's stamina to maximum (Celeste Player.cs line 2025)
 *
 * @param player The player to refill
 */
void refillStamina(Player* player);

/**
 * Normal spring bounce - launches player upward (Celeste Player.cs line 1844)
 *
 * @param player The player to bounce
 * @param fromY The Y position to snap player to (spring top) in pixels
 */
void playerBounce(Player* player, int fromY);

/**
 * Super spring bounce - launches player higher and cancels horizontal momentum (Celeste Player.cs line 1878)
 *
 * @param player The player to bounce
 * @param fromY The Y position to snap player to (spring top) in pixels
 */
void playerSuperBounce(Player* player, int fromY);

/**
 * Side spring bounce - launches player horizontally (Celeste Player.cs line 1919)
 *
 * @param player The player to bounce
 * @param dir Direction to launch: 1=right, -1=left
 * @param fromX The X position to snap player to (spring side) in pixels
 * @param fromY The Y position to snap player to (spring vertical center) in pixels
 */
void playerSideBounce(Player* player, int dir, int fromX, int fromY);

/**
 * Red boost - enters boost state targeting red bubble center (Celeste Player.cs line 3779)
 *
 * @param player The player to boost
 * @param centerX The bubble center X position in pixels
 * @param centerY The bubble center Y position in pixels
 */
void playerRedBoost(Player* player, int centerX, int centerY);
void playerGreenBoost(Player* player, int centerX, int centerY);

#endif
