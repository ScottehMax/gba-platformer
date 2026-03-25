#include "transition.h"
#include "menu/menu.h"
#include "core/game_math.h"
#include "generated/connections.h"
#include "player/state.h"

// ---------------------------------------------------------------------------
// Blend register constants (fade fallback)
// ---------------------------------------------------------------------------
#define BLDCNT_ALPHA   ((1 << 6) | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 13))
#define BLDALPHA_VAL   ((7 << 0) | (9 << 8))
#define BLDCNT_FADEBLK ((2 << 6) | 0x1F)

// Max pixels per frame during scroll — must be ≤ 16 (2 tiles) so the tilemap
// incremental update never needs to write into the visible region.
#define SCROLL_PX_PER_FRAME 8
#define FADE_FRAMES   12   // Duration of fade fallback
#define VRAM_TILE_LIMIT 512 // Char block 0 max (block 1 used by text)

// GBA screen dimensions
#define SCREEN_W 240
#define SCREEN_H 160

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
typedef enum {
    TRANS_NONE = 0,
    TRANS_SCROLL,
    TRANS_SCROLL_COMMIT,
    TRANS_FADE_OUT,
    TRANS_FADE_IN,
} TransPhase;

typedef struct {
    TransPhase phase;

    // --- scroll ---
    const Level* fromLevel;
    const Level* toLevel;
    int fromTileX0, fromTileY0;  // tile-unit origins in virtual space
    int toTileX0,   toTileY0;
    int fromTileVramOffset;
    int tileVramOffset;
    int seamPrefillAxis;
    int canReuseTilemapOnCommit;

    // Fixed-point ×256 camera position during scroll
    int virtualCamX256;
    int virtualCamY256;
    int virtualCamDX256;
    int virtualCamDY256;
    int virtualEndX256;
    int virtualEndY256;
    int scrollTimer;

    // --- both ---
    int targetLevelIdx;
    int newPlayerX;   // fixed-point ×256
    int newPlayerY;
    int newCameraX;   // pixel
    int newCameraY;

    // --- fade fallback ---
    int timer;
} TransState;

static TransState g_trans;
static int g_levelIdx  = -1;
static int g_cameraX   = 0;
static int g_cameraY   = 0;

