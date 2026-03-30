#include "menu.h"
#include "core/text.h"
#include "core/input.h"
#include "core/vram_layout.h"
#include "level/level.h"
#include "collision/collision.h"
#include "generated/connections.h"

// Menu state
static int inMenu = 1;              // Start in menu mode
static int menuSelection = 0;       // Currently highlighted level
static u16 prevKeys = 0;            // Previous frame keys for edge detection
static int menuInitialized = 0;     // Whether menu text has been drawn

// Fixed slot indices for menu UI
#define MENU_SLOT_TITLE 0
#define MENU_SLOT_LEVEL_START 1
#define MENU_SLOT_INSTRUCTIONS_1 (MENU_SLOT_LEVEL_START + LEVEL_COUNT)
#define MENU_SLOT_INSTRUCTIONS_2 (MENU_SLOT_INSTRUCTIONS_1 + 1)
#define MENU_SLOT_COUNT (MENU_SLOT_INSTRUCTIONS_2 + 1)

#if MENU_SLOT_COUNT > BG_TEXT_MAX_SLOTS
#error "Menu uses more BG text slots than available"
#endif

// Current level (set by menu)
static const Level* currentLevel = NULL;
static int currentLevelIndex = -1;  // -1 means in menu

// Tilemap state (accessed by menu functions)
static int oldCameraTileX = -1;
static int oldCameraTileY = -1;

static inline u8 gameplayScreenBase(u8 bgLayer) {
    return (u8)(SB_NIGHTSKY + bgLayer);
}

static void clearGameplayTilemaps(void) {
    volatile u16* bg1Map = (volatile u16*)(0x06000000 + (SB_BG1 << 11));
    volatile u16* bg2Map = (volatile u16*)(0x06000000 + (SB_BG2 << 11));
    for (int i = 0; i < 32 * 32; i++) {
        bg1Map[i] = 0;
        bg2Map[i] = 0;
    }
}

static void configureGameplayBgs(void) {
    REG_BG1CNT = (SB_BG1 << 8) | (0 << 2) | (0 << 0);
    REG_BG2CNT = (SB_BG2 << 8) | (0 << 2) | (1 << 0);
}

// Forward declarations
static void initGameplayForLevel(int levelIndex, Player* player, Camera* camera);

void initMenu(void) {
    inMenu = 1;
    menuSelection = 0;
    prevKeys = 0;
    menuInitialized = 0;
    currentLevel = NULL;
    currentLevelIndex = -1;
    oldCameraTileX = -1;
    oldCameraTileY = -1;
}

void renderMenu(void) {
    // Draw static text (only once)
    if (!menuInitialized) {
        draw_bg_text_slot("SELECT LEVEL", 8, 3, MENU_SLOT_TITLE);
        draw_bg_text_slot("UP/DOWN: Navigate", 4, 16, MENU_SLOT_INSTRUCTIONS_1);
        draw_bg_text_slot("A: Start", 4, 17, MENU_SLOT_INSTRUCTIONS_2);
        menuInitialized = 1;
    }

    // Update level list (changes based on selection)
    for (int i = 0; i < LEVEL_COUNT; i++) {
        char line[32];
        if (i == menuSelection) {
            // Selected item with arrow
            line[0] = '>';
            line[1] = ' ';
            int j;
            for (j = 0; g_levelNames[i][j] != '\0' && j < 28; j++) {
                line[j + 2] = g_levelNames[i][j];
            }
            line[j + 2] = '\0';
        } else {
            // Unselected item with spacing
            line[0] = ' ';
            line[1] = ' ';
            int j;
            for (j = 0; g_levelNames[i][j] != '\0' && j < 28; j++) {
                line[j + 2] = g_levelNames[i][j];
            }
            line[j + 2] = '\0';
        }
        draw_bg_text_slot(line, 8, 7 + i, MENU_SLOT_LEVEL_START + i);
    }
}

int updateAndRenderMenu(u16 keys, u16 pressed, Player* player, Camera* camera) {
    int oldSelection = menuSelection;

    if (pressed & BTN_UP) {
        menuSelection--;
        if (menuSelection < 0) {
            menuSelection = LEVEL_COUNT - 1;  // Wrap to bottom
        }
    } else if (pressed & BTN_DOWN) {
        menuSelection++;
        if (menuSelection >= LEVEL_COUNT) {
            menuSelection = 0;  // Wrap to top
        }
    }

    // Re-render if selection changed
    if (menuSelection != oldSelection) {
        renderMenu();
    }

    // Start selected level
    if (pressed & BTN_CONFIRM) {
        initGameplayForLevel(menuSelection, player, camera);
        return 0;  // Transitioning to gameplay
    }

    return 1;  // Still in menu
}

