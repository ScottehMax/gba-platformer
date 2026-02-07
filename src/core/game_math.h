#ifndef GAME_MATH_H
#define GAME_MATH_H

// Fixed-point math (8 bits fractional) for smooth sub-pixel movement
#define FIXED_SHIFT 8
#define FIXED_ONE (1 << FIXED_SHIFT)

// BUG IN GBA CODE: Missing dt multiplication when integrating position!
// Celeste: Position += Speed * dt AND Speed += Accel * dt
// GBA:     x += vx (NO /60!) BUT vy += accel/60
//
// This means velocities act as "per frame" but accelerations are "per second"
// Conversion:
//   VELOCITIES: Celeste_pixels/sec * (1 frame / 60 sec) * (256 fp / 1 pixel) = Celeste * 256/60
//   ACCELERATIONS: Need (CONST/60) to equal change per frame
//                  Change per frame = Celeste_accel * dt² * 256 = Celeste * (1/3600) * 256
//                  So: CONST/60 = Celeste/3600*256 → CONST = Celeste*256/60
#define TO_GBA(x) ((x) * 256.0f / 60.0f)

// Physics constants (from Celeste Player.cs)
#define GRAVITY TO_GBA(900.0f)        // 3840
#define JUMP_STRENGTH TO_GBA(-105.0f) // -448
#define MAX_SPEED TO_GBA(90.0f)       // 384
#define ACCELERATION TO_GBA(1000.0f)  // 4266.67
#define RUN_REDUCE TO_GBA(400.0f)     // 1706.67
#define AIR_MULT 0.65f
#define DASH_SPEED TO_GBA(240.0f)     // 1024
#define DASH_LENGTH 9 // Frames of dash duration (0.15s at 60fps)
#define END_DASH_SPEED TO_GBA(160.0f) // 682.67
#define COYOTE_TIME 6  // Frames of grace period after walking off edge, 0.1 seconds at 60fps
#define MAX_FALL_SPEED TO_GBA(160.0f)     // 682.67
#define FAST_MAX_FALL_SPEED TO_GBA(240.0f) // 1024
#define FAST_MAX_ACCEL TO_GBA(300.0f)     // 1280

// Jump feel
#define JUMP_HORIZONTAL_BOOST TO_GBA(40.0f) // 170.67
#define JUMP_BUFFER_TIME 5  // Frames to remember jump press
#define JUMP_RELEASE_MULTIPLIER 2  // Divide vy by this when jump released
#define VAR_JUMP_TIME 12  // Frames to maintain jump velocity (0.2s at 60fps)
// #define JUMP_PEAK_THRESHOLD (FIXED_ONE / 2)  // vy threshold for "peak" (128 = 0.5)
#define PEAK_GRAVITY_MULTIPLIER 2  // Divide gravity by this at peak
#define HALF_GRAV_THRESHOLD TO_GBA(40.0f) // 170.67 - When abs(vy) is below this, apply reduced gravity for floaty feel


// Corner correction
#define CORNER_CORRECTION_DISTANCE (FIXED_ONE * 6)  // Max pixels nudge
#define DASH_LEDGE_POP_HEIGHT (FIXED_ONE * 6)  // Max pixels upward pop
#define BONK_NUDGE_RANGE (FIXED_ONE * 6)  // Max pixels for ceiling corner nudge

// Player constants
#define PLAYER_RADIUS 8
#define TRAIL_LENGTH 10

#endif
