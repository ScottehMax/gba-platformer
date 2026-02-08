#include "player.h"
#include "util/calc.h"

void initPlayer(Player* player, const Level* level) {
    player->x = level->playerSpawnX << FIXED_SHIFT;
    player->y = level->playerSpawnY << FIXED_SHIFT;
    player->vx = 0;
    player->vy = 0;
    player->maxFall = MAX_FALL_SPEED;
    player->onGround = 0;
    player->coyoteTime = 0;
    player->jumpBuffer = 0;
    player->jumpHeld = 0;
    player->varJumpSpeed = 0;
    player->varJumpTimer = 0;
    player->dashing = 0;
    player->dashes = 1;  // Start with 1 dash
    player->maxDashes = 1;
    player->dashCooldownTimer = 0;
    player->dashRefillCooldownTimer = 0;
    player->facingRight = 1;
    player->prevKeys = 0;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->wallSlideDir = 0;
    player->dashAttackTimer = 0;
    player->trailIndex = 0;
    player->trailTimer = 0;  // Counts down, when reaches 0 creates next trail sprite
    player->trailFadeTimer = TRAIL_LENGTH * 2;  // Start fully faded

    // Initialize trail positions off-screen
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        player->trailX[i] = -1000 << FIXED_SHIFT;
        player->trailY[i] = -1000 << FIXED_SHIFT;
        player->trailFacing[i] = player->facingRight;
    }
}

