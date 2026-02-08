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

// Wall slide/jump
#define WALL_SLIDE_START_MAX TO_GBA(20.0f)  // 85.33 - Very slow initial wall slide speed
#define WALL_SLIDE_TIME 72  // Frames before wall slide reaches normal fall speed (1.2s at 60fps)
#define WALL_JUMP_H_SPEED TO_GBA(130.0f)  // 554.67 - Horizontal speed for wall jump (MaxRun + JumpHBoost)
#define WALL_JUMP_CHECK_DIST 3  // Pixels to check for wall

// Super jump (hyper dash) mechanics
#define SUPER_JUMP_H TO_GBA(260.0f)  // 1109.33 - Horizontal speed for super jump from dash
#define SUPER_JUMP_SPEED JUMP_STRENGTH  // Same vertical as normal jump
#define SUPER_WALL_JUMP_H TO_GBA(170.0f)  // 725.33 - MaxRun + JumpHBoost * 2
#define SUPER_WALL_JUMP_SPEED TO_GBA(-160.0f)  // -682.67 - Stronger than normal jump
#define SUPER_WALL_JUMP_VAR_TIME 15  // 0.25s at 60fps - Longer var jump time
#define DASH_ATTACK_TIME 18  // 0.3s at 60fps - Window for super jumps after dash


// Corner correction
#define CORNER_CORRECTION_DISTANCE (FIXED_ONE * 6)  // Max pixels nudge
#define DASH_LEDGE_POP_HEIGHT (FIXED_ONE * 6)  // Max pixels upward pop
#define BONK_NUDGE_RANGE (FIXED_ONE * 6)  // Max pixels for ceiling corner nudge

// Player constants
#define PLAYER_RADIUS 8
#define TRAIL_LENGTH 10

#endif
