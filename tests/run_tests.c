#ifdef DESKTOP_BUILD

#include <stdio.h>
#include <stdlib.h>
#include "test_framework.h"
#include "level/level.h"

// External test declarations
extern const MechanicsTest test_diagonal_dash_slide;
extern const MechanicsTest test_dash_height;
extern const MechanicsTest test_climb_stamina_glitch;
extern const MechanicsTest test_wall_grab_slide;
extern const MechanicsTest test_climb_hop_ledge;
extern const MechanicsTest test_spring_bounce_superjump;

// Test registry
static const MechanicsTest* all_tests[] = {
    &test_diagonal_dash_slide,
    &test_dash_height,
    &test_climb_stamina_glitch,
    &test_wall_grab_slide,
    &test_climb_hop_ledge,
    &test_spring_bounce_superjump,
    // Add more tests here as they're created
};

int main(int argc, char** argv) {
    printf("GBA Platformer - Mechanics Test Suite\n");
    printf("======================================\n");

    TestResults results;
    initTestResults(&results);

    // Default level for tests that don't specify one
    // Individual tests can override this by setting .level in their struct
    const Level* defaultLevel = &level3;

    // Run all tests
    int numTests = sizeof(all_tests) / sizeof(all_tests[0]);
    for (int i = 0; i < numTests; i++) {
        runMechanicsTest(all_tests[i], defaultLevel, &results);
    }

    // Print summary
    printTestSummary(&results);

    // Exit with failure code if any tests failed
    return (results.failed > 0) ? 1 : 0;
}

#endif // DESKTOP_BUILD
