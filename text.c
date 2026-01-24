#include "text.h"
#include <string.h>

#define FONT_TILE_START 512  // Font tiles start at index 512 in sprite VRAM
#define FONT_PALETTE 1        // Font uses palette slot 1

// Character widths array definition
const unsigned char font_char_widths[] = {
     3,  2,  4,  6,  4,  6,  7,  2,  3,  3,  6,  4,  2,  4,  2,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  4,  4,  4,  4,
     7,  4,  4,  4,  4,  3,  3,  4,  4,  4,  3,  4,  3,  6,  5,  4,
     4,  4,  4,  3,  4,  4,  4,  6,  4,  4,  4,  3,  4,  3,  6,  4,
     2,  4,  4,  4,  4,  4,  3,  4,  4,  2,  2,  4,  2,  6,  4,  4,
     4,  4,  3,  3,  3,  4,  4,  6,  4,  4,  3,  4,  2,  4,  5,
};

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
