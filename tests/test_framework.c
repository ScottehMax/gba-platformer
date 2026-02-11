#ifdef DESKTOP_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "test_framework.h"
#include "player/player.h"
#include "core/game_math.h"

void initTestResults(TestResults* results) {
    results->passed = 0;
    results->failed = 0;
    results->currentTest = NULL;
}

void runMechanicsTest(const MechanicsTest* test, const Level* defaultLevel, TestResults* results) {
    results->currentTest = test->name;

    printf("\n[TEST] %s\n", test->name);
    printf("  Description: %s\n", test->description);

    // Use test-specific level if provided, otherwise use default
    const Level* level = test->level ? test->level : defaultLevel;

    // Initialize player
    Player player;
    initPlayer(&player, level);

    // Override starting position if test specifies one
    if (test->startX != 0 || test->startY != 0) {
        player.x = test->startX;
        player.y = test->startY;
        player.vx = 0;
        player.vy = 0;
    }

    // Run replay
    int testFailed = 0;
    for (int frame = 0; frame < test->frameCount; frame++) {
        u16 keys = test->inputs[frame];

        // Custom per-frame verification
        if (test->verifyFrame) {
            test->verifyFrame(&player, frame, results);
            if (results->failed > 0) {
                testFailed = 1;
                break;
            }
        }

        // Update player
        updatePlayer(&player, keys, level);
    }

    if (testFailed) {
        return;  // Already counted as failed
    }

    // Verify final state
    int finalFailed = 0;

    if (test->expectFinalX != -1) {
        int tolerance = FIXED_ONE * 2;  // 2 pixel tolerance
        if (abs(player.x - test->expectFinalX) > tolerance) {
            printf("  FAIL: Final X position (expected %d, got %d)\n",
                   test->expectFinalX >> FIXED_SHIFT, player.x >> FIXED_SHIFT);
            finalFailed = 1;
        }
    }

    if (test->expectFinalY != -1) {
        int tolerance = FIXED_ONE * 2;  // 2 pixel tolerance
        if (abs(player.y - test->expectFinalY) > tolerance) {
            printf("  FAIL: Final Y position (expected %d, got %d)\n",
                   test->expectFinalY >> FIXED_SHIFT, player.y >> FIXED_SHIFT);
            finalFailed = 1;
        }
    }

    if (test->expectFinalVX != -999.0f) {
        float tolerance = 5.0f;
        if (fabs(player.vx - test->expectFinalVX) > tolerance) {
            printf("  FAIL: Final VX (expected %.1f, got %.1f)\n",
                   test->expectFinalVX, player.vx);
            finalFailed = 1;
        }
    }

    if (test->expectFinalVY != -999.0f) {
        float tolerance = 5.0f;
        if (fabs(player.vy - test->expectFinalVY) > tolerance) {
            printf("  FAIL: Final VY (expected %.1f, got %.1f)\n",
                   test->expectFinalVY, player.vy);
            finalFailed = 1;
        }
    }

    if (test->expectFinalState != -1) {
        if (player.stateMachine.state != test->expectFinalState) {
            printf("  FAIL: Final state (expected %d, got %d)\n",
                   test->expectFinalState, player.stateMachine.state);
            finalFailed = 1;
        }
    }

    if (finalFailed) {
        results->failed++;
        printf("  ❌ FAILED\n");
    } else {
        results->passed++;
        printf("  ✓ PASSED\n");
    }
}

void printTestSummary(const TestResults* results) {
    printf("\n========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Passed: %d\n", results->passed);
    printf("Failed: %d\n", results->failed);
    printf("Total:  %d\n", results->passed + results->failed);

    if (results->failed == 0) {
        printf("\n✓ All tests passed!\n");
    } else {
        printf("\n❌ Some tests failed!\n");
    }
}

#endif // DESKTOP_BUILD