void updatePlayer(Player* player, u16 keys, const Level* level) {
    // Detect button presses (pressed this frame but not last frame)
    u16 pressed = keys & ~player->prevKeys;

    // Dash cooldowns
    if (player->dashCooldownTimer > 0) {
        player->dashCooldownTimer--;
    }

    if (player->dashRefillCooldownTimer > 0) {
        player->dashRefillCooldownTimer--;
    } else if (player->onGround && player->dashes < player->maxDashes) {
        // Refill dash when on ground and refill cooldown expired
        player->dashes = player->maxDashes;
    }

    // Decay jump buffer
    if (player->jumpBuffer > 0) {
        player->jumpBuffer--;
    }

    // Decay var jump timer
    if (player->varJumpTimer > 0) {
        player->varJumpTimer--;
    }

    // Decay dash attack timer
    if (player->dashAttackTimer > 0) {
        player->dashAttackTimer--;
    }

    // R button: Dash (8-directional or forward) - only on press, not hold
    if ((pressed & KEY_R) && player->dashCooldownTimer == 0 && player->dashing == 0 && player->dashes > 0) {
        // Consume dash
        player->dashes = max(0, player->dashes - 1);

        player->dashing = DASH_LENGTH + 1;  // +1 because we decrement on the same frame
        player->dashCooldownTimer = DASH_COOLDOWN_TIME;
        player->dashRefillCooldownTimer = DASH_REFILL_COOLDOWN_TIME;
        player->jumpHeld = 0;
        player->varJumpTimer = 0;  // CRITICAL FIX: Clear varJumpTimer to prevent jump from affecting post-dash velocity
        player->dashAttackTimer = DASH_ATTACK_TIME;  // Enable super jumps during and after dash

        // Clear all trail positions to prevent old trails from showing
        for (int i = 0; i < TRAIL_LENGTH; i++) {
            player->trailX[i] = -1000 << FIXED_SHIFT;
            player->trailY[i] = -1000 << FIXED_SHIFT;
        }

        // Create first trail sprite at dash start (Celeste line 3588)
        player->trailIndex = 0;
        player->trailX[0] = player->x;
        player->trailY[0] = player->y;
        player->trailFacing[0] = player->facingRight;

        // Set timer for next trail sprite (0.08s = ~5 frames at 60fps, Celeste line 3589)
        player->trailTimer = 5;
        player->trailFadeTimer = 0; // Reset fade timer for new dash

        // Check which directions are held
        int dashX = 0, dashY = 0;
        if (keys & KEY_LEFT) dashX = -1;
        if (keys & KEY_RIGHT) dashX = 1;
        if (keys & KEY_UP) dashY = -1;
        if (keys & KEY_DOWN) dashY = 1;

        // If no direction held, dash forward based on facing
        if (dashX == 0 && dashY == 0) {
            dashX = player->facingRight ? 1 : -1;
        }

        // Apply dash velocity (normalize diagonals to maintain speed)
        if (dashX != 0 && dashY != 0) {
            // Diagonal: multiply by ~0.707 (using 181/256 as approximation)
            player->vx = (dashX * DASH_SPEED * 0.707f);
            player->vy = (dashY * DASH_SPEED * 0.707f);
        } else {
            player->vx = dashX * DASH_SPEED;
            player->vy = dashY * DASH_SPEED;
        }
    }

    // Countdown dash timer
    if (player->dashing > 0) {
        player->dashing--;

        // Create trail sprite when timer expires (Celeste line 3479-3483)
        if (player->trailTimer > 0) {
            player->trailTimer--;
            if (player->trailTimer == 0) {
                // Create second trail sprite
                player->trailX[1] = player->x;
                player->trailY[1] = player->y;
                player->trailFacing[1] = player->facingRight;
            }
        }

        // Create final trail sprite when dash ends (Celeste line 3621)
        if (player->dashing == 0) {
            player->trailX[2] = player->x;
            player->trailY[2] = player->y;
            player->trailFacing[2] = player->facingRight;

            player->trailFadeTimer = 0;
            player->trailIndex = 2;  // Track which was last sprite created

            // Set end dash speed (like Celeste)
            float dashDirX = player->vx > 0 ? 1.0f : (player->vx < 0 ? -1.0f : 0.0f);
            float dashDirY = player->vy > 0 ? 1.0f : (player->vy < 0 ? -1.0f : 0.0f);
            player->vx = dashDirX * END_DASH_SPEED;
            player->vy = dashDirY * END_DASH_SPEED;

            // Upward dash multiplier
            if (player->vy < 0) {
                player->vy *= 0.75f;  // EndDashUpMult
            }
        }
    }

    // Update trail fade after dash ends (3 sprites * 2 frames each = 6 frames)
    if (player->dashing == 0 && player->trailFadeTimer < TRAIL_LENGTH * 2) {
        player->trailFadeTimer++;
    }

    // Horizontal movement (disabled during dash)
    if (player->dashing == 0) {
        // if (keys & KEY_LEFT) {
        //     player->vx -= ACCELERATION;
        //     if (player->vx < -MAX_SPEED) {
        //         player->vx = -MAX_SPEED;
        //     }
        //     player->facingRight = 0;  // Face left
        // } else if (keys & KEY_RIGHT) {
        //     player->vx += ACCELERATION;
        //     if (player->vx > MAX_SPEED) {
        //         player->vx = MAX_SPEED;
        //     }
        //     player->facingRight = 1;  // Face right
        // } else {
            // Apply friction when no input
            // float mult = onGround ? 1 : AirMult;
            // if (Math.Abs(Speed.X) > max && Math.Sign(Speed.X) == moveX)
            //     Speed.X = Calc.Approach(Speed.X, max * moveX, RunReduce * mult * Engine.DeltaTime);  //Reduce back from beyond the max speed
            // else
            //     Speed.X = Calc.Approach(Speed.X, max * moveX, RunAccel * mult * Engine.DeltaTime);   //Approach the max speed

        if (keys & KEY_LEFT) {
            player->facingRight = 0;  // Face left
        } else if (keys & KEY_RIGHT) {
            player->facingRight = 1;  // Face right
        }
        int moveX = keys & KEY_RIGHT ? 1 : (keys & KEY_LEFT ? -1 : 0);
        float mult = player->onGround ? 1 : AIR_MULT;
        // if (player->vx > 0) {
        //     player->vx = approach(player->vx, MAX_SPEED, ACCELERATION * mult / 60.0f);
        // } else if (player->vx < 0) {
        //     player->vx = approach(player->vx, -MAX_SPEED, ACCELERATION * mult / 60.0f);
        // }
        if (ABS(player->vx) > MAX_SPEED && ((moveX > 0 && player->vx > 0) || (moveX < 0 && player->vx < 0))) {
            player->vx = approach(player->vx, MAX_SPEED * moveX, RUN_REDUCE * mult / 60.0f);
        } else {
            player->vx = approach(player->vx, MAX_SPEED * moveX, ACCELERATION * mult / 60.0f);
        }
            // if (player->vx > 0) {
            //     player->vx -= friction;
            //     if (player->vx < 0) player->vx = 0;
            // } else if (player->vx < 0) {
            //     player->vx += friction;
            //     if (player->vx > 0) player->vx = 0;
            // }
        // }
    }

    // Decay wall slide timer when not wall sliding
    if (player->wallSlideDir != 0) {
        player->wallSlideTimer = max(player->wallSlideTimer - 1, 0);
        player->wallSlideDir = 0;
    }

    // A button: Jump - only on press, not hold
    if (pressed & KEY_A) {
        // Check for super jump (during/after dash)
        if (player->dashAttackTimer > 0) {
            // Super jump from ground (hyper dash)
            if (player->onGround || player->coyoteTime > 0) {
                int facingDir = player->facingRight ? 1 : -1;
                player->vx = facingDir * SUPER_JUMP_H;
                player->vy = SUPER_JUMP_SPEED;
                player->varJumpSpeed = SUPER_JUMP_SPEED;
                player->varJumpTimer = VAR_JUMP_TIME;
                player->wallSlideTimer = WALL_SLIDE_TIME;
                player->dashAttackTimer = 0;
                player->onGround = 0;
                player->coyoteTime = 0;
                player->jumpBuffer = 0;
                player->jumpHeld = 1;
                player->dashing = 0;  // Cancel dash
            }
            // Super wall jump from dash
            else if (checkWall(player, level, 1)) {
                player->vx = -1 * SUPER_WALL_JUMP_H;  // Wall on right, jump left
                player->vy = SUPER_WALL_JUMP_SPEED;
                player->varJumpSpeed = SUPER_WALL_JUMP_SPEED;
                player->varJumpTimer = SUPER_WALL_JUMP_VAR_TIME;
                player->wallSlideTimer = WALL_SLIDE_TIME;
                player->dashAttackTimer = 0;
                player->coyoteTime = 0;
                player->jumpBuffer = 0;
                player->jumpHeld = 1;
                player->facingRight = 1;  // Face away from wall
                player->dashing = 0;  // Cancel dash
            }
            else if (checkWall(player, level, -1)) {
                player->vx = 1 * SUPER_WALL_JUMP_H;  // Wall on left, jump right
                player->vy = SUPER_WALL_JUMP_SPEED;
                player->varJumpSpeed = SUPER_WALL_JUMP_SPEED;
                player->varJumpTimer = SUPER_WALL_JUMP_VAR_TIME;
                player->wallSlideTimer = WALL_SLIDE_TIME;
                player->dashAttackTimer = 0;
                player->coyoteTime = 0;
                player->jumpBuffer = 0;
                player->jumpHeld = 1;
                player->facingRight = 0;  // Face away from wall
                player->dashing = 0;  // Cancel dash
            }
        }
        // Regular wall jump check
        else {
            int wallJumpDir = 0;
            if (!player->onGround && player->coyoteTime == 0) {
                if (checkWall(player, level, 1)) {
                    wallJumpDir = -1;  // Wall on right, jump left
                } else if (checkWall(player, level, -1)) {
                    wallJumpDir = 1;   // Wall on left, jump right
                }
            }

            if (wallJumpDir != 0) {
                // Wall jump!
                player->vx = wallJumpDir * WALL_JUMP_H_SPEED;
                player->vy = JUMP_STRENGTH;
                player->varJumpSpeed = JUMP_STRENGTH;
                player->varJumpTimer = VAR_JUMP_TIME;
                player->wallSlideTimer = WALL_SLIDE_TIME;  // Reset wall slide timer
                player->coyoteTime = 0;
                player->jumpBuffer = 0;
                player->jumpHeld = 1;
                player->facingRight = wallJumpDir > 0 ? 1 : 0;
            } else if (player->onGround || player->coyoteTime > 0) {
                // Normal jump
                int moveX = keys & KEY_RIGHT ? 1 : (keys & KEY_LEFT ? -1 : 0);
                player->vx += moveX * JUMP_HORIZONTAL_BOOST;  // Add horizontal boost based on input direction
                // Execute jump immediately
                player->vy = JUMP_STRENGTH;
                player->varJumpSpeed = JUMP_STRENGTH;  // Store initial jump velocity
                player->varJumpTimer = VAR_JUMP_TIME;   // Start var jump window
                player->wallSlideTimer = WALL_SLIDE_TIME;  // Reset wall slide timer
                player->onGround = 0;
                player->coyoteTime = 0;  // Consume coyote time on jump
                player->jumpBuffer = 0;
                player->jumpHeld = 1;
            } else {
                // Buffer for later
                player->jumpBuffer = JUMP_BUFFER_TIME;
            }
        }
    }

    // Wall slide detection (before gravity application)
    float targetMaxFall = MAX_FALL_SPEED;
    if (!player->onGround && player->dashing == 0) {
        int moveX = keys & KEY_RIGHT ? 1 : (keys & KEY_LEFT ? -1 : 0);
        int facingDir = player->facingRight ? 1 : -1;

        // Wall slide conditions (from Celeste):
        // - Moving toward wall OR no input
        // - Falling (vy >= 0)
        // - wallSlideTimer > 0
        // - Wall exists in facing direction
        if (player->vy >= 0 && player->wallSlideTimer > 0 && checkWall(player, level, facingDir)) {
            if (moveX == facingDir || moveX == 0) {
                player->wallSlideDir = facingDir;
                // Lerp from WALL_SLIDE_START_MAX (20) to MAX_FALL_SPEED (160) over WALL_SLIDE_TIME
                float t = player->wallSlideTimer / (float)WALL_SLIDE_TIME;
                targetMaxFall = WALL_SLIDE_START_MAX + (MAX_FALL_SPEED - WALL_SLIDE_START_MAX) * (1.0f - t);
            }
        }

        // Fast fall: update maxFall based on down input
        if (player->wallSlideDir == 0) {
            // Use threshold to account for approach() not reaching exact values
            if ((keys & KEY_DOWN) && player->vy >= MAX_FALL_SPEED * 0.95f) {
                targetMaxFall = FAST_MAX_FALL_SPEED;
            }
        }

        player->maxFall = approach(player->maxFall, targetMaxFall, FAST_MAX_ACCEL / 60.0f);
    }

    // Apply gravity (but not during dash, to preserve dash trajectory)
    if (!player->onGround && player->dashing == 0) {
        int absVy = player->vy < 0 ? -player->vy : player->vy;
        float mult = (absVy < HALF_GRAV_THRESHOLD && (keys & KEY_A)) ? (GRAVITY / PEAK_GRAVITY_MULTIPLIER) : GRAVITY;

        player->vy = approach(player->vy, player->maxFall, mult / 60.0f);

        // Variable jump: clamp upward velocity during var jump time (like Celeste)
        // This prevents gravity from slowing down the jump for the first ~12 frames
        if (player->varJumpTimer > 0) {
            if (keys & KEY_A) {
                // Holding jump: maintain initial jump velocity (don't let gravity slow it)
                if (player->vy > player->varJumpSpeed) {
                    player->vy = player->varJumpSpeed;
                }
            } else {
                // Released jump: end var jump time
                player->varJumpTimer = 0;
            }
        }
    }

    // Variable jump height: cut upward velocity on release
    if (player->dashing == 0 && player->jumpHeld && !(keys & KEY_A) && player->vy < 0) {
        player->vy /= JUMP_RELEASE_MULTIPLIER;
        player->jumpHeld = 0;
    }

    // Swept collision - move along each axis and stop at first collision
    collideHorizontal(player, level);
    collideVertical(player, level);

    // Execute buffered jump on landing
    if (player->jumpBuffer > 0 && player->onGround) {
        player->vy = JUMP_STRENGTH;
        player->varJumpSpeed = JUMP_STRENGTH;
        player->varJumpTimer = VAR_JUMP_TIME;
        player->onGround = 0;
        player->coyoteTime = 0;
        player->jumpBuffer = 0;
        player->jumpHeld = 1;
    }

    // Update coyote time
    if (player->onGround) {
        player->coyoteTime = COYOTE_TIME;  // Reset when grounded
        player->jumpHeld = 0;
        player->wallSlideTimer = WALL_SLIDE_TIME;  // Reset wall slide timer on landing
    } else if (player->coyoteTime > 0) {
        player->coyoteTime--;  // Count down when airborne
    }

    // Update previous keys for next frame
    player->prevKeys = keys;
}
