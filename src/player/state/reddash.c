#include "../state.h"
#include "../player.h"
#include "util/calc.h"
#include "collision/collision.h"
#include "core/game_math.h"
#include "core/input.h"

// Forward declarations
static void superJump(Player* player);
static void superWallJump(Player* player, int dir);

void redDashBegin(Player* player, const Level* level) {
    // Celeste RedDashBegin (line 3834-3854)
    (void)level;  // Unused in RedDash state

    player->dashCooldownTimer = DASH_COOLDOWN_TIME;
    player->dashRefillCooldownTimer = DASH_REFILL_COOLDOWN_TIME;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->dashAttackTimer = DASH_ATTACK_TIME;

    // Set speed and direction immediately using lastAim (Celeste line 3922-3923)
    // Unlike normal dash, RedDash doesn't wait a frame
    player->vx = fpMul(player->lastAimX, DASH_SPEED << FIXED_SHIFT) >> FIXED_SHIFT;
    player->vy = fpMul(player->lastAimY, DASH_SPEED << FIXED_SHIFT) >> FIXED_SHIFT;
    player->dashDirX = player->lastAimX > 0 ? 1 : (player->lastAimX < 0 ? -1 : 0);
    player->dashDirY = player->lastAimY > 0 ? 1 : (player->lastAimY < 0 ? -1 : 0);

    // Update facing (Celeste line 3567-3568)
    if (player->dashDirX != 0) {
        player->facingRight = player->dashDirX > 0 ? 1 : 0;
    }

    // Unduck if in air (Celeste line 3852-3853)
    if (!player->onGround && player->ducking) {
        player->ducking = 0;
    }

    // RedDash doesn't use dashing timer - it's infinite until interrupted
    player->dashing = 0;

    clearPlayerDashTrail(player);

    // Create first trail sprite
    player->trailX[0] = player->x;
    player->trailY[0] = player->y;
    player->trailFacing[0] = player->facingRight;
    player->trailTimer = 5;  // 0.08s for next trail
    player->trailFadeTimer = 0;
    player->trailIndex = 0;
}

void redDashEnd(Player* player) {
    // Celeste RedDashEnd (line 3856-3859)
    // CallDashEvents() - not needed for GBA
    (void)player;
}

