#ifndef DESKTOP_STUBS_H
#define DESKTOP_STUBS_H

#ifdef DESKTOP_BUILD

#include <stdint.h>
#include <stddef.h>

// GBA types
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

// GBA key defines
#define KEY_A        0x0001
#define KEY_B        0x0002
#define KEY_SELECT   0x0004
#define KEY_START    0x0008
#define KEY_RIGHT    0x0010
#define KEY_LEFT     0x0020
#define KEY_UP       0x0040
#define KEY_DOWN     0x0080
#define KEY_R        0x0100
#define KEY_L        0x0200

// Math helpers (from tonc)
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define ABS(x) ((x) < 0 ? -(x) : (x))

// Stubs for missing functions
void* memset(void* s, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);

#endif // DESKTOP_BUILD
#endif // DESKTOP_STUBS_H
