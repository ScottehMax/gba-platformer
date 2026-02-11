#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#ifdef DESKTOP_BUILD
#include "desktop/desktop_stubs.h"
#else
#include <tonc.h>
#endif

#include "core/game_types.h"
#include "level/level.h"

// Test result tracking
typedef struct {
    int passed;
    int failed;
    const char* currentTest;
} TestResults;

// Test case definition
typedef struct {
    const char* name;
    const char* description;
    const u16* inputs;
    int frameCount;
    const Level* level;    // Level to use for this test (if NULL, uses default)

    // Optional: custom verification per frame
    void (*verifyFrame)(const Player* player, int frame, TestResults* results);

    // Expected final state
    int expectFinalX;      // Fixed-point expected X position (or -1 to skip)
    int expectFinalY;      // Fixed-point expected Y position (or -1 to skip)
    float expectFinalVX;   // Expected final VX (or use -999.0f to skip)
    float expectFinalVY;   // Expected final VY (or use -999.0f to skip)
    int expectFinalState;  // Expected final state (or -1 to skip)
} MechanicsTest;

// Test assertion macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  FAIL: %s (frame %d)\n", message, frame); \
            results->failed++; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_RANGE(value, expected, tolerance, message) \
    do { \
        if ((value) < (expected) - (tolerance) || (value) > (expected) + (tolerance)) { \
            printf("  FAIL: %s (expected %.1f, got %.1f, frame %d)\n", \
                   message, (float)(expected), (float)(value), frame); \
            results->failed++; \
            return; \
        } \
    } while(0)

// Test runner functions
void initTestResults(TestResults* results);
void runMechanicsTest(const MechanicsTest* test, const Level* level, TestResults* results);
void printTestSummary(const TestResults* results);

#endif // TEST_FRAMEWORK_H
