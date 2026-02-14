#ifndef PLAYER_H
#define PLAYER_H

#ifdef DESKTOP_BUILD
#include "desktop/desktop_stubs.h"
#else
#include <tonc.h>
#endif
#include "core/game_types.h"
#include "core/game_math.h"
#include "collision/collision.h"

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
 * @param fromY The Y position to snap player to (spring top)
 */
void playerBounce(Player* player, float fromY);

/**
 * Super spring bounce - launches player higher and cancels horizontal momentum (Celeste Player.cs line 1878)
 *
 * @param player The player to bounce
 * @param fromY The Y position to snap player to (spring top)
 */
void playerSuperBounce(Player* player, float fromY);

/**
 * Side spring bounce - launches player horizontally (Celeste Player.cs line 1919)
 *
 * @param player The player to bounce
 * @param dir Direction to launch: 1=right, -1=left
 * @param fromX The X position to snap player to (spring side)
 * @param fromY The Y position to snap player to (spring vertical center)
 */
void playerSideBounce(Player* player, int dir, float fromX, float fromY);

#endif