void returnToMenu(void) {
    inMenu = 1;

    // Hide player sprite (move offscreen)
    volatile u16* oam = (volatile u16*)MEM_OAM;
    oam[0] = 160;  // Y coordinate offscreen

    // Hide spring sprites
    for (int i = OAM_SPRING_BASE; i < OAM_SPRING_BASE + OAM_SPRING_COUNT; i++) {
        oam[i * 4] = 160;  // Y coordinate offscreen
    }

    // Clear BG1 and BG2 tilemaps (hide level tiles)
    clearGameplayTilemaps();
    configureGameplayBgs();

    // Clear all text (both menu and profiling)
    clear_bg_text();
    menuInitialized = 0;         // Reset so menu text will be redrawn

    // Show menu
    renderMenu();
}

int isInMenuMode(void) {
    return inMenu;
}

const Level* getCurrentLevel(void) {
    return currentLevel;
}

// Internal: Reset tilemap state to force full refresh
void resetTilemapState(void) {
    oldCameraTileX = -1;
    oldCameraTileY = -1;
}

// Initialize gameplay for a selected level
static void initGameplayForLevel(int levelIndex, Player* player, Camera* camera) {
    currentLevel = g_levels[levelIndex];
    currentLevelIndex = levelIndex;

    // Clear menu text and reset menu state
    clear_bg_text();
    menuInitialized = 0;  // Menu slots are now invalid

    // Load level tiles to VRAM
    loadLevelToVRAM(currentLevel);

    clearGameplayTilemaps();
    configureGameplayBgs();

    // Reset tilemap state variables to force full refresh
    resetTilemapState();

    // Set up BG control registers for each layer
    for (u8 i = 0; i < currentLevel->layerCount; i++) {
        const TileLayer* layer = &currentLevel->layers[i];
        u8 bgLayer = layer->bgLayer;
        u8 priority = layer->priority;
        u8 screenBase = gameplayScreenBase(bgLayer);

        if (bgLayer == 1) {
            REG_BG1CNT = (screenBase << 8) | (0 << 2) | (priority << 0);
        } else if (bgLayer == 2) {
            REG_BG2CNT = (screenBase << 8) | (0 << 2) | (priority << 0);
        }
    }

    // Initialize tilemaps for each layer
    for (u8 layerIdx = 0; layerIdx < currentLevel->layerCount; layerIdx++) {
        const TileLayer* layer = &currentLevel->layers[layerIdx];
        u8 bgLayer = layer->bgLayer;
        u8 screenBase = gameplayScreenBase(bgLayer);

        volatile u16* bgMap = (volatile u16*)(0x06000000 + (screenBase << 11));

        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                u16 tileId = getTileAt(currentLevel, layerIdx, x, y);
                bgMap[y * 32 + x] = mapTileEntry(g_levelTileEntries, tileId);
            }
        }
    }

    // Reset player to level spawn point
    initPlayer(player, currentLevel);

    // Reset camera
    camera->x = 0;
    camera->y = 0;

    // Show player sprite (make sure it's visible)
    volatile u16* oam = (volatile u16*)MEM_OAM;
    oam[0] = 0;

    // Switch to gameplay mode
    inMenu = 0;
}

int getCurrentLevelIndex(void) {
    return currentLevelIndex;
}

void switchToLevel(int levelIndex, Player* player, Camera* camera) {
    // Validate level index
    if (levelIndex < 0 || levelIndex >= LEVEL_COUNT) {
        return;
    }

    // Switch to the requested level
    initGameplayForLevel(levelIndex, player, camera);

    // Initialize player for the level
    initPlayer(player, currentLevel);

    // Reset camera position
    camera->x = 0;
    camera->y = 0;
}

void loadLevelForTransition(int levelIndex) {
    // Validate level index
    if (levelIndex < 0 || levelIndex >= LEVEL_COUNT) {
        return;
    }

    currentLevel = g_levels[levelIndex];
    currentLevelIndex = levelIndex;
    int reusingScrollTilemap = (g_levelBLayerTiles[0] != 0);

    // If a scroll transition just completed, level B's tile data is already
    // decompressed in g_levelBLayerTiles. Use the fast path (pointer swap +
    // VRAM write only) to avoid RLUnCompWram overflowing VBlank and causing
    // VRAM writes during active display (which produces single-frame tile glitches).
    if (reusingScrollTilemap) {
        adoptLevelBBuffer(currentLevel);
    } else {
        // Fade fallback: level B was never loaded into the B buffer, full load needed.
        loadLevelToVRAM(currentLevel);
        clearGameplayTilemaps();
    }

    configureGameplayBgs();

    // Set up BG control registers for each layer
    for (u8 i = 0; i < currentLevel->layerCount; i++) {
        const TileLayer* layer = &currentLevel->layers[i];
        u8 bgLayer = layer->bgLayer;
        u8 priority = layer->priority;
        u8 screenBase = gameplayScreenBase(bgLayer);

        if (bgLayer == 1) {
            REG_BG1CNT = (screenBase << 8) | (0 << 2) | (priority << 0);
        } else if (bgLayer == 2) {
            REG_BG2CNT = (screenBase << 8) | (0 << 2) | (priority << 0);
        }
    }
    // NOTE: Does NOT call initPlayer, resetTilemapState, or reset camera.
    // The transition system places the player and camera. The large camera
    // delta on the first gameplay frame will trigger a full tilemap refresh.
}