#ifdef DESKTOP_BUILD
static const Level* const* s_overrideLevels = NULL;
static int s_overrideLevelCount = 0;
static const ScreenConnection* s_overrideConnections = NULL;
static int s_overrideConnectionCount = 0;
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static inline int clamp_val(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static int clampCameraXForLevel(const Level* level, int cameraX) {
    int maxCamX = level->width * 8 - SCREEN_W;
    if (maxCamX < 0) maxCamX = 0;
    return clamp_val(cameraX, 0, maxCamX);
}

static int clampCameraYForLevel(const Level* level, int cameraY) {
    int maxCamY = level->height * 8 - SCREEN_H;
    if (maxCamY < 0) maxCamY = 0;
    return clamp_val(cameraY, 0, maxCamY);
}

static int clampPlayerXForLevel(const Level* level, int playerX) {
    int minX = PLAYER_WIDTH / 2;
    int maxX = level->width * 8 - PLAYER_WIDTH / 2;
    if (maxX < minX) maxX = minX;
    return clamp_val(playerX, minX, maxX);
}

static int clampPlayerYForLevel(const Level* level, int playerY) {
    int minY = -PLAYER_TOP(0);
    int maxY = level->height * 8 - PLAYER_BOTTOM(0) - 1;
    if (maxY < minY) maxY = minY;
    return clamp_val(playerY, minY, maxY);
}

#ifdef DESKTOP_BUILD
void setTransitionTestOverrides(const Level* const* levels, int levelCount,
                                const ScreenConnection* connections, int connectionCount) {
    s_overrideLevels = (levels && levelCount > 0) ? levels : NULL;
    s_overrideLevelCount = (levels && levelCount > 0) ? levelCount : 0;
    s_overrideConnections = (connections && connectionCount > 0) ? connections : NULL;
    s_overrideConnectionCount = (connections && connectionCount > 0) ? connectionCount : 0;
}

void clearTransitionTestOverrides(void) {
    s_overrideLevels = NULL;
    s_overrideLevelCount = 0;
    s_overrideConnections = NULL;
    s_overrideConnectionCount = 0;
}
#endif

static const Level* getRegisteredLevel(int levelIdx) {
#ifdef DESKTOP_BUILD
    if (s_overrideLevels) {
        if (levelIdx < 0 || levelIdx >= s_overrideLevelCount) return NULL;
        return s_overrideLevels[levelIdx];
    }
#endif
    if (levelIdx < 0 || levelIdx >= LEVEL_COUNT) return NULL;
    return g_levels[levelIdx];
}

static const ScreenConnection* getRegisteredConnections(void) {
#ifdef DESKTOP_BUILD
    if (s_overrideConnections) return s_overrideConnections;
#endif
    return g_connections;
}

static int getRegisteredConnectionCount(void) {
#ifdef DESKTOP_BUILD
    if (s_overrideConnections) return s_overrideConnectionCount;
#endif
    return g_connectionCount;
}

static int chooseIncomingTileVramOffset(int currentOffset, int currentCount, int incomingCount) {
    if (incomingCount > VRAM_TILE_LIMIT) return -1;

    if (currentOffset > 0 && incomingCount <= currentOffset) {
        return 0;
    }

    int afterOffset = currentOffset + currentCount;
    if (afterOffset + incomingCount <= VRAM_TILE_LIMIT) {
        return afterOffset;
    }

    return -1;
}

static inline u16 getTileBAt(const Level* level, u8 layerIndex, int tileX, int tileY) {
    if (layerIndex >= level->layerCount) return 0;
    if (tileX < 0 || tileX >= level->width || tileY < 0 || tileY >= level->height) return 0;
    if (!g_levelBLayerTiles[layerIndex]) return 0;
    return g_levelBLayerTiles[layerIndex][tileY * level->width + tileX];
}

// ---------------------------------------------------------------------------
// Public: scroll tile entry (called by main.c tilemap loop)
// ---------------------------------------------------------------------------
u16 getScrollTileEntry(int layerIdx, int virtualTileX, int virtualTileY) {
    // From level?
    int fX = virtualTileX - g_trans.fromTileX0;
    int fY = virtualTileY - g_trans.fromTileY0;
    if (fX >= 0 && fX < g_trans.fromLevel->width &&
        fY >= 0 && fY < g_trans.fromLevel->height) {
        u16 tid = getTileAt(g_trans.fromLevel, (u8)layerIdx, fX, fY);
        u8  pal = (tid < g_trans.fromLevel->uniqueTileCount)
                  ? g_trans.fromLevel->tilePaletteBanks[tid] : 0;
        return (tid + (u16)g_trans.fromTileVramOffset) | ((u16)pal << 12);
    }
    // To level?
    int tX = virtualTileX - g_trans.toTileX0;
    int tY = virtualTileY - g_trans.toTileY0;
    if (tX >= 0 && tX < g_trans.toLevel->width &&
        tY >= 0 && tY < g_trans.toLevel->height) {
        u16 tid  = getTileBAt(g_trans.toLevel, (u8)layerIdx, tX, tY);
        if (tid == 0) {
            return 0;
        }
        u16 vtid = tid + (u16)g_trans.tileVramOffset;
        u8  pal  = (tid < g_trans.toLevel->uniqueTileCount)
                   ? g_trans.toLevel->tilePaletteBanks[tid] : 0;
        return vtid | ((u16)pal << 12);
    }
    return 0;
}

void getScrollTransInfo(ScrollTransInfo* out) {
    if (g_trans.phase == TRANS_SCROLL || g_trans.phase == TRANS_SCROLL_COMMIT) {
        out->active        = 1;
        out->fromLevel     = g_trans.fromLevel;
        out->toLevel       = g_trans.toLevel;
        out->fromTileX0    = g_trans.fromTileX0;
        out->fromTileY0    = g_trans.fromTileY0;
        out->toTileX0      = g_trans.toTileX0;
        out->toTileY0      = g_trans.toTileY0;
        out->tileVramOffset = g_trans.tileVramOffset;
        out->seamPrefillAxis = g_trans.seamPrefillAxis;
        out->canReuseTilemapOnCommit = g_trans.canReuseTilemapOnCommit;
    } else {
        out->active = 0;
        out->seamPrefillAxis = 0;
        out->canReuseTilemapOnCommit = 0;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void initTransition(void) {
    g_trans.phase = TRANS_NONE;
    g_trans.timer = 0;
    g_trans.seamPrefillAxis = 0;
    g_trans.canReuseTilemapOnCommit = 0;
    g_levelIdx    = -1;
}

void setTransitionLevelContext(int levelIdx, int cameraX, int cameraY, int playerX, int playerY) {
    (void)playerX;
    (void)playerY;

    g_levelIdx = levelIdx;
    g_cameraX  = cameraX;
    g_cameraY  = cameraY;
}

int tryTriggerTransition(const Level* level, int side, int perpPos) {
    if (!level || g_trans.phase != TRANS_NONE || g_levelIdx < 0) return 0;

    // Find matching connection
    const ScreenConnection* connections = getRegisteredConnections();
    int connectionCount = getRegisteredConnectionCount();
    const ScreenConnection* conn = NULL;
    for (int i = 0; i < connectionCount; i++) {
        if (connections[i].fromLevelIdx == (u8)g_levelIdx &&
            (int)connections[i].fromSide == side) {
            conn = &connections[i];
            break;
        }
    }
    if (!conn) return 0;

    const Level* fromLevel = level;
    const Level* toLevel   = getRegisteredLevel(conn->toLevelIdx);
    if (!toLevel) return 0;

    int newLevelW = toLevel->width  * 8;
    int newLevelH = toLevel->height * 8;
    int offset    = conn->posOffset;

    // Destination camera position (clamped to level B bounds)
    int newCameraX, newCameraY;
    switch ((ConnectionSide)conn->toSide) {
        case CONN_SIDE_LEFT:
            newCameraX = 0;
            newCameraY = clampCameraYForLevel(toLevel, g_cameraY + offset);
            g_trans.newPlayerX = clampPlayerXForLevel(toLevel, PLAYER_WIDTH / 2 + 12) << FIXED_SHIFT;
            g_trans.newPlayerY = clampPlayerYForLevel(toLevel, perpPos + offset) << FIXED_SHIFT;
            break;
        case CONN_SIDE_RIGHT:
            newCameraX = clampCameraXForLevel(toLevel, newLevelW - SCREEN_W);
            newCameraY = clampCameraYForLevel(toLevel, g_cameraY + offset);
            g_trans.newPlayerX = clampPlayerXForLevel(toLevel, newLevelW - PLAYER_WIDTH / 2 - 12) << FIXED_SHIFT;
            g_trans.newPlayerY = clampPlayerYForLevel(toLevel, perpPos + offset) << FIXED_SHIFT;
            break;
        case CONN_SIDE_TOP:
            newCameraX = clampCameraXForLevel(toLevel, g_cameraX + offset);
            newCameraY = 0;
            g_trans.newPlayerX = clampPlayerXForLevel(toLevel, perpPos + offset) << FIXED_SHIFT;
            g_trans.newPlayerY = clampPlayerYForLevel(toLevel, 5) << FIXED_SHIFT;
            break;
        case CONN_SIDE_BOTTOM:
            newCameraX = clampCameraXForLevel(toLevel, g_cameraX + offset);
            newCameraY = clampCameraYForLevel(toLevel, newLevelH - SCREEN_H);
            g_trans.newPlayerX = clampPlayerXForLevel(toLevel, perpPos + offset) << FIXED_SHIFT;
            g_trans.newPlayerY = clampPlayerYForLevel(toLevel, newLevelH - PLAYER_BOTTOM(0) - 2) << FIXED_SHIFT;
            break;
        default: return 0;
    }

    int scrollTileOffsetX = (g_cameraX >> 3) - (newCameraX >> 3);
    int scrollTileOffsetY = (g_cameraY >> 3) - (newCameraY >> 3);

    g_trans.targetLevelIdx = conn->toLevelIdx;
    g_trans.newCameraX     = newCameraX;
    g_trans.newCameraY     = newCameraY;

    // -----------------------------------------------------------------
    // Decide: scroll (if tiles fit) or fade fallback
    // -----------------------------------------------------------------
    int N_A = fromLevel->uniqueTileCount;
    int N_B = toLevel->uniqueTileCount;
    int fromTileVramOffset = getLevelTileVramOffset();
    int toTileVramOffset = chooseIncomingTileVramOffset(fromTileVramOffset, N_A, N_B);

    if (N_A + N_B <= VRAM_TILE_LIMIT && toTileVramOffset >= 0) {
        // ---- Scroll transition ----
        loadLevelBToVRAM(toLevel, toTileVramOffset);

        g_trans.fromLevel     = fromLevel;
        g_trans.toLevel       = toLevel;
        g_trans.fromTileVramOffset = fromTileVramOffset;
        g_trans.tileVramOffset = toTileVramOffset;

        // Virtual layout: levels laid out side-by-side along the scroll axis.
        // The perpendicular offset is tile-quantized here; the exact pixel
        // offset is still applied to the committed camera/player positions.
        int virtualStartX, virtualStartY; // camera start (pixels)
        int virtualEndX,   virtualEndY;   // camera end (pixels)

        g_trans.seamPrefillAxis = 0;
        g_trans.canReuseTilemapOnCommit = 0;

        switch ((ConnectionSide)conn->fromSide) {
            case CONN_SIDE_RIGHT:  // exits right → B to the right
                g_trans.fromTileX0 = 0;
                g_trans.fromTileY0 = 0;
                g_trans.toTileX0   = fromLevel->width;
                g_trans.toTileY0   = scrollTileOffsetY;
                g_trans.seamPrefillAxis = 1;
                virtualStartX = g_cameraX;
                virtualStartY = g_cameraY;
                virtualEndX   = fromLevel->width * 8 + newCameraX;
                virtualEndY   = g_cameraY;
                break;

            case CONN_SIDE_LEFT:   // exits left → B to the left
                g_trans.fromTileX0 = toLevel->width;
                g_trans.fromTileY0 = 0;
                g_trans.toTileX0   = 0;
                g_trans.toTileY0   = scrollTileOffsetY;
                g_trans.seamPrefillAxis = 1;
                virtualStartX = toLevel->width * 8 + g_cameraX;
                virtualStartY = g_cameraY;
                virtualEndX   = newCameraX;
                virtualEndY   = g_cameraY;
                break;

            case CONN_SIDE_BOTTOM: // exits bottom → B below
                g_trans.fromTileX0 = 0;
                g_trans.fromTileY0 = 0;
                g_trans.toTileX0   = scrollTileOffsetX;
                g_trans.toTileY0   = fromLevel->height;
                g_trans.seamPrefillAxis = 2;
                virtualStartX = g_cameraX;
                virtualStartY = g_cameraY;
                virtualEndX   = g_cameraX;
                virtualEndY   = fromLevel->height * 8 + newCameraY;
                break;

            case CONN_SIDE_TOP:    // exits top → B above
                g_trans.fromTileX0 = 0;
                g_trans.fromTileY0 = toLevel->height;
                g_trans.toTileX0   = scrollTileOffsetX;
                g_trans.toTileY0   = 0;
                g_trans.seamPrefillAxis = 2;
                virtualStartX = g_cameraX;
                virtualStartY = toLevel->height * 8 + g_cameraY;
                virtualEndX   = g_cameraX;
                virtualEndY   = newCameraY;
                break;

            default:
                // Fallthrough to fade
                goto do_fade;
        }

        int totalDX = virtualEndX - virtualStartX;
        int totalDY = virtualEndY - virtualStartY;
        // Choose frame count so neither axis exceeds SCROLL_PX_PER_FRAME per frame.
        int dist = totalDX < 0 ? -totalDX : totalDX;
        int distY = totalDY < 0 ? -totalDY : totalDY;
        if (distY > dist) dist = distY;
        int scrollFrames = (dist + SCROLL_PX_PER_FRAME - 1) / SCROLL_PX_PER_FRAME;
        if (scrollFrames < 1) scrollFrames = 1;
        g_trans.virtualCamX256  = virtualStartX << 8;
        g_trans.virtualCamY256  = virtualStartY << 8;
        g_trans.virtualCamDX256 = (totalDX << 8) / scrollFrames;
        g_trans.virtualCamDY256 = (totalDY << 8) / scrollFrames;
        g_trans.virtualEndX256  = virtualEndX << 8;
        g_trans.virtualEndY256  = virtualEndY << 8;
        g_trans.scrollTimer = scrollFrames;
        g_trans.canReuseTilemapOnCommit =
            (((virtualEndX >> 3) - g_trans.toTileX0) == (newCameraX >> 3)) &&
            (((virtualEndY >> 3) - g_trans.toTileY0) == (newCameraY >> 3));

        g_trans.phase = TRANS_SCROLL;
        return 1;
    }

do_fade:
    // ---- Fade fallback ----
    g_trans.phase = TRANS_FADE_OUT;
    g_trans.timer = FADE_FRAMES;
#ifndef DESKTOP_BUILD
    REG_BLDCNT = BLDCNT_FADEBLK;
    REG_BLDY   = 0;
#endif
    return 1;
}

int updateTransition(Player* player, Camera* camera) {
    if (g_trans.phase == TRANS_SCROLL) {
        g_trans.scrollTimer--;

        // Advance camera every frame including the last, so it reaches virtualEndX
        // exactly and the final frame has pixel-perfect visual continuity.
        g_trans.virtualCamX256 += g_trans.virtualCamDX256;
        g_trans.virtualCamY256 += g_trans.virtualCamDY256;
        camera->x = g_trans.virtualCamX256 >> 8;
        camera->y = g_trans.virtualCamY256 >> 8;

        if (g_trans.scrollTimer <= 0) {
            // Keep the final scrolled frame visible for one whole frame, then
            // commit the level swap on the next fresh VBlank.
            g_trans.virtualCamX256 = g_trans.virtualEndX256;
            g_trans.virtualCamY256 = g_trans.virtualEndY256;
            camera->x = g_trans.virtualCamX256 >> 8;
            camera->y = g_trans.virtualCamY256 >> 8;
            g_trans.phase = TRANS_SCROLL_COMMIT;
            return 1;
        }

        return 1;
    }

    if (g_trans.phase == TRANS_SCROLL_COMMIT) {
        loadLevelForTransition(g_trans.targetLevelIdx);
        setLevelTileVramOffset(g_trans.tileVramOffset);
        g_levelIdx = g_trans.targetLevelIdx;

        player->x  = g_trans.newPlayerX;
        player->y  = g_trans.newPlayerY;
        player->onGround           = 0;
        player->stateMachine.state = ST_NORMAL;
        player->dashing            = 0;

        camera->x = g_trans.newCameraX;
        camera->y = g_trans.newCameraY;

        g_trans.phase = TRANS_NONE;
        return 0;
    }

    if (g_trans.phase == TRANS_FADE_OUT) {
        g_trans.timer--;
        int brightness = ((FADE_FRAMES - g_trans.timer) * 16) / FADE_FRAMES;
        if (brightness > 16) brightness = 16;
#ifndef DESKTOP_BUILD
        REG_BLDY = (u16)brightness;
#endif

        if (g_trans.timer <= 0) {
#ifndef DESKTOP_BUILD
            REG_BLDY = 16;
#endif
            loadLevelForTransition(g_trans.targetLevelIdx);
            setLevelTileVramOffset(0);
            g_levelIdx = g_trans.targetLevelIdx;

            player->x  = g_trans.newPlayerX;
            player->y  = g_trans.newPlayerY;
            player->onGround           = 0;
            player->stateMachine.state = ST_NORMAL;
            player->dashing            = 0;

            camera->x = g_trans.newCameraX;
            camera->y = g_trans.newCameraY;

            g_trans.phase = TRANS_FADE_IN;
            g_trans.timer = FADE_FRAMES;
        }
        return 1;
    }

    if (g_trans.phase == TRANS_FADE_IN) {
        g_trans.timer--;
        int brightness = (g_trans.timer * 16) / FADE_FRAMES;
        if (brightness > 16) brightness = 16;
#ifndef DESKTOP_BUILD
        REG_BLDY = (u16)brightness;
#endif

        if (g_trans.timer <= 0) {
#ifndef DESKTOP_BUILD
            REG_BLDY     = 0;
            REG_BLDCNT   = BLDCNT_ALPHA;
            REG_BLDALPHA = BLDALPHA_VAL;
#endif
            g_trans.phase = TRANS_NONE;
            return 0;
        }
        return 1;
    }

    return 0;
}

int isTransitioning(void) {
    return g_trans.phase != TRANS_NONE;
}

void getTransitionVirtualCamera(int* outX, int* outY) {
    if (outX) *outX = g_trans.virtualCamX256 >> 8;
    if (outY) *outY = g_trans.virtualCamY256 >> 8;
}

void consumeTransitionSeamPrefill(void) {
    g_trans.seamPrefillAxis = 0;
}
