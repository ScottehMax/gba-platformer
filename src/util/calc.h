#ifndef CALC_H
#define CALC_H

#ifdef DESKTOP_BUILD
#include "desktop/desktop_stubs.h"
#else
#include <tonc.h>
#endif

// Smoothly approach a target value by a maximum step amount (float version)
static inline float approach(float val, float target, float maxMove) {
    return val > target ? max(val - maxMove, target) : min(val + maxMove, target);
}

// Fixed-point version of approach (for integers)
static inline int approachInt(int val, int target, int maxMove) {
    if (val < target) {
        val += maxMove;
        if (val > target) val = target;
    } else if (val > target) {
        val -= maxMove;
        if (val < target) val = target;
    }
    return val;
}

// Fixed-point multiplication: (a * b) >> FIXED_SHIFT
// Avoids overflow by using wider integer type
static inline int fpMul(int a, int b) {
    return ((s32)a * (s32)b) >> 8;  // FIXED_SHIFT = 8
}

#endif // CALC_H