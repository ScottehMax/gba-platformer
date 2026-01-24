#include "text.h"
#include <string.h>

#define FONT_TILE_START 512  // Font tiles start at index 512 in sprite VRAM
#define FONT_PALETTE 1        // Font uses palette slot 1

// Background text tile start (in background char block 0)
#define BG_FONT_TILE_START 256  // Start at tile 256 (after game tiles)

// Character widths array definition
const unsigned char font_char_widths[] = {
     3,  2,  4,  6,  4,  6,  7,  2,  3,  3,  6,  4,  2,  4,  2,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  4,  4,  4,  4,
     7,  4,  4,  4,  4,  3,  3,  4,  4,  4,  3,  4,  3,  6,  5,  4,
     4,  4,  4,  3,  4,  4,  4,  6,  4,  4,  4,  3,  4,  3,  6,  4,
     2,  4,  4,  4,  4,  4,  3,  4,  4,  2,  2,  4,  2,  6,  4,  4,
     4,  4,  3,  3,  3,  4,  4,  6,  4,  4,  3,  4,  2,  4,  5,
};

// ============================================================================
// BACKGROUND TEXT FUNCTIONS (BG1-based)
// ============================================================================

void init_bg_text() {
    // Copy font tiles to background VRAM char block 1 (0x06004000)
    // This keeps text tiles separate from game tiles in char block 0
    volatile u32* bgTiles = (volatile u32*)0x06004000;  // Char block 1
    
    for (int i = 0; i < tinypixieTilesLen / 4; i++) {
        bgTiles[i] = tinypixieTiles[i];
    }
    
    // Set up BG1 control register (16-color mode, screen base 17, char base 1)
    // Screen base 17 = 0x06008800 (after BG0's screen at 0x06008000)
    // Char base 1 = 0x06004000
    *((volatile u16*)0x0400000A) = 0x0000 | (17 << 8) | (1 << 2);
    
    // Clear the text background
    clear_bg_text();
}

void clear_bg_text() {
    // BG1 screen map at base block 17 (0x06008800)
    volatile u16* bgMap = (volatile u16*)0x06008800;
    
    // Clear entire 32x32 tile map (fill with tile 0 = space/transparent)
    for (int i = 0; i < 32 * 32; i++) {
        bgMap[i] = 0;  // Tile 0 is empty
    }
}

void clear_bg_text_region(int tile_x, int tile_y, int width, int height) {
    volatile u16* bgMap = (volatile u16*)0x06008800;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int tx = tile_x + x;
            int ty = tile_y + y;
            if (tx >= 0 && tx < 32 && ty >= 0 && ty < 32) {
                bgMap[ty * 32 + tx] = 0;
            }
        }
    }
}

void draw_bg_text(const char* str, int tile_x, int tile_y) {
    volatile u16* bgMap = (volatile u16*)0x06008800;
    
    int cursor_x = tile_x;
    
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        
        // Handle newlines
        if (c == '\n') {
            tile_y++;
            cursor_x = tile_x;
            continue;
        }
        
        // Skip if out of bounds
        if (cursor_x >= 32 || tile_y >= 32) break;
        
        // Map ASCII character to tile index (tiles start at 0 in char block 1)
        if (c >= FONT_START_CHAR && c <= FONT_END_CHAR) {
            int char_index = c - FONT_START_CHAR;
            int tile_index = char_index;
            // Set tile with palette 1 (bit 12-15 = palette number in 16-color mode)
            bgMap[tile_y * 32 + cursor_x] = tile_index | (1 << 12);
            cursor_x++;
        }
    }
}

void draw_bg_text_px(const char* str, int px_x, int px_y) {
    // Convert pixel coordinates to tile coordinates
    draw_bg_text(str, px_x / 8, px_y / 8);
}

// ============================================================================
// SPRITE TEXT FUNCTIONS (OAM-based)
// ============================================================================

int draw_char(char c, int x, int y, int oam_index) {
    if (c < FONT_START_CHAR || c > FONT_END_CHAR) {
        return 0; // Invalid character
    }
    
    int char_index = c - FONT_START_CHAR;
    int width = font_char_widths[char_index];
    
    // Calculate position in spritesheet (characters are in a 16-wide grid)
    // Each character cell is now 8x8 pixels, spritesheet is 128x48 (16x6 tiles)
    int char_row = char_index / FONT_CHARS_PER_ROW;
    int char_col = char_index % FONT_CHARS_PER_ROW;
    
    // Each character is exactly 1 tile (8x8)
    int tile_index = FONT_TILE_START + (char_row * FONT_CHARS_PER_ROW) + char_col;
    
    // Set OAM entry (8x8 sprite, 16-color mode, palette 1)
    OAM[oam_index].attr0 = (y & 0xFF) | (0 << 8) | (0 << 10) | (0 << 13) | (0 << 14);
    OAM[oam_index].attr1 = (x & 0x1FF) | (0 << 12) | (0 << 13) | (0 << 14);
    OAM[oam_index].attr2 = tile_index | (FONT_PALETTE << 12) | (0 << 10);
    
    return width;
}

int draw_text(const char* str, int x, int y, int start_oam_index) {
    int cursor_x = x;
    int oam_used = 0;
    
    for (int i = 0; str[i] != '\0' && (start_oam_index + oam_used) < 128; i++) {
        int width = draw_char(str[i], cursor_x, y, start_oam_index + oam_used);
        if (width > 0) {
            cursor_x += width; // No extra spacing - characters touch
            oam_used++;
        }
    }
    
    return oam_used;
}

int text_width(const char* str) {
    int total_width = 0;
    
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        if (c >= FONT_START_CHAR && c <= FONT_END_CHAR) {
            int char_index = c - FONT_START_CHAR;
            total_width += font_char_widths[char_index]; // No spacing
        }
    }
    
    return total_width;
}
