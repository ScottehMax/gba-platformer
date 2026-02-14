#include "player.h"
#include "state.h"
#include "util/calc.h"
#include "core/input.h"

void initPlayer(Player* player, const Level* level) {
    player->x = level->playerSpawnX << FIXED_SHIFT;
    player->y = level->playerSpawnY << FIXED_SHIFT;
    player->vx = 0;
    player->vy = 0;
    player->maxFall = MAX_FALL_SPEED;
    player->onGround = 0;
    player->wasOnGround = 0;
    player->coyoteTime = 0;
    player->jumpBuffer = 0;
    player->jumpHeld = 0;
    player->autoJump = 0;
    player->autoJumpTimer = 0;
    player->liftBoostX = 0;
    player->liftBoostY = 0;
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
    player->dashDirX = 0;
    player->dashDirY = 0;
    player->beforeDashSpeedX = 0;
    player->ducking = 0;
    player->trailIndex = 0;
    player->trailTimer = 0;  // Counts down, when reaches 0 creates next trail sprite
    player->trailFadeTimer = TRAIL_LENGTH * 2;  // Start fully faded

    // Initialize trail positions off-screen
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        player->trailX[i] = -1000 << FIXED_SHIFT;
        player->trailY[i] = -1000 << FIXED_SHIFT;
        player->trailFacing[i] = player->facingRight;
    }

    // Initialize climb state
    player->stamina = CLIMB_MAX_STAMINA;
    player->climbNoMoveTimer = 0;
    player->lastClimbMove = 0;
    player->wallBoostTimer = 0;
    player->wallBoostDir = 0;

    // Initialize climb hop system (Celeste line 218-219)
    player->hopWaitX = 0;
    player->hopWaitXSpeed = 0;
    player->forceMoveX = 0;
    player->forceMoveXTimer = 0;

    // Initialize state machine (Celeste line 322-332)
    initStateMachine(&player->stateMachine);
    setStateCallbacks(&player->stateMachine, ST_NORMAL, normalUpdate, normalBegin, normalEnd);
    setStateCallbacks(&player->stateMachine, ST_DASH, dashUpdate, dashBegin, dashEnd);
    setStateCallbacks(&player->stateMachine, ST_CLIMB, climbUpdate, climbBegin, climbEnd);
    // TODO: Add more states as needed

    // Set initial state to Normal
    setState(&player->stateMachine, ST_NORMAL, player, level);
}

