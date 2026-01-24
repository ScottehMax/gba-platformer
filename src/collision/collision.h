#ifndef COLLISION_H
#define COLLISION_H

#include "gba.h"
#include "src/core/game_types.h"
#include "src/core/game_math.h"
#include "src/level/level.h"

/**
 * Perform horizontal collision sweep
 * Moves player horizontally and resolves collisions with tiles and level bounds
 *
 * @param player The player to move
 * @param level The level to check collisions against
 */
void collideHorizontal(Player* player, const Level* level);

/**
 * Perform vertical collision sweep
 * Moves player vertically and resolves collisions with tiles and level bounds
 * Updates onGround flag and dash state
 *
 * @param player The player to move
 * @param level The level to check collisions against
 */
void collideVertical(Player* player, const Level* level);

#endif
