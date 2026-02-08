#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <tonc.h>
#include "game_math.h"

typedef struct {
    int x;  // Fixed-point
    int y;  // Fixed-point
    float vx; // Fixed-point
    float vy; // Fixed-point
    float maxFall; // Current max fall speed (ramps for fast fall)
    int onGround;
    int coyoteTime;   // Frames remaining for coyote time jump
    int jumpBuffer;   // Frames remaining in jump buffer
    int jumpHeld;     // 1 if jump was initiated this aerial phase
    float varJumpSpeed; // Initial jump velocity for var jump clamping
    int varJumpTimer;   // Frames remaining for variable jump window
    int dashing;
    int dashes;                // Current dashes available (0-2)
    int maxDashes;             // Maximum dashes (usually 1)
    int dashCooldownTimer;     // Frames before can dash again
    int dashRefillCooldownTimer;  // Frames before dash refills on ground
    int facingRight;  // 1 = right, 0 = left
    u16 prevKeys;     // Previous frame keys for detecting button presses

    // Wall slide/jump
    float wallSlideTimer;  // Time remaining for wall slide ability (resets on ground/jump)
    int wallSlideDir;      // Direction of current wall slide: 1=right, -1=left, 0=none

    // Dash attack (for super jumps)
    int dashAttackTimer;   // Frames remaining where super jumps can be triggered (set on dash start)

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
