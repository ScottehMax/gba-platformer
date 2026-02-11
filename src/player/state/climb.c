#include "../state.h"
#include "util/calc.h"
#include "collision/collision.h"
#include "core/game_math.h"

// Forward declarations
static void climbJump(Player* player, u16 keys);
static void climbHop(Player* player);
static void wallJump(Player* player, int dir);
static int slipCheck(const Player* player, const Level* level, int addY);

void climbBegin(Player* player) {
    // Celeste ClimbBegin (line 3056-3078)
    player->vx = 0;
    player->vy *= CLIMB_GRAB_Y_MULT;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->climbNoMoveTimer = CLIMB_NO_MOVE_TIME;
    player->lastClimbMove = 0;

    // Note: Wall snapping (Celeste line 3068-3072) requires level parameter
    // which isn't available in Begin callback. The player should already
    // be close enough to the wall from normal state's wall check.
    // If needed, this can be done in the first frame of climbUpdate.
}

void climbEnd(Player* player) {
    // Celeste ClimbEnd (line 3080-3091)
    // Most cleanup is audio/visual effects which GBA doesn't need
}

int climbUpdate(Player* player, u16 keys, const Level* level) {
    // Celeste ClimbUpdate (line 3102-3277)

    // Detect button presses
    u16 pressed = keys & ~player->prevKeys;
    int moveX = keys & KEY_RIGHT ? 1 : (keys & KEY_LEFT ? -1 : 0);
    int moveY = keys & KEY_DOWN ? 1 : (keys & KEY_UP ? -1 : 0);
    int facingDir = player->facingRight ? 1 : -1;

    // Refill stamina on ground (Celeste line 3107-3108)
    if (player->onGround) {
        player->stamina = CLIMB_MAX_STAMINA;
    }

    // Wall Jump or Climb Jump (Celeste line 3111-3119)
    if (pressed & KEY_A) {
        if (moveX == -facingDir) {
            // Jump away from wall
            wallJump(player, -facingDir);
        } else {
            // Climb jump (jump up while staying on wall)
            climbJump(player, keys);
        }
        return ST_NORMAL;
    }

    // Dashing (Celeste line 3121-3126)
    if ((pressed & KEY_R) && player->dashCooldownTimer == 0 && player->dashes > 0) {
        player->dashes = player->dashes > 0 ? player->dashes - 1 : 0;
        return ST_DASH;
    }

    // Let go of wall (Celeste line 3128-3134)
    if (!(keys & KEY_L)) {
        return ST_NORMAL;
    }

    // No wall to hold (Celeste line 3136-3152)
    if (!checkWallAt(player, level, facingDir, 0, CLIMB_CHECK_DIST)) {
        // Climbed over ledge
        if (player->vy < 0) {
            climbHop(player);
        }
        return ST_NORMAL;
    }

    // Climbing movement (Celeste line 3179-3230)
    float target = 0;
    int trySlip = 0;

    if (player->climbNoMoveTimer <= 0) {
        if (moveY == -1) {
            // Climbing up
            target = CLIMB_UP_SPEED;

            // Check if can climb up or if blocked
            int checkY = (player->y - (PLAYER_RADIUS_Y << FIXED_SHIFT)) >> FIXED_SHIFT;
            int tileY = checkY / 8;
            int screenX = player->x >> FIXED_SHIFT;
            int tileX = screenX / 8;
            u16 tile = getTileAt(level, 0, tileX, tileY);

            if (isTileSolid(level, tile) || slipCheck(player, level, -1)) {
                // Blocked or at top, try to hop
                if (player->vy < 0) {
                    player->vy = 0;
                }
                target = 0;
                trySlip = 1;
            } else if (slipCheck(player, level, 0)) {
                // Hands above ledge, hop over
                climbHop(player);
                return ST_NORMAL;
            }
        } else if (moveY == 1) {
            // Climbing down
            target = CLIMB_DOWN_SPEED;

            if (player->onGround) {
                if (player->vy > 0) {
                    player->vy = 0;
                }
                target = 0;
            }
        } else {
            // Not moving
            trySlip = 1;
        }
    } else {
        trySlip = 1;
    }

    player->lastClimbMove = target < 0 ? -1 : (target > 0 ? 1 : 0);

    // Slip down if hands above ledge and no input (Celeste line 3226-3227)
    if (trySlip && slipCheck(player, level, 0)) {
        target = CLIMB_SLIP_SPEED;
    }

    // Set speed (Celeste line 3230)
    player->vy = approach(player->vy, target, CLIMB_ACCEL / 60.0f);

    // Down limit: don't fall off bottom of wall (Celeste line 3233-3235)
    if (moveY != 1 && player->vy > 0) {
        if (!checkWallAt(player, level, facingDir, 0, CLIMB_CHECK_DIST)) {
            player->vy = 0;
        }
    }

    // Stamina drain (Celeste line 3238-3267)
    if (player->climbNoMoveTimer <= 0) {
        if (player->lastClimbMove == -1) {
            // Climbing up costs stamina
            player->stamina -= CLIMB_UP_COST / 60.0f;
        } else if (player->lastClimbMove == 0) {
            // Hanging still costs stamina
            player->stamina -= CLIMB_STILL_COST / 60.0f;
        }
        // Climbing down is free
    }

    // Too tired, let go (Celeste line 3270-3274)
    if (player->stamina <= 0) {
        return ST_NORMAL;
    }

    return ST_CLIMB;
}

