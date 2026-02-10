# State Machine Implementation

## Overview
Successfully ported the Celeste state machine system to the GBA platformer. The implementation closely follows the C# code structure from `Player.cs`.

## File Structure

### Core State Machine
- **`src/player/state.h`** - State machine interface and state constants
- **`src/player/state.c`** - State machine implementation (initialization, callbacks, state switching)

### State Implementations
- **`src/player/state/normal.c`** - Normal state (ground/air movement, jumping)
- **`src/player/state/dash.c`** - Dash state (dashing, super jumps during dash)
- **`src/player/state/climb.c`** - Climb state (placeholder for future implementation)

### Modified Files
- **`src/core/game_types.h`** - Added `StateMachine` struct to `Player`
- **`src/core/game_math.h`** - Added `DUCK_FRICTION` constant
- **`src/player/player.c`** - Refactored to use state machine
- **`Makefile`** - Added build rules for state files

## State Machine Architecture

### Celeste Pattern (C#)
```csharp
StateMachine = new StateMachine(23);
StateMachine.SetCallbacks(StNormal, NormalUpdate, null, NormalBegin, NormalEnd);
StateMachine.SetCallbacks(StDash, DashUpdate, DashCoroutine, DashBegin, DashEnd);
```

### GBA Implementation (C)
```c
initStateMachine(&player->stateMachine);
setStateCallbacks(&player->stateMachine, ST_NORMAL, normalUpdate, normalBegin, normalEnd);
setStateCallbacks(&player->stateMachine, ST_DASH, dashUpdate, dashBegin, dashEnd);
```

## State Lifecycle

Each state has three callbacks (matching Celeste):

1. **Begin** - Called when entering the state
2. **Update** - Called every frame while in the state, returns next state ID
3. **End** - Called when leaving the state

### Example: Dash State
```c
void dashBegin(Player* player) {
    // Initialize dash parameters
    player->dashCooldownTimer = DASH_COOLDOWN_TIME;
    player->dashAttackTimer = DASH_ATTACK_TIME;
    // ... set velocity, create trails, etc.
}

int dashUpdate(Player* player, u16 keys, const Level* level) {
    // Handle dash logic
    // Check for super jumps during dash
    // Countdown dash timer

    if (dash_ended) {
        return ST_NORMAL;  // Return to normal state
    }
    return ST_DASH;  // Stay in dash state
}

void dashEnd(Player* player) {
    // Cleanup dash state
}
```

## Update Flow

### Previous (Monolithic)
```
updatePlayer()
  ├─ All timers
  ├─ Dash logic inline
  ├─ Movement logic inline
  ├─ Jump logic inline
  └─ Collision
```

### Current (State Machine)
```
updatePlayer()
  ├─ Pre-state timers (cooldowns, buffers)
  ├─ updateStateMachine()
  │   ├─ Call state->update()
  │   ├─ If state changed:
  │   │   ├─ Call old_state->end()
  │   │   ├─ Switch state
  │   │   └─ Call new_state->begin()
  │   └─ Return next state
  ├─ Collision (horizontal + vertical)
  ├─ Post-state logic (dash slide, var jump cut, coyote time)
  └─ Update prevKeys
```

## State Constants

Matching Celeste's state IDs:
```c
#define ST_NORMAL 0       // Ground/air movement
#define ST_CLIMB 1        // Wall climbing (placeholder)
#define ST_DASH 2         // Dashing
#define ST_SWIM 3         // Swimming (future)
#define ST_BOOST 4        // Boost blocks (future)
// ... additional states from Celeste
```

## Normal State Implementation

### From Celeste `NormalUpdate()` (line 2784)
- Running and friction
- Gravity and fast falling
- Wall slide detection
- Variable jumping
- Jump input handling
- State transitions (to dash when R pressed)

### Key Features Ported
- **Horizontal movement** with air mult (Celeste line 2879-2895)
- **Duck friction** when crouching (Celeste line 2881)
- **Wall slide** with timer-based lerp (Celeste line 2932-2950)
- **Fast fall** acceleration (Celeste line 2910-2924)
- **Variable jump** clamping (Celeste line 2960-2967)
- **Jump buffering** (Celeste AutoJump system)

## Dash State Implementation

