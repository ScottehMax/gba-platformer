#include "../state.h"
#include "util/calc.h"
#include "collision/collision.h"
#include "core/game_math.h"
#include "core/input.h"

void hitSquashBegin(Player* player, const Level* level) {
    // Celeste HitSquashBegin (line 3938-3941)
    (void)level;  // Unused in HitSquash state
    player->hitSquashNoMoveTimer = HIT_SQUASH_NO_MOVE_TIME;
}

void hitSquashEnd(Player* player) {
    // Celeste has no HitSquashEnd method (no cleanup needed)
    (void)player;
}

int hitSquashUpdate(Player* player, u16 keys, const Level* level) {
    // Celeste HitSquashUpdate (line 3943-3974)

    // Apply friction to slow down (Celeste line 3945-3946)
    player->vx = approach(player->vx, 0, HIT_SQUASH_FRICTION_PF);
    player->vy = approach(player->vy, 0, HIT_SQUASH_FRICTION_PF);

    // Detect button presses
    u16 pressed = keys & ~player->prevKeys;
    int moveX = keys & BTN_RIGHT ? 1 : (keys & BTN_LEFT ? -1 : 0);

    // Check for jump (Celeste line 3948-3960)
    if (pressed & BTN_JUMP) {
        if (player->onGround) {
            // Ground jump (Celeste line 3951)
            jump(player, keys);
            return ST_NORMAL;
        } else if (checkWall(player, level, 1)) {
            // Wall jump left (Celeste line 3952-3953)
            wallJump(player, -1, moveX);
            return ST_NORMAL;
        } else if (checkWall(player, level, -1)) {
            // Wall jump right (Celeste line 3954-3955)
            wallJump(player, 1, moveX);
            return ST_NORMAL;
        } else {
            // Consume buffer if can't jump (Celeste line 3957)
            player->jumpBuffer = 0;
        }
    }

    // Check for dash (Celeste line 3962-3963)
    // Note: Need to check if can dash (dashes > 0, cooldown == 0)
    if ((pressed & BTN_DASH) && player->dashes > 0 && player->dashCooldownTimer == 0) {
        // Consume dash
        player->dashes = player->dashes > 0 ? player->dashes - 1 : 0;
        return ST_DASH;
    }

    // Check for climb (Celeste line 3965-3966)
    if (keys & BTN_GRAB) {
        int facingDir = player->facingRight ? 1 : -1;
        // Requires stamina > 20 (not tired) and wall in facing direction
        if (player->stamina > CLIMB_TIRED_THRESHOLD &&
            checkWallAt(player, level, facingDir, 0, CLIMB_CHECK_DIST)) {
            return ST_CLIMB;
        }
    }

    // Countdown timer (Celeste line 3968-3971)
    if (player->hitSquashNoMoveTimer > 0) {
        player->hitSquashNoMoveTimer--;
    } else {
        // Timer expired, return to normal (Celeste line 3971)
        return ST_NORMAL;
    }

    return ST_HIT_SQUASH;  // Stay in HitSquash state
}
