#include "level.h"
#include "grassy_stone.h"
#include "plants.h"
#include "decals.h"

// ---------------------------------------------------------------------------
// Decompressed tile data buffers (in EWRAM on GBA)
// Sized for the largest possible level: 512x40 tiles, 2 layers
// ---------------------------------------------------------------------------
#define TILE_BUFFER_SIZE (512 * 40 * 2)  // u16s, covers 2 layers of 512x40
#define LEVEL_VRAM_TILE_LIMIT 512

#ifdef DESKTOP_BUILD
static u16 g_tileBuffer[TILE_BUFFER_SIZE];
static u16 g_tileBBuffer[TILE_BUFFER_SIZE];
#else
static u16 g_tileBuffer[TILE_BUFFER_SIZE]  __attribute__((section(".ewram"), aligned(4)));
static u16 g_tileBBuffer[TILE_BUFFER_SIZE] __attribute__((section(".ewram"), aligned(4)));
#endif
u16* g_levelLayerTiles[4]  = {0, 0, 0, 0};
u16* g_levelBLayerTiles[4] = {0, 0, 0, 0};

// Swappable base pointers: adoptLevelBBuffer swaps these so that
// g_levelLayerTiles and the next loadLevelBToVRAM call always use
// different physical storage (preventing buffer corruption on reverse transitions).
static u16* s_mainBufBase = g_tileBuffer;
static u16* s_sBufBase    = g_tileBBuffer;

static int g_tileVramOffset = 0;
static int g_levelBTileVramOffset = 0;
int getLevelTileVramOffset(void) { return g_tileVramOffset; }
void setLevelTileVramOffset(int offset) { g_tileVramOffset = offset; }

#ifdef DESKTOP_BUILD
// Test helpers: expose which physical buffer main and secondary currently use.
const u16* getMainBufBase(void)  { return s_mainBufBase; }
const u16* getSecBufBase(void)   { return s_sBufBase; }
const u16* getTileBufA(void)     { return g_tileBuffer; }
const u16* getTileBufB(void)     { return g_tileBBuffer; }
static u16 g_desktopVramTiles[LEVEL_VRAM_TILE_LIMIT];
const u16* getDesktopVramTiles(void) { return g_desktopVramTiles; }
#endif

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

// Internal: write one level's unique tiles into VRAM starting at vramSlot.
static void writeTilesToVRAM(const Level* level, int vramSlot) {
#ifdef DESKTOP_BUILD
    for (u16 i = 0; i < level->uniqueTileCount && (vramSlot + i) < LEVEL_VRAM_TILE_LIMIT; i++) {
        g_desktopVramTiles[vramSlot + i] = level->uniqueTileIds[i];
    }
    return;
#else
    volatile u32* bgTiles = (volatile u32*)0x06000000;

    for (u16 i = 0; i < level->uniqueTileCount; i++) {
        u16 originalTileId = level->uniqueTileIds[i];

        if (originalTileId == 0) {
            for (int j = 0; j < 8; j++)
                bgTiles[(vramSlot + i) * 8 + j] = 0x00000000;
            continue;
        }

        const TilesetMetadata* tileset = 0;
        for (int ts = 0; ts < TILESET_COUNT; ts++) {
            if (originalTileId >= tilesets[ts].firstTileId &&
                originalTileId <= tilesets[ts].lastTileId) {
                tileset = &tilesets[ts];
                break;
            }
        }
        if (!tileset) continue;

        u16 tilesetIndex = originalTileId - tileset->firstTileId;
        u32 srcOffset  = tilesetIndex * 8;
        u32 dstOffset  = (vramSlot + i) * 8;
        for (int j = 0; j < 8; j++)
            bgTiles[dstOffset + j] = tileset->tileData[srcOffset + j];
    }
#endif // DESKTOP_BUILD
}

void loadLevelToVRAM(const Level* level) {
    g_tileVramOffset = 0;
    g_levelBTileVramOffset = 0;

    // Decompress tile layers into RAM
    u16* bufPtr = s_mainBufBase;
    u32 tilesPerLayer = (u32)level->width * level->height;
    for (u8 i = 0; i < level->layerCount && i < 4; i++) {
        g_levelLayerTiles[i] = bufPtr;
        RLUnCompWram(level->layers[i].rleData, bufPtr);
        bufPtr += tilesPerLayer;
    }
    for (u8 i = level->layerCount; i < 4; i++)
        g_levelLayerTiles[i] = 0;

    writeTilesToVRAM(level, 0);
}

void loadLevelBToVRAM(const Level* level, int vramOffset) {
    g_levelBTileVramOffset = vramOffset;

    // Decompress tile layers into the secondary RAM buffer
    u16* bufPtr = s_sBufBase;
    u32 tilesPerLayer = (u32)level->width * level->height;
    for (u8 i = 0; i < level->layerCount && i < 4; i++) {
        g_levelBLayerTiles[i] = bufPtr;
        RLUnCompWram(level->layers[i].rleData, bufPtr);
        bufPtr += tilesPerLayer;
    }
    for (u8 i = level->layerCount; i < 4; i++)
        g_levelBLayerTiles[i] = 0;

    writeTilesToVRAM(level, vramOffset);
}

void adoptLevelBBuffer(const Level* level) {
    // Fast path used at scroll transition end: level B was already decompressed
    // by loadLevelBToVRAM and its tile graphics are already in VRAM at
    // g_levelBTileVramOffset. Swap the base pointers first so that the next
    // loadLevelBToVRAM call writes into the OLD main buffer, not the one
    // g_levelLayerTiles now points into. Without this swap, both arrays would
    // reference the same physical storage and a reverse transition would corrupt
    // the active level's tile data mid-scroll.
    u16* tmp    = s_mainBufBase;
    s_mainBufBase = s_sBufBase;
    s_sBufBase    = tmp;

    g_tileVramOffset = g_levelBTileVramOffset;
    for (u8 i = 0; i < level->layerCount && i < 4; i++) {
        g_levelLayerTiles[i] = g_levelBLayerTiles[i];
        g_levelBLayerTiles[i] = 0;
    }
    for (u8 i = level->layerCount; i < 4; i++) {
        g_levelLayerTiles[i] = 0;
        g_levelBLayerTiles[i] = 0;
    }
    g_levelBTileVramOffset = 0;
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
