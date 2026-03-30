#ifndef VRAM_LAYOUT_H
#define VRAM_LAYOUT_H

// Centralised VRAM, palette, and OAM allocation map.
// Every module that touches GBA video hardware should include this header
// instead of embedding magic numbers.

// --- BG palette banks (16 colours each, pal_bg_mem) ---
#define PAL_BG_TERRAIN      0   // grassy_stone (colours 0-15)
#define PAL_BG_FONT         1   // tinypixie font (colours 16-31)
#define PAL_BG_PLANTS       2   // plants tileset (colours 32-47)
#define PAL_BG_DECALS       3   // decals tileset (colours 48-63)
#define PAL_BG_NIGHTSKY     4   // nightsky background (colours 64-79)

// --- OBJ palette banks (16 colours each, pal_obj_mem) ---
#define PAL_OBJ_PLAYER      0   // player sprite
#define PAL_OBJ_TRAIL_BASE  1   // dash trail fade palettes (banks 1-10)
#define PAL_OBJ_TRAIL_COUNT 10
#define PAL_OBJ_SPRING      11  // spring entities
#define PAL_OBJ_RED_BUBBLE  12  // red bubble entities
#define PAL_OBJ_GREEN_BUBBLE 13 // green bubble entities

// --- OBJ tile indices (4bpp, tile_mem[4]) ---
#define TILE_OBJ_PLAYER     0   // 16x16 player (tiles 0-3)
#define TILE_OBJ_ENTITY     4   // shared 8x8 filled square for entities

// --- OAM sprite index ranges ---
#define OAM_PLAYER          0
#define OAM_TRAIL_BASE      1   // trail sprites 1..3
#define OAM_SPRING_BASE     16  // springs 16..47
#define OAM_SPRING_COUNT    32
#define OAM_RED_BUBBLE_BASE 48  // red bubbles 48..79
#define OAM_RED_BUBBLE_COUNT 32
#define OAM_GREEN_BUBBLE_BASE 80 // green bubbles 80..111
#define OAM_GREEN_BUBBLE_COUNT 32

// --- BG screen bases (0x06000000 + (base << 11)) ---
#define SB_NIGHTSKY         24  // BG0 nightsky tilemap
#define SB_BG1              25  // BG1 gameplay layer
#define SB_BG2              26  // BG2 gameplay layer

// --- BG char bases (0x06000000 + (base << 14)) ---
#define CB_NIGHTSKY         2   // nightsky tile graphics

#endif // VRAM_LAYOUT_H
