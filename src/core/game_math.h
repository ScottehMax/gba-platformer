#ifndef GAME_MATH_H
#define GAME_MATH_H

// Fixed-point math (8 bits fractional) for smooth sub-pixel movement
#define FIXED_SHIFT 8
#define FIXED_ONE (1 << FIXED_SHIFT)

// Physics constants
#define GRAVITY (FIXED_ONE / 2)
#define JUMP_STRENGTH (FIXED_ONE * 5)
#define MAX_SPEED (FIXED_ONE * 3)
#define ACCELERATION (FIXED_ONE * 1)
#define FRICTION (FIXED_ONE / 6)
#define AIR_FRICTION (FIXED_ONE / 4)
#define DASH_SPEED (FIXED_ONE * 5)
#define COYOTE_TIME 6  // Frames of grace period after walking off edge
#define MAX_FALL_SPEED (FIXED_ONE * 6)

// Jump feel
#define JUMP_BUFFER_TIME 5  // Frames to remember jump press
#define JUMP_RELEASE_MULTIPLIER 2  // Divide vy by this when jump released
#define JUMP_PEAK_THRESHOLD (FIXED_ONE / 2)  // vy threshold for "peak" (128 = 0.5)
#define PEAK_GRAVITY_MULTIPLIER 2  // Divide gravity by this at peak

// Corner correction
#define CORNER_CORRECTION_DISTANCE (FIXED_ONE * 6)  // Max pixels nudge
#define DASH_LEDGE_POP_HEIGHT (FIXED_ONE * 6)  // Max pixels upward pop
#define BONK_NUDGE_RANGE (FIXED_ONE * 6)  // Max pixels for ceiling corner nudge

// Player constants
#define PLAYER_RADIUS 8
#define TRAIL_LENGTH 10

#endif
