#ifndef TEXT_H
#define TEXT_H

#include "gba.h"
#include "tinypixie.h"
#include "assets/tinypixie_widths.h"

/*
 * Text Drawing API for GBA
 * ========================
 * 
 * This API provides simple text rendering using the TinyPixie font.
 * Characters are rendered as 8x8 OAM sprites with variable width.
 * 
 * SETUP:
 * ------
 * 1. Load font tiles to sprite VRAM:
 *    volatile u32* spriteTiles = (volatile u32*)0x06010000;
 *    int fontTileStart = 512; // Starting tile index for font
 *    for (int i = 0; i < tinypixieTilesLen / 4; i++) {
 *        spriteTiles[fontTileStart * 8 + i] = tinypixieTiles[i];
 *    }
 * 
 * 2. Load font palette to sprite palette:
 *    volatile u16* spritePalette = (volatile u16*)0x05000200;
 *    for (int i = 0; i < 16; i++) {
 *        spritePalette[i] = tinypixiePal[i];
 *    }
 * 
 * 3. Enable sprites in display control:
 *    REG_DISPCNT = VIDEOMODE_0 | BG0_ENABLE | OBJ_ENABLE | OBJ_1D_MAP;
 * 
 * USAGE:
 * ------
 * Draw text at (x, y) using OAM sprites starting at index 100:
 *    int sprites_used = draw_text("Hello World!", 10, 10, 100);
 * 
 * Get width of text before drawing:
 *    int width = text_width("Score: 1234");
 * 
 * NOTES:
 * ------
 * - Each character uses one OAM sprite (128 sprites max on GBA)
 * - Characters are automatically spaced with 1px gap
 * - Supports ASCII printable characters (32-126)
 * - Font uses variable width for better readability
 * - Remember to reserve OAM sprite indices for text!
 */

// Draw a single character at screen position (uses OAM sprites)
// Returns the width of the character drawn
int draw_char(char c, int x, int y, int oam_index);

// Draw a string at screen position (uses multiple OAM sprites)
// Returns the number of OAM sprites used
int draw_text(const char* str, int x, int y, int start_oam_index);

// Get the pixel width of a string (useful for centering or alignment)
int text_width(const char* str);

#endif
