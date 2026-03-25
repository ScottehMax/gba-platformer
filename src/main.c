#include <tonc.h>
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
#include "core/input.h"
#include "level/level.h"
#include "camera/camera.h"
#include "collision/collision.h"
#include "player/player.h"
#include "player/player_render.h"
#include "util/calc.h"
#include "menu/menu.h"
#include "core/replay.h"
#include "transition/transition.h"
#include "entities/spring.h"
#include "entities/redbubble.h"
#include "entities/greenbubble.h"

// Tileset palette bank assignments
#define PALETTE_GRASSY_STONE 0
#define PALETTE_FONT         1
#define PALETTE_PLANTS       2
#define PALETTE_DECALS       3

// Fixed slot indices for profiling (8-15)
#define PROFILING_SLOT_FPS 8
#define PROFILING_SLOT_PLAYER 9
#define PROFILING_SLOT_CAMERA 10
#define PROFILING_SLOT_TILEMAP 11
#define PROFILING_SLOT_RENDER 12
#define PROFILING_SLOT_TOTAL 13

// Tilemap state (used in game loop)
static int oldCameraTileX = -1;
static int oldCameraTileY = -1;
static int wasScrolling   = 0;
static int lastScrollToTileX0 = 0;
static int lastScrollToTileY0 = 0;
static int lastScrollBgOriginX = 0;
static int lastScrollBgOriginY = 0;
static int bgTileOriginX = 0;
static int bgTileOriginY = 0;

static inline u16 tileEntryNormal(const Level* level, u8 layerIdx, int lx, int ly) {
    u16 tid = getTileAt(level, layerIdx, lx, ly);
    return (tid + (u16)getLevelTileVramOffset()) | ((u16)level->tilePaletteBanks[tid] << 12);
}

static inline int floorDiv8(int v) {
    return (v >= 0) ? (v / 8) : -(((-v) + 7) / 8);
}

static void prefillScrollSeam(volatile u16* bgMap, u8 layerIdx,
                              const ScrollTransInfo* scrollInfo,
                              int scrollBgOriginX, int scrollBgOriginY,
                              int cameraTileX, int cameraTileY) {
    if (scrollInfo->seamPrefillAxis == 1) {
        int seamStartsOnRight = scrollInfo->toTileX0 > scrollInfo->fromTileX0;
        for (int s = 0; s < 2; s++) {
            int virtualX = seamStartsOnRight
                ? (scrollInfo->toTileX0 + s)
                : (scrollInfo->fromTileX0 - 1 - s);
            int mapX = virtualX + scrollBgOriginX;
            int mx = mapX & 31;
            for (int ty = 0; ty < 32; ty++) {
                int mapY = cameraTileY + ty;
                int virtualY = mapY - scrollBgOriginY;
                bgMap[(mapY & 31) * 32 + mx] = getScrollTileEntry(layerIdx, virtualX, virtualY);
            }
        }
    } else if (scrollInfo->seamPrefillAxis == 2) {
        int seamStartsBelow = scrollInfo->toTileY0 > scrollInfo->fromTileY0;
        for (int s = 0; s < 2; s++) {
            int virtualY = seamStartsBelow
                ? (scrollInfo->toTileY0 + s)
                : (scrollInfo->fromTileY0 - 1 - s);
            int mapY = virtualY + scrollBgOriginY;
            int my = mapY & 31;
            for (int tx = 0; tx < 32; tx++) {
                int mapX = cameraTileX + tx;
                int virtualX = mapX - scrollBgOriginX;
                bgMap[my * 32 + (mapX & 31)] = getScrollTileEntry(layerIdx, virtualX, virtualY);
            }
        }
    }
}

// Profiling state
static int profilingInitialized = 0;

