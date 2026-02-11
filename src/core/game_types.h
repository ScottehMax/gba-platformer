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
    float vx; // Floating-point
    float vy; // Floating-point
    float maxFall; // Current max fall speed (ramps for fast fall)
    int onGround;
    int coyoteTime;   // Frames remaining for coyote time jump
    int jumpBuffer;   // Frames remaining in jump buffer
    int jumpHeld;     // 1 if jump was initiated this aerial phase
    float varJumpSpeed; // Initial jump velocity for var jump clamping
    int varJumpTimer;   // Frames remaining for variable jump window
    int dashing;
    int dashes;                // Current dashes available (0-2)
    int maxDashes;             // Maximum dashes (usually 1)
    int dashCooldownTimer;     // Frames before can dash again
    int dashRefillCooldownTimer;  // Frames before dash refills on ground
    int facingRight;  // 1 = right, 0 = left
    u16 prevKeys;     // Previous frame keys for detecting button presses

    // Wall slide/jump
    float wallSlideTimer;  // Time remaining for wall slide ability (resets on ground/jump)
    int wallSlideDir;      // Direction of current wall slide: 1=right, -1=left, 0=none

    // Dash attack (for super jumps)
    int dashAttackTimer;   // Frames remaining where super jumps can be triggered (set on dash start)
    int dashDirX;          // Dash direction X: -1=left, 0=none, 1=right
    int dashDirY;          // Dash direction Y: -1=up, 0=none, 1=down
    float beforeDashSpeedX;  // Horizontal speed before dash (for preserving higher speeds)
    int ducking;           // 1 if player is ducking (affects super jump height)

    // Climbing
    float stamina;         // Current stamina (0-110)
    int climbNoMoveTimer;  // Frames before climb movement is allowed (prevents grab spam)
    int lastClimbMove;     // Last climb movement direction: -1=up, 0=still, 1=down
    int wallBoostTimer;    // Frames remaining to convert climb jump to wall jump (refunds stamina)
    int wallBoostDir;      // Direction to press for wall boost: -1=left, 1=right

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
