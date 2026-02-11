#include "../state.h"
#include "util/calc.h"
#include "collision/collision.h"
#include "core/game_math.h"

// Forward declarations
static void superJump(Player* player);
static void superWallJump(Player* player, int dir);
static void wallJump(Player* player, int dir);

void dashBegin(Player* player) {
    // Celeste DashBegin (line 3442-3467)
    player->beforeDashSpeedX = player->vx;
    player->dashCooldownTimer = DASH_COOLDOWN_TIME;
    player->dashRefillCooldownTimer = DASH_REFILL_COOLDOWN_TIME;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->dashAttackTimer = DASH_ATTACK_TIME;

    // Zero speed initially (Celeste line 3462)
    // The actual dash speed is set after collision (matching yield return null)
    player->vx = 0;
    player->vy = 0;
    player->dashDirX = 0;
    player->dashDirY = 0;

    // Unduck if in air (Celeste line 3465-3466)
    if (!player->onGround && player->ducking) {
        player->ducking = 0;
    }

    // Initialize dash timer (0.15s = 9 frames)
    player->dashing = DASH_LENGTH;

    // Clear old trails
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        player->trailX[i] = -1000 << FIXED_SHIFT;
        player->trailY[i] = -1000 << FIXED_SHIFT;
    }
}

// Called after collision to check for dash slide (must run after collision updates onGround)
void dashSlideCheck(Player* player) {
    // Celeste DashCoroutine line 3594-3603 (after first yield)
    // Only check if dashing with diagonal down direction
    if (player->dashing > 0 && player->onGround && player->dashDirX != 0 && player->dashDirY > 0) {
        player->dashDirX = (player->dashDirX > 0) ? 1 : -1;
        player->dashDirY = 0;
        player->vy = 0;
        player->vx *= DODGE_SLIDE_SPEED_MULT;
        player->ducking = 1;
    }
}

// Called after collision to set dash velocity (emulates Celeste's yield return null)
void dashCoroutineResume(Player* player, u16 keys) {
    // This matches the Celeste DashCoroutine after "yield return null" (line 3548+)
    // Only run once: when dashDirX/Y are still zero (haven't set velocity yet)
    if (player->dashDirX != 0 || player->dashDirY != 0) {
        return;
    }

    // Speed was stored in dashBegin (before zeroing out velocity)

    // Get dash direction from current input (matches lastAim)
    int dashX = 0, dashY = 0;
    if (keys & KEY_LEFT) dashX = -1;
    if (keys & KEY_RIGHT) dashX = 1;
    if (keys & KEY_UP) dashY = -1;
    if (keys & KEY_DOWN) dashY = 1;

    // If no direction held, dash forward based on facing
    if (dashX == 0 && dashY == 0) {
        dashX = player->facingRight ? 1 : -1;
    }

    // Store dash direction
    player->dashDirX = dashX;
    player->dashDirY = dashY;

    // Calculate dash velocity (Celeste line 3556-3559)
    float newSpeedX = dashX * DASH_SPEED;
    float newSpeedY = dashY * DASH_SPEED;

    // Normalize diagonal dashes
    if (dashX != 0 && dashY != 0) {
        newSpeedX *= 0.707f;
        newSpeedY *= 0.707f;
    }

    // Preserve higher speed if moving faster in same direction (Celeste line 3557-3558)
    if ((player->beforeDashSpeedX > 0 && newSpeedX > 0) || (player->beforeDashSpeedX < 0 && newSpeedX < 0)) {
        if (ABS(player->beforeDashSpeedX) > ABS(newSpeedX)) {
            newSpeedX = player->beforeDashSpeedX;
        }
    }

    player->vx = newSpeedX;
    player->vy = newSpeedY;

    // Update facing (Celeste line 3567-3568)
    if (player->dashDirX != 0) {
        player->facingRight = player->dashDirX > 0 ? 1 : 0;
    }

    // Create first trail sprite (Celeste line 3588)
    player->trailX[0] = player->x;
    player->trailY[0] = player->y;
    player->trailFacing[0] = player->facingRight;
    player->trailTimer = 5;  // 0.08s for next trail
    player->trailFadeTimer = 0;
    player->trailIndex = 0;
}

void dashEnd(Player* player) {
    // Celeste DashEnd (line 3469-3472)
    // CallDashEvents() - not needed for GBA
}

