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

#define REG_DISPCNT *((volatile u16*)(MEM_IO))
#define REG_VCOUNT  *((volatile u16*)0x04000006)

#define VIDEOMODE_0 0x0000
#define BG0_ENABLE  0x0100
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
} __attribute__((packed, aligned(4))) OBJ_ATTR;

#define OAM ((volatile OBJ_ATTR*)MEM_OAM)

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