int main() {
    irq_init(NULL);
    irq_add(II_VBLANK, NULL);
    
    // Mode 0 with BG0, BG1, BG2, BG3 and sprites enabled
    // BG0 = nightsky, BG1 = decorative layer, BG2 = terrain layer, BG3 = text
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1 | DCNT_BG2 | DCNT_BG3 | DCNT_OBJ | DCNT_OBJ_1D;

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
    volatile u16* bgPalette = pal_bg_mem;
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

    // Initialize background text system (BG3 - uses char block 1)
    init_bg_text();

    // Copy sprite palette to VRAM
    volatile u16* spritePalette = pal_obj_mem;

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
                spritePalette[(pal + 1) * 16 + i] = RGB15(r, g, b);
            }
        }
    }

    // Copy player sprite to VRAM (char block 4)
    volatile u32* spriteTiles = (volatile u32*)tile_mem[4];
    for (int i = 0; i < 32; i++) {  // 16-color mode: 4 tiles, 8 u32s per tile
        spriteTiles[i] = skellyTiles[i];
    }

    // Create red square sprite for springs (tile 4 = 8x8 red square)
    // Tile 4 is at index 4 * 8 = 32 u32s offset (each tile is 8 u32s in 4bpp mode)
    volatile u32* redSquareTile = &spriteTiles[32];
    for (int i = 0; i < 8; i++) {  // 8 u32s per tile in 4bpp mode
        // Each u32 contains 8 pixels (4 bits each)
        // Pixel value 1 = use palette color 1 in spring palette bank (11)
        redSquareTile[i] = 0x11111111;  // All pixels = color 1
    }

    // Create dedicated palette bank 11 for springs (colors 176-191)
    // This avoids interfering with player palette (bank 0) or trail palettes (banks 1-10)
    spritePalette[11 * 16 + 0] = 0;  // Transparent
    spritePalette[11 * 16 + 1] = RGB15(31, 0, 0);  // Bright red

    // Create dedicated palette bank 12 for red bubbles (colors 192-207)
    spritePalette[12 * 16 + 0] = 0;  // Transparent
    spritePalette[12 * 16 + 1] = RGB15(31, 16, 0);  // Orange (red + half green)

    // Create dedicated palette bank 13 for green bubbles (colors 208-223)
    spritePalette[13 * 16 + 0] = 0;  // Transparent
    spritePalette[13 * 16 + 1] = RGB15(0, 31, 0);  // Bright green

    // Set up sprite 0 as 16x16, 16-color mode, priority 1
    volatile u16* oam = (volatile u16*)MEM_OAM;
    oam[0] = 0;
    oam[1] = (1 << 14);
    oam[2] = (1 << 10);  // Priority 1

    // Hide other sprites
    for (int i = 1; i < 128; i++) {
        oam[i * 4] = 160;
    }

    // Initialize player, camera, and spring manager (will be set properly when level loads)
    Player player;
    Camera camera;
    camera.x = 0;
    camera.y = 0;

    SpringManager springManager;
    initSpringManager(&springManager);

    RedBubbleManager redBubbleManager;
    initRedBubbleManager(&redBubbleManager);

    GreenBubbleManager greenBubbleManager;
    initGreenBubbleManager(&greenBubbleManager);

    // Hide player sprite initially (we're in menu mode)
    oam[0] = 160;  // Y coordinate offscreen (reuse oam pointer from above)

    // Clear BG1 and BG2 tilemaps (hide level tiles in menu)
    volatile u16* bg1Map = (volatile u16*)(0x06000000 + (25 << 11));
    volatile u16* bg2Map = (volatile u16*)(0x06000000 + (26 << 11));
    for (int i = 0; i < 32 * 32; i++) {
        bg1Map[i] = 0;
        bg2Map[i] = 0;
    }

    // Initialize and show the level selection menu
    initMenu();
    renderMenu();

    // Initialize timers for FPS counter
    // Timer 0: counts at 16.384 KHz (overflow after ~4 seconds)
    // Timer 1: cascades from Timer 0 for extended range
    REG_TM0CNT_L = 0;  // Initial value
    REG_TM1CNT_L = 0;
    REG_TM0CNT_H = TM_ENABLE | TM_FREQ_1024;  // Enable, prescaler 1024
    REG_TM1CNT_H = TM_ENABLE | TM_CASCADE;    // Enable, cascade from Timer 0

    // Frame counter and profiling (only used during gameplay)
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

    // Previous frame keys for edge detection
    u16 prevKeys = 0;

    // Replay system
    ReplayState replay;
    initReplay(&replay);
    initTransition();
    char replayStr[32] = "";

    // Track current level for spring reloading
    int lastLevelIndex = -1;

    // Game loop
    while (1) {
        u16 frameStart = REG_TM0CNT_L;  // Measure from start of frame
        VBlankIntrWait();  // Efficient VBlank wait using BIOS interrupt
        key_poll();
        u16 realKeys = key_curr_state();
        u16 keys = realKeys;

        // Replay controls (SELECT + L/R/B)
        if ((realKeys & BTN_SELECT) && (realKeys & ~prevKeys & BTN_REPLAY_START)) {
            // SELECT+L: Start recording and save current position and level
            startRecording(&replay);
            setReplayStartPosition(&replay, player.x, player.y);
            setReplayLevel(&replay, getCurrentLevelIndex());
            profilingInitialized = 0;  // Force redraw to show replay status
        } else if ((realKeys & BTN_SELECT) && (realKeys & ~prevKeys & BTN_REPLAY_PLAY)) {
            // SELECT+R: Start playback and restore position
            int startX, startY;
            getReplayStartPosition(&replay, &startX, &startY);
            player.x = startX;
            player.y = startY;
            player.vx = 0;
            player.vy = 0;
            startPlayback(&replay);
            profilingInitialized = 0;  // Force redraw to show replay status
        } else if ((realKeys & BTN_SELECT) && (realKeys & ~prevKeys & BTN_REPLAY_SAVE)) {
            // SELECT+B: Stop and save replay to SRAM
            if (replay.mode != REPLAY_MODE_OFF) {
                stopReplay(&replay);
                saveReplayToSRAM(&replay);
                // Show confirmation
                siprintf(replayStr, "SAVED %d frames", replay.frameCount);
                draw_bg_text_slot(replayStr, 1, 7, 14);
                profilingInitialized = 0;  // Force redraw later
            }
        } else if ((realKeys & BTN_SELECT) && (realKeys & ~prevKeys & BTN_REPLAY_LOAD)) {
            // SELECT+DOWN: Load replay from SRAM, switch level if needed, and restore position
            loadReplayFromSRAM(&replay);
            if (replay.frameCount > 0) {
                int replayLevelIndex = getReplayLevel(&replay);

                // Switch to the replay's level if different from current
                if (replayLevelIndex != getCurrentLevelIndex()) {
                    switchToLevel(replayLevelIndex, &player, &camera);
                }

                int startX, startY;
                getReplayStartPosition(&replay, &startX, &startY);
                player.x = startX;
                player.y = startY;
                player.vx = 0;
                player.vy = 0;
                startPlayback(&replay);
                siprintf(replayStr, "LOADED %d frames", replay.frameCount);
                draw_bg_text_slot(replayStr, 1, 7, 14);
                profilingInitialized = 0;
            } else {
                siprintf(replayStr, "No replay in save");
                draw_bg_text_slot(replayStr, 1, 7, 14);
            }
        }

        // Use replay input if playing back
        if (replay.mode == REPLAY_MODE_PLAYBACK) {
            keys = getPlaybackInput(&replay);
            if (replay.mode == REPLAY_MODE_OFF) {
                // Playback finished
                profilingInitialized = 0;  // Force redraw
            }
        }

        // Record input if recording
        if (replay.mode == REPLAY_MODE_RECORDING) {
            recordFrame(&replay, keys);
        }

        u16 pressed = keys & ~prevKeys;
        prevKeys = keys;

        if (isInMenuMode()) {
            // Menu mode
            updateAndRenderMenu(keys, pressed, &player, &camera);
        } else {
            // Gameplay mode
            frameCount++;

            // Draw profiling text on first entry
            if (!profilingInitialized) {
                draw_bg_text_slot(fpsStr, 1, 1, PROFILING_SLOT_FPS);
                draw_bg_text_slot(playerTimeStr, 1, 2, PROFILING_SLOT_PLAYER);
                draw_bg_text_slot(cameraTimeStr, 1, 3, PROFILING_SLOT_CAMERA);
                draw_bg_text_slot(tilemapTimeStr, 1, 4, PROFILING_SLOT_TILEMAP);
                draw_bg_text_slot(renderTimeStr, 1, 5, PROFILING_SLOT_RENDER);
                draw_bg_text_slot(totalTimeStr, 1, 6, PROFILING_SLOT_TOTAL);

                // Replay status
                if (replay.mode == REPLAY_MODE_RECORDING) {
                    siprintf(replayStr, "REC: %d/%d", replay.frameCount, MAX_REPLAY_FRAMES);
                } else if (replay.mode == REPLAY_MODE_PLAYBACK) {
                    siprintf(replayStr, "PLAY: %d/%d", replay.currentFrame, replay.frameCount);
                } else {
                    siprintf(replayStr, "L:Rec B:Save DOWN:Load");
                }
                draw_bg_text_slot(replayStr, 1, 7, 14);

                profilingInitialized = 1;
            }

            // Update replay status every 60 frames
            if (frameCount % 60 == 0 && replay.mode != REPLAY_MODE_OFF) {
                if (replay.mode == REPLAY_MODE_RECORDING) {
                    siprintf(replayStr, "REC: %d/%d", replay.frameCount, MAX_REPLAY_FRAMES);
                } else if (replay.mode == REPLAY_MODE_PLAYBACK) {
                    siprintf(replayStr, "PLAY: %d/%d", replay.currentFrame, replay.frameCount);
                }
                draw_bg_text_slot(replayStr, 1, 7, 14);
            }

            // Check for START to return to menu
            if (pressed & BTN_MENU) {
                returnToMenu();
                profilingInitialized = 0;  // Reset profiling display for next time
                oldCameraTileX = -1;       // Reset tilemap state
                oldCameraTileY = -1;
                wasScrolling   = 0;
                lastScrollBgOriginX = 0;
                lastScrollBgOriginY = 0;
                bgTileOriginX = 0;
                bgTileOriginY = 0;
                continue;
            }

            // Get current level (initial read; may be updated after transition)
            const Level* currentLevel = getCurrentLevel();
            int currentLevelIndex = getCurrentLevelIndex();

            // Set transition context so collision code knows the current level index + camera
            setTransitionLevelContext(currentLevelIndex, camera.x, camera.y, player.x, player.y);

            int transitionActiveAtFrameStart = isTransitioning();

            // Profile: Player update
            u16 t0 = REG_TM0CNT_L;
            if (transitionActiveAtFrameStart) {
                updateTransition(&player, &camera);
            } else {
                updatePlayer(&player, keys, currentLevel);
                updateSprings(&springManager, &player);
                updateRedBubbles(&redBubbleManager, &player);
                updateGreenBubbles(&greenBubbleManager, &player);
            }
            u16 t1 = REG_TM0CNT_L;
            u16 dtPlayer = t1 - t0;
            if (dtPlayer > maxPlayer) maxPlayer = dtPlayer;

            // Re-read level state in case a transition just switched levels
            currentLevel = getCurrentLevel();
            currentLevelIndex = getCurrentLevelIndex();

            int levelChanged = (currentLevelIndex != lastLevelIndex);

            // Skip camera updates both when a transition started this frame and
            // when a transition is committing this frame after starting earlier.
            int transitionBusyThisFrame = transitionActiveAtFrameStart || isTransitioning();
            if (!transitionBusyThisFrame) {
                updateCamera(&camera, &player, currentLevel);
            }
            u16 t2 = REG_TM0CNT_L;
            u16 dtCamera = t2 - t1;
            if (dtCamera > maxCamera) maxCamera = dtCamera;

            // Tilemap update - supports both normal play and scroll transitions.
            // During a scroll transition camera.x/y are virtual coordinates spanning
            // both levels; getScrollTileEntry() selects the right level per tile.
            ScrollTransInfo scrollInfo;
            getScrollTransInfo(&scrollInfo);

            int scrollJustStarted = (!wasScrolling && scrollInfo.active);
            int scrollJustEnded   = ( wasScrolling && !scrollInfo.active);
            int usedSeamPrefill   = 0;
            int scrollBgOriginX   = bgTileOriginX;
            int scrollBgOriginY   = bgTileOriginY;
            int scrollCameraX     = camera.x;
            int scrollCameraY     = camera.y;

            if (scrollInfo.active) {
                if (scrollJustStarted) {
                    getTransitionVirtualCamera(&scrollCameraX, &scrollCameraY);
                }
                scrollBgOriginX = bgTileOriginX - scrollInfo.fromTileX0;
                scrollBgOriginY = bgTileOriginY - scrollInfo.fromTileY0;
            }

            if (levelChanged && !scrollJustEnded) {
                bgTileOriginX = 0;
                bgTileOriginY = 0;
            }

            if (scrollJustEnded) {
                bgTileOriginX = lastScrollBgOriginX + lastScrollToTileX0;
                bgTileOriginY = lastScrollBgOriginY + lastScrollToTileY0;
            }

            int bgCameraX = scrollInfo.active ? (scrollCameraX + scrollBgOriginX * 8)
                                              : (camera.x + bgTileOriginX * 8);
            int bgCameraY = scrollInfo.active ? (scrollCameraY + scrollBgOriginY * 8)
                                              : (camera.y + bgTileOriginY * 8);

            // Set hardware scroll registers first, then update tilemap.
            // For the incremental path (delta ≤ 2 tiles) this is safe: the column
            // being written is always past the visible right/bottom edge at the new
            // scroll position, so the hardware never sees the old content there.
            REG_BG1HOFS = bgCameraX;
            REG_BG1VOFS = bgCameraY;
            REG_BG2HOFS = bgCameraX;
            REG_BG2VOFS = bgCameraY;

            // On the trigger frame (scrollJustStarted) camera.x/y are still in the
            // from-level's physical coordinate space. Use the transition system's
            // virtual start camera plus a scroll-specific ring origin so the existing
            // 32x32 ring buffer stays valid without a full redraw.
            if (scrollJustEnded) {
                oldCameraTileX = floorDiv8(bgCameraX);
                oldCameraTileY = floorDiv8(bgCameraY);
            }
            if (scrollInfo.active) {
                lastScrollToTileX0 = scrollInfo.toTileX0;
                lastScrollToTileY0 = scrollInfo.toTileY0;
                lastScrollBgOriginX = scrollBgOriginX;
                lastScrollBgOriginY = scrollBgOriginY;
            }
            wasScrolling = scrollInfo.active;

            int cameraTileX = floorDiv8(bgCameraX);
            int cameraTileY = floorDiv8(bgCameraY);

            if (cameraTileX != oldCameraTileX || cameraTileY != oldCameraTileY) {
                int deltaX = cameraTileX - oldCameraTileX;
                int deltaY = cameraTileY - oldCameraTileY;

                // Always iterate all supported BG layers so extra layers get cleared
                // (tile 0 = transparent) when switching to a level with fewer layers.
                // Levels with fewer layers return tile 0 for out-of-range layerIdx.
                const u8 MAX_BG_LAYERS = 2;

                usedSeamPrefill = scrollInfo.active &&
                                  scrollInfo.seamPrefillAxis != 0 &&
                                  !scrollJustStarted;

                int tileOriginX = scrollInfo.active ? scrollBgOriginX : bgTileOriginX;
                int tileOriginY = scrollInfo.active ? scrollBgOriginY : bgTileOriginY;

                for (u8 layerIdx = 0; layerIdx < MAX_BG_LAYERS; layerIdx++) {
                    u8 bgLayer, screenBase;
                    if (layerIdx < currentLevel->layerCount) {
                        bgLayer = currentLevel->layers[layerIdx].bgLayer;
                    } else if (scrollInfo.active && layerIdx < scrollInfo.toLevel->layerCount) {
                        bgLayer = scrollInfo.toLevel->layers[layerIdx].bgLayer;
                    } else {
                        bgLayer = layerIdx;  // default: layer N maps to BG(N+1)
                    }
                    screenBase = 25 + bgLayer;

                    volatile u16* bgMap = (volatile u16*)(0x06000000 + (screenBase << 11));

#define TILE_ENTRY(lx, ly) \
    (scrollInfo.active \
        ? getScrollTileEntry(layerIdx, (lx) - tileOriginX, (ly) - tileOriginY) \
        : tileEntryNormal(currentLevel, layerIdx, (lx) - tileOriginX, (ly) - tileOriginY))

                    int adx = deltaX < 0 ? -deltaX : deltaX;
                    int ady = deltaY < 0 ? -deltaY : deltaY;
                    if (oldCameraTileX == -1 || adx > 2 || ady > 2) {
                        // Full refresh (only on init or after large jumps like scroll end)
                        for (int ty = 0; ty < 32; ty++) {
                            for (int tx = 0; tx < 32; tx++) {
                                int lx = cameraTileX + tx;
                                int ly = cameraTileY + ty;
                                bgMap[(ly & 31) * 32 + (lx & 31)] = TILE_ENTRY(lx, ly);
                            }
                        }
                    } else {
                        if (usedSeamPrefill) {
                            prefillScrollSeam(bgMap, layerIdx, &scrollInfo,
                                              scrollBgOriginX, scrollBgOriginY,
                                              cameraTileX, cameraTileY);
                        }

                        // Incremental: write up to 2 new columns and/or rows.
                        // At max 2 tiles/frame (16px) the new columns are always past
                        // the visible right edge at the new register position — no glitch.
                        if (deltaX != 0) {
                            for (int s = 0; s < adx; s++) {
                                int lx = (deltaX > 0) ? (cameraTileX + 31 - s)
                                                      : (cameraTileX + s);
                                int mx = lx & 31;
                                for (int ty = 0; ty < 32; ty++) {
                                    int ly = cameraTileY + ty;
                                    bgMap[(ly & 31) * 32 + mx] = TILE_ENTRY(lx, ly);
                                }
                            }
                        }
                        if (deltaY != 0) {
                            for (int s = 0; s < ady; s++) {
                                int ly = (deltaY > 0) ? (cameraTileY + 31 - s)
                                                      : (cameraTileY + s);
                                int my = ly & 31;
                                for (int tx = 0; tx < 32; tx++) {
                                    int lx = cameraTileX + tx;
                                    bgMap[my * 32 + (lx & 31)] = TILE_ENTRY(lx, ly);
                                }
                            }
                        }
                    }
#undef TILE_ENTRY
                }

                if (usedSeamPrefill) {
                    consumeTransitionSeamPrefill();
                }
                oldCameraTileX = cameraTileX;
                oldCameraTileY = cameraTileY;
            }

            // Profile: Tilemap update
            u16 t3 = REG_TM0CNT_L;
            u16 dtTilemap = t3 - t2;
            if (dtTilemap > maxTilemap) maxTilemap = dtTilemap;

            // Keep transition-end tilemap writes first in VBlank to avoid
            // one-frame BG1 garbage when switching levels.
            if (levelChanged) {
                loadSpringsFromLevel(&springManager, currentLevel);
                loadRedBubblesFromLevel(&redBubbleManager, currentLevel);
                loadGreenBubblesFromLevel(&greenBubbleManager, currentLevel);
                lastLevelIndex = currentLevelIndex;
            }

            // Profile: Rendering
            // During a scroll transition, camera.x/y are in virtual space but the player
            // and entities use physical level coordinates. Subtract the from-level's virtual
            // origin so screen positions are computed correctly.
            // During a scroll transition, camera.x/y are in virtual space from frame T+1
            // onwards (after updateTransition first advances it). On the trigger frame
            // (scrollJustStarted), camera.x is still in physical space, so do NOT offset.
            Camera renderCamera = camera;
            if (scrollInfo.active && !scrollJustStarted) {
                renderCamera.x = camera.x - scrollInfo.fromTileX0 * 8;
                renderCamera.y = camera.y - scrollInfo.fromTileY0 * 8;
            }
            u16 playerPriority = scrollInfo.active ? 0 : 1;
            drawPlayer(&player, &renderCamera, playerPriority);
            renderSprings(&springManager, renderCamera.x, renderCamera.y);
            renderRedBubbles(&redBubbleManager, renderCamera.x, renderCamera.y);
            renderGreenBubbles(&greenBubbleManager, renderCamera.x, renderCamera.y);
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
                draw_bg_text_slot(fpsStr, 1, 1, PROFILING_SLOT_FPS);

                // Update profiling display - show MAX values (worst frame)
                int_to_string(maxPlayer, playerTimeStr, sizeof(playerTimeStr), "P:");
                draw_bg_text_slot(playerTimeStr, 1, 2, PROFILING_SLOT_PLAYER);

                int_to_string(maxCamera, cameraTimeStr, sizeof(cameraTimeStr), "C:");
                draw_bg_text_slot(cameraTimeStr, 1, 3, PROFILING_SLOT_CAMERA);

                int_to_string(maxTilemap, tilemapTimeStr, sizeof(tilemapTimeStr), "T:");
                draw_bg_text_slot(tilemapTimeStr, 1, 4, PROFILING_SLOT_TILEMAP);

                int_to_string(maxRender, renderTimeStr, sizeof(renderTimeStr), "R:");
                draw_bg_text_slot(renderTimeStr, 1, 5, PROFILING_SLOT_RENDER);

                int_to_string(maxTotal, totalTimeStr, sizeof(totalTimeStr), "Max:");
                draw_bg_text_slot(totalTimeStr, 1, 6, PROFILING_SLOT_TOTAL);

                // Reset max trackers
                maxPlayer = 0;
                maxCamera = 0;
                maxTilemap = 0;
                maxRender = 0;
                maxTotal = 0;
            }
        }  // End gameplay mode
    }  // End while loop

    return 0;
}
