#ifndef LEVEL_H
#define LEVEL_H

#ifdef DESKTOP_BUILD
#include "desktop/desktop_stubs.h"
#else
#include <tonc.h>
#endif

// Collision type for each tile position
typedef enum {
    COL_NONE     = 0,  // No collision (passable)
    COL_SOLID    = 1,  // Fully solid (blocks all directions)
    COL_JUMPTHRU = 2,  // One-way platform (blocks only from above when falling)
} CollisionType;

// Object types enum - add new types here
typedef enum {
    OBJ_NONE = 0,
    OBJ_SPRING = 1,
    OBJ_SPRING_SUPER = 2,
    OBJ_SPRING_WALL_LEFT = 3,
    OBJ_SPRING_WALL_RIGHT = 4,
    OBJ_RED_BUBBLE = 5,
    OBJ_GREEN_BUBBLE = 6,
    // Add more object types here as needed
} ObjectType;

typedef struct {
    ObjectType type;
    u16 x;
    u16 y;
} LevelObject;

typedef struct {
    const char* name;
    u16 firstId;
    u16 tileCount;
    const u32* tileData;
    u32 tileDataLen;
    const u16* paletteData;
    u16 paletteLen;
} TilesetInfo;

typedef struct {
    const char* name;
    u8 bgLayer;      // Which BG layer (0-3)
    u8 priority;     // Priority (0-3, lower = in front)
    const u32* rleData;  // BIOS RLE compressed tile data (SWI 0x14 format)
} TileLayer;

// RAM buffers holding decompressed tile data for the current level.
// Populated by loadLevelToVRAM(). Index matches layer index in Level.layers.
extern u16* g_levelLayerTiles[4];

typedef struct {
    const char* name;
    u16 width;
    u16 height;
    u8 layerCount;
    const TileLayer* layers;
    u16 objectCount;
    const LevelObject* objects;
    u16 playerSpawnX;
    u16 playerSpawnY;
    u8 tilesetCount;
    const TilesetInfo* tilesets;
    const u8* collisionMap;  // Packed 4-bit collision: 2 tiles per byte, low nibble first
    u16 uniqueTileCount;
    const u16* uniqueTileIds;
    const u8* tilePaletteBanks;
} Level;

#ifdef DESKTOP_BUILD
#include "../../generated/level3.h"
#else
#include "level3.h"
#endif

/**
 * Get the tile ID at the specified tile coordinates for a specific layer
 *
 * @param level The level to query
 * @param layerIndex The layer index (0 = first layer)
 * @param tileX The tile X coordinate
 * @param tileY The tile Y coordinate
 * @return The tile ID, or 0 if out of bounds
 */
static inline u16 getTileAt(const Level* level, u8 layerIndex, int tileX, int tileY) {
    if (layerIndex >= level->layerCount) return 0;
    if (tileX < 0 || tileX >= level->width || tileY < 0 || tileY >= level->height) return 0;
    if (!g_levelLayerTiles[layerIndex]) return 0;
    return g_levelLayerTiles[layerIndex][tileY * level->width + tileX];
}

/**
 * Get the collision type for a tile at the given tile coordinates.
 *
 * @param level The level to query
 * @param tileX The tile X coordinate
 * @param tileY The tile Y coordinate
 * @return CollisionType (COL_NONE, COL_SOLID, or COL_JUMPTHRU)
 */
static inline CollisionType getTileCollision(const Level* level, int tileX, int tileY) {
    if (tileX < 0 || tileX >= level->width || tileY < 0 || tileY >= level->height) {
        return COL_NONE;
    }
    int idx = tileY * level->width + tileX;
    u8 packed = level->collisionMap[idx >> 1];
    return (CollisionType)((idx & 1) ? (packed >> 4) : (packed & 0x0F));
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
