#ifdef DESKTOP_BUILD

#include <stdio.h>
#include "../test_framework.h"
#include "core/game_math.h"
#include "player/state.h"
#include "level/level.h"

// Replay data from the diagonal dash bug test
static const u16 diagonal_dash_inputs[] = {
    0x0204, 0x0204, 0x0204, 0x0204, 0x0204, 0x0204, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0091, 0x0091, 0x0091, 0x0091, 0x0091,
    0x0091, 0x0091, 0x0091, 0x0091, 0x0091, 0x0091, 0x0090, 0x0090,
    0x0190, 0x0190, 0x0190, 0x0190, 0x0190, 0x0190, 0x0190, 0x0190,
    0x0110, 0x0110, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
    0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
    0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
};

// Custom verification: ensure player doesn't fly forever in Dash state
static void verifyNoDashBug(const Player* player, int frame, TestResults* results) {
    // After frame 80, player should NOT be in Dash state with dashing=0
    // (which would indicate the infinite dash bug)
    if (frame > 80) {
        if (player->stateMachine.state == ST_DASH && player->dashing == 0) {
            printf("  FAIL: Infinite dash bug detected at frame %d (state=DASH, dashing=0)\n", frame);
            results->failed++;
        }
    }

    // After frame 100, player should have stopped or nearly stopped (VX < 100)
    if (frame > 100) {
        if (player->vx > 100.0f) {
            printf("  FAIL: Player still moving too fast at frame %d (vx=%.1f)\n", frame, player->vx);
            results->failed++;
        }
    }
}

const MechanicsTest test_diagonal_dash_slide = {
    .name = "Diagonal Dash Slide",
    .description = "Dash diagonally into ground should trigger dash slide, not infinite dash",
    .inputs = diagonal_dash_inputs,
    .frameCount = sizeof(diagonal_dash_inputs) / sizeof(diagonal_dash_inputs[0]),
    .level = &Tutorial_Level,
    .verifyFrame = verifyNoDashBug,
    .expectFinalX = 77 << FIXED_SHIFT,  // Should stop around X=77
    .expectFinalY = 112 << FIXED_SHIFT, // Should be on ground at Y=112
    .expectFinalVX = 0.0f,              // Should have stopped
    .expectFinalVY = 0.0f,              // Should be on ground
    .expectFinalState = ST_NORMAL,      // Should be in Normal state, not stuck in Dash
};

#endif // DESKTOP_BUILD
