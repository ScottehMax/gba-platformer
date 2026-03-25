#ifdef DESKTOP_BUILD

#include <stdio.h>
#include <math.h>
#include "../test_framework.h"
#include "core/game_math.h"
#include "player/state.h"
#include "level/level.h"

// Replay data extracted from game.sav (climb hop not popping onto ledge)
static const u16 climb_hop_inputs[] = {
    0x0000, 0x0000, 0x0204, 0x0204, 0x0204, 0x0200, 0x0200, 0x0200,
    0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
    0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
    0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0240, 0x0240,
    0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240,
    0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240,
    0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240,
    0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240,
    0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240,
    0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240,
    0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240,
    0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240,
    0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240,
    0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240, 0x0240,
    0x0240, 0x0240, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
    0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200,
    0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0200, 0x0204,
    0x0204, 0x0204, 0x0204,
};

static int climbHopFrame = -1;
static int hasReportedFailure = 0;
static int prevState = -1;

static void verifyClimbHopLedge(const Player* player, int frame, TestResults* results) {
    int currentState = player->stateMachine.state;

    // Detect actual state changes (not just previousState field)
    if (prevState == ST_CLIMB && currentState == ST_NORMAL && player->vy < 0) {
        if (climbHopFrame < 0) {
            climbHopFrame = frame;
            printf("  INFO: Climb hop at frame %d, y=%.1fpx\n", frame, player->y / 256.0f);
        }
    }

    // Monitor position for a few frames after hop
    if (climbHopFrame >= 0 && frame >= climbHopFrame && frame <= climbHopFrame + 15) {
        if ((frame - climbHopFrame) % 3 == 0) {
            printf("  DEBUG: Frame %d (+%d): x=%.1fpx, y=%.1fpx, vx=%d, vy=%d, state=%d\n",
                   frame, frame - climbHopFrame,
                   player->x / 256.0f, player->y / 256.0f,
                   player->vx, player->vy, currentState);
        }
    }

    // Detect re-entering climb after the hop
    if (climbHopFrame >= 0 && prevState == ST_NORMAL && currentState == ST_CLIMB) {
        if (!hasReportedFailure) {
            printf("  FAIL: Player re-entered climb state at frame %d!\n", frame);
            printf("        Frame %d after hop, vy=%d, hopWaitX=%d, forceMoveXTimer=%d\n",
                   frame - climbHopFrame, player->vy, player->hopWaitX, player->forceMoveXTimer);
            printf("        Player should stay in normal state after hop\n");
            results->failed++;
            hasReportedFailure = 1;
        }
    }

    prevState = currentState;

    // Detect when player hops (exits climb state while moving upward)
    if (climbHopFrame < 0 &&
        player->stateMachine.previousState == ST_CLIMB &&
        player->stateMachine.state == ST_NORMAL &&
        player->vy < 0) {
        climbHopFrame = frame;
        printf("  INFO: Climb hop detected at frame %d, y=%.1fpx\n", frame, player->y / 256.0f);
    }

    // At the end, check if player is on the ledge (not on the ground)
    // The ledge is around y=88px based on the starting position
    // If the player falls back down, they'll be much lower (around y=118px)
    if (frame == 138) {  // Last frame
        float finalY = player->y / 256.0f;
        printf("  INFO: Final position: y=%.1fpx, state=%d\n", finalY, player->stateMachine.state);

        // If player successfully hopped onto ledge, they should be at ~88px
        // If they failed, they fall back to ground at ~118px
        if (finalY > 100.0f) {
            if (!hasReportedFailure) {
                printf("  FAIL: Player did not climb onto ledge! Final y=%.1fpx (expected < 100px)\n", finalY);
                printf("        This suggests the climb hop didn't properly place player on top of wall\n");
                results->failed++;
                hasReportedFailure = 1;
            }
        } else {
            printf("  PASS: Player successfully climbed onto ledge (y=%.1fpx)\n", finalY);
            results->passed++;
        }
    }
}

const MechanicsTest test_climb_hop_ledge = {
    .name = "Climb Hop Ledge Pop",
    .description = "Player should pop onto ledge when climbing to the top, not fall back down",
    .inputs = climb_hop_inputs,
    .frameCount = sizeof(climb_hop_inputs) / sizeof(climb_hop_inputs[0]),
    .level = &level3,
    .startX = 115758,  // Fixed-point (256 = 1 pixel)
    .startY = 30400,   // Fixed-point (256 = 1 pixel)
    .verifyFrame = verifyClimbHopLedge,
    .expectFinalX = -1,
    .expectFinalY = -1,
    .expectFinalVX = -999,
    .expectFinalVY = -999,
    .expectFinalState = -1,
};

#endif // DESKTOP_BUILD
