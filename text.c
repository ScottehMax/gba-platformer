#include "text.h"
#include <string.h>

#define FONT_TILE_START 512  // Font tiles start at index 512 in sprite VRAM
#define FONT_PALETTE 1        // Font uses palette slot 1

// Background text tile start (in background char block 1)
#define BG_TEXT_DYNAMIC_START 96  // Start at tile 96 in char block 1 for dynamic text tiles

// Character widths array definition
const unsigned char font_char_widths[] = {
     3,  2,  4,  6,  4,  6,  7,  2,  3,  3,  6,  4,  2,  4,  2,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  4,  4,  4,  4,
     7,  4,  4,  4,  4,  3,  3,  4,  4,  4,  3,  4,  3,  6,  5,  4,
     4,  4,  4,  3,  4,  4,  4,  6,  4,  4,  4,  3,  4,  3,  6,  4,
     2,  4,  4,  4,  4,  4,  3,  4,  4,  2,  2,  4,  2,  6,  4,  4,
     4,  4,  3,  3,  3,  4,  4,  6,  4,  4,  3,  4,  2,  4,  5,
};

// Helper: Get pixel from font tile (4bpp format)
static u8 get_font_pixel(int char_index, int px, int py) {
    // Font tiles are in char block 1 at 0x06004000
    volatile u32* fontData = (volatile u32*)0x06004000;
    
    // Each tile is 8 u32s (32 bytes) in 4bpp format
    int tileStart = char_index * 8;  // 8 u32s per tile
    
    // In 4bpp, each u32 holds 8 pixels (4 bits each)
    int row_offset = py;  // Which u32 in the tile (one per row)
    u32 row_data = fontData[tileStart + row_offset];
    
    // Extract the pixel (4 bits) from the u32
    int shift = px * 4;
    return (row_data >> shift) & 0xF;
}

// Helper: Set pixel in dynamic tile buffer (4bpp format)
static void set_tile_pixel(u32* tileData, int px, int py, u8 colorIndex) {
    // Each row is one u32, with 8 pixels (4 bits each)
    int shift = px * 4;
    tileData[py] = (tileData[py] & ~(0xF << shift)) | ((colorIndex & 0xF) << shift);
}

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

void draw_bg_text(const char* str, int tile_x, int tile_y, int dynamic_tile_slot) {
    volatile u16* bgMap = (volatile u16*)0x06008800;
    volatile u32* charBlock1 = (volatile u32*)0x06004000;
    
    // Calculate starting tile in char block 1
    int base_tile = BG_TEXT_DYNAMIC_START + dynamic_tile_slot;
    
    // Calculate string width in pixels
    int total_width = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        if (c >= FONT_START_CHAR && c <= FONT_END_CHAR) {
            int char_index = c - FONT_START_CHAR;
            total_width += font_char_widths[char_index];
        }
    }
    
    // Calculate number of tiles needed
    int tiles_needed = (total_width + 7) / 8;
    if (tiles_needed > 20) tiles_needed = 20;  // Limit to prevent overflow
    
    // Create pixel buffer for the text (max 20 tiles = 160 pixels wide, 8 pixels tall)
    u8 pixelBuffer[8][160];
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 160; x++) {
            pixelBuffer[y][x] = 0;  // Transparent
        }
    }
    
    // Render text into pixel buffer with variable width
    int cursor_x = 0;
    for (int i = 0; str[i] != '\0' && cursor_x < 160; i++) {
        char c = str[i];
        if (c >= FONT_START_CHAR && c <= FONT_END_CHAR) {
            int char_index = c - FONT_START_CHAR;
            int width = font_char_widths[char_index];
            
            // Copy character pixels to buffer
            for (int py = 0; py < 8; py++) {
                for (int px = 0; px < width && (cursor_x + px) < 160; px++) {
                    u8 pixel = get_font_pixel(char_index, px, py);
                    pixelBuffer[py][cursor_x + px] = pixel;
                }
            }
            
            cursor_x += width;
        }
    }
    
    // Convert pixel buffer into 8x8 tiles and upload to VRAM
    for (int tile_idx = 0; tile_idx < tiles_needed; tile_idx++) {
        u32 tileData[8];  // 8 u32s per tile (8 rows, 8 pixels per row, 4bpp)
        
        // Clear tile
        for (int i = 0; i < 8; i++) {
            tileData[i] = 0;
        }
        
        // Extract 8x8 tile from pixel buffer
        int start_x = tile_idx * 8;
        for (int py = 0; py < 8; py++) {
            for (int px = 0; px < 8; px++) {
                u8 pixel = pixelBuffer[py][start_x + px];
                set_tile_pixel(tileData, px, py, pixel);
            }
        }
        
        // Upload to VRAM - each tile is 8 u32s
        int vram_tile_offset = (base_tile + tile_idx) * 8;
        for (int i = 0; i < 8; i++) {
            charBlock1[vram_tile_offset + i] = tileData[i];
        }
        
        // Update background map
        if ((tile_x + tile_idx) < 32 && tile_y < 32) {
            int tile_num = base_tile + tile_idx;
            bgMap[tile_y * 32 + (tile_x + tile_idx)] = tile_num | (1 << 12);
        }
    }
}

void draw_bg_text_px(const char* str, int px_x, int px_y) {
    // Convert pixel coordinates to tile coordinates
    draw_bg_text(str, px_x / 8, px_y / 8, 0);
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