// Helper: Climb Jump (Celeste line 1813-1842)
static void climbJump(Player* player, u16 keys) {
    int moveX = keys & KEY_RIGHT ? 1 : (keys & KEY_LEFT ? -1 : 0);

    // Consume stamina (Celeste line 1817)
    if (!player->onGround) {
        player->stamina -= CLIMB_JUMP_COST;
    }

    // Normal jump
    player->ducking = 0;
    player->vy = JUMP_STRENGTH;
    player->varJumpSpeed = JUMP_STRENGTH;
    player->varJumpTimer = VAR_JUMP_TIME;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->onGround = 0;
    player->coyoteTime = 0;
    player->jumpBuffer = 0;
    player->jumpHeld = 1;

    // If no horizontal input, boost away from wall (Celeste line 1826-1830)
    if (moveX == 0) {
        int facingDir = player->facingRight ? 1 : -1;
        player->vx = -facingDir * JUMP_HORIZONTAL_BOOST;
    }
}

// Helper: Climb Hop (Celeste line 3291-3314)
static void climbHop(Player* player) {
    int facingDir = player->facingRight ? 1 : -1;

    player->vx = facingDir * CLIMB_HOP_X;

    // Min with current vy (don't reduce upward speed)
    if (player->vy > CLIMB_HOP_Y) {
        player->vy = CLIMB_HOP_Y;
    }
}

// Helper: Slip Check (Celeste line 3316-3325)
// Returns true if player's hands are above the ledge (should slip/hop)
static int slipCheck(const Player* player, const Level* level, int addY) {
    int facingDir = player->facingRight ? 1 : -1;
    int screenX = player->x >> FIXED_SHIFT;
    int screenY = player->y >> FIXED_SHIFT;

    // Check position at player's top + 4 pixels + addY
    int checkX, checkY;
    if (facingDir == 1) {
        // Facing right: check top-right
        checkX = screenX + PLAYER_RADIUS_X;
    } else {
        // Facing left: check top-left - 1
        checkX = screenX - PLAYER_RADIUS_X - 1;
    }
    checkY = screenY - PLAYER_RADIUS_Y + 4 + addY;

    int tileX = checkX / 8;
    int tileY = checkY / 8;
    u16 tile1 = getTileAt(level, 0, tileX, tileY);

    // Also check 4 pixels above that
    int checkY2 = checkY - 4 + addY;
    int tileY2 = checkY2 / 8;
    u16 tile2 = getTileAt(level, 0, tileX, tileY2);

    // If both positions are NOT solid, player is above the ledge
    return !isTileSolid(level, tile1) && !isTileSolid(level, tile2);
}

// Helper: Wall Jump from climb (already defined in normal.c, but need local version)
static void wallJump(Player* player, int dir) {
    player->ducking = 0;
    player->vx = dir * WALL_JUMP_H_SPEED;
    player->vy = JUMP_STRENGTH;
    player->varJumpSpeed = JUMP_STRENGTH;
    player->varJumpTimer = VAR_JUMP_TIME;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->coyoteTime = 0;
    player->jumpBuffer = 0;
    player->jumpHeld = 1;
    player->facingRight = dir > 0 ? 1 : 0;
}
