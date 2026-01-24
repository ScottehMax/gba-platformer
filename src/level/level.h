#ifndef LEVEL_H
#define LEVEL_H

#include "gba.h"
#include "level3.h"

/**
 * Get the tile ID at the specified tile coordinates
 *
 * @param level The level to query
 * @param tileX The tile X coordinate
 * @param tileY The tile Y coordinate
 * @return The tile ID, or 0 if out of bounds
 */
static inline u8 getTileAt(const Level* level, int tileX, int tileY) {
    if (tileX < 0 || tileX >= level->width || tileY < 0 || tileY >= level->height) {
        return 0; // Out of bounds
    }
    return level->tiles[tileY * level->width + tileX];
}

/**
 * Check if a tile ID is solid (collideable)
 *
 * @param tileId The tile ID to check
 * @return 1 if solid, 0 if not
 */
static inline int isTileSolid(u8 tileId) {
    // Tile 0 is empty/air
    // All other tiles (1-55) are solid
    return tileId >= 1;
}

/**
 * Load level tile data to VRAM
 * (Currently a placeholder for future dynamic tile loading)
 *
 * @param level The level to load
 */
void loadLevelToVRAM(const Level* level);

#endif
