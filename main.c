#include "gba.h"
#include "skelly.h"
#include "grassy_stone.h"
#include "level3.h"
#include "text.h"
#include "src/core/game_math.h"
#include "src/core/game_types.h"
#include "src/core/debug_utils.h"
#include "src/level/level.h"
#include "src/camera/camera.h"
#include "src/collision/collision.h"
#include "src/player/player.h"
#include "src/player/player_render.h"

void vsync() {
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

int main() {
    // Load level
    const Level* currentLevel = &Tutorial_Level;
    
    // Mode 0 with BG0, BG1 and sprites enabled
    REG_DISPCNT = VIDEOMODE_0 | BG0_ENABLE | (1 << 9) | OBJ_ENABLE | OBJ_1D_MAP;  // BG1_ENABLE = bit 9

    // Enable alpha blending for sprites
    // BLDCNT: Effect=Alpha blend (bit 6), NO global OBJ target (sprites set semi-transparent individually)
    // 2nd target=BG0+BG1+BD (bits 8,9,13) - what semi-transparent sprites blend with
    REG_BLDCNT = (1 << 6) | (1 << 8) | (1 << 9) | (1 << 13);
    // BLDALPHA: Set blend coefficients EVA (sprite) and EVB (background) - must sum to 16 or less
    REG_BLDALPHA = (7 << 0) | (9 << 8);  // ~44% trail, ~56% background (more transparent)

    // Set up background palette
    volatile u16* bgPalette = MEM_BG_PALETTE;

    // Copy grassy_stone palette to background palette
    for (int i = 0; i < 256; i++) {
        bgPalette[i] = grassy_stonePal[i];
    }

    // Override palette index 0 with dark blue for sky
    bgPalette[0] = COLOR(3, 6, 15);
    
    // Load font palette to palette slot 1 (colors 16-31) to avoid conflict with game tiles
    for (int i = 0; i < 16; i++) {
        bgPalette[16 + i] = tinypixiePal[i];
    }

    // Create background tiles in VRAM
    volatile u32* bgTiles = MEM_BG_TILES;

    // Tile 0: Sky (all pixels palette index 0)
    for (int i = 0; i < 16; i++) {
        bgTiles[i] = 0x00000000;
    }

    // Copy all grassy_stone tiles (starting at tile index 1)
    // grassy_stoneTilesLen is the byte length, divide by 4 to get u32 count
    int tileCount = grassy_stoneTilesLen / 4;
    for (int i = 0; i < tileCount; i++) {
        bgTiles[16 + i] = grassy_stoneTiles[i];
    }

    // Set BG0 control register (256 color mode = bit 7, screen base 16, char base 0)
    REG_BG0CNT = 0x0080 | (16 << 8);
    
    // Initialize background text system (BG1 - uses char block 1)
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

    // Set up sprite 0 as 16x16, 16-color mode
    volatile u16* oam = (volatile u16*)MEM_OAM;
    oam[0] = 0;
    oam[1] = (1 << 14);
    oam[2] = 0;

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

    // Background map pointer
    volatile u16* bgMap = MEM_BG0_MAP;

    // Frame counter for demo
    int frameCount = 0;
    char posXStr[32] = "X: 0";
    char posYStr[32] = "Y: 0";
    char velXStr[32] = "VX: 0";
    char velYStr[32] = "VY: 0";
    char groundStr[32] = "Ground: No";
    
    // Allocate text slots once
    draw_bg_text_auto("Debug Info", 1, 1);
    int posXSlot = draw_bg_text_auto(posXStr, 1, 2);
    int posYSlot = draw_bg_text_auto(posYStr, 1, 3);
    int velXSlot = draw_bg_text_auto(velXStr, 1, 4);
    int velYSlot = draw_bg_text_auto(velYStr, 1, 5);
    int groundSlot = draw_bg_text_auto(groundStr, 1, 6);

    // Game loop
    while (1) {
        vsync();

        u16 keys = getKeys();
        updatePlayer(&player, keys, currentLevel);
        updateCamera(&camera, &player, currentLevel);

        // Track previous camera tile position to avoid redundant background updates
        static int prevCameraTileX = -1;
        static int prevCameraTileY = -1;

        // Calculate which tiles are visible
        int startTileX = camera.x / 8;
        int startTileY = camera.y / 8;

        // Only update background map if camera moved to a different tile
        if (startTileX != prevCameraTileX || startTileY != prevCameraTileY) {
            // GBA background is 32x32 tiles, we render from level data
            for (int y = 0; y < 32; y++) {
                for (int x = 0; x < 32; x++) {
                    int levelTileX = startTileX + x;
                    int levelTileY = startTileY + y;

                    u8 tileId = getTileAt(currentLevel, levelTileX, levelTileY);
                    bgMap[y * 32 + x] = tileId;
                }
            }

            prevCameraTileX = startTileX;
            prevCameraTileY = startTileY;
        }
        
        // Set background scroll registers
        REG_BG0HOFS = camera.x % 8;
        REG_BG0VOFS = camera.y % 8;
        
        drawPlayer(&player, &camera);
        
        // Update debug info every frame
        // Position X (in pixels)
        int posX = player.x >> FIXED_SHIFT;
        int posY = player.y >> FIXED_SHIFT;
        
        // Convert X position to string
        int_to_string(posX, posXStr, sizeof(posXStr), "X: ");
        draw_bg_text_slot(posXStr, 1, 2, posXSlot);
        
        // Convert Y position to string
        posY = player.y >> FIXED_SHIFT;
        int_to_string(posY, posYStr, sizeof(posYStr), "Y: ");
        draw_bg_text_slot(posYStr, 1, 3, posYSlot);
        
        // Convert VX velocity to string
        int velX = player.vx >> FIXED_SHIFT;
        int_to_string(velX, velXStr, sizeof(velXStr), "VX: ");
        draw_bg_text_slot(velXStr, 1, 4, velXSlot);
        
        // Convert VY velocity to string
        int velY = player.vy >> FIXED_SHIFT;
        int_to_string(velY, velYStr, sizeof(velYStr), "VY: ");
        draw_bg_text_slot(velYStr, 1, 5, velYSlot);
        
        // On ground status
        if (player.onGround) {
            draw_bg_text_slot("Ground: Yes", 1, 6, groundSlot);
        } else {
            draw_bg_text_slot("Ground: No", 1, 6, groundSlot);
        }
        
        frameCount++;
    }

    return 0;
}