int redDashUpdate(Player* player, u16 keys, const Level* level) {
    // Celeste RedDashUpdate (line 3861-3916)

    // Detect button presses
    u16 pressed = keys & ~player->prevKeys;
    int moveX = keys & BTN_RIGHT ? 1 : (keys & BTN_LEFT ? -1 : 0);

    // Trail timer (same as regular dash)
    if (player->trailTimer > 0) {
        player->trailTimer--;
        if (player->trailTimer == 0 && player->trailIndex < TRAIL_LENGTH - 1) {
            player->trailIndex++;
            player->trailX[player->trailIndex] = player->x;
            player->trailY[player->trailIndex] = player->y;
            player->trailFacing[player->trailIndex] = player->facingRight;
        }
    }

    // Check for new dash (Celeste line 3865-3866)
    // CanDash = has dashes available and cooldown expired
    if ((pressed & BTN_DASH) && player->dashCooldownTimer == 0 && player->dashes > 0) {
        // Consume dash (Celeste StartDash() line 3408)
        player->dashes = player->dashes > 0 ? player->dashes - 1 : 0;
        // Return to regular dash state
        return ST_DASH;
    }

    // Super Jump (horizontal dash only) - Celeste line 3868-3880
    if (player->dashDirY == 0) {
        // JumpThru Correction (same as regular dash)
        {
            int screenX = player->x >> FIXED_SHIFT;
            int screenY = player->y >> FIXED_SHIFT;
            int playerLeft = screenX - PLAYER_WIDTH / 2;
            int playerRight = screenX + PLAYER_WIDTH / 2;
            int playerBottom = PLAYER_BOTTOM(screenY);
            int tileMinX = playerLeft / 8;
            int tileMaxX = playerRight / 8;
            int tileMinY = PLAYER_TOP(screenY) / 8;
            int tileMaxY = playerBottom / 8;
            for (int ty = tileMinY; ty <= tileMaxY; ty++) {
                for (int tx = tileMinX; tx <= tileMaxX; tx++) {
                    if (getTileCollision(level, tx, ty) != COL_JUMPTHRU) continue;
                    int tileTop = ty * 8;
                    int tileLeft = tx * 8;
                    int tileRight = (tx + 1) * 8;
                    if (playerRight > tileLeft && playerLeft < tileRight &&
                        playerBottom > tileTop && playerBottom - tileTop <= DASH_H_JUMPTHRU_NUDGE) {
                        player->y = (tileTop - PLAYER_HEIGHT / 2 - PLAYER_HITBOX_Y_SHIFT) << FIXED_SHIFT;
                    }
                }
            }
        }

        if ((pressed & BTN_JUMP) && (player->onGround || player->coyoteTime > 0)) {
            superJump(player);
            return ST_NORMAL;
        }
    }
    // Super Wall Jump (upward dash only) - Celeste line 3882-3896
    else if (player->dashDirX == 0 && player->dashDirY == -1) {
        if (pressed & BTN_JUMP) {
            if (checkWall(player, level, 1)) {
                superWallJump(player, -1);
                return ST_NORMAL;
            } else if (checkWall(player, level, -1)) {
                superWallJump(player, 1);
                return ST_NORMAL;
            }
        }
    }
    // All other dash directions: wall jump only - Celeste line 3898-3913
    else {
        if (pressed & BTN_JUMP) {
            if (checkWall(player, level, 1)) {
                wallJump(player, -1, moveX);
                return ST_NORMAL;
            } else if (checkWall(player, level, -1)) {
                wallJump(player, 1, moveX);
                return ST_NORMAL;
            }
        }
    }

    return ST_RED_DASH;  // Stay in RedDash state
}

// Helper: Super Jump (same as regular dash)
static void superJump(Player* player) {
    int facingDir = player->facingRight ? 1 : -1;

    player->vx = facingDir * SUPER_JUMP_H;
    player->vy = SUPER_JUMP_SPEED;

    // Apply lift boost from moving platforms
    player->vx += player->liftBoostX;
    player->vy += player->liftBoostY;

    // Duck multipliers
    if (player->ducking) {
        player->ducking = 0;
        player->vx = fpMul(player->vx, FP_DUCK_JUMP_X_MULT);
        player->vy = fpMul(player->vy, FP_DUCK_JUMP_Y_MULT);
    }

    player->varJumpSpeed = player->vy;
    player->varJumpTimer = VAR_JUMP_TIME;
    player->autoJump = 0;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->dashAttackTimer = 0;
    player->dashing = 0;  // End dash so trail can fade
    player->onGround = 0;
    player->coyoteTime = 0;
    player->jumpBuffer = 0;
    player->jumpHeld = 1;
}

// Helper: Super Wall Jump (same as regular dash)
static void superWallJump(Player* player, int dir) {
    player->ducking = 0;
    player->vx = dir * SUPER_WALL_JUMP_H;
    player->vy = SUPER_WALL_JUMP_SPEED;

    // Apply lift boost from moving platforms
    player->vx += player->liftBoostX;
    player->vy += player->liftBoostY;

    player->varJumpSpeed = SUPER_WALL_JUMP_SPEED;
    player->varJumpTimer = SUPER_WALL_JUMP_VAR_TIME;
    player->autoJump = 0;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->dashAttackTimer = 0;
    player->dashing = 0;  // End dash so trail can fade
    player->coyoteTime = 0;
    player->jumpBuffer = 0;
    player->jumpHeld = 1;
    player->facingRight = dir > 0 ? 1 : 0;
}
