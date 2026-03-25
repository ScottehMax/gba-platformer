#ifdef DESKTOP_BUILD

#include <stdio.h>
#include <math.h>
#include "../test_framework.h"
#include "core/game_math.h"
#include "player/state.h"
#include "level/level.h"

// Replay data from game.sav (dash height issue)
static const u16 dash_height_inputs[] = {
    0x0204, 0x0204, 0x0204, 0x0204, 0x0204, 0x0204, 0x0204, 0x0204,
    0x0204, 0x0204, 0x0204, 0x0204, 0x0204, 0x0004, 0x0004, 0x0004,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0010, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050, 0x0050,
    0x0050, 0x0051, 0x0051, 0x0051, 0x0051, 0x0051, 0x0051, 0x0051,
    0x0051, 0x0051, 0x0051, 0x0051, 0x0051, 0x0051, 0x0051, 0x0051,
    0x0051, 0x0051, 0x0051, 0x0051, 0x0051, 0x0051, 0x0050, 0x0050,
    0x0150, 0x0150, 0x0150, 0x0150, 0x0150, 0x0150, 0x0150, 0x0150,
    0x0150, 0x0150, 0x0150, 0x0150, 0x0150, 0x0150, 0x0150, 0x0150,
    0x0150, 0x0150, 0x0150, 0x0150, 0x0150, 0x0150, 0x0150, 0x0150,
    0x0150, 0x0150, 0x0150, 0x0150, 0x0150, 0x0150, 0x0150, 0x0150,
    0x0150, 0x0150, 0x0150, 0x0140, 0x0140, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
};

static int dashStartFrame = -1;
static int dashPressFrame = -1;
static int expectedVx = 0;
static int expectedVy = 0;
static int checkedDash = 0;

static void verifyDashHeight(const Player* player, int frame, TestResults* results) {
    if (checkedDash) {
        return;
    }

    // Record expected dash speed on the frame R is pressed.
    if (dashPressFrame < 0 && (dash_height_inputs[frame] & KEY_R)) {
        dashPressFrame = frame;
        int dashX = 0;
        int dashY = 0;
        if (dash_height_inputs[frame] & KEY_LEFT) dashX = -1;
        if (dash_height_inputs[frame] & KEY_RIGHT) dashX = 1;
        if (dash_height_inputs[frame] & KEY_UP) dashY = -1;
        if (dash_height_inputs[frame] & KEY_DOWN) dashY = 1;
        if (dashX == 0 && dashY == 0) {
            dashX = player->facingRight ? 1 : -1;
        }

        expectedVx = dashX * DASH_SPEED;
        expectedVy = dashY * DASH_SPEED;
        if (dashX != 0 && dashY != 0) {
            expectedVx = (expectedVx * FP_DIAG_NORMALIZE) >> FIXED_SHIFT;
            expectedVy = (expectedVy * FP_DIAG_NORMALIZE) >> FIXED_SHIFT;
        }
    }

    if (dashPressFrame >= 0 && dashStartFrame < 0 && player->stateMachine.state == ST_DASH && player->dashing > 0) {
        dashStartFrame = frame;
    }

    if (dashStartFrame >= 0 && player->stateMachine.state == ST_DASH) {
        int tolerance = 5;
        int vxDiff = player->vx - expectedVx;
        int vyDiff = player->vy - expectedVy;
        if (vxDiff < 0) vxDiff = -vxDiff;
        if (vyDiff < 0) vyDiff = -vyDiff;
        if (vxDiff > tolerance || vyDiff > tolerance) {
            printf("  FAIL: Dash speed drifted (vx=%d vy=%d expected %d/%d)\n",
                   player->vx, player->vy, expectedVx, expectedVy);
            results->failed++;
            checkedDash = 1;
        }
    }

    if (dashStartFrame >= 0 && frame > dashStartFrame + 12) {
        checkedDash = 1;
    }
}

const MechanicsTest test_dash_height = {
    .name = "Dash Height Replay",
    .description = "Dash should maintain upward speed (no gravity drag during dash)",
    .inputs = dash_height_inputs,
    .frameCount = sizeof(dash_height_inputs) / sizeof(dash_height_inputs[0]),
    .level = &level3,
    .startX = 6144,
    .startY = 28672,
    .verifyFrame = verifyDashHeight,
    .expectFinalX = -1,
    .expectFinalY = -1,
    .expectFinalVX = -999,
    .expectFinalVY = -999,
    .expectFinalState = -1,
};

#endif // DESKTOP_BUILD
