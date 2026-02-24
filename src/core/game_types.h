#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#ifdef DESKTOP_BUILD
#include "desktop/desktop_stubs.h"
#else
#include <tonc.h>
#endif

#include "game_math.h"

// Forward declarations
typedef struct Player Player;

// State callback structure (using void* to avoid circular dependencies)
typedef struct {
    void* update;   // int (*)(Player*, u16, const Level*)
    void* begin;    // void (*)(Player*)
    void* end;      // void (*)(Player*)
} StateCallbacks;

// State machine structure
typedef struct {
    int state;
    int previousState;
    StateCallbacks callbacks[10];  // Max 10 states for now
} StateMachine;

// Player structure
struct Player {
    int x;  // Fixed-point
    int y;  // Fixed-point
    int vx; // Fixed-point
    int vy; // Fixed-point
    int maxFall; // Current max fall speed (ramps for fast fall)
    int onGround;
    int wasOnGround;  // Previous frame onGround (for lift boost detection)
    int coyoteTime;   // Frames remaining for coyote time jump
    int jumpBuffer;   // Frames remaining in jump buffer
    int jumpHeld;     // 1 if jump was initiated this aerial phase
    int autoJump;     // 1 if AutoJump is active (simulates holding jump button for maintained jump height)
    int autoJumpTimer;  // Frames remaining for AutoJump window
    int liftBoostX; // Velocity from moving platforms (X component)
    int liftBoostY; // Velocity from moving platforms (Y component)
    int varJumpSpeed; // Initial jump velocity for var jump clamping
    int varJumpTimer;   // Frames remaining for variable jump window
    int dashing;
    int dashes;                // Current dashes available (0-2)
    int maxDashes;             // Maximum dashes (usually 1)
    int dashCooldownTimer;     // Frames before can dash again
    int dashRefillCooldownTimer;  // Frames before dash refills on ground
    int facingRight;  // 1 = right, 0 = left
    u16 prevKeys;     // Previous frame keys for detecting button presses

    // Wall slide/jump
    int wallSlideTimer;  // Time remaining for wall slide ability (resets on ground/jump)
    int wallSlideDir;      // Direction of current wall slide: 1=right, -1=left, 0=none

    // Dash attack (for super jumps)
    int dashAttackTimer;   // Frames remaining where super jumps can be triggered (set on dash start)
    int dashDirX;          // Dash direction X: -1=left, 0=none, 1=right
    int dashDirY;          // Dash direction Y: -1=up, 0=none, 1=down
    int beforeDashSpeedX;  // Horizontal speed before dash (for preserving higher speeds)
    int ducking;           // 1 if player is ducking (affects super jump height)
    int lastAimX;          // Last aim direction X: -256=left, 0=none, 256=right (fixed-point)
    int lastAimY;          // Last aim direction Y: -256=up, 0=none, 256=down (fixed-point)

    // Climbing
    int stamina;         // Current stamina (0-28160 in fixed-point, 0-110 logical)
    int climbNoMoveTimer;  // Frames before climb movement is allowed (prevents grab spam)
    int lastClimbMove;     // Last climb movement direction: -1=up, 0=still, 1=down
    int wallBoostTimer;    // Frames remaining to convert climb jump to wall jump (refunds stamina)
    int wallBoostDir;      // Direction to press for wall boost: -1=left, 1=right

    // Climb hop system (Celeste Player.cs line 218-219)
    int hopWaitX;          // If you climb hop onto a solid, snap beside it until you get above it
    int hopWaitXSpeed;   // Horizontal speed to apply when hopWaitX releases
    int forceMoveX;        // Force horizontal input to this value
    int forceMoveXTimer; // Frames remaining to force horizontal input

    // HitSquash state (Celeste Player.cs line 3936)
    int hitSquashNoMoveTimer;  // Frames remaining before can control movement after wall hit

    // Boost state (Celeste Player.cs line 3767-3768)
    int boostTargetX;  // Target position X in fixed-point
    int boostTargetY;  // Target position Y in fixed-point
    int boostRed;      // 1 if red bubble (triggers RedDash), 0 if green (triggers Dash)
    int boostTimer;    // Frames remaining in boost (auto-dash when reaches 0)
    int currentBubbleX;  // X position of bubble player is using (in pixels, for CurrentBooster check)
    int currentBubbleY;  // Y position of bubble player is using (in pixels, for CurrentBooster check)

    // Dash trail
    int trailX[TRAIL_LENGTH];  // Fixed-point positions
    int trailY[TRAIL_LENGTH];
    int trailFacing[TRAIL_LENGTH];
    int trailIndex;  // Current trail buffer index
    int trailTimer;  // Frames since last trail update
    int trailFadeTimer;  // Frames since dash ended (for gradual fade)

    // State machine
    StateMachine stateMachine;
};

typedef struct {
    int x;  // Camera X in pixels
    int y;  // Camera Y in pixels
} Camera;

#endif
