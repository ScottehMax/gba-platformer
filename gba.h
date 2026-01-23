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
