#ifndef CAMERA_H
#define CAMERA_H

#include <tonc.h>
#include "core/game_types.h"
#include "core/game_math.h"
#include "level/level.h"

/**
 * Update camera position to follow the player with deadzone
 *
 * @param camera The camera to update
 * @param player The player to follow
 * @param level The level (for bounds checking)
 */
void updateCamera(Camera* camera, Player* player, const Level* level);

/**
 * Apply the camera deadzone/clamp logic for a player position already expressed
 * in pixels. This lets transitions land on the same camera pose that normal
 * gameplay will use on the next frame.
 */
void settleCameraToPlayer(Camera* camera, int playerX, int playerY, const Level* level);

#endif
