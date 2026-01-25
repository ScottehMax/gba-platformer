#ifndef LEVEL_H
#define LEVEL_H

#include "core/gba.h"
#include "level3.h"

/**
 * Get the tile ID at the specified tile coordinates
 *
 * @param level The level to query
 * @param tileX The tile X coordinate
 * @param tileY The tile Y coordinate
 * @return The tile ID, or 0 if out of bounds
 */
static inline u16 getTileAt(const Level* level, int tileX, int tileY) {
    if (tileX < 0 || tileX >= level->width || tileY < 0 || tileY >= level->height) {
        return 0; // Out of bounds
    }
    return level->tiles[tileY * level->width + tileX];
}

/**
 * Check if a tile ID is solid (collideable)
 *
 * @param level The level to query (for collision bitmap lookup)
 * @param tileId The tile ID to check
 * @return 1 if solid, 0 if not
 */
static inline int isTileSolid(const Level* level, u16 tileId) {
    // Tile 0 is empty/air
    if (tileId == 0) return 0;
    
    // If level has a collision bitmap, use it
    if (level->collisionBitmap) {
        u32 byteIndex = tileId / 8;
        u8 bitIndex = tileId % 8;
        u8 byte = (level->collisionBitmap[byteIndex / 4] >> ((byteIndex % 4) * 8)) & 0xFF;
        return (byte & (1 << bitIndex)) != 0;
    }
    
    // Backward compatibility: tiles 1-55 (grassy_stone) are solid
    return tileId >= 1 && tileId <= 55;
}

/**
 * Load level tile data to VRAM
 * Scans the level and loads only the tiles that are actually used
 *
 * @param level The level to load
 */
void loadLevelToVRAM(const Level* level);

/**
 * Get the VRAM tile index for a tile value
 * Tiles are already VRAM indices (remapped at build time)
 *
 * @param vramIndex The tile value from the level (already a VRAM index)
 * @return The VRAM tile index
 */
u16 getVramTileIndex(u16 vramIndex);

/**
 * Get the palette bank for a tile
 *
 * @param vramIndex The VRAM tile index
 * @param level The level (needed to look up original tile ID)
 * @return The palette bank (0-15)
 */
u8 getTilePaletteBank(u16 vramIndex, const Level* level);

/**
 * Check if a tile is decorative (goes on BG1) vs terrain (goes on BG0)
 *
 * @param vramIndex The VRAM tile index
 * @param level The level (needed to look up original tile ID)
 * @return 1 if decorative, 0 if terrain
 */
int isTileDecorative(u16 vramIndex, const Level* level);

#endif
