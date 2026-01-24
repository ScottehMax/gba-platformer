#include "level.h"
#include "grassy_stone.h"
#include "plants.h"
#include "decals.h"

#define TILESET_COUNT 3

// Palette bank for each tileset
#define PALETTE_GRASSY_STONE 0
#define PALETTE_PLANTS 2
#define PALETTE_DECALS 3

// Tileset metadata
typedef struct {
    u16 firstTileId;
    u16 lastTileId;
    const u32* tileData;
    u8 paletteBank;
} TilesetMetadata;

static const TilesetMetadata tilesets[TILESET_COUNT] = {
    { 1, 55, grassy_stoneTiles, PALETTE_GRASSY_STONE },
    { 56, 215, plantsTiles, PALETTE_PLANTS },
    { 216, 1440, decalsTiles, PALETTE_DECALS }
};

void loadLevelToVRAM(const Level* level) {
    volatile u32* bgTiles = (volatile u32*)0x06000000;
    
    // Load tiles based on the pre-computed unique tile list
    // Level tiles were already remapped at build time to VRAM indices
    // uniqueTileIds[i] holds the original tile ID that should be loaded to VRAM slot i
    for (u16 i = 0; i < level->uniqueTileCount; i++) {
        u16 originalTileId = level->uniqueTileIds[i];
        
        // Handle sky tile specially
        if (originalTileId == 0) {
            // Sky: all pixels transparent (palette index 0)
            for (int j = 0; j < 8; j++) {
                bgTiles[i * 8 + j] = 0x00000000;
            }
            continue;
        }
        
        // Find which tileset this tile belongs to
        const TilesetMetadata* tileset = 0;
        for (int ts = 0; ts < TILESET_COUNT; ts++) {
            if (originalTileId >= tilesets[ts].firstTileId && originalTileId <= tilesets[ts].lastTileId) {
                tileset = &tilesets[ts];
                break;
            }
        }
        
        if (!tileset) continue;  // Unknown tile
        
        // Calculate offset in source tileset
        u16 tilesetIndex = originalTileId - tileset->firstTileId;
        u32 srcOffset = tilesetIndex * 8;  // 8 u32s per tile in 4-bit mode
        
        // Copy tile to VRAM at slot i
        u32 vramOffset = i * 8;
        for (int j = 0; j < 8; j++) {
            bgTiles[vramOffset + j] = tileset->tileData[srcOffset + j];
        }
    }
}

u16 getVramTileIndex(u16 vramIndex) {
    // Tiles are already VRAM indices (remapped at build time)
    return vramIndex;
}

u8 getTilePaletteBank(u16 vramIndex, const Level* level) {
    // Palette banks are pre-computed at build time
    if (vramIndex >= level->uniqueTileCount) return 0;
    return level->tilePaletteBanks[vramIndex];
}

int isTileDecorative(u16 vramIndex, const Level* level) {
    // Not needed anymore - no separate layers
    return 0;
}
