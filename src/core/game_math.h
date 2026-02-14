#ifndef GAME_MATH_H
#define GAME_MATH_H

// Fixed-point math (8 bits fractional) for smooth sub-pixel movement
#define FIXED_SHIFT 8
#define FIXED_ONE (1 << FIXED_SHIFT)

#define TO_GBA(x) ((x) * 256.0f / 60.0f)

// Physics constants
#define GRAVITY TO_GBA(900.0f)        // 3840
#define JUMP_STRENGTH TO_GBA(-105.0f) // -448
#define MAX_SPEED TO_GBA(90.0f)       // 384
#define ACCELERATION TO_GBA(1000.0f)  // 4266.67
#define RUN_REDUCE TO_GBA(400.0f)     // 1706.67
#define AIR_MULT 0.65f
#define DASH_SPEED TO_GBA(240.0f)     // 1024
#define DASH_LENGTH 9 // Frames of dash duration (0.15s at 60fps)
#define END_DASH_SPEED TO_GBA(160.0f) // 682.67
#define DASH_COOLDOWN_TIME 12  // 0.2s at 60fps - cooldown before can dash again
#define DASH_REFILL_COOLDOWN_TIME 6  // 0.1s at 60fps - cooldown before dash refills on ground
#define COYOTE_TIME 6  // Frames of grace period after walking off edge, 0.1 seconds at 60fps
#define MAX_FALL_SPEED TO_GBA(160.0f)     // 682.67
#define FAST_MAX_FALL_SPEED TO_GBA(240.0f) // 1024
#define FAST_MAX_ACCEL TO_GBA(300.0f)     // 1280

// Jump feel
#define JUMP_HORIZONTAL_BOOST TO_GBA(40.0f) // 170.67
#define JUMP_BUFFER_TIME 5  // Frames to remember jump press
#define JUMP_RELEASE_MULTIPLIER 2  // Divide vy by this when jump released
#define VAR_JUMP_TIME 12  // Frames to maintain jump velocity (0.2s at 60fps)
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
#define DODGE_SLIDE_SPEED_MULT 1.2f  // Speed multiplier for diagonal down dash on ground
#define DUCK_SUPER_JUMP_X_MULT 1.25f  // Horizontal boost for ducking super jump
#define DUCK_SUPER_JUMP_Y_MULT 0.5f   // Vertical reduction for ducking super jump (makes it shorter)

// Ducking
#define DUCK_FRICTION TO_GBA(500.0f)  // 2133.33 - Friction when ducking on ground
#define DUCK_CORRECT_CHECK 4  // Pixels to check for duck slide
#define DUCK_CORRECT_SLIDE TO_GBA(50.0f)  // 213.33 - Speed when sliding to unduck


// Climbing
#define CLIMB_MAX_STAMINA 110.0f
#define CLIMB_UP_COST (100.0f / 2.2f)  // 45.45 per second
#define CLIMB_STILL_COST (100.0f / 10.0f)  // 10 per second
#define CLIMB_JUMP_COST (110.0f / 4.0f)  // 27.5 per jump
#define CLIMB_TIRED_THRESHOLD 20.0f
#define CLIMB_UP_SPEED TO_GBA(-45.0f)  // -192
#define CLIMB_DOWN_SPEED TO_GBA(80.0f)  // 341.33
#define CLIMB_SLIP_SPEED TO_GBA(30.0f)  // 128
#define CLIMB_ACCEL TO_GBA(900.0f)  // 3840
#define CLIMB_GRAB_Y_MULT 0.2f
#define CLIMB_NO_MOVE_TIME 6  // 0.1s at 60fps
#define CLIMB_CHECK_DIST 2  // Pixels to check for wall
#define CLIMB_UP_CHECK_DIST 2  // Pixels to check for climb up
#define CLIMB_HOP_Y TO_GBA(-120.0f)  // -512
#define CLIMB_HOP_X TO_GBA(100.0f)  // 426.67
#define CLIMB_HOP_FORCE_TIME 12  // 0.2s at 60fps - Force no horizontal input after climb hop (Celeste line 117)
#define CLIMB_JUMP_BOOST_TIME 12  // 0.2s at 60fps - Window to convert climb jump to wall jump

// Bounce/Spring mechanics (Celeste Player.cs line 38, 63-66, 1916-1917)
#define BOUNCE_SPEED TO_GBA(-140.0f)  // -597.33 - Normal spring vertical speed
#define SUPER_BOUNCE_SPEED TO_GBA(-185.0f)  // -789.33 - Super spring vertical speed
#define SIDE_BOUNCE_SPEED TO_GBA(240.0f)  // 1024 - Wall spring horizontal speed
#define BOUNCE_VAR_JUMP_TIME 12  // 0.2s at 60fps - Can control height after bounce
#define SUPER_BOUNCE_VAR_JUMP_TIME 12  // 0.2s at 60fps - Can control height after super bounce
#define BOUNCE_AUTO_JUMP_TIME 6  // 0.1s at 60fps - Maintains float after bounce
#define SIDE_BOUNCE_FORCE_MOVE_TIME 18  // 0.3s at 60fps - Force movement after side bounce

// Corner correction
#define CORNER_CORRECTION_DISTANCE (FIXED_ONE * 6)  // Max pixels nudge
#define DASH_LEDGE_POP_HEIGHT (FIXED_ONE * 6)  // Max pixels upward pop
#define BONK_NUDGE_RANGE (FIXED_ONE * 6)  // Max pixels for ceiling corner nudge

// Player constants (exactly matching Celeste's 8x11 hitbox)
#define PLAYER_WIDTH 8
#define PLAYER_HEIGHT 11
#define PLAYER_HITBOX_Y_SHIFT 3  // Higher value = player stands higher on ground (shifts hitbox down relative to center)
#define PLAYER_TOP(y) ((y) - PLAYER_HEIGHT / 2 - 1 + PLAYER_HITBOX_Y_SHIFT)
#define PLAYER_BOTTOM(y) ((y) + PLAYER_HEIGHT / 2 + PLAYER_HITBOX_Y_SHIFT)
#define PLAYER_RADIUS_X 4  // For compatibility
#define PLAYER_RADIUS_Y 5  // For compatibility
#define TRAIL_LENGTH 3  // Trail sprites per dash

#endif
