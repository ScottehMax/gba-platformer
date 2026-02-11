#ifndef MENU_H
#define MENU_H

#include <tonc.h>
#include "core/game_types.h"
#include "player/player.h"
#include "camera/camera.h"

// Initialize menu system
void initMenu(void);

// Render the level selection menu
void renderMenu(void);

// Update menu state based on input
// Returns: 1 if still in menu, 0 if transitioning to gameplay
int updateAndRenderMenu(u16 keys, u16 pressed, Player* player, Camera* camera);

// Return to menu from gameplay
void returnToMenu(void);

// Check if currently in menu mode
int isInMenuMode(void);

// Get the currently loaded level (NULL if in menu)
const Level* getCurrentLevel(void);

// Get current level index (-1 if in menu)
int getCurrentLevelIndex(void);

// Switch to a specific level by index
void switchToLevel(int levelIndex, Player* player, Camera* camera);

#endif // MENU_H
