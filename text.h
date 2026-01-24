#ifndef TEXT_H
#define TEXT_H

#include "gba.h"
#include "tinypixie.h"
#include "assets/tinypixie_widths.h"

// Background text functions (BG1-based) - for lots of text
void init_bg_text();
void clear_bg_text();
void clear_bg_text_region(int tile_x, int tile_y, int width, int height);
void draw_bg_text(const char* str, int tile_x, int tile_y);
void draw_bg_text_px(const char* str, int px_x, int px_y);

// Sprite text functions (OAM-based) - for small dynamic text
int draw_char(char c, int x, int y, int oam_index);
int draw_text(const char* str, int x, int y, int start_oam_index);
int text_width(const char* str);

#endif
