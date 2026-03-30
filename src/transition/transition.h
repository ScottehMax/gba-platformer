#ifndef TRANSITION_H
#define TRANSITION_H

#include "core/game_types.h"
#include "camera/camera.h"
#include "level/level.h"

// Side identifiers for level connections
typedef enum {
    CONN_SIDE_RIGHT  = 0,
    CONN_SIDE_LEFT   = 1,
    CONN_SIDE_BOTTOM = 2,
    CONN_SIDE_TOP    = 3,
} ConnectionSide;

// A connection between two level screens.
// When the player exits fromLevel on fromSide at a perpendicular position perpPos,
// a transition triggers if fromStart <= perpPos < fromEnd.
// The destination perpendicular position is: perpPos - fromStart + toStart.
typedef struct {
    u8 fromLevelIdx;
    u8 toLevelIdx;
    ConnectionSide fromSide;
    ConnectionSide toSide;
    s16 fromStart;  // Range start on from-side perp axis (local px, inclusive)
    s16 fromEnd;    // Range end on from-side perp axis (local px, exclusive)
    s16 toStart;    // Corresponding start on to-side perp axis (local px)
} ScreenConnection;

// ---------------------------------------------------------------------------
// Scroll transition info (read by main.c's tilemap loop)
// ---------------------------------------------------------------------------
typedef struct {
    int active;                  // 1 while scroll animation is running
    const Level* fromLevel;
    const Level* toLevel;
    int fromTileX0, fromTileY0;  // from-level origin in virtual tile space
    int toTileX0,   toTileY0;    // to-level origin in virtual tile space
    int tileVramOffset;          // add to toLevel tile IDs → VRAM index
    int seamPrefillAxis;         // 0=none, 1=prefill columns, 2=prefill rows
    int canReuseTilemapOnCommit; // 1 if main can rotate/reuse the scroll tilemap on commit
} ScrollTransInfo;

// Fill *out with current scroll state. active=0 when not scrolling.
void getScrollTransInfo(ScrollTransInfo* out);

// Get tile map entry (tileId | palBank<<12) for a virtual tile during scroll.
u16 getScrollTileEntry(int layerIdx, int virtualTileX, int virtualTileY);

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Initialize the transition system.
void initTransition(void);

// Call once per frame, before physics runs.
// cameraX/Y: current camera pixel position; playerX/Y: fixed-point player position.
void setTransitionLevelContext(int levelIdx, int cameraX, int cameraY, int playerX, int playerY);

// Called from collision code when the player would be clamped at a level boundary.
// Returns 1 if a transition was started (caller must NOT clamp the player).
// Returns 0 if no connection found (caller clamps normally).
int tryTriggerTransition(const Level* level, int side, int perpPos, Player* player);

// Update the active transition. Call once per frame.
// Returns 1 if a transition is in progress (caller should skip physics/camera update).
// Returns 0 when idle.
int updateTransition(Player* player, Camera* camera);

// Returns 1 if a screen transition is currently in progress.
int isTransitioning(void);

// Returns the current virtual camera position during a scroll transition.
// On the trigger frame, camera.x hasn't been synced yet — use this to get the
// correct virtual start before the first tilemap refresh.
void getTransitionVirtualCamera(int* outX, int* outY);

// Called by main.c after it has prefilled the transition seam columns/rows.
void consumeTransitionSeamPrefill(void);

#ifdef DESKTOP_BUILD
// Desktop-only test hooks for overriding the generated level/connection tables.
void setTransitionTestOverrides(const Level* const* levels, int levelCount,
                                const ScreenConnection* connections, int connectionCount);
void clearTransitionTestOverrides(void);
#endif

#endif // TRANSITION_H
