#ifndef GAME_MATH_H
#define GAME_MATH_H

// Fixed-point math (8 bits fractional) for smooth sub-pixel movement
#define FIXED_SHIFT 8
#define FIXED_ONE (1 << FIXED_SHIFT)

// Convert Celeste velocity (pixels/second) to GBA fixed-point (units per frame)
// Formula: value * 256 / 60 (pure integer arithmetic)
#define TO_GBA(x) ((x) * 256 / 60)

// Fixed-point multipliers (FIXED_ONE = 256 = 1.0)
#define FP_AIR_MULT 166           // 0.65f * 256
#define FP_DODGE_SLIDE_MULT 307   // 1.2f * 256
#define FP_DUCK_JUMP_X_MULT 320   // 1.25f * 256
#define FP_DUCK_JUMP_Y_MULT 128   // 0.5f * 256
#define FP_CLIMB_GRAB_Y_MULT 51   // 0.2f * 256
#define FP_DIAG_NORMALIZE 181     // 0.707f * 256 (sqrt(2)/2)
#define FP_END_DASH_UP_MULT 192   // 0.75f * 256
#define FP_FAST_FALL_THRESHOLD 243 // 0.95f * 256

// Physics constants (integer arithmetic)
#define GRAVITY TO_GBA(900)        // 3840
#define JUMP_STRENGTH TO_GBA(-105) // -448
#define MAX_SPEED TO_GBA(90)       // 384
#define ACCELERATION TO_GBA(1000)  // 4266
#define RUN_REDUCE TO_GBA(400)     // 1706
#define DASH_SPEED TO_GBA(240)     // 1024
#define DASH_LENGTH 9 // Frames of dash duration (0.15s at 60fps)
#define END_DASH_SPEED TO_GBA(160) // 682
#define DASH_COOLDOWN_TIME 12  // 0.2s at 60fps - cooldown before can dash again
#define DASH_REFILL_COOLDOWN_TIME 6  // 0.1s at 60fps - cooldown before dash refills on ground
#define COYOTE_TIME 6  // Frames of grace period after walking off edge, 0.1 seconds at 60fps
#define MAX_FALL_SPEED TO_GBA(160)     // 682
#define FAST_MAX_FALL_SPEED TO_GBA(240) // 1024
#define FAST_MAX_ACCEL TO_GBA(300)     // 1280

// Jump feel
#define JUMP_HORIZONTAL_BOOST TO_GBA(40) // 170
#define JUMP_BUFFER_TIME 5  // Frames to remember jump press
#define JUMP_RELEASE_MULTIPLIER 2  // Divide vy by this when jump released
#define VAR_JUMP_TIME 12  // Frames to maintain jump velocity (0.2s at 60fps)
#define PEAK_GRAVITY_MULTIPLIER 2  // Divide gravity by this at peak
#define HALF_GRAV_THRESHOLD TO_GBA(40) // 170 - When abs(vy) is below this, apply reduced gravity for floaty feel

// Wall slide/jump
#define WALL_SLIDE_START_MAX TO_GBA(20)  // 85 - Very slow initial wall slide speed
#define WALL_SLIDE_TIME 72  // Frames before wall slide reaches normal fall speed (1.2s at 60fps)
#define WALL_JUMP_H_SPEED TO_GBA(130)  // 554 - Horizontal speed for wall jump (MaxRun + JumpHBoost)
#define WALL_JUMP_CHECK_DIST 3  // Pixels to check for wall
#define WALL_JUMP_FORCE_TIME 10  // 0.16s at 60fps - Force movement away from wall (Celeste line 57)

// Super jump (hyper dash) mechanics
#define SUPER_JUMP_H TO_GBA(260)  // 1109 - Horizontal speed for super jump from dash
#define SUPER_JUMP_SPEED JUMP_STRENGTH  // Same vertical as normal jump
#define SUPER_WALL_JUMP_H TO_GBA(170)  // 725 - MaxRun + JumpHBoost * 2
#define SUPER_WALL_JUMP_SPEED TO_GBA(-160)  // -682 - Stronger than normal jump
#define SUPER_WALL_JUMP_VAR_TIME 15  // 0.25s at 60fps - Longer var jump time
#define DASH_ATTACK_TIME 18  // 0.3s at 60fps - Window for super jumps after dash

// Ducking
#define DUCK_FRICTION TO_GBA(500)  // 2133 - Friction when ducking on ground
#define DUCK_CORRECT_CHECK 4  // Pixels to check for duck slide
#define DUCK_CORRECT_SLIDE TO_GBA(50)  // 213 - Speed when sliding to unduck

