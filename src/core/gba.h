#ifndef GBA_H
#define GBA_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed short s16;

// Video memory and control registers
#define MEM_IO          0x04000000
#define MEM_VRAM        0x06000000
#define MEM_PALETTE     0x05000000
#define MEM_OAM         0x07000000

// I/O Register addresses
#define REG_DISPCNT     *((volatile u16*)(MEM_IO))
#define REG_VCOUNT      *((volatile u16*)(MEM_IO + 0x06))
#define REG_BG0CNT      *((volatile u16*)(MEM_IO + 0x08))
#define REG_BG1CNT      *((volatile u16*)(MEM_IO + 0x0A))
#define REG_BG2CNT      *((volatile u16*)(MEM_IO + 0x0C))
#define REG_BG3CNT      *((volatile u16*)(MEM_IO + 0x0E))
#define REG_BG0HOFS     *((volatile u16*)(MEM_IO + 0x10))
#define REG_BG0VOFS     *((volatile u16*)(MEM_IO + 0x12))
#define REG_BG1HOFS     *((volatile u16*)(MEM_IO + 0x14))
#define REG_BG1VOFS     *((volatile u16*)(MEM_IO + 0x16))
#define REG_BG2HOFS     *((volatile u16*)(MEM_IO + 0x18))
#define REG_BG2VOFS     *((volatile u16*)(MEM_IO + 0x1A))
#define REG_BLDCNT      *((volatile u16*)(MEM_IO + 0x50))
#define REG_BLDALPHA    *((volatile u16*)(MEM_IO + 0x52))

// VRAM addresses
#define MEM_BG_TILES    ((volatile u32*)(MEM_VRAM))
#define MEM_BG0_MAP     ((volatile u16*)(MEM_VRAM + 0x8000))
#define MEM_SPRITE_TILES ((volatile u32*)(MEM_VRAM + 0x10000))

// Character blocks (16KB each, 4 total for BG)
#define CHAR_BLOCK(n)   ((volatile u32*)(MEM_VRAM + ((n) * 0x4000)))
// Screen blocks (2KB each, 32 total)
#define SCREEN_BLOCK(n) ((volatile u16*)(MEM_VRAM + ((n) * 0x800)))

// Palette addresses
#define MEM_BG_PALETTE  ((volatile u16*)(MEM_PALETTE))
#define MEM_SPRITE_PALETTE ((volatile u16*)(MEM_PALETTE + 0x200))

#define VIDEOMODE_0 0x0000
#define BG0_ENABLE  0x0100
#define BG1_ENABLE  0x0200
#define OBJ_ENABLE  0x1000
#define OBJ_1D_MAP  0x0040

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 160

#define COLOR(r, g, b) ((r) | ((g) << 5) | ((b) << 10))

// OAM (Object Attribute Memory) structures
typedef struct {
    u16 attr0;
    u16 attr1;
    u16 attr2;
    s16 fill;
} OBJ_ATTR;

#define OAM ((volatile OBJ_ATTR*)MEM_OAM)

// Timer registers
#define REG_TM0CNT_L (*(volatile u16*)(MEM_IO + 0x100))
#define REG_TM0CNT_H (*(volatile u16*)(MEM_IO + 0x102))
#define REG_TM1CNT_L (*(volatile u16*)(MEM_IO + 0x104))
#define REG_TM1CNT_H (*(volatile u16*)(MEM_IO + 0x106))

// Timer control flags
#define TM_ENABLE    0x0080
#define TM_CASCADE   0x0004
#define TM_FREQ_1    0x0000  // 16.78 MHz
#define TM_FREQ_64   0x0001  // 262.21 KHz
#define TM_FREQ_256  0x0002  // 65.536 KHz
#define TM_FREQ_1024 0x0003  // 16.384 KHz

// Input registers
#define REG_KEYINPUT (*(volatile u16*)0x04000130)

#define KEY_A      0x0001
#define KEY_B      0x0002
#define KEY_SELECT 0x0004
#define KEY_START  0x0008
#define KEY_RIGHT  0x0010
#define KEY_LEFT   0x0020
#define KEY_UP     0x0040
#define KEY_DOWN   0x0080
#define KEY_R      0x0100
#define KEY_L      0x0200

inline u16 getKeys(void) {
    return ~REG_KEYINPUT & 0x03FF;
}

inline void setPalette(u8 index, u16 color) {
    ((volatile u16*)MEM_PALETTE)[index] = color;
}

#endif