void updatePlayer(Player* player, u16 keys, const Level* level) {
    // === PRE-STATE UPDATE LOGIC ===
    // Timers that tick down every frame regardless of state

    // AutoJump timer (Celeste line 747-757)
    // Dash sets AutoJump=true and AutoJumpTimer=0, which stays active until landing
    if (player->autoJumpTimer > 0) {
        if (player->autoJump) {
            player->autoJumpTimer--;
            if (player->autoJumpTimer <= 0) {
                player->autoJump = 0;
            }
        } else {
            player->autoJumpTimer = 0;
        }
    }

    // Dash cooldowns (Celeste ticks these in engine update)
    if (player->dashCooldownTimer > 0) {
        player->dashCooldownTimer--;
    }
    if (player->dashRefillCooldownTimer > 0) {
        player->dashRefillCooldownTimer--;
    }

    // Jump buffer decay
    if (player->jumpBuffer > 0) {
        player->jumpBuffer--;
    }

    // Var jump timer decay
    if (player->varJumpTimer > 0) {
        player->varJumpTimer--;
    }

    // Dash attack timer decay
    if (player->dashAttackTimer > 0) {
        player->dashAttackTimer--;
    }

    // Wall slide timer decay when not actively wall sliding
    if (player->wallSlideDir == 0 && player->wallSlideTimer > 0) {
        player->wallSlideTimer--;
    }
    player->wallSlideDir = 0;  // Reset each frame, state will set if wall sliding

    // Trail fade timer (for dash trails after dash ends)
    if (player->dashing == 0 && player->trailFadeTimer < TRAIL_LENGTH * 8) {
        player->trailFadeTimer++;
    }

    // Climb no move timer
    if (player->climbNoMoveTimer > 0) {
        player->climbNoMoveTimer--;
    }

    // Wall boost timer
    if (player->wallBoostTimer > 0) {
        player->wallBoostTimer--;
        // Wall Boost (Celeste line 689-698)
        // After climb jump with no horizontal input, pressing away from wall converts to wall jump
        int moveX = keys & BTN_RIGHT ? 1 : (keys & BTN_LEFT ? -1 : 0);
        if (moveX == player->wallBoostDir) {
            // Convert climb jump to wall jump and refund stamina
            player->vx = player->wallBoostDir * WALL_JUMP_H_SPEED;
            player->stamina += CLIMB_JUMP_COST;
            player->wallBoostTimer = 0;
        }
    }

    // Force move X timer (Celeste line 762)
    if (player->forceMoveXTimer > 0) {
        player->forceMoveXTimer--;
    }

    // Update facing based on input (Celeste line 786-794)
    // This allows reverse hypers: dash one direction, hold opposite direction, jump
    // NOTE: Does NOT update during climb, pickup, or similar states
    int moveX = keys & BTN_RIGHT ? 1 : (keys & BTN_LEFT ? -1 : 0);
    if (moveX != 0 && player->stateMachine.state != ST_CLIMB) {
        player->facingRight = moveX > 0 ? 1 : 0;
    }

    // === STATE MACHINE UPDATE ===
    // This is where the state-specific logic runs (Celeste line 632: StateMachine.Update())
    updateStateMachine(&player->stateMachine, player, keys, level);

    // === POST-STATE UPDATE LOGIC ===
    // Physics and collision that happens after state logic

    // Apply collision (Celeste does this in Actor.Update via MoveH/MoveV)
    collideHorizontal(player, level);
    collideVertical(player, level);

    // Hop Wait X - delays horizontal movement after climb hop until above ledge (Celeste line 814-823)
    if (player->hopWaitX != 0) {
        // If moving away from wall OR moving down, cancel hopWaitX (Celeste line 816-817)
        int vxSign = player->vx > 0 ? 1 : (player->vx < 0 ? -1 : 0);
        if (vxSign == -player->hopWaitX || player->vy > 0) {
            player->hopWaitX = 0;
        } else {
            // Check if there's no solid at Position + UnitX * hopWaitX (Celeste line 818)
            // Check at center so full hitbox clears before applying velocity
            if (!checkWallAt(player, level, player->hopWaitX, 0, 1)) {
                // No wall ahead - apply the horizontal speed (Celeste line 820-821)
                player->vx = player->hopWaitXSpeed;
                player->hopWaitX = 0;
            }
        }
    }

    // LiftBoost application on walk-off (Celeste line 2787-2788)
    // When walking off a platform with upward velocity from moving platform
    if (player->liftBoostY < 0 && player->wasOnGround && !player->onGround && player->vy >= 0) {
        player->vy = player->liftBoostY;
    }

    // Dash coroutine resume (emulates Celeste's yield return null)
    // This sets the dash velocity AFTER collision has updated onGround
    if (player->stateMachine.state == ST_DASH) {
        dashCoroutineResume(player, keys);
    }

    // Variable jump height: cut upward velocity on release (Celeste line 1090)
    // This is shared across states
    if (player->stateMachine.state != ST_DASH && player->jumpHeld && !(keys & BTN_JUMP) && player->vy < 0) {
        player->vy /= JUMP_RELEASE_MULTIPLIER;
        player->jumpHeld = 0;
    }

    // Execute buffered jump on landing (Celeste uses Input.Jump.Pressed logic)
    if (player->jumpBuffer > 0 && player->onGround) {
        player->vy = JUMP_STRENGTH + player->liftBoostY;  // Apply lift boost
        player->vx += player->liftBoostX;  // Apply horizontal lift boost
        player->varJumpSpeed = JUMP_STRENGTH;
        player->varJumpTimer = VAR_JUMP_TIME;
        player->onGround = 0;
        player->coyoteTime = 0;
        player->jumpBuffer = 0;
        player->jumpHeld = 1;
        player->autoJump = 0;  // Clear AutoJump when manually jumping
    }

    // After Dash / On Ground (Celeste line 702-707, 714-720)
    // Clear AutoJump when landing (not during climb)
    if (player->onGround && player->stateMachine.state != ST_CLIMB) {
        player->autoJump = 0;
        player->wallSlideTimer = WALL_SLIDE_TIME;  // Reset wall slide timer on landing
        player->stamina = CLIMB_MAX_STAMINA;  // Refill stamina on ground (Celeste line 3107-3108)
    }

    // Coyote time / Jump Grace (Celeste line 714-720)
    if (player->onGround) {
        player->coyoteTime = COYOTE_TIME;
        player->jumpHeld = 0;
    } else if (player->coyoteTime > 0) {
        player->coyoteTime--;
    }

    // Dash slide check (Celeste DashCoroutine after collision)
    if (player->stateMachine.state == ST_DASH) {
        dashSlideCheck(player);
    }

    // Refill dash when on ground (Celeste line 1047-1050)
    if (player->dashRefillCooldownTimer == 0 && player->onGround && player->dashes < player->maxDashes) {
        player->dashes = player->maxDashes;
    }

    // Consume dash when dash is triggered (handled by Normal state returning ST_DASH)
    // The actual dash consumption happens in dashBegin()

    // Update previous keys for next frame
    player->prevKeys = keys;

    // Track wasOnGround for next frame's walk-off detection (Celeste line 1082)
    // MUST be at the end after all processing, so it captures this frame's final onGround state
    player->wasOnGround = player->onGround;
}