int dashUpdate(Player* player, u16 keys, const Level* level) {
    // Detect button presses
    u16 pressed = keys & ~player->prevKeys;

    // Trail timer (Celeste line 3478-3484)
    if (player->trailTimer > 0) {
        player->trailTimer--;
        if (player->trailTimer == 0 && player->trailIndex < TRAIL_LENGTH - 1) {
            player->trailIndex++;
            player->trailX[player->trailIndex] = player->x;
            player->trailY[player->trailIndex] = player->y;
            player->trailFacing[player->trailIndex] = player->facingRight;
        }
    }

    // Super Jump (horizontal dash only) - Celeste line 3495-3508
    if (player->dashDirY == 0) {
        if ((pressed & KEY_A) && (player->onGround || player->coyoteTime > 0)) {
            superJump(player);
            return ST_NORMAL;
        }
    }

    // Super Wall Jump (upward dash only) - Celeste line 3510-3525
    if (player->dashDirX == 0 && player->dashDirY == -1) {
        if (pressed & KEY_A) {
            if (checkWall(player, level, 1)) {
                superWallJump(player, -1);
                return ST_NORMAL;
            } else if (checkWall(player, level, -1)) {
                superWallJump(player, 1);
                return ST_NORMAL;
            }
        }
    }
    // All other dash directions: wall jump only - Celeste line 3526-3541
    else {
        if (pressed & KEY_A) {
            if (checkWall(player, level, 1)) {
                wallJump(player, -1);
                return ST_NORMAL;
            } else if (checkWall(player, level, -1)) {
                wallJump(player, 1);
                return ST_NORMAL;
            }
        }
    }

    // Countdown dash timer
    if (player->dashing > 0) {
        player->dashing--;

        // Dash ended (Celeste DashCoroutine line 3619-3633)
        if (player->dashing == 0) {
            // Create final trail sprite
            if (player->trailIndex < TRAIL_LENGTH - 1) {
                player->trailIndex++;
                player->trailX[player->trailIndex] = player->x;
                player->trailY[player->trailIndex] = player->y;
                player->trailFacing[player->trailIndex] = player->facingRight;
            }

            // Set end dash speed (Celeste line 3625-3632)
            if (player->dashDirY <= 0) {
                player->vx = player->dashDirX * END_DASH_SPEED;
                player->vy = player->dashDirY * END_DASH_SPEED;

                if (player->vy < 0) {
                    player->vy *= 0.75f;  // EndDashUpMult
                }
            }

            // Return to normal state
            return ST_NORMAL;
        }
    }

    return ST_DASH;  // Stay in dash state
}

// Helper: Super Jump (Celeste SuperJump() around line 2200+)
static void superJump(Player* player) {
    int facingDir = player->facingRight ? 1 : -1;

    player->vx = facingDir * SUPER_JUMP_H;
    player->vy = SUPER_JUMP_SPEED;

    // Duck multipliers (Celeste line 2229+)
    if (player->ducking) {
        player->ducking = 0;
        player->vx *= DUCK_SUPER_JUMP_X_MULT;
        player->vy *= DUCK_SUPER_JUMP_Y_MULT;
    }

    player->varJumpSpeed = player->vy;
    player->varJumpTimer = VAR_JUMP_TIME;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->dashAttackTimer = 0;
    player->onGround = 0;
    player->coyoteTime = 0;
    player->jumpBuffer = 0;
    player->jumpHeld = 1;
}

// Helper: Super Wall Jump (Celeste SuperWallJump() around line 2250+)
static void superWallJump(Player* player, int dir) {
    player->ducking = 0;
    player->vx = dir * SUPER_WALL_JUMP_H;
    player->vy = SUPER_WALL_JUMP_SPEED;
    player->varJumpSpeed = SUPER_WALL_JUMP_SPEED;
    player->varJumpTimer = SUPER_WALL_JUMP_VAR_TIME;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->dashAttackTimer = 0;
    player->coyoteTime = 0;
    player->jumpBuffer = 0;
    player->jumpHeld = 1;
    player->facingRight = dir > 0 ? 1 : 0;
}

// Helper: Wall Jump (Celeste WallJump() around line 2100+)
static void wallJump(Player* player, int dir) {
    player->ducking = 0;
    player->vx = dir * WALL_JUMP_H_SPEED;
    player->vy = JUMP_STRENGTH;
    player->varJumpSpeed = JUMP_STRENGTH;
    player->varJumpTimer = VAR_JUMP_TIME;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->dashAttackTimer = 0;
    player->coyoteTime = 0;
    player->jumpBuffer = 0;
    player->jumpHeld = 1;
    player->facingRight = dir > 0 ? 1 : 0;
}
