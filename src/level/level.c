#include "level.h"
#include "grassy_stone.h"
#include "plants.h"
#include "decals.h"

// ---------------------------------------------------------------------------
// Decompressed tile data buffers (in EWRAM on GBA)
// Sized for the largest possible level: 512x40 tiles, 2 layers
// ---------------------------------------------------------------------------
#define TILE_BUFFER_SIZE (512 * 40 * 2)  // u16s, covers 2 layers of 512x40

static u16 g_tileBuffer[TILE_BUFFER_SIZE] __attribute__((section(".ewram"), aligned(4)));
u16* g_levelLayerTiles[4] = {0, 0, 0, 0};

// ---------------------------------------------------------------------------
// Desktop stub for BIOS RLE decompression (GBA provides this via SWI 0x14)
// ---------------------------------------------------------------------------
#ifdef DESKTOP_BUILD
static void RLUnCompWram(const void* src, void* dst) {
    const u8* s = (const u8*)src;
    u8* d = (u8*)dst;
    u32 decompressed_size = (u32)s[1] | ((u32)s[2] << 8) | ((u32)s[3] << 16);
    s += 4;
    u32 written = 0;
    while (written < decompressed_size) {
        u8 flag = *s++;
        if (flag & 0x80) {
            u32 len = (flag & 0x7F) + 3;
            u8 val = *s++;
            for (u32 j = 0; j < len && written < decompressed_size; j++, written++)
                *d++ = val;
        } else {
            u32 len = (flag & 0x7F) + 1;
            for (u32 j = 0; j < len && written < decompressed_size; j++, written++)
                *d++ = *s++;
        }
    }
}
#endif

#define TILESET_COUNT 3

// Palette bank for each tileset
#define PALETTE_GRASSY_STONE 0
#define PALETTE_PLANTS 2
#define PALETTE_DECALS 3

// Tileset metadata
typedef struct {
    u16 firstTileId;
    u16 lastTileId;
    const unsigned int* tileData;
    u8 paletteBank;
} TilesetMetadata;

static const TilesetMetadata tilesets[TILESET_COUNT] = {
    { 1, 55, grassy_stoneTiles, PALETTE_GRASSY_STONE },
    { 56, 215, plantsTiles, PALETTE_PLANTS },
    { 216, 1440, decalsTiles, PALETTE_DECALS }
};

void loadLevelToVRAM(const Level* level) {
    // Decompress tile layers from ROM into RAM buffers
    u16* bufPtr = g_tileBuffer;
    u32 tilesPerLayer = (u32)level->width * level->height;
    for (u8 i = 0; i < level->layerCount && i < 4; i++) {
        g_levelLayerTiles[i] = bufPtr;
        RLUnCompWram(level->layers[i].rleData, bufPtr);
        bufPtr += tilesPerLayer;
    }

    // Clear any unused layer pointers
    for (u8 i = level->layerCount; i < 4; i++)
        g_levelLayerTiles[i] = 0;

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
