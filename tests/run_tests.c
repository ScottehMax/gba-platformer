#ifdef DESKTOP_BUILD

#include <stdio.h>
#include <stdlib.h>
#include "test_framework.h"
#include "level/level.h"

// External test declarations
extern const MechanicsTest test_diagonal_dash_slide;
extern const MechanicsTest test_dash_height;

// Test registry
static const MechanicsTest* all_tests[] = {
    &test_diagonal_dash_slide,
    &test_dash_height,
    // Add more tests here as they're created
};

int main(int argc, char** argv) {
    printf("GBA Platformer - Mechanics Test Suite\n");
    printf("======================================\n");

    TestResults results;
    initTestResults(&results);

    // Use the real level for testing
    const Level* testLevel = &Tutorial_Level;

    // Run all tests
    int numTests = sizeof(all_tests) / sizeof(all_tests[0]);
    for (int i = 0; i < numTests; i++) {
        runMechanicsTest(all_tests[i], testLevel, &results);
    }

    // Print summary
    printTestSummary(&results);

    // Exit with failure code if any tests failed
    return (results.failed > 0) ? 1 : 0;
}

#endif // DESKTOP_BUILD
