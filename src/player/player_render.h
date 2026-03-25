#ifndef PLAYER_RENDER_H
#define PLAYER_RENDER_H

#include <tonc.h>
#include "core/game_types.h"
#include "core/game_math.h"

/**
 * Draw the player sprite and dash trail
 *
 * @param player The player to draw
 * @param camera The camera for world-to-screen coordinate conversion
 * @param objPriority OBJ priority to use while rendering the player and trail
 */
void drawPlayer(Player* player, Camera* camera, u16 objPriority);

#endif
