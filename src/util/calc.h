#ifndef CALC_H
#define CALC_H

#include <tonc.h>

// Smoothly approach a target value by a maximum step amount
static inline float approach(float val, float target, float maxMove) {
    return val > target ? max(val - maxMove, target) : min(val + maxMove, target);
}

#endif // CALC_H