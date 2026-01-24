#ifndef CAMERA_H
#define CAMERA_H

#include "gba.h"
#include "src/core/game_types.h"
#include "src/core/game_math.h"
#include "src/level/level.h"

/**
 * Update camera position to follow the player with deadzone
 *
 * @param camera The camera to update
 * @param player The player to follow
 * @param level The level (for bounds checking)
 */
void updateCamera(Camera* camera, Player* player, const Level* level);

#endif
