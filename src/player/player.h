#ifndef PLAYER_H
#define PLAYER_H

#include <tonc.h>
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

#endif
