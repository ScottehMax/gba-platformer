#include "level.h"

void loadLevelToVRAM(const Level* level) {
    // Set up background tiles from level data
    // The level uses tile IDs 0-8, which map to our existing tiles
    // This function could be extended to dynamically load tiles based on level needs

    // Tiles are already set up in main(), so nothing extra needed here
    // In a more advanced implementation, we'd stream tiles as camera scrolls
}