// Per-frame physics rates (pre-divided by 60 for performance)
#define DUCK_FRICTION_PF (DUCK_FRICTION / 60)      // ~35 per frame
#define RUN_REDUCE_PF (RUN_REDUCE / 60)            // ~28 per frame
#define ACCELERATION_PF (ACCELERATION / 60)         // ~71 per frame
#define FAST_MAX_ACCEL_PF (FAST_MAX_ACCEL / 60)    // ~21 per frame
#define GRAVITY_PF (GRAVITY / 60)                   // ~64 per frame
#define CLIMB_ACCEL_PF (CLIMB_ACCEL / 60)          // ~64 per frame

// Stamina costs per frame (pre-computed as fixed-point integers)
#define CLIMB_UP_COST_PF 194     // 100/2.2/60*256 = 193.94
#define CLIMB_STILL_COST_PF 43   // 100/10/60*256 = 42.67


// Climbing
#define CLIMB_MAX_STAMINA (110 * FIXED_ONE)  // 28160 in fixed-point
#define CLIMB_UP_COST 45  // 45.45 per second (approximated to 45)
#define CLIMB_STILL_COST 10  // 10 per second
#define CLIMB_JUMP_COST ((110 * FIXED_ONE) / 4)  // 7040 = 27.5 in fixed-point
#define CLIMB_TIRED_THRESHOLD (20 * FIXED_ONE)  // 5120 in fixed-point
#define CLIMB_UP_SPEED TO_GBA(-45)  // -192
#define CLIMB_DOWN_SPEED TO_GBA(80)  // 341
#define CLIMB_SLIP_SPEED TO_GBA(30)  // 128
#define CLIMB_ACCEL TO_GBA(900)  // 3840
#define CLIMB_NO_MOVE_TIME 6  // 0.1s at 60fps
#define CLIMB_CHECK_DIST 2  // Pixels to check for wall
#define CLIMB_UP_CHECK_DIST 2  // Pixels to check for climb up
#define CLIMB_HOP_Y TO_GBA(-120)  // -512
#define CLIMB_HOP_X TO_GBA(100)  // 426
#define CLIMB_HOP_FORCE_TIME 12  // 0.2s at 60fps - Force no horizontal input after climb hop (Celeste line 117)
#define CLIMB_JUMP_BOOST_TIME 12  // 0.2s at 60fps - Window to convert climb jump to wall jump

// Bounce/Spring mechanics (Celeste Player.cs line 38, 63-66, 1916-1917)
#define BOUNCE_SPEED TO_GBA(-140)  // -597 - Normal spring vertical speed
#define SUPER_BOUNCE_SPEED TO_GBA(-185)  // -789 - Super spring vertical speed
#define SIDE_BOUNCE_SPEED TO_GBA(240)  // 1024 - Wall spring horizontal speed
#define BOUNCE_VAR_JUMP_TIME 12  // 0.2s at 60fps - Can control height after bounce
#define SUPER_BOUNCE_VAR_JUMP_TIME 12  // 0.2s at 60fps - Can control height after super bounce
#define BOUNCE_AUTO_JUMP_TIME 6  // 0.1s at 60fps - Maintains float after bounce
#define SIDE_BOUNCE_FORCE_MOVE_TIME 18  // 0.3s at 60fps - Force movement after side bounce

// HitSquash state (Celeste Player.cs line 3933-3934)
#define HIT_SQUASH_NO_MOVE_TIME 6  // 0.1s at 60fps - No control briefly after hitting wall
#define HIT_SQUASH_FRICTION TO_GBA(800)  // 3413 - Friction to slow down
#define HIT_SQUASH_FRICTION_PF (HIT_SQUASH_FRICTION / 60)  // ~57 per frame

// Boost state (Celeste Player.cs line 86, 3788-3828)
#define BOOST_TIME 15  // 0.25s at 60fps - Duration before auto-dash
#define BOOST_MOVE_SPEED TO_GBA(80)  // 341 - Speed moving to bubble center
#define BOOST_MOVE_SPEED_PF (BOOST_MOVE_SPEED / 60)  // ~5 per frame

// Corner correction
#define CORNER_CORRECTION_DISTANCE (FIXED_ONE * 6)  // Max pixels nudge
#define DASH_LEDGE_POP_HEIGHT (FIXED_ONE * 6)  // Max pixels upward pop
#define BONK_NUDGE_RANGE (FIXED_ONE * 6)  // Max pixels for ceiling corner nudge
#define DASH_H_JUMPTHRU_NUDGE 6  // Max pixels to nudge player onto jump-thru during horizontal dash
#define DASH_V_FLOOR_SNAP_DIST 3  // Max pixels to snap player down to floor during horizontal dash-attack (Celeste DashVFloorSnapDist)

// Dash trail
#define DASH_TRAIL_INTERVAL 5  // Frames between trail sprites (0.08s at 60fps)

// Boost state
#define BOOST_AIM_OFFSET 3  // Pixels of aim adjustment in boost state (Celeste targetAdd)

// Climbing
#define CLIMB_SLIP_CHECK_PX 4  // Pixels from top to check for ledge slip (Celeste SlipCheck)

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
