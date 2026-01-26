#include "core/gba.h"
#include "skelly.h"
#include "grassy_stone.h"
#include "plants.h"
#include "decals.h"
#include "core/text.h"
#include "nightsky.h"
#include <stdlib.h>
#include "core/game_math.h"
#include "core/game_types.h"
#include "core/debug_utils.h"
#include "level/level.h"
#include "camera/camera.h"
#include "collision/collision.h"
#include "player/player.h"
#include "player/player_render.h"

// Tileset palette bank assignments
#define PALETTE_GRASSY_STONE 0
#define PALETTE_FONT         1
#define PALETTE_PLANTS       2
#define PALETTE_DECALS       3

void vsync() {
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

int main() {
    // Load level
    const Level* currentLevel = &Tutorial_Level;
    
    // Mode 0 with BG0, BG1, BG2, BG3 and sprites enabled
    // BG0 = nightsky, BG1 = decorative layer, BG2 = terrain layer, BG3 = text
    REG_DISPCNT = VIDEOMODE_0 | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11) | OBJ_ENABLE | OBJ_1D_MAP;  // BG0_ENABLE = bit 8, BG1_ENABLE = bit 9, BG2_ENABLE = bit 10, BG3_ENABLE = bit 11

    // Load nightsky tiles to VRAM (char block 2)
    volatile u32* nightskyTilesDst = (volatile u32*)(0x06000000 + (2 << 14));
    for (int i = 0; i < nightskyTilesLen / 4; i++) {
        nightskyTilesDst[i] = ((const u32*)nightskyTiles)[i];
    }

    // Load nightsky tilemap to screen base 24 (BG0) - NOT 20, which is used by BG3 text!
    // Adjust tilemap entries to use palette bank 4
    volatile u16* nightskyMapDst = (volatile u16*)(0x06000000 + (24 << 11));
    for (int i = 0; i < nightskyMapLen / 2; i++) {
        u16 tileEntry = ((const u16*)nightskyMap)[i];
        u16 tileIndex = tileEntry & 0x03FF;  // Extract tile index
        u16 flags = tileEntry & 0xFC00;      // Extract flip/rotation flags
        // Set palette bank 4
        nightskyMapDst[i] = tileIndex | flags | (4 << 12);
    }

    // Load nightsky palette to palette bank 4 (colors 64-79)
    volatile u16* bgPalette = MEM_BG_PALETTE;
    for (int i = 0; i < nightskyPalLen / 2; i++) {
        bgPalette[64 + i] = nightskyPal[i];
    }

    // Set BG0 control register (4-bit color, screen base 24, char base 2, priority 3 - behind everything)
    REG_BG0CNT = (24 << 8) | (2 << 2) | (3 << 0);

    // Set BG0 scroll to 0,0
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;

    // Enable alpha blending for sprites
    // BLDCNT: Effect=Alpha blend (bit 6), NO global OBJ target (sprites set semi-transparent individually)
    // 2nd target=BG0+BG1+BG2+BD (bits 8,9,10,13) - what semi-transparent sprites blend with
    REG_BLDCNT = (1 << 6) | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 13);
    // Set blend coefficients EVA (sprite) and EVB (background) - must sum to 16 or less
    REG_BLDALPHA = (7 << 0) | (9 << 8);  // ~44% trail, ~56% background (more transparent)

    // Palette bank 0: grassy_stone (colors 0-15)
    for (int i = 0; i < 16; i++) {
        bgPalette[PALETTE_GRASSY_STONE * 16 + i] = grassy_stonePal[i];
    }

    // Make palette index 0 transparent for grassy_stone
    bgPalette[0] = 0;
    
    // Palette bank 1: Font (colors 16-31)
    for (int i = 0; i < 16; i++) {
        bgPalette[PALETTE_FONT * 16 + i] = tinypixiePal[i];
    }
    
    // Palette bank 2: plants (colors 32-47)
    for (int i = 0; i < 16; i++) {
        bgPalette[PALETTE_PLANTS * 16 + i] = plantsPal[i];
    }
    
    // Palette bank 3: decals (colors 48-63)
    for (int i = 0; i < 16; i++) {
        bgPalette[PALETTE_DECALS * 16 + i] = decalsPal[i];
    }

    // Load only the tiles used in the current level to VRAM
    loadLevelToVRAM(&Tutorial_Level);

    // Set up BG control registers for each layer based on level data
    // Screen base assignments: BG1=25, BG2=26
    // Layer priorities and BG assignments are stored in the level data
    for (u8 i = 0; i < currentLevel->layerCount; i++) {
        const TileLayer* layer = &currentLevel->layers[i];
        u8 bgLayer = layer->bgLayer;
        u8 priority = layer->priority;
        u8 screenBase = 25 + bgLayer;  // BG1=25, BG2=26, etc.

        if (bgLayer == 1) {
            REG_BG1CNT = (screenBase << 8) | (0 << 2) | (priority << 0);
        } else if (bgLayer == 2) {
            REG_BG2CNT = (screenBase << 8) | (0 << 2) | (priority << 0);
        }
    }

    // Initialize background text system (BG3 - uses char block 1)
    init_bg_text();

    // Copy sprite palette to VRAM
    volatile u16* spritePalette = MEM_SPRITE_PALETTE;

    // Palette 0: Normal sprite colors
    for (int i = 0; i < 16; i++) {
        spritePalette[i] = skellyPal[i];
    }

    // Palettes 1-10: Light blue/cyan silhouettes with varying opacity for dash trail fade
    // Create 10 palettes with very gradual color transitions for smooth fade effect
    for (int pal = 0; pal < 10; pal++) {
        for (int i = 0; i < 16; i++) {
            if (i == 0) {
                spritePalette[(pal + 1) * 16 + i] = 0;  // Index 0 is transparent
            } else {
                // Very gradual fade from bright to light blue
                // Palette 1: Most opaque (10, 20, 31)
                // Palette 10: Lightest (2, 6, 16)
                int r = 10 - (pal * 8) / 10;  // 10 -> 2
                int g = 20 - (pal * 14) / 10; // 20 -> 6
                int b = 31 - (pal * 15) / 10; // 31 -> 16
                if (r < 2) r = 2;
                if (g < 6) g = 6;
                if (b < 16) b = 16;
                spritePalette[(pal + 1) * 16 + i] = COLOR(r, g, b);
            }
        }
    }

    // Copy player sprite to VRAM (char block 4)
    volatile u32* spriteTiles = MEM_SPRITE_TILES;
    for (int i = 0; i < 32; i++) {  // 16-color mode: 4 tiles, 8 u32s per tile
        spriteTiles[i] = skellyTiles[i];
    }

    // Set up sprite 0 as 16x16, 16-color mode, priority 1
    volatile u16* oam = (volatile u16*)MEM_OAM;
    oam[0] = 0;
    oam[1] = (1 << 14);
    oam[2] = (1 << 10);  // Priority 1

    // Hide other sprites
    for (int i = 1; i < 128; i++) {
        oam[i * 4] = 160;
    }

    // Initialize player from level spawn point
    Player player;
    initPlayer(&player, currentLevel);
    
    // Initialize camera
    Camera camera;
    camera.x = 0;
    camera.y = 0;

    // Initialize tilemaps for each layer
    for (u8 layerIdx = 0; layerIdx < currentLevel->layerCount; layerIdx++) {
        const TileLayer* layer = &currentLevel->layers[layerIdx];
        u8 bgLayer = layer->bgLayer;
        u8 screenBase = 25 + bgLayer;  // BG1=25, BG2=26

        volatile u16* bgMap = (volatile u16*)(0x06000000 + (screenBase << 11));

        // Initialize tilemap once at startup
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                u16 vramIndex = getTileAt(currentLevel, layerIdx, x, y);
                u8 paletteBank = getTilePaletteBank(vramIndex, currentLevel);
                bgMap[y * 32 + x] = vramIndex | (paletteBank << 12);
            }
        }
    }

    // Initialize timers for FPS counter
    // Timer 0: counts at 16.384 KHz (overflow after ~4 seconds)
    // Timer 1: cascades from Timer 0 for extended range
    REG_TM0CNT_L = 0;  // Initial value
    REG_TM1CNT_L = 0;
    REG_TM0CNT_H = TM_ENABLE | TM_FREQ_1024;  // Enable, prescaler 1024
    REG_TM1CNT_H = TM_ENABLE | TM_CASCADE;    // Enable, cascade from Timer 0

    // Frame counter and profiling
    int frameCount = 0;
    u32 lastTimerValue = 0;
    u16 fps = 60;

    // Profiling: track max over 16 frames (not average, since FPS = slowest frame)
    u16 maxPlayer = 0, maxCamera = 0, maxTilemap = 0, maxRender = 0, maxTotal = 0;

    char fpsStr[32] = "FPS: 60";
    char playerTimeStr[32] = "P:0";
    char cameraTimeStr[32] = "C:0";
    char tilemapTimeStr[32] = "T:0";
    char renderTimeStr[32] = "R:0";
    char totalTimeStr[32] = "Tot:0";

    // Allocate text slots once
    int fpsSlot = draw_bg_text_auto(fpsStr, 1, 1);
    int playerTimeSlot = draw_bg_text_auto(playerTimeStr, 1, 2);
    int cameraTimeSlot = draw_bg_text_auto(cameraTimeStr, 1, 3);
    int tilemapTimeSlot = draw_bg_text_auto(tilemapTimeStr, 1, 4);
    int renderTimeSlot = draw_bg_text_auto(renderTimeStr, 1, 5);
    int totalTimeSlot = draw_bg_text_auto(totalTimeStr, 1, 6);

    // Game loop
    while (1) {
        u16 frameStart = REG_TM0CNT_L;
        vsync();
        frameCount++;

        // Profile: Player update
        u16 t0 = REG_TM0CNT_L;
        u16 keys = getKeys();
        updatePlayer(&player, keys, currentLevel);
        u16 t1 = REG_TM0CNT_L;
        u16 dtPlayer = t1 - t0;
        if (dtPlayer > maxPlayer) maxPlayer = dtPlayer;

        // Profile: Camera update
        updateCamera(&camera, &player, currentLevel);
        u16 t2 = REG_TM0CNT_L;
        u16 dtCamera = t2 - t1;
        if (dtCamera > maxCamera) maxCamera = dtCamera;

        // Use hardware scrolling in pixel space for all terrain layers
        REG_BG1HOFS = camera.x;
        REG_BG1VOFS = camera.y;
        REG_BG2HOFS = camera.x;
        REG_BG2VOFS = camera.y;

        // Optimized tilemap update using hardware scrolling wraparound
        // The tilemap buffer is circular - hardware wraps at 256x256 pixels (32x32 tiles)
        // Tilemap position (x%32, y%32) contains level tile at floor(camera/8) + [x, y]
        static int oldCameraTileX = -1;
        static int oldCameraTileY = -1;
        int cameraTileX = camera.x / 8;
        int cameraTileY = camera.y / 8;

        if (cameraTileX != oldCameraTileX || cameraTileY != oldCameraTileY) {
            // Determine scroll direction
            int deltaX = cameraTileX - oldCameraTileX;
            int deltaY = cameraTileY - oldCameraTileY;

            // Update all layers
            for (u8 layerIdx = 0; layerIdx < currentLevel->layerCount; layerIdx++) {
                const TileLayer* layer = &currentLevel->layers[layerIdx];
                u8 bgLayer = layer->bgLayer;
                u8 screenBase = 25 + bgLayer;  // BG1=25, BG2=26

                volatile u16* bgMap = (volatile u16*)(0x06000000 + (screenBase << 11));

                // First time or large jump - fill entire tilemap
                if (oldCameraTileX == -1 || deltaX < -1 || deltaX > 1 || deltaY < -1 || deltaY > 1) {
                    for (int ty = 0; ty < 32; ty++) {
                        for (int tx = 0; tx < 32; tx++) {
                            int levelX = cameraTileX + tx;
                            int levelY = cameraTileY + ty;
                            u16 tileId = getTileAt(currentLevel, layerIdx, levelX, levelY);
                            // Use wraparound: tilemap position wraps at 32
                            int mapX = (cameraTileX + tx) & 31;
                            int mapY = (cameraTileY + ty) & 31;
                            bgMap[mapY * 32 + mapX] = tileId | (currentLevel->tilePaletteBanks[tileId] << 12);
                        }
                    }
                } else {
                    // Incremental update - only update new tiles entering the 32x32 window
                    if (deltaX != 0) {
                        // Scrolled horizontally - update one column
                        int levelX = (deltaX > 0) ? (cameraTileX + 31) : cameraTileX;
                        int mapX = levelX & 31;
                        for (int ty = 0; ty < 32; ty++) {
                            int levelY = cameraTileY + ty;
                            int mapY = levelY & 31;
                            u16 tileId = getTileAt(currentLevel, layerIdx, levelX, levelY);
                            bgMap[mapY * 32 + mapX] = tileId | (currentLevel->tilePaletteBanks[tileId] << 12);
                        }
                    }
                    if (deltaY != 0) {
                        // Scrolled vertically - update one row
                        int levelY = (deltaY > 0) ? (cameraTileY + 31) : cameraTileY;
                        int mapY = levelY & 31;
                        for (int tx = 0; tx < 32; tx++) {
                            int levelX = cameraTileX + tx;
                            int mapX = levelX & 31;
                            u16 tileId = getTileAt(currentLevel, layerIdx, levelX, levelY);
                            bgMap[mapY * 32 + mapX] = tileId | (currentLevel->tilePaletteBanks[tileId] << 12);
                        }
                    }
                }
            }

            oldCameraTileX = cameraTileX;
            oldCameraTileY = cameraTileY;
        }

        // Profile: Tilemap update
        u16 t3 = REG_TM0CNT_L;
        u16 dtTilemap = t3 - t2;
        if (dtTilemap > maxTilemap) maxTilemap = dtTilemap;

        // Profile: Rendering
        drawPlayer(&player, &camera);
        u16 t4 = REG_TM0CNT_L;
        u16 dtRender = t4 - t3;
        if (dtRender > maxRender) maxRender = dtRender;

        // Track subsystem total
        u16 subsystemTotal = dtPlayer + dtCamera + dtTilemap + dtRender;
        if (subsystemTotal > maxTotal) maxTotal = subsystemTotal;

        // Measure COMPLETE frame time (vsync to vsync)
        u16 frameEnd = REG_TM0CNT_L;
        u16 completeFrameTime = frameEnd - frameStart;
        if (completeFrameTime > maxTotal) maxTotal = completeFrameTime;

        // Calculate FPS and profiling stats every 16 frames
        if ((frameCount & 15) == 0) {
            // Read timer value (combined 32-bit from Timer 1:0)
            u32 currentTimerValue = (REG_TM1CNT_L << 16) | REG_TM0CNT_L;
            u32 timerDelta = currentTimerValue - lastTimerValue;

            // Timer runs at 16.384 KHz (16384 ticks per second)
            // FPS = frames / (ticks / 16384) = (frames * 16384) / ticks
            // For 16 frames: FPS = (16 * 16384) / timerDelta
            if (timerDelta > 0) {
                fps = (16 * 16384) / timerDelta;
            }

            lastTimerValue = currentTimerValue;
            int_to_string(fps, fpsStr, sizeof(fpsStr), "FPS:");
            draw_bg_text_slot(fpsStr, 1, 1, fpsSlot);

            // Update profiling display - show MAX values (worst frame)
            int_to_string(maxPlayer, playerTimeStr, sizeof(playerTimeStr), "P:");
            draw_bg_text_slot(playerTimeStr, 1, 2, playerTimeSlot);

            int_to_string(maxCamera, cameraTimeStr, sizeof(cameraTimeStr), "C:");
            draw_bg_text_slot(cameraTimeStr, 1, 3, cameraTimeSlot);

            int_to_string(maxTilemap, tilemapTimeStr, sizeof(tilemapTimeStr), "T:");
            draw_bg_text_slot(tilemapTimeStr, 1, 4, tilemapTimeSlot);

            int_to_string(maxRender, renderTimeStr, sizeof(renderTimeStr), "R:");
            draw_bg_text_slot(renderTimeStr, 1, 5, renderTimeSlot);

            int_to_string(maxTotal, totalTimeStr, sizeof(totalTimeStr), "Max:");
            draw_bg_text_slot(totalTimeStr, 1, 6, totalTimeSlot);

            // Reset max trackers
            maxPlayer = 0;
            maxCamera = 0;
            maxTilemap = 0;
            maxRender = 0;
            maxTotal = 0;
        }
    }

    return 0;
}
