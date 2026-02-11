#include "player.h"
#include "state.h"
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

    // === STATE MACHINE UPDATE ===
    // This is where the state-specific logic runs (Celeste line 632: StateMachine.Update())
    updateStateMachine(&player->stateMachine, player, keys, level);

    // === POST-STATE UPDATE LOGIC ===
    // Physics and collision that happens after state logic

    // Apply collision (Celeste does this in Actor.Update via MoveH/MoveV)
    collideHorizontal(player, level);
    collideVertical(player, level);

    // Dash coroutine resume (emulates Celeste's yield return null)
    // This sets the dash velocity AFTER collision has updated onGround
    if (player->stateMachine.state == ST_DASH) {
        dashCoroutineResume(player, keys);
    }

    // Variable jump height: cut upward velocity on release (Celeste line 1090)
    // This is shared across states
    if (player->stateMachine.state != ST_DASH && player->jumpHeld && !(keys & KEY_A) && player->vy < 0) {
        player->vy /= JUMP_RELEASE_MULTIPLIER;
        player->jumpHeld = 0;
    }

    // Execute buffered jump on landing (Celeste AutoJump system)
    if (player->jumpBuffer > 0 && player->onGround) {
        player->vy = JUMP_STRENGTH;
        player->varJumpSpeed = JUMP_STRENGTH;
        player->varJumpTimer = VAR_JUMP_TIME;
        player->onGround = 0;
        player->coyoteTime = 0;
        player->jumpBuffer = 0;
        player->jumpHeld = 1;
    }

    // Update coyote time (Celeste calls this jumpGraceTimer)
    if (player->onGround) {
        player->coyoteTime = COYOTE_TIME;  // Reset when grounded
        player->jumpHeld = 0;
        player->wallSlideTimer = WALL_SLIDE_TIME;  // Reset wall slide timer on landing
        player->stamina = CLIMB_MAX_STAMINA;  // Refill stamina on ground (Celeste line 3107-3108)
    } else if (player->coyoteTime > 0) {
        player->coyoteTime--;  // Count down when airborne
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
}

