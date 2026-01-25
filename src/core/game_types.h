#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include "core/gba.h"
#include "game_math.h"

typedef struct {
    int x;  // Fixed-point
    int y;  // Fixed-point
    int vx; // Fixed-point
    int vy; // Fixed-point
    int onGround;
    int coyoteTime;   // Frames remaining for coyote time jump
    int dashing;
    int dashCooldown;
    int facingRight;  // 1 = right, 0 = left
    u16 prevKeys;     // Previous frame keys for detecting button presses

    // Dash trail
    int trailX[TRAIL_LENGTH];  // Fixed-point positions
    int trailY[TRAIL_LENGTH];
    int trailFacing[TRAIL_LENGTH];
    int trailIndex;  // Current trail buffer index
    int trailTimer;  // Frames since last trail update
    int trailFadeTimer;  // Frames since dash ended (for gradual fade)
} Player;

typedef struct {
    int x;  // Camera X in pixels
    int y;  // Camera Y in pixels
} Camera;

#endif
