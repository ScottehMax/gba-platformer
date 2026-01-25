#include "core/gba.h"
#include "skelly.h"
#include "grassy_stone.h"
#include "plants.h"
#include "decals.h"
#include "level3.h"
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
    
    // Mode 0 with BG0, BG2, BG3 and sprites enabled
    // BG0 = nightsky, BG2 = terrain, decorations, BG3 = text
    REG_DISPCNT = VIDEOMODE_0 | (1 << 8) | (1 << 10) | (1 << 11) | OBJ_ENABLE | OBJ_1D_MAP;  // BG0_ENABLE = bit 8, BG2_ENABLE = bit 10, BG3_ENABLE = bit 11

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
    // 2nd target=BG0+BG1+BD (bits 8,9,13) - what semi-transparent sprites blend with
    REG_BLDCNT = (1 << 6) | (1 << 8) | (1 << 9) | (1 << 13);
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

    // Set BG2 control register (4-bit color, screen base 26, char base 0, priority 1)
    REG_BG2CNT = (26 << 8) | (0 << 2) | (1 << 0);
    
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

    // Background map pointer (32x32 tilemap on BG2 at screen base 26)
    volatile u16* bgMap = (volatile u16*)(0x06000000 + (26 << 11));
    
    // Initialize tilemap once at startup
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            u16 vramIndex = getTileAt(currentLevel, x, y);
            u8 paletteBank = getTilePaletteBank(vramIndex, currentLevel);
            bgMap[y * 32 + x] = vramIndex | (paletteBank << 12);
        }
    }

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
        frameCount++;

        u16 keys = getKeys();
        updatePlayer(&player, keys, currentLevel);
        updateCamera(&camera, &player, currentLevel);
        
        // Same approach as before - simple and working
        static int oldCameraTileX = -1;
        static int oldCameraTileY = -1;
        int cameraTileX = camera.x / 8;
        int cameraTileY = camera.y / 8;
        
        if (cameraTileX != oldCameraTileX || cameraTileY != oldCameraTileY) {
            // Update the single tilemap - exactly like the original
            for (int y = 0; y < 32; y++) {
                for (int x = 0; x < 32; x++) {
                    int tx = cameraTileX + x;
                    int ty = cameraTileY + y;
                    u16 tileId = getTileAt(currentLevel, tx, ty);
                    // Palette bank is pre-computed and stored with tile
                    bgMap[y * 32 + x] = tileId | (currentLevel->tilePaletteBanks[tileId] << 12);
                }
            }
            oldCameraTileX = cameraTileX;
            oldCameraTileY = cameraTileY;
        }
        
        REG_BG2HOFS = camera.x % 8;
        REG_BG2VOFS = camera.y % 8;
        
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
