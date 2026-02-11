#ifndef COLLISION_H
#define COLLISION_H

#ifdef DESKTOP_BUILD
#include "desktop/desktop_stubs.h"
#else
#include <tonc.h>
#endif
#include "core/game_types.h"
#include "core/game_math.h"
#include "level/level.h"

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

/**
 * Check if the player's hitbox collides at a given screen position.
 *
 * @param level The level to check against
 * @param screenX Player center X in pixels
 * @param screenY Player center Y in pixels
 * @return 1 if colliding, 0 otherwise
 */
int isPositionCollidingAt(const Level* level, int screenX, int screenY);

/**
 * Check if there's a wall adjacent to the player at a Y offset
 *
 * @param player The player to check
 * @param level The level to check against
 * @param dir Direction to check: 1 for right, -1 for left
 * @param yAdd Additional Y offset in pixels
 * @return 1 if there's a wall in that direction, 0 otherwise
 */
int checkWallAt(const Player* player, const Level* level, int dir, int yAdd, int dist);

/**
 * Check if there's a wall adjacent to the player
 *
 * @param player The player to check
 * @param level The level to check against
 * @param dir Direction to check: 1 for right, -1 for left
 * @return 1 if there's a wall in that direction, 0 otherwise
 */
int checkWall(const Player* player, const Level* level, int dir);

/**
 * Check if there's a ceiling above the player that would prevent unducking
 *
 * @param player The player to check
 * @param level The level to check against
 * @return 1 if there's a ceiling blocking unduck, 0 if clear
 */
int checkCeiling(const Player* player, const Level* level);

#endif
