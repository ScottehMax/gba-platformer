#include "menu.h"
#include "core/text.h"
#include "core/input.h"
#include "level/level.h"
#include "collision/collision.h"
#include "generated/celeste1.h"
#include "generated/level1.h"
#include "generated/smb11.h"

// Level registry for menu
typedef struct {
    const char* displayName;
    const Level* level;
} LevelEntry;

static const LevelEntry levels[] = {
    {"Tutorial", &Tutorial_Level},
    {"Celeste 1", &Celeste1},
    {"Test Level", &Test_Level_1},
    {"SMB 1-1", &smb11}
};
#define LEVEL_COUNT (sizeof(levels) / sizeof(levels[0]))

// Menu state
static int inMenu = 1;              // Start in menu mode
static int menuSelection = 0;       // Currently highlighted level
static u16 prevKeys = 0;            // Previous frame keys for edge detection
static int menuInitialized = 0;     // Whether menu text has been drawn

// Fixed slot indices for menu (0-6)
#define MENU_SLOT_TITLE 0
#define MENU_SLOT_LEVEL_START 1
#define MENU_SLOT_INSTRUCTIONS_1 5
#define MENU_SLOT_INSTRUCTIONS_2 6

// Current level (set by menu)
static const Level* currentLevel = NULL;
static int currentLevelIndex = -1;  // -1 means in menu

// Tilemap state (accessed by menu functions)
static int oldCameraTileX = -1;
static int oldCameraTileY = -1;

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
            for (j = 0; levels[i].displayName[j] != '\0' && j < 28; j++) {
                line[j + 2] = levels[i].displayName[j];
            }
            line[j + 2] = '\0';
        } else {
            // Unselected item with spacing
            line[0] = ' ';
            line[1] = ' ';
            int j;
            for (j = 0; levels[i].displayName[j] != '\0' && j < 28; j++) {
                line[j + 2] = levels[i].displayName[j];
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

    // Clear BG1 and BG2 tilemaps (hide level tiles)
    volatile u16* bg1Map = (volatile u16*)(0x06000000 + (25 << 11));
    volatile u16* bg2Map = (volatile u16*)(0x06000000 + (26 << 11));
    for (int i = 0; i < 32 * 32; i++) {
        bg1Map[i] = 0;
        bg2Map[i] = 0;
    }

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
    currentLevel = levels[levelIndex].level;
    currentLevelIndex = levelIndex;

    // Clear menu text and reset menu state
    clear_bg_text();
    menuInitialized = 0;  // Menu slots are now invalid

    // Load level tiles to VRAM
    loadLevelToVRAM(currentLevel);

    // Reset tilemap state variables to force full refresh
    resetTilemapState();

    // Set up BG control registers for each layer
    for (u8 i = 0; i < currentLevel->layerCount; i++) {
        const TileLayer* layer = &currentLevel->layers[i];
        u8 bgLayer = layer->bgLayer;
        u8 priority = layer->priority;
        u8 screenBase = 25 + bgLayer;

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
        u8 screenBase = 25 + bgLayer;

        volatile u16* bgMap = (volatile u16*)(0x06000000 + (screenBase << 11));

        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                u16 vramIndex = getTileAt(currentLevel, layerIdx, x, y);
                u8 paletteBank = getTilePaletteBank(vramIndex, currentLevel);
                bgMap[y * 32 + x] = vramIndex | (paletteBank << 12);
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
