#include "../state.h"
#include "util/calc.h"
#include "collision/collision.h"
#include "core/game_math.h"
#include "core/input.h"


void normalBegin(Player* player, const Level* level) {
    // Reset max fall to normal (Celeste line 2762)
    (void)level;  // Unused in normal state
    player->maxFall = MAX_FALL_SPEED;
}

void normalEnd(Player* player) {
    // Clear climb hop state (Celeste line 2765-2769)
    player->hopWaitX = 0;
}

int normalUpdate(Player* player, u16 keys, const Level* level) {
    // Detect button presses (pressed this frame but not last frame)
    u16 pressed = keys & ~player->prevKeys;

    // moveX for input direction (Celeste uses Input.MoveX)
    int moveX = keys & BTN_RIGHT ? 1 : (keys & BTN_LEFT ? -1 : 0);

    // Force Move X - overrides input after climb hop (Celeste line 760-764)
    if (player->forceMoveXTimer > 0) {
        moveX = player->forceMoveX;
    }

    // Update facing direction based on movement (Celeste updates Facing in Sprite logic)
    if (moveX != 0) {
        player->facingRight = moveX > 0 ? 1 : 0;
    }

    // Dashing (Celeste line 2824-2828)
    // Check if can dash and dash button pressed
    if ((pressed & BTN_DASH) && player->dashCooldownTimer == 0 && player->dashes > 0) {
        // Consume dash (Celeste StartDash() line 3408)
        player->dashes = player->dashes > 0 ? player->dashes - 1 : 0;
        // Return to dash state (will trigger dashBegin via state machine)
        return ST_DASH;
    }

    // Ducking (Celeste line 2831-2862)
    if (player->ducking) {
        if (player->onGround && !(keys & BTN_DOWN)) {
            // Try to unduck (Celeste line 2835-2855)
            if (!checkCeiling(player, level)) {
                player->ducking = 0;
            } else if (player->vx == 0) {
                // Duck correct slide: try to slide sideways to find unblock position
                // Celeste line 2842-2854
                for (int i = DUCK_CORRECT_CHECK; i > 0; i--) {
                    int origX = player->x;
                    // Try right
                    player->x = player->x + (i << FIXED_SHIFT);
                    if (!checkCeiling(player, level)) {
                        player->x = origX;
                        player->vx = DUCK_CORRECT_SLIDE;
                        break;
                    }
                    // Try left
                    player->x = origX - (i << FIXED_SHIFT);
                    if (!checkCeiling(player, level)) {
                        player->x = origX;
                        player->vx = -DUCK_CORRECT_SLIDE;
                        break;
                    }
                    player->x = origX;  // Restore position
                }
            }
        }
    } else if (player->onGround && (keys & BTN_DOWN) && player->vy >= 0) {
        player->ducking = 1;
    }

    // Running and Friction (Celeste line 2879-2895)
    if (player->ducking && player->onGround) {
        // Duck friction (Celeste line 2881)
        player->vx = approachInt(player->vx, 0, DUCK_FRICTION_PF);
    } else {
        int mult = player->onGround ? FIXED_ONE : FP_AIR_MULT;

        // Reduce speed if moving faster than max in same direction
        if (ABS(player->vx) > MAX_SPEED && ((moveX > 0 && player->vx > 0) || (moveX < 0 && player->vx < 0))) {
            player->vx = approachInt(player->vx, MAX_SPEED * moveX, fpMul(RUN_REDUCE_PF, mult));
        } else {
            player->vx = approachInt(player->vx, MAX_SPEED * moveX, fpMul(ACCELERATION_PF, mult));
        }
    }

    // Vertical physics (Celeste line 2897-2958)
    if (!player->onGround) {
        int max = MAX_FALL_SPEED;

        // Wall Slide (Celeste line 2932-2950)
        int facingDir = player->facingRight ? 1 : -1;
        if ((moveX == facingDir || (moveX == 0 && (keys & BTN_GRAB))) && !(keys & BTN_DOWN)) {
            if (player->vy >= 0 && player->wallSlideTimer > 0 && checkWall(player, level, facingDir)) {
                player->ducking = 0;
                player->wallSlideDir = facingDir;
            }

            if (player->wallSlideDir != 0) {
                // Lerp from WallSlideStartMax to MaxFall over WallSlideTime (Celeste line 2946)
                int lerpFactor = ((WALL_SLIDE_TIME - player->wallSlideTimer) * FIXED_ONE) / WALL_SLIDE_TIME;
                max = WALL_SLIDE_START_MAX + fpMul(MAX_FALL_SPEED - WALL_SLIDE_START_MAX, lerpFactor);
            }
        }

        // Fast fall (Celeste line 2910-2924)
        if (player->wallSlideDir == 0) {
            if ((keys & BTN_DOWN) && player->vy >= fpMul(MAX_FALL_SPEED, FP_FAST_FALL_THRESHOLD)) {
                max = FAST_MAX_FALL_SPEED;
            }
        }

        // Approach max fall speed
        player->maxFall = approachInt(player->maxFall, max, FAST_MAX_ACCEL_PF);

        // Gravity (Celeste line 2952-2957)
        // AutoJump simulates holding jump button for reduced gravity
        int absVy = player->vy < 0 ? -player->vy : player->vy;
        int gravMult = (absVy < HALF_GRAV_THRESHOLD && ((keys & BTN_JUMP) || player->autoJump)) ? FIXED_ONE / PEAK_GRAVITY_MULTIPLIER : FIXED_ONE;
        player->vy = approachInt(player->vy, player->maxFall, fpMul(GRAVITY_PF, gravMult));
    }

    // Variable Jumping (Celeste line 2960-2967)
    // AutoJump simulates holding jump button for maintained jump height
    if (player->varJumpTimer > 0) {
        if ((keys & BTN_JUMP) || player->autoJump) {
            player->vy = player->vy < player->varJumpSpeed ? player->vy : player->varJumpSpeed;
        } else {
            player->varJumpTimer = 0;
        }
    }

    // Climbing (Celeste line 2800-2819)
    // CheckStamina logic (Celeste line 3035-3042): account for wallBoostTimer
    int checkStamina = player->stamina;
    if (player->wallBoostTimer > 0) {
        checkStamina += CLIMB_JUMP_COST;
    }

    // Climb grab
    if ((keys & BTN_GRAB) && checkStamina > CLIMB_TIRED_THRESHOLD && !player->ducking) {
        int facingDir = player->facingRight ? 1 : -1;

        // Allow grab from ground or air when not moving away
        int speedDir = player->vx > 0 ? 1 : (player->vx < 0 ? -1 : 0);
        if (player->vy >= 0 && speedDir != -facingDir) {
            if (checkWallAt(player, level, facingDir, 0, CLIMB_CHECK_DIST)) {
                player->ducking = 0;
                return ST_CLIMB;
            }

            if (!(keys & BTN_DOWN)) {
                for (int i = 1; i <= CLIMB_UP_CHECK_DIST; i++) {
                    if (!isPositionCollidingAt(level, player->x >> FIXED_SHIFT, (player->y >> FIXED_SHIFT) - i) &&
                        checkWallAt(player, level, facingDir, -i, CLIMB_CHECK_DIST)) {
                        player->y -= i << FIXED_SHIFT;
                        player->ducking = 0;
                        return ST_CLIMB;
                    }
                }
            }
        }
    }

    // Jumping (Celeste line 2969-3004)
    if (pressed & BTN_JUMP) {
        if (player->coyoteTime > 0) {
            // Normal jump
            jump(player, keys);
        } else {
            // Wall jump checks (Celeste line 2977-2997)
            // Note: Super wall jumps during dash attack are handled in dash state
            if (checkWall(player, level, 1)) {
                // Wall on right
                wallJump(player, -1, moveX);
            } else if (checkWall(player, level, -1)) {
                // Wall on left
                wallJump(player, 1, moveX);
            } else {
                // Buffer jump for later (not in shown Celeste code, but in Jump() method)
                player->jumpBuffer = JUMP_BUFFER_TIME;
            }
        }
    }

    // Execute buffered jump on landing (handled in player.c update before state update)
    // Celeste handles this differently with AutoJump system

    return ST_NORMAL;  // Stay in normal state
}