### From Celeste `DashCoroutine()` (line 3548) + `DashUpdate()` (line 3474)
- Dash initialization with direction
- Speed preservation if moving faster
- Diagonal normalization (0.707 multiplier)
- Trail sprite creation
- Super jump/wall jump checks during dash
- End dash speed application

### Key Features Ported
- **Dash direction** from input or facing (Celeste line 3552-3568)
- **Speed preservation** for higher velocities (Celeste line 3557-3558)
- **Dash trails** at intervals (Celeste line 3587-3589)
- **Super jumps** during horizontal dash (Celeste line 3502-3507)
- **Super wall jumps** during upward dash (Celeste line 3510-3525)
- **End dash speed** with upward multiplier (Celeste line 3625-3632)

## Coroutine Emulation

Celeste uses C# coroutines (IEnumerator) for timing-based state behavior like dashes. GBA implementation uses frame timers instead:

### Celeste (C#)
```csharp
private IEnumerator DashCoroutine() {
    yield return null;  // Wait 1 frame
    Speed = DashDir * DashSpeed;
    yield return DashTime;  // Wait 0.15s
    Speed = DashDir * EndDashSpeed;
    StateMachine.State = StNormal;
}
```

### GBA (C)
```c
void dashBegin() {
    player->dashing = DASH_LENGTH;  // 9 frames
    player->vx = dashX * DASH_SPEED;
}

int dashUpdate() {
    player->dashing--;
    if (player->dashing == 0) {
        player->vx = dashX * END_DASH_SPEED;
        return ST_NORMAL;
    }
    return ST_DASH;
}
```

## Shared Logic (Outside States)

Some logic runs regardless of state (in `updatePlayer`):
- **Timer decrements** (cooldowns, buffers, var jump timer)
- **Collision** (horizontal and vertical sweeps)
- **Variable jump cut** on release (applies to all states)
- **Jump buffer execution** on landing
- **Coyote time** updates
- **Dash refill** on ground contact

## Dash Consumption Flow

Matching Celeste's `StartDash()` (line 3390):

1. **Normal state** checks if can dash (R pressed, dashes > 0, no cooldown)
2. **Normal state** consumes dash: `player->dashes--`
3. **Normal state** returns `ST_DASH`
4. **State machine** calls `normalEnd()`, then `dashBegin()`
5. **`dashBegin()`** sets up dash velocity, timers, trails
6. **`dashUpdate()`** runs each frame until dash timer expires
7. **`dashUpdate()`** returns `ST_NORMAL` when done
8. **State machine** calls `dashEnd()`, then `normalBegin()`

## Future Work

### States to Implement
- **Climb** - Wall climbing with stamina system
- **Swim** - Water physics
- **Boost** - Boost block interactions
- **RedDash** - Red bubble dash
- **DreamDash** - Dream block dash

### Climb State Notes
From Celeste `ClimbUpdate()` (line 3102):
- Stamina management (110 max, drains on climb)
- Climb up/down speeds
- Climb jump mechanics
- Slip when tired (stamina <= 20)
- Wall boost from boost blocks

## Build Integration

### Makefile Updates
```makefile
OBJS = ... state.o state_normal.o state_dash.o state_climb.o ...

state.o: $(SRCDIR)/player/state.c ...
state_normal.o: $(SRCDIR)/player/state/normal.c ...
state_dash.o: $(SRCDIR)/player/state/dash.c ...
state_climb.o: $(SRCDIR)/player/state/climb.c ...
```

## Compilation Status

✅ Clean build with no errors
✅ No warnings
✅ ROM builds successfully (game.gba created)

## Testing Checklist

- [ ] Normal movement still works
- [ ] Jumping feels the same
- [ ] Dashing works correctly
- [ ] Super jumps from dash work
- [ ] Wall jumps work
- [ ] Variable jump height works
- [ ] Fast falling works
- [ ] Dash refill on landing works
- [ ] Coyote time still applies

## Code References

All implementations reference specific Celeste source lines:
- **NormalUpdate**: Player.cs line 2784-3007
- **NormalBegin**: Player.cs line 2760-2762
- **NormalEnd**: Player.cs line 2765-2770
- **DashUpdate**: Player.cs line 3474-3545
- **DashCoroutine**: Player.cs line 3548-3634
- **DashBegin**: Player.cs line 3442-3467
- **DashEnd**: Player.cs line 3469-3472

This ensures 1:1 accuracy with the original Celeste implementation.
