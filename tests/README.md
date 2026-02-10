# GBA Platformer - Mechanics Test Suite

Automated regression testing for game mechanics to prevent bugs from reappearing.

## Running Tests

```bash
# Clean, build, and run tests
make -f Makefile.test clean test

# Or just run (without rebuilding)
make -f Makefile.test test
```

Exit code 0 = all tests passed, non-zero = failures.

## Test Structure

Tests are organized by category:

- `mechanics/` - Physics and movement tests
- More categories can be added as needed

## Writing a New Test

### 1. Create Test File

Create a new file in the appropriate category (e.g., `tests/mechanics/wall_jump.c`):

```c
#ifdef DESKTOP_BUILD

#include <stdio.h>
#include "../test_framework.h"
#include "core/game_math.h"
#include "player/state.h"

// Record a replay by running on GBA with SELECT+L / SELECT+B
// Then extract with: python tools/extract_replay.py game.sav
static const u16 wall_jump_inputs[] = {
    0x0000, 0x0010, 0x0011, // ... your replay data
};

// Optional: custom per-frame verification
static void verifyWallJump(const Player* player, int frame, TestResults* results) {
    // Check specific conditions at specific frames
    if (frame == 50 && player->vy > 0) {
        printf("  FAIL: Should be moving upward at frame %d\n", frame);
        results->failed++;
    }
}

const MechanicsTest test_wall_jump = {
    .name = "Wall Jump",
    .description = "Player should jump off wall when pressing jump",
    .inputs = wall_jump_inputs,
    .frameCount = sizeof(wall_jump_inputs) / sizeof(wall_jump_inputs[0]),
    .verifyFrame = verifyWallJump,  // Optional, can be NULL

    // Expected final state (use -1 or -999.0f to skip checks)
    .expectFinalX = 50 << FIXED_SHIFT,
    .expectFinalY = 80 << FIXED_SHIFT,
    .expectFinalVX = -999.0f,  // Skip VX check
    .expectFinalVY = 0.0f,
    .expectFinalState = ST_NORMAL,
};

#endif // DESKTOP_BUILD
```

### 2. Register Test

Add your test to `tests/run_tests.c`:

```c
extern const MechanicsTest test_wall_jump;  // Add declaration

static const MechanicsTest* all_tests[] = {
    &test_diagonal_dash_slide,
    &test_wall_jump,  // Add to array
};
```

### 3. Update Makefile

Add to `Makefile.test`:

```makefile
TEST_CASE_SRCS = \
    tests/mechanics/diagonal_dash_slide.c \
    tests/mechanics/wall_jump.c
```

### 4. Run Tests

```bash
make -f Makefile.test clean test
```

## Recording Replays for Tests

1. Build and run the GBA version
2. Press SELECT+L to start recording
3. Perform the mechanic you want to test
4. Press SELECT+B to save the replay
5. Extract replay data: `python tools/extract_replay.py game.sav`
6. Copy the output array into your test file

## Continuous Integration

The test suite can be integrated into CI:

```bash
# In your CI script
make -f Makefile.test clean
make -f Makefile.test test
if [ $? -ne 0 ]; then
    echo "Tests failed!"
    exit 1
fi
```

## Example Tests

- `mechanics/diagonal_dash_slide.c` - Prevents infinite dash bug regression
- (Add more as created)

## Tips

- Use descriptive test names
- Add comments explaining what the test validates
- Include both positive tests (correct behavior) and negative tests (bug prevention)
- Use `verifyFrame` callback for complex frame-by-frame assertions
- Keep replays short (< 200 frames) for fast test execution
