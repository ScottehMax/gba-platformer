#include "../state.h"
#include "util/calc.h"
#include "collision/collision.h"
#include "core/game_math.h"
#include "core/input.h"

void boostBegin(Player* player, const Level* level) {
    // Celeste BoostBegin (line 3788-3792)
    (void)level;  // Unused in boost state

    // Instantly move player to bubble center (GBA simplification for snappier feel)
    player->x = player->boostTargetX;
    player->y = player->boostTargetY;

    // Refill dash and stamina (Celeste line 3790-3791)
    player->dashes = player->maxDashes;
    player->stamina = CLIMB_MAX_STAMINA;

    // Zero velocity (Celeste line 3773)
    player->vx = 0;
    player->vy = 0;

    // Clear dashing flag so trails can fade
    player->dashing = 0;
}

void boostEnd(Player* player) {
    // Celeste BoostEnd (line 3794-3799)
    // Move player to exact target position
    player->x = player->boostTargetX;
    player->y = player->boostTargetY;
}

int boostUpdate(Player* player, u16 keys, const Level* level) {
    // Celeste BoostUpdate (line 3801-3818)
    // Player physically moves to bubble center, then auto-transitions to RedDash
    (void)level;  // Unused in boost update

    // Get aim direction from input
    int aimX = 0, aimY = 0;
    if (keys & BTN_LEFT) aimX = -1;
    if (keys & BTN_RIGHT) aimX = 1;
    if (keys & BTN_UP) aimY = -1;
    if (keys & BTN_DOWN) aimY = 1;

    // targetAdd = Input.Aim.Value * 3 (Celeste line 3803)
    // Allows player to aim slightly off-center while moving
    int targetAddX = aimX * 3 * FIXED_ONE;
    int targetAddY = aimY * 3 * FIXED_ONE;

    // Physically move player position toward bubble center at BoostMoveSpeed (Celeste line 3804-3806)
    int targetX = player->boostTargetX + targetAddX;
    int targetY = player->boostTargetY + targetAddY;

    player->x = approach(player->x, targetX, BOOST_MOVE_SPEED_PF);
    player->y = approach(player->y, targetY, BOOST_MOVE_SPEED_PF);

    // Check for dash press (Celeste line 3808-3815)
    u16 pressed = keys & ~player->prevKeys;
    if (pressed & BTN_DASH) {
        if (player->boostRed) {
            return ST_RED_DASH;
        } else {
            return ST_DASH;
        }
    }

    // Countdown boost timer (Celeste BoostCoroutine line 3820-3827)
    if (player->boostTimer > 0) {
        player->boostTimer--;
        if (player->boostTimer == 0) {
            // Auto-dash when timer expires
            if (player->boostRed) {
                return ST_RED_DASH;
            } else {
                return ST_DASH;
            }
        }
    }

    return ST_BOOST;  // Stay in boost state
}
