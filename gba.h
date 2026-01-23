#ifndef GBA_H
#define GBA_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

// Video memory and control registers
#define MEM_IO      0x04000000
#define MEM_VRAM    0x06000000
#define MEM_PALETTE 0x05000000

#define REG_DISPCNT *((volatile u16*)(MEM_IO))
#define REG_VCOUNT  *((volatile u16*)0x04000006)

#define VIDEOMODE_4 0x0004
#define BGMODE_2    0x0400
#define BACKBUFFER  0x0010

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 160

#define COLOR(r, g, b) ((r) | ((g) << 5) | ((b) << 10))

// Palette indices
#define COLOR_BLACK 0
#define COLOR_RED   1
#define COLOR_GREEN 2
#define COLOR_SKY   3
#define COLOR_WHITE 4

// DMA registers
#define REG_DMA3SAD  (*(volatile u32*)0x040000D4)
#define REG_DMA3DAD  (*(volatile u32*)0x040000D8)
#define REG_DMA3CNT  (*(volatile u32*)0x040000DC)

#define DMA_ENABLE    0x80000000
#define DMA_SRC_FIXED 0x01000000
#define DMA_DST_INC   0x00000000
#define DMA_16        0x00000000
#define DMA_32        0x04000000

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

// Fast DMA fill function
static inline void dmaFill(void* dest, u32 value, u32 count) {
    static volatile u32 dmaFillValue;
    dmaFillValue = value;
    REG_DMA3SAD = (u32)&dmaFillValue;
    REG_DMA3DAD = (u32)dest;
    REG_DMA3CNT = count | DMA_SRC_FIXED | DMA_DST_INC | DMA_32 | DMA_ENABLE;
}

// Drawing functions for Mode 4 (8-bit palettized)
volatile u8* backBuffer = (volatile u8*)(MEM_VRAM + 0xA000);

inline void flipPage(void) {
    REG_DISPCNT ^= BACKBUFFER;
    if (REG_DISPCNT & BACKBUFFER) {
        backBuffer = (volatile u8*)MEM_VRAM;
    } else {
        backBuffer = (volatile u8*)(MEM_VRAM + 0xA000);
    }
}

inline void setPixel(int x, int y, u8 colorIndex) {
    backBuffer[y * SCREEN_WIDTH + x] = colorIndex;
}

inline void drawRect(int x, int y, int width, int height, u8 colorIndex) {
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int px = x + j;
            int py = y + i;
            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                backBuffer[py * SCREEN_WIDTH + px] = colorIndex;
            }
        }
    }
}

inline void drawCircle(int cx, int cy, int radius, u8 colorIndex) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                int px = cx + x;
                int py = cy + y;
                if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                    backBuffer[py * SCREEN_WIDTH + px] = colorIndex;
                }
            }
        }
    }
}

inline void setPalette(u8 index, u16 color) {
    ((volatile u16*)MEM_PALETTE)[index] = color;
}

#endif