// Helper function: Normal Jump (Celeste Jump() method around line 1900+)
void jump(Player* player, u16 keys) {
    int moveX = keys & BTN_RIGHT ? 1 : (keys & BTN_LEFT ? -1 : 0);

    player->ducking = 0;
    player->vx += moveX * JUMP_HORIZONTAL_BOOST;
    player->vy = JUMP_STRENGTH;

    // Apply lift boost from moving platforms (Celeste line 1672)
    player->vx += player->liftBoostX;
    player->vy += player->liftBoostY;

    player->varJumpSpeed = player->vy;
    player->varJumpTimer = VAR_JUMP_TIME;
    player->autoJump = 0;  // Clear AutoJump (Celeste line 1665)
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->dashAttackTimer = 0;  // Clear dash attack window
    player->dashing = 0;  // End dash so trail can fade
    player->onGround = 0;
    player->coyoteTime = 0;
    player->jumpBuffer = 0;
    player->jumpHeld = 1;
}

// Helper function: Wall Jump (Celeste WallJump() method line 1736-1782)
void wallJump(Player* player, int dir, int moveX) {
    player->ducking = 0;

    // Force movement away from wall if holding any direction (Celeste line 1746-1750)
    if (moveX != 0) {
        player->forceMoveX = dir;
        player->forceMoveXTimer = WALL_JUMP_FORCE_TIME;
    }

    player->vx = dir * WALL_JUMP_H_SPEED;
    player->vy = JUMP_STRENGTH;

    // Apply lift boost from moving platforms (Celeste line 1762)
    player->vx += player->liftBoostX;
    player->vy += player->liftBoostY;

    player->varJumpSpeed = JUMP_STRENGTH;
    player->varJumpTimer = VAR_JUMP_TIME;
    player->autoJump = 0;  // Clear AutoJump (Celeste line 1742)
    player->dashAttackTimer = 0;  // Clear dash attack window (Celeste line 1743)
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->wallBoostTimer = 0;  // Clear wall boost (Celeste line 1745)
    player->dashing = 0;  // End dash so trail can fade
    player->coyoteTime = 0;
    player->jumpBuffer = 0;
    player->jumpHeld = 1;
    player->facingRight = dir > 0 ? 1 : 0;
}

