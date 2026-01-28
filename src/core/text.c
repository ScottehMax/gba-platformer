#include "text.h"
#include <string.h>

#define FONT_TILE_START 512  // Font tiles start at index 512 in sprite VRAM
#define FONT_PALETTE 1        // Font uses palette slot 1

// Background text tile start (in background char block 1)
#define BG_TEXT_DYNAMIC_START 1   // Start at tile 1 in char block 1 for dynamic text tiles
#define TEXT_SLOT_TILES 28        // Number of tiles allocated per text slot
#define MAX_TEXT_SLOTS 18         // Maximum number of text strings that can be allocated

// Tile slot tracking
static u8 tile_slot_used[MAX_TEXT_SLOTS] = {0};  // 0 = free, 1 = in use
static int next_free_slot = 0;

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
    // Read directly from ROM font data
    const u32* fontData = (const u32*)tinypixieTiles;

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
// BACKGROUND TEXT FUNCTIONS (BG3-based)
// ============================================================================

void init_bg_text() {
    // Set up BG3 control register (16-color mode, screen base 28, char base 1)
    // Screen base 28 (moved to avoid char block 2 conflict)
    // Char base 1
    // Note: We only use char block 1 for dynamic text tiles (starting at BG_TEXT_DYNAMIC_START)
    // Font character tiles are read directly from ROM, not stored in VRAM
    REG_BG3CNT = 0x0000 | (28 << 8) | (1 << 2);

    // Clear the text background
    clear_bg_text();

    // Reset slot tracking
    for (int i = 0; i < MAX_TEXT_SLOTS; i++) {
        tile_slot_used[i] = 0;
    }
    next_free_slot = 0;
}

void clear_bg_text() {
    // BG3 screen map at base block 28
    volatile u16* bgMap = (volatile u16*)se_mem[28];

    // Clear entire 32x32 tile map (fill with tile 0 = space/transparent)
    for (int i = 0; i < 32 * 32; i++) {
        bgMap[i] = 0;  // Tile 0 is empty
    }
}

void clear_bg_text_region(int tile_x, int tile_y, int width, int height) {
    volatile u16* bgMap = (volatile u16*)se_mem[28];
    
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

// Internal function to draw text to a specific slot
static void draw_bg_text_internal(const char* str, int tile_x, int tile_y, int dynamic_tile_slot) {
    volatile u16* bgMap = (volatile u16*)se_mem[28];
    volatile u32* charBlock1 = (volatile u32*)tile_mem[1];
    
    // Calculate starting tile in char block 1
    int base_tile = BG_TEXT_DYNAMIC_START + (dynamic_tile_slot * TEXT_SLOT_TILES);
    
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
    if (tiles_needed > TEXT_SLOT_TILES) tiles_needed = TEXT_SLOT_TILES;  // Limit to prevent overflow

    // Create pixel buffer for the text (8 pixels tall, width = TEXT_SLOT_TILES * 8 pixels)
    u8 pixelBuffer[8][TEXT_SLOT_TILES * 8];
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < TEXT_SLOT_TILES * 8; x++) {
            pixelBuffer[y][x] = 0;  // Transparent
        }
    }

    // Render text into pixel buffer with variable width
    int cursor_x = 0;
    for (int i = 0; str[i] != '\0' && cursor_x < TEXT_SLOT_TILES * 8; i++) {
        char c = str[i];
        if (c >= FONT_START_CHAR && c <= FONT_END_CHAR) {
            int char_index = c - FONT_START_CHAR;
            int width = font_char_widths[char_index];

            // Copy character pixels to buffer
            for (int py = 0; py < 8; py++) {
                for (int px = 0; px < width && (cursor_x + px) < TEXT_SLOT_TILES * 8; px++) {
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

// Allocate and draw text automatically (returns slot ID)
int draw_bg_text_auto(const char* str, int tile_x, int tile_y) {
    // Find a free slot
    int slot = -1;
    for (int i = 0; i < MAX_TEXT_SLOTS; i++) {
        if (tile_slot_used[i] == 0) {
            slot = i;
            tile_slot_used[i] = 1;
            break;
        }
    }
    
    if (slot == -1) {
        return -1;  // No free slots
    }
    
    draw_bg_text_internal(str, tile_x, tile_y, slot);
    return slot;
}

// Update text in an existing slot
void draw_bg_text_slot(const char* str, int tile_x, int tile_y, int slot_id) {
    if (slot_id < 0 || slot_id >= MAX_TEXT_SLOTS) return;
    draw_bg_text_internal(str, tile_x, tile_y, slot_id);
}

// Free a slot for reuse
void free_bg_text_slot(int slot_id) {
    if (slot_id >= 0 && slot_id < MAX_TEXT_SLOTS) {
        tile_slot_used[slot_id] = 0;
    }
}

void draw_bg_text_px(const char* str, int px_x, int px_y) {
    // Convert pixel coordinates to tile coordinates
    draw_bg_text_auto(str, px_x / 8, px_y / 8);
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
    oam_mem[oam_index].attr0 = (y & 0xFF) | (0 << 8) | (0 << 10) | (0 << 13) | (0 << 14);
    oam_mem[oam_index].attr1 = (x & 0x1FF) | (0 << 12) | (0 << 13) | (0 << 14);
    oam_mem[oam_index].attr2 = tile_index | (FONT_PALETTE << 12) | (0 << 10);
    
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
