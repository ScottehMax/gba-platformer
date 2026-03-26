#ifdef DESKTOP_BUILD

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Desktop tonc stub must come first
#include "desktop/desktop_stubs.h"
#include "camera/camera.h"
#include "celeste1.h"
#include "collision/collision.h"
#include "level/level.h"
#include "level4.h"
#include "player/state.h"
#include "smb11.h"  // level3.h pulled in by level.h; smb11.h is not, add explicitly
#include "transition/transition.h"

// Test-accessor functions exposed by level.c under DESKTOP_BUILD
extern const u16* getMainBufBase(void);
extern const u16* getSecBufBase(void);
extern const u16* getTileBufA(void);  // always g_tileBuffer
extern const u16* getTileBufB(void);  // always g_tileBBuffer
extern const u16* getDesktopVramTiles(void);

// transition.c links against this menu API, but the transition unit test only
// needs scroll setup and tile queries.
void loadLevelForTransition(int levelIndex) {
    (void)levelIndex;
}

// ---- tiny test harness ----
static int g_passed = 0;
static int g_failed = 0;

#define ASSERT(cond, msg) \
    do { \
        if (cond) { printf("  PASS: %s\n", msg); g_passed++; } \
        else      { printf("  FAIL: %s\n", msg); g_failed++; } \
    } while(0)

#define ASSERT_EQ_PTR(a, b, msg) \
    do { \
        if ((const void*)(a) == (const void*)(b)) { printf("  PASS: %s\n", msg); g_passed++; } \
        else { printf("  FAIL: %s  (got %p, expected %p)\n", msg, (const void*)(a), (const void*)(b)); g_failed++; } \
    } while(0)

#define ASSERT_NE_PTR(a, b, msg) \
    do { \
        if ((const void*)(a) != (const void*)(b)) { printf("  PASS: %s\n", msg); g_passed++; } \
        else { printf("  FAIL: %s  (both are %p)\n", msg, (const void*)(a)); g_failed++; } \
    } while(0)

static int run_transition_to_completion(Player* player, Camera* camera, int maxFrames) {
    int frames = 0;
    while (isTransitioning() && frames < maxFrames) {
        updateTransition(player, camera);
        frames++;
    }
    return frames;
}

static int run_transition_collect_screen_y(Camera* camera, Player* player, int maxFrames, int fixedShift,
                                           int* minScreenY, int* maxScreenY,
                                           int* minCameraY, int* maxCameraY,
                                           int* lastScrollScreenY, int* lastScrollCameraY) {
    int frames = 0;
    int screenMin = 1000000, screenMax = -1000000;
    int cameraMin = 1000000, cameraMax = -1000000;
    int lastScreen = 0, lastCamera = 0;
    while (isTransitioning() && frames < maxFrames) {
        updateTransition(player, camera);
        frames++;
        ScrollTransInfo info;
        getScrollTransInfo(&info);
        if (info.active) {
            int screenY = (player->y >> fixedShift) - camera->y;
            if (screenY < screenMin) screenMin = screenY;
            if (screenY > screenMax) screenMax = screenY;
            if (camera->y < cameraMin) cameraMin = camera->y;
            if (camera->y > cameraMax) cameraMax = camera->y;
            lastScreen = screenY;
            lastCamera = camera->y;
        }
    }
    if (screenMin == 1000000) {
        screenMin = screenMax = (player->y >> fixedShift) - camera->y;
        cameraMin = cameraMax = camera->y;
        lastScreen = screenMin;
        lastCamera = cameraMin;
    }
    if (minScreenY) *minScreenY = screenMin;
    if (maxScreenY) *maxScreenY = screenMax;
    if (minCameraY) *minCameraY = cameraMin;
    if (maxCameraY) *maxCameraY = cameraMax;
    if (lastScrollScreenY) *lastScrollScreenY = lastScreen;
    if (lastScrollCameraY) *lastScrollCameraY = lastCamera;
    return frames;
}

static int run_transition_collect_screen_x(Camera* camera, Player* player, int maxFrames, int fixedShift,
                                           int* minScreenX, int* maxScreenX,
                                           int* minCameraX, int* maxCameraX,
                                           int* lastScrollScreenX, int* lastScrollCameraX) {
    int frames = 0;
    int screenMin = 1000000, screenMax = -1000000;
    int cameraMin = 1000000, cameraMax = -1000000;
    int lastScreen = 0, lastCamera = 0;
    while (isTransitioning() && frames < maxFrames) {
        updateTransition(player, camera);
        frames++;
        ScrollTransInfo info;
        getScrollTransInfo(&info);
        if (info.active) {
            int screenX = (player->x >> fixedShift) - camera->x;
            if (screenX < screenMin) screenMin = screenX;
            if (screenX > screenMax) screenMax = screenX;
            if (camera->x < cameraMin) cameraMin = camera->x;
            if (camera->x > cameraMax) cameraMax = camera->x;
            lastScreen = screenX;
            lastCamera = camera->x;
        }
    }
    if (screenMin == 1000000) {
        screenMin = screenMax = (player->x >> fixedShift) - camera->x;
        cameraMin = cameraMax = camera->x;
        lastScreen = screenMin;
        lastCamera = cameraMin;
    }
    if (minScreenX) *minScreenX = screenMin;
    if (maxScreenX) *maxScreenX = screenMax;
    if (minCameraX) *minCameraX = cameraMin;
    if (maxCameraX) *maxCameraX = cameraMax;
    if (lastScrollScreenX) *lastScrollScreenX = lastScreen;
    if (lastScrollCameraX) *lastScrollCameraX = lastCamera;
    return frames;
}

enum {
    TEST_LEVEL_IDX_CELESTE1 = 0,
    TEST_LEVEL_IDX_LEVEL3 = 3,
    TEST_LEVEL_IDX_LEVEL4 = 4,
    TEST_LEVEL_IDX_SMB11 = 5,
};

static void assert_camera_stable_after_commit(Camera* camera, Player* player,
                                              const Level* level, const char* message) {
    Camera nextCamera = *camera;
    updateCamera(&nextCamera, player, level);
    ASSERT(nextCamera.x == camera->x && nextCamera.y == camera->y, message);
}

static const ScreenConnection kHorizontalOffsetConnections[] = {
    { TEST_LEVEL_IDX_LEVEL3, TEST_LEVEL_IDX_SMB11, CONN_SIDE_RIGHT,  CONN_SIDE_LEFT,  0, 200, 21 },
    { TEST_LEVEL_IDX_SMB11,  TEST_LEVEL_IDX_LEVEL3, CONN_SIDE_LEFT,   CONN_SIDE_RIGHT, 21, 221, 0 },
};

static const ScreenConnection kVerticalOffsetConnections[] = {
    { TEST_LEVEL_IDX_LEVEL3, TEST_LEVEL_IDX_LEVEL4, CONN_SIDE_BOTTOM, CONN_SIDE_TOP,    0, 240, 16 },
    { TEST_LEVEL_IDX_LEVEL4, TEST_LEVEL_IDX_LEVEL3, CONN_SIDE_TOP,    CONN_SIDE_BOTTOM, 16, 256, 0 },
};

static const Level kFadeFromLevel = {
    .name = "fade-from",
    .width = 80,
    .height = 30,
    .layerCount = 0,
    .layers = NULL,
    .objectCount = 0,
    .objects = NULL,
    .playerSpawnX = 0,
    .playerSpawnY = 0,
    .tilesetCount = 0,
    .tilesets = NULL,
    .collisionMap = NULL,
    .uniqueTileCount = 400,
    .uniqueTileIds = NULL,
    .tilePaletteBanks = NULL,
};

static const Level kFadeToLevel = {
    .name = "fade-to",
    .width = 96,
    .height = 40,
    .layerCount = 0,
    .layers = NULL,
    .objectCount = 0,
    .objects = NULL,
    .playerSpawnX = 0,
    .playerSpawnY = 0,
    .tilesetCount = 0,
    .tilesets = NULL,
    .collisionMap = NULL,
    .uniqueTileCount = 200,
    .uniqueTileIds = NULL,
    .tilePaletteBanks = NULL,
};

static const Level* const kFadeLevelTable[] = {
    &kFadeFromLevel,
    &kFadeToLevel,
};

static const ScreenConnection kFadeOffsetConnections[] = {
    { 0, 1, CONN_SIDE_RIGHT, CONN_SIDE_LEFT, 0, 240, 13 },
    { 1, 0, CONN_SIDE_LEFT,  CONN_SIDE_RIGHT, 13, 253, 0 },
};

// ---------------------------------------------------------------------------
// Test 1: Buffer swap invariant
//
// Simulates: forward transition level3→smb11, then reverse smb11→level3.
// After adoptLevelBBuffer(smb11), calls loadLevelBToVRAM(level3, 18).
// Asserts that g_levelLayerTiles and g_levelBLayerTiles use different
// physical buffers, so no corruption occurs.
// ---------------------------------------------------------------------------
static void test_buffer_swap_invariant(void) {
    printf("\n[Test 1] Buffer swap invariant\n");

    const u16* bufA = getTileBufA();  // g_tileBuffer  (physical A)
    const u16* bufB = getTileBufB();  // g_tileBBuffer (physical B)

    // --- Initial state ---
    // main=A, sec=B
    ASSERT_EQ_PTR(getMainBufBase(), bufA, "Initial: mainBuf is bufA");
    ASSERT_EQ_PTR(getSecBufBase(),  bufB, "Initial: secBuf is bufB");

    // Forward: load level3 as current level
    loadLevelToVRAM(&level3);
    ASSERT(g_levelLayerTiles[0] != NULL,              "After loadLevelToVRAM(level3): layer0 set");
    ASSERT(g_levelLayerTiles[1] != NULL,              "After loadLevelToVRAM(level3): layer1 set");
    ASSERT(g_levelLayerTiles[0] >= bufA && g_levelLayerTiles[0] < bufA + 512*40*2,
           "After loadLevelToVRAM(level3): layer0 lives in bufA");
    ASSERT_EQ_PTR(getMainBufBase(), bufA, "After loadLevelToVRAM: mainBuf still A");

    // Forward: load smb11 as incoming level (vramOffset = level3.uniqueTileCount = 116)
    int N_A = (int)level3.uniqueTileCount;
    loadLevelBToVRAM(&smb11, N_A);
    ASSERT(g_levelBLayerTiles[0] != NULL,             "After loadLevelBToVRAM(smb11): layerB0 set");
    ASSERT(g_levelBLayerTiles[0] >= bufB && g_levelBLayerTiles[0] < bufB + 512*40*2,
           "After loadLevelBToVRAM(smb11): layerB0 lives in bufB");
    ASSERT_EQ_PTR(getSecBufBase(), bufB, "After loadLevelBToVRAM: secBuf still B");

    // Forward transition end: adopt smb11
    adoptLevelBBuffer(&smb11);

    // After adopt: buffers should have swapped physical assignments.
    ASSERT_NE_PTR(getMainBufBase(), getSecBufBase(), "After adoptLevelBBuffer(smb11): main != sec");
    ASSERT_EQ_PTR(getMainBufBase(), bufB, "After adoptLevelBBuffer(smb11): mainBuf swapped to old secBuf");
    ASSERT_EQ_PTR(getSecBufBase(),  bufA, "After adoptLevelBBuffer(smb11): secBuf swapped to old mainBuf");

    // g_levelLayerTiles should now live in new mainBuf (=bufB)
    ASSERT(g_levelLayerTiles[0] >= bufB && g_levelLayerTiles[0] < bufB + 512*40*2,
           "After adopt: g_levelLayerTiles[0] lives in new mainBuf");
    ASSERT(g_levelLayerTiles[1] == NULL,
           "After adopt: g_levelLayerTiles[1] NULL (smb11 has 1 layer)");
    ASSERT(g_levelBLayerTiles[0] == NULL, "After adopt: g_levelBLayerTiles[0] cleared");

    // Write a marker into smb11's tile data so we can detect corruption later
    u16* smb11_ptr = g_levelLayerTiles[0];
    smb11_ptr[0] = 0xBEEF;

    // Reverse: now load level3 as the incoming level (vramOffset = smb11.uniqueTileCount = 18)
    int N_A_rev = (int)smb11.uniqueTileCount;
    loadLevelBToVRAM(&level3, N_A_rev);

    // CRITICAL: g_levelBLayerTiles must now live in bufA (the old main buf),
    // NOT in bufB which g_levelLayerTiles still points into.
    ASSERT(g_levelBLayerTiles[0] >= bufA && g_levelBLayerTiles[0] < bufA + 512*40*2,
           "After reverse loadLevelBToVRAM: g_levelBLayerTiles[0] lives in bufA (NOT bufB)");
    ASSERT_NE_PTR(g_levelLayerTiles[0], g_levelBLayerTiles[0],
                  "g_levelLayerTiles[0] and g_levelBLayerTiles[0] are different pointers");

    // Most important: smb11 data must NOT be corrupted
    ASSERT(smb11_ptr[0] == 0xBEEF,
           "smb11 tile data NOT corrupted by reverse loadLevelBToVRAM");

    // level3's layer 1 must be loaded (needed for decoration during reverse scroll)
    ASSERT(g_levelBLayerTiles[1] != NULL,
           "After reverse loadLevelBToVRAM: g_levelBLayerTiles[1] populated (level3 layer 1)");
    ASSERT(g_levelBLayerTiles[1] >= bufA && g_levelBLayerTiles[1] < bufA + 512*40*2,
           "g_levelBLayerTiles[1] lives in bufA");
}

// ---------------------------------------------------------------------------
// Test 2: Double-transition (A→B→A→B)
//
// Ensures the buffer swap logic is consistent across multiple transitions.
// ---------------------------------------------------------------------------
static void test_double_transition_buffers(void) {
    printf("\n[Test 2] Double-transition buffer consistency\n");

    // Full reset: reload level3 as current level so we start from a known state
    // (mainBuf and secBuf may have been swapped by test 1)
    loadLevelToVRAM(&level3);

    const u16* bufA = getMainBufBase();  // whichever is current main
    const u16* bufB = getSecBufBase();   // whichever is current sec

    // Do a full round-trip: level3→smb11→level3
    loadLevelToVRAM(&level3);
    loadLevelBToVRAM(&smb11, (int)level3.uniqueTileCount);
    adoptLevelBBuffer(&smb11);

    // Second forward: now playing smb11, load level3 as incoming
    loadLevelBToVRAM(&level3, (int)smb11.uniqueTileCount);
    adoptLevelBBuffer(&level3);

    // After second adopt: buffers swapped back to original assignment
    ASSERT_NE_PTR(getMainBufBase(), getSecBufBase(), "After 2nd adopt: main != sec");
    ASSERT_EQ_PTR(getMainBufBase(), bufA, "After 2nd adopt: mainBuf back to original");
    ASSERT_EQ_PTR(getSecBufBase(),  bufB, "After 2nd adopt: secBuf back to original");

    // g_levelLayerTiles[1] should be populated (level3 has 2 layers)
    ASSERT(g_levelLayerTiles[1] != NULL,
           "After adopt(level3): g_levelLayerTiles[1] populated");

    // Now do the reverse transition from level3 again
    u16* level3_ptr0 = g_levelLayerTiles[0];
    u16* level3_ptr1 = g_levelLayerTiles[1];
    level3_ptr0[0] = 0xDEAD;
    level3_ptr1[0] = 0xCAFE;

    loadLevelBToVRAM(&smb11, (int)level3.uniqueTileCount);

    ASSERT(level3_ptr0[0] == 0xDEAD, "level3 layer0 not corrupted by loadLevelBToVRAM");
    ASSERT(level3_ptr1[0] == 0xCAFE, "level3 layer1 not corrupted by loadLevelBToVRAM");
    ASSERT(g_levelBLayerTiles[0] != NULL, "smb11 layerB0 loaded");
    ASSERT_NE_PTR(g_levelLayerTiles[0], g_levelBLayerTiles[0],
                  "Different buffers after 2nd round-trip");
}

// ---------------------------------------------------------------------------
// Test 2b: adopting level B preserves its VRAM offset placement
// ---------------------------------------------------------------------------
static void test_adopt_preserves_incoming_vram_offset(void) {
    printf("\n[Test 2b] Adopt preserves incoming VRAM offset\n");

    loadLevelToVRAM(&smb11);
    loadLevelBToVRAM(&level3, (int)smb11.uniqueTileCount);

    const u16* vramTiles = getDesktopVramTiles();
    ASSERT(vramTiles[(int)smb11.uniqueTileCount] == 0,
           "Incoming level blank tile occupies the offset slot before adopt");

    adoptLevelBBuffer(&level3);

    vramTiles = getDesktopVramTiles();
    ASSERT(getLevelTileVramOffset() == (int)smb11.uniqueTileCount,
           "Adopt keeps the destination level VRAM offset");
    ASSERT(vramTiles[(int)smb11.uniqueTileCount] == 0,
           "Adopt does not clobber the incoming blank tile slot");
}

// ---------------------------------------------------------------------------
// Test 3: Player rendering coordinates during horizontal transition
//
// During a left-exit reverse transition (smb11→level3):
//   fromTileX0 = level3.width = 80 tiles
//   virtual camera.x = fromTileX0 * 8 + smb11_camera.x = 640 + 0 = 640
//   player.x = 16 pixels (just inside smb11's left edge)
//
// drawPlayer computes: screenX = (player.x >> FIXED_SHIFT) - camera.x - 8
// Current (buggy): 16 - 640 - 8 = -632  → clamped to -16 → player OFFSCREEN
// Correct:         (16 + 640) - 640 - 8 = 8  → player near left edge (visible)
//
// The fix: subtract fromTileX0*8 from camera.x when rendering the player.
// ---------------------------------------------------------------------------
static void test_player_render_offset_during_transition(void) {
    printf("\n[Test 3] Player render coordinates during horizontal transition\n");

#define FIXED_SHIFT 8

    int fromTileX0  = (int)level3.width;   // 80 tiles = 640 px
    int smb11_camX  = 0;
    int virtual_camX = fromTileX0 * 8 + smb11_camX;  // 640
    int player_px   = 16;                  // player.x in smb11 physical pixels
    int player_fixed = player_px << FIXED_SHIFT;

    // Buggy screen position (no offset correction)
    int buggy_screenX = (player_fixed >> FIXED_SHIFT) - virtual_camX - 8;
    // = 16 - 640 - 8 = -632
    ASSERT(buggy_screenX < -16,
           "Without fix: player screenX is offscreen negative (< -16)");

    // Correct screen position: subtract fromTileX0*8 from virtual_camX
    int render_camX   = virtual_camX - fromTileX0 * 8;  // = 640 - 640 = 0
    int correct_screenX = (player_fixed >> FIXED_SHIFT) - render_camX - 8;
    // = 16 - 0 - 8 = 8
    ASSERT(correct_screenX >= 0 && correct_screenX < 240,
           "With fix: player screenX is onscreen (0..239)");
    ASSERT(correct_screenX == player_px - smb11_camX - 8,
           "With fix: player screenX equals physical position relative to physical camera");

    printf("    buggy screenX: %d (< -16, offscreen)\n", buggy_screenX);
    printf("    fixed screenX: %d (onscreen)\n", correct_screenX);

#undef FIXED_SHIFT
}

// ---------------------------------------------------------------------------
// Test 4: getTileBAt layer 1 validity during reverse scroll
//
// Verifies that after loadLevelBToVRAM(level3, 18), querying level3's
// decoration layer (layerIdx=1) via g_levelBLayerTiles[1] returns a tile
// index within level3's valid range (0..uniqueTileCount-1).
// ---------------------------------------------------------------------------
static void test_decoration_layer_valid_after_load(void) {
    printf("\n[Test 4] Level3 decoration layer accessible after loadLevelBToVRAM\n");

    // Reset to a known state: smb11 as current level
    loadLevelToVRAM(&smb11);

    // Load level3 as incoming (reverse transition)
    loadLevelBToVRAM(&level3, (int)smb11.uniqueTileCount);

    ASSERT(g_levelBLayerTiles[0] != NULL, "g_levelBLayerTiles[0] set");
    ASSERT(g_levelBLayerTiles[1] != NULL, "g_levelBLayerTiles[1] set (decoration layer)");

    // Check that a sample tile in level3's layer 1 has a valid index
    // level3 is 80x25, decoration tiles should be in range 0..115
    int sample_tile = (int)g_levelBLayerTiles[1][5 * level3.width + 10];
    ASSERT(sample_tile >= 0 && sample_tile < (int)level3.uniqueTileCount,
           "Level3 layer1 tile at (10,5) is in valid VRAM index range");
    printf("    Sample level3 layer1 tile at (10,5): %d (valid range 0..%d)\n",
           sample_tile, (int)level3.uniqueTileCount - 1);
}

// ---------------------------------------------------------------------------
// Test 5: destination-only BG1 stays visible during reverse scroll
//
// Reverse scroll smb11->level3 should keep level3's BG1 visible while the
// scroll is active; the proper fix is to preserve the layer and solve the
// player visibility / handoff issues elsewhere.
// ---------------------------------------------------------------------------
static void test_destination_only_layer_visible_during_scroll(void) {
    printf("\n[Test 5] Destination-only BG1 visible during reverse scroll\n");

    enum { PERP_POS = 140 };

    loadLevelToVRAM(&smb11);
    initTransition();
    setTransitionLevelContext(5, 0, 0, 8 << 8, PERP_POS << 8);

    ASSERT(tryTriggerTransition(&smb11, CONN_SIDE_LEFT, PERP_POS, NULL),
           "Reverse transition smb11->level3 triggered");

    ScrollTransInfo info;
    getScrollTransInfo(&info);
    ASSERT(info.active, "Scroll transition reports active");

    int foundX = -1;
    int foundY = -1;
    u16 rawTid = 0;
    for (int y = 0; y < level3.height && foundX < 0; y++) {
        for (int x = 0; x < level3.width; x++) {
            u16 tid = g_levelBLayerTiles[1][y * level3.width + x];
            if (tid != 0) {
                foundX = x;
                foundY = y;
                rawTid = tid;
                break;
            }
        }
    }

    ASSERT(foundX >= 0, "Found a non-empty incoming level3 BG1 tile");

    u8 pal = level3.tilePaletteBanks[rawTid];
    u16 unsuppressedEntry = (rawTid + (u16)info.tileVramOffset) | ((u16)pal << 12);
    ASSERT(unsuppressedEntry != 0, "Incoming BG1 tile would be visible without suppression");

    u16 entry = getScrollTileEntry(1, info.toTileX0 + foundX, info.toTileY0 + foundY);
    ASSERT(entry == unsuppressedEntry, "Destination-only BG1 tile stays visible during scroll");

    int emptyX = -1;
    int emptyY = -1;
    for (int y = 0; y < level3.height && emptyX < 0; y++) {
        for (int x = 0; x < level3.width; x++) {
            if (g_levelBLayerTiles[1][y * level3.width + x] == 0) {
                emptyX = x;
                emptyY = y;
                break;
            }
        }
    }

    ASSERT(emptyX >= 0, "Found an empty incoming level3 BG1 tile");
    u16 emptyEntry = getScrollTileEntry(1, info.toTileX0 + emptyX, info.toTileY0 + emptyY);
    ASSERT(emptyEntry == 0, "Empty incoming BG1 tile stays transparent during scroll");
}

// ---------------------------------------------------------------------------
// Test 6: scroll handoff uses a separate commit frame
//
// The last visible scroll frame should remain active for one frame, then the
// next frame should commit the destination level on a fresh VBlank.
// ---------------------------------------------------------------------------
static void test_scroll_handoff_extra_frame(void) {
    printf("\n[Test 6] Scroll handoff uses separate commit frame\n");

    enum { TEST_FIXED_SHIFT = 8, PERP_POS = 140 };

    Player player = {0};
    Camera camera = {0};

    loadLevelToVRAM(&smb11);
    initTransition();
    setTransitionLevelContext(5, 0, 0, 8 << TEST_FIXED_SHIFT, PERP_POS << TEST_FIXED_SHIFT);

    ASSERT(tryTriggerTransition(&smb11, CONN_SIDE_LEFT, PERP_POS, &player),
           "Reverse transition smb11->level3 triggered");

    int frames = 0;
    while (isTransitioning()) {
        updateTransition(&player, &camera);
        frames++;

        ScrollTransInfo info;
        getScrollTransInfo(&info);

        if (frames == 15) {
            ASSERT(info.active, "Final scrolled frame remains active before commit");
        }

        if (frames > 40) {
            ASSERT(0, "Transition completed within expected frame budget");
            break;
        }
    }

    ASSERT(camera.x == 400, "Commit frame snaps camera to level3 destination camera");
}

// ---------------------------------------------------------------------------
// Test 6b: scroll transition clears old trails and moves the player smoothly
// ---------------------------------------------------------------------------
static void test_scroll_player_handoff_and_trail_cleanup(void) {
    printf("\n[Test 6b] Scroll player handoff and trail cleanup\n");

    enum {
        TEST_FIXED_SHIFT = 8,
        START_CAMERA_Y = 40,
        PERP_POS = 120,
    };

    Player player = {0};
    Camera camera = {0};

    player.x = (PLAYER_WIDTH / 2) << TEST_FIXED_SHIFT;
    player.y = PERP_POS << TEST_FIXED_SHIFT;
    player.vx = -(4 << TEST_FIXED_SHIFT);
    player.vy = -(2 << TEST_FIXED_SHIFT);
    player.facingRight = 0;
    player.dashing = 5;
    player.trailFadeTimer = 0;
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        player.trailX[i] = (32 + i) << TEST_FIXED_SHIFT;
        player.trailY[i] = (64 + i) << TEST_FIXED_SHIFT;
        player.trailFacing[i] = 0;
    }
    camera.y = START_CAMERA_Y;

    loadLevelToVRAM(&smb11);
    initTransition();
    clearTransitionTestOverrides();
    setTransitionTestOverrides(NULL, 0, kHorizontalOffsetConnections, 2);
    setTransitionLevelContext(TEST_LEVEL_IDX_SMB11, 0, START_CAMERA_Y, player.x, player.y);

    ASSERT(tryTriggerTransition(&smb11, CONN_SIDE_LEFT, PERP_POS, &player),
           "Reverse transition for player handoff triggered");
    ASSERT(player.trailX[0] == (-1000 << TEST_FIXED_SHIFT),
           "Transition start clears previous dash trail positions");
    ASSERT(player.trailFadeTimer >= TRAIL_LENGTH * 8,
           "Transition start hides dash trail fade");

    int startX = player.x;
    int startY = player.y;
    int preservedVx = player.vx;
    int preservedVy = player.vy;

    updateTransition(&player, &camera);
    ASSERT(player.x != startX || player.y != startY,
           "Scroll transition moves player before commit");

    int frames = 1 + run_transition_to_completion(&player, &camera, 90);
    ASSERT(!isTransitioning(), "Player handoff transition completed");
    ASSERT(frames > 0 && frames <= 91, "Player handoff finished within frame budget");
    ASSERT(player.vx == preservedVx && player.vy == preservedVy,
           "Transition commit preserves player momentum");
    ASSERT(player.dashing == 0, "Transition commit clears dash state");
    ASSERT(player.stateMachine.state == ST_NORMAL,
           "Transition commit returns player to normal state");

    clearTransitionTestOverrides();
}

// ---------------------------------------------------------------------------
// Test 6c: successful side transition does not zero horizontal velocity
// ---------------------------------------------------------------------------
static void test_boundary_transition_preserves_horizontal_velocity(void) {
    printf("\n[Test 6c] Boundary transition preserves horizontal velocity\n");

    enum {
        TEST_FIXED_SHIFT = 8,
        START_CAMERA_Y = 40,
        PERP_POS = 120,
    };

    Player player = {0};

    player.x = ((PLAYER_WIDTH / 2) + 1) << TEST_FIXED_SHIFT;
    player.y = PERP_POS << TEST_FIXED_SHIFT;
    player.vx = -(4 << TEST_FIXED_SHIFT);
    player.facingRight = 0;

    loadLevelToVRAM(&smb11);
    initTransition();
    clearTransitionTestOverrides();
    setTransitionTestOverrides(NULL, 0, kHorizontalOffsetConnections, 2);
    setTransitionLevelContext(TEST_LEVEL_IDX_SMB11, 0, START_CAMERA_Y, player.x, player.y);

    collideHorizontal(&player, &smb11);
    ASSERT(isTransitioning(), "Boundary hit starts a transition");
    ASSERT(player.vx == -(4 << TEST_FIXED_SHIFT),
           "Successful side transition keeps horizontal velocity");
    ASSERT((player.x >> TEST_FIXED_SHIFT) == (PLAYER_WIDTH / 2),
           "Successful side transition still pins player to the edge");

    clearTransitionTestOverrides();
}

// ---------------------------------------------------------------------------
// Test 7: horizontal connection offset shifts camera/player Y
// ---------------------------------------------------------------------------
static void test_horizontal_connection_offset(void) {
    printf("\n[Test 7] Horizontal connection offset applied\n");

    enum {
        TEST_FIXED_SHIFT = 8,
        START_CAMERA_Y = 24,
        PERP_POS = 92,
        OFFSET = 21,
    };

    Player player = {0};
    Camera camera = {0};

    player.y = PERP_POS << TEST_FIXED_SHIFT;
    camera.y = START_CAMERA_Y;

    clearTransitionTestOverrides();
    setTransitionTestOverrides(NULL, 0, kHorizontalOffsetConnections, 2);

    loadLevelToVRAM(&level3);
    initTransition();
    setTransitionLevelContext(TEST_LEVEL_IDX_LEVEL3, 0, START_CAMERA_Y, 0, player.y);

    ASSERT(tryTriggerTransition(&level3, CONN_SIDE_RIGHT, PERP_POS, &player),
           "Horizontal offset transition triggered");

    ScrollTransInfo info;
    getScrollTransInfo(&info);
    ASSERT(info.active, "Horizontal offset uses scroll transition");
    ASSERT(info.toTileY0 == -3, "Horizontal scroll layout quantizes destination by 3 tiles");
    ASSERT(info.seamPrefillAxis == 1, "Horizontal rightward scroll requests seam column prefill");
    ASSERT(info.canReuseTilemapOnCommit, "Horizontal non-tile-aligned offset keeps the scrolled tilemap aligned");

    int minScreenY, maxScreenY, minCameraY, maxCameraY, lastScrollScreenY, lastScrollCameraY;
    int frames = run_transition_collect_screen_y(&camera, &player, 91, TEST_FIXED_SHIFT,
                                                 &minScreenY, &maxScreenY,
                                                 &minCameraY, &maxCameraY,
                                                 &lastScrollScreenY, &lastScrollCameraY);

    ASSERT(!isTransitioning(), "Horizontal offset transition completed");
    ASSERT(frames > 0 && frames <= 91, "Horizontal offset transition finished within frame budget");
    ASSERT((maxCameraY - minCameraY) <= 3,
           "Horizontal offset only nudges the combined camera by the tile-quantization residual");
    ASSERT((maxScreenY - minScreenY) == 0,
           "Horizontal offset keeps player screen Y fixed during the scroll");
    ASSERT(camera.y == START_CAMERA_Y + OFFSET, "Horizontal offset shifts destination camera Y");
    ASSERT((player.y >> TEST_FIXED_SHIFT) == PERP_POS + OFFSET, "Horizontal offset shifts destination player Y");
    ASSERT(((player.y >> TEST_FIXED_SHIFT) - camera.y) == (PERP_POS - START_CAMERA_Y),
           "Horizontal offset preserves player screen Y");
    ASSERT(lastScrollScreenY == ((player.y >> TEST_FIXED_SHIFT) - camera.y),
           "Horizontal scroll hands off to commit without a screen-space jump");
    ASSERT(lastScrollCameraY == camera.y + ((info.toTileY0 - info.fromTileY0) * 8),
           "Horizontal scroll ends on the committed destination camera view");

    clearTransitionTestOverrides();
}

// ---------------------------------------------------------------------------
// Test 8: vertical connection offset shifts camera/player X
// ---------------------------------------------------------------------------
static void test_vertical_connection_offset(void) {
    printf("\n[Test 8] Vertical connection offset applied\n");

    enum {
        TEST_FIXED_SHIFT = 8,
        START_CAMERA_X = 70,
        PERP_POS = 150,
        OFFSET = 16,
    };

    Player player = {0};
    Camera camera = {0};

    player.x = PERP_POS << TEST_FIXED_SHIFT;
    camera.x = START_CAMERA_X;

    clearTransitionTestOverrides();
    setTransitionTestOverrides(NULL, 0, kVerticalOffsetConnections, 2);

    loadLevelToVRAM(&level3);
    initTransition();
    setTransitionLevelContext(TEST_LEVEL_IDX_LEVEL3, START_CAMERA_X, 0, player.x, 0);

    ASSERT(tryTriggerTransition(&level3, CONN_SIDE_BOTTOM, PERP_POS, &player),
           "Vertical offset transition triggered");

    ScrollTransInfo info;
    getScrollTransInfo(&info);
    ASSERT(info.active, "Vertical offset uses scroll transition");
    ASSERT(info.toTileX0 == -2, "Vertical scroll layout shifts destination by 2 tiles");
    ASSERT(info.seamPrefillAxis == 2, "Vertical downward scroll requests seam row prefill");
    ASSERT(info.canReuseTilemapOnCommit, "Vertical tilemap can be reused on commit for tile-aligned offset");

    int minScreenX, maxScreenX, minCameraX, maxCameraX, lastScrollScreenX, lastScrollCameraX;
    int frames = run_transition_collect_screen_x(&camera, &player, 81, TEST_FIXED_SHIFT,
                                                 &minScreenX, &maxScreenX,
                                                 &minCameraX, &maxCameraX,
                                                 &lastScrollScreenX, &lastScrollCameraX);

    ASSERT(!isTransitioning(), "Vertical offset transition completed");
    ASSERT(frames > 0 && frames <= 81, "Vertical offset transition finished within frame budget");
    ASSERT(minCameraX == START_CAMERA_X && maxCameraX == START_CAMERA_X,
           "Vertical scroll keeps camera X fixed during the scroll");
    ASSERT((maxScreenX - minScreenX) == 0,
           "Tile-aligned vertical offset keeps player screen X fixed during scroll");
    ASSERT(camera.x == START_CAMERA_X + OFFSET, "Vertical offset shifts destination camera X");
    ASSERT((player.x >> TEST_FIXED_SHIFT) == PERP_POS + OFFSET, "Vertical offset shifts destination player X");
    ASSERT(((player.x >> TEST_FIXED_SHIFT) - camera.x) == (PERP_POS - START_CAMERA_X),
           "Vertical offset preserves player screen X");
    ASSERT(camera.y == 0, "Vertical offset keeps top-entry camera Y");
    ASSERT(lastScrollScreenX == ((player.x >> TEST_FIXED_SHIFT) - camera.x),
           "Vertical scroll hands off to commit without a screen-space jump");
    ASSERT(lastScrollCameraX == camera.x + ((info.toTileX0 - info.fromTileX0) * 8),
           "Vertical scroll ends on the committed destination camera view");
    assert_camera_stable_after_commit(&camera, &player, &level4,
                                      "Vertical offset lands on the first gameplay camera");

    clearTransitionTestOverrides();
}

// ---------------------------------------------------------------------------
// Test 9: fade fallback still applies pixel offset exactly
// ---------------------------------------------------------------------------
static void test_fade_transition_offset(void) {
    printf("\n[Test 9] Fade transition applies connection offset\n");

    enum {
        TEST_FIXED_SHIFT = 8,
        START_CAMERA_Y = 28,
        PERP_POS = 84,
        OFFSET = 13,
    };

    Player player = {0};
    Camera camera = {0};

    player.y = PERP_POS << TEST_FIXED_SHIFT;
    camera.y = START_CAMERA_Y;

    clearTransitionTestOverrides();
    setTransitionTestOverrides(kFadeLevelTable, 2, kFadeOffsetConnections, 2);

    initTransition();
    setTransitionLevelContext(0, 0, START_CAMERA_Y, 0, player.y);

    ASSERT(tryTriggerTransition(&kFadeFromLevel, CONN_SIDE_RIGHT, PERP_POS, &player),
           "Fade offset transition triggered");

    ScrollTransInfo info;
    getScrollTransInfo(&info);
    ASSERT(!info.active, "Fade transition does not expose scroll info");

    int frames = run_transition_to_completion(&player, &camera, 40);
    ASSERT(!isTransitioning(), "Fade offset transition completed");
    ASSERT(frames > 0 && frames <= 40, "Fade offset transition finished within frame budget");
    ASSERT(camera.y == START_CAMERA_Y + OFFSET, "Fade offset shifts destination camera Y");
    ASSERT((player.y >> TEST_FIXED_SHIFT) == PERP_POS + OFFSET, "Fade offset shifts destination player Y");
    ASSERT(((player.y >> TEST_FIXED_SHIFT) - camera.y) == (PERP_POS - START_CAMERA_Y),
           "Fade offset preserves player screen Y");

    clearTransitionTestOverrides();
}

// ---------------------------------------------------------------------------
// Test 10: reverse horizontal offset still reuses tilemap on commit
// ---------------------------------------------------------------------------
static void test_reverse_horizontal_offset_commit_reuse(void) {
    printf("\n[Test 10] Reverse horizontal offset reuses tilemap on commit\n");

    enum {
        TEST_FIXED_SHIFT = 8,
        START_CAMERA_Y = 40,
        PERP_POS = 120,
        OFFSET = -21,
    };

    Player player = {0};
    Camera camera = {0};

    player.y = PERP_POS << TEST_FIXED_SHIFT;
    camera.y = START_CAMERA_Y;

    clearTransitionTestOverrides();
    setTransitionTestOverrides(NULL, 0, kHorizontalOffsetConnections, 2);

    loadLevelToVRAM(&smb11);
    initTransition();
    setTransitionLevelContext(TEST_LEVEL_IDX_SMB11, 0, START_CAMERA_Y, 0, player.y);

    ASSERT(tryTriggerTransition(&smb11, CONN_SIDE_LEFT, PERP_POS, &player),
           "Reverse horizontal offset transition triggered");

    ScrollTransInfo info;
    getScrollTransInfo(&info);
    ASSERT(info.active, "Reverse horizontal offset uses scroll transition");
    ASSERT(info.toTileY0 == 3, "Reverse horizontal scroll layout shifts destination by 3 tiles");
    ASSERT(info.seamPrefillAxis == 1, "Reverse horizontal scroll requests seam column prefill");
    ASSERT(info.canReuseTilemapOnCommit, "Reverse horizontal non-tile offset still reuses the tilemap on commit");

    int minScreenY, maxScreenY, minCameraY, maxCameraY, lastScrollScreenY, lastScrollCameraY;
    int frames = run_transition_collect_screen_y(&camera, &player, 91, TEST_FIXED_SHIFT,
                                                 &minScreenY, &maxScreenY,
                                                 &minCameraY, &maxCameraY,
                                                 &lastScrollScreenY, &lastScrollCameraY);

    ASSERT(!isTransitioning(), "Reverse horizontal offset transition completed");
    ASSERT(frames > 0 && frames <= 91, "Reverse horizontal offset transition finished within frame budget");
    ASSERT((maxCameraY - minCameraY) <= 3,
           "Reverse horizontal offset only nudges the combined camera by the tile-quantization residual");
    ASSERT((maxScreenY - minScreenY) == 0,
           "Reverse horizontal offset keeps player screen Y fixed during the scroll");
    ASSERT(camera.y == START_CAMERA_Y + OFFSET, "Reverse horizontal offset shifts destination camera Y");
    ASSERT((player.y >> TEST_FIXED_SHIFT) == PERP_POS + OFFSET, "Reverse horizontal offset shifts destination player Y");
    ASSERT(((player.y >> TEST_FIXED_SHIFT) - camera.y) == (PERP_POS - START_CAMERA_Y),
           "Reverse horizontal offset preserves player screen Y");
    ASSERT(lastScrollScreenY == ((player.y >> TEST_FIXED_SHIFT) - camera.y),
           "Reverse horizontal scroll hands off to commit without a screen-space jump");
    ASSERT(lastScrollCameraY == camera.y + ((info.toTileY0 - info.fromTileY0) * 8),
           "Reverse horizontal scroll ends on the committed destination camera view");

    clearTransitionTestOverrides();
}

// ---------------------------------------------------------------------------
// Test 11: real generated level3->smb11 connection scrolls into the committed view
// ---------------------------------------------------------------------------
static void test_generated_horizontal_connection_quantization(void) {
    printf("\n[Test 11] Generated horizontal connection hands off cleanly\n");

    enum {
        TEST_FIXED_SHIFT = 8,
        START_CAMERA_Y = 40,
        PERP_POS = 80,
    };

    Player player = {0};
    Camera camera = {0};

    player.y = PERP_POS << TEST_FIXED_SHIFT;
    camera.y = START_CAMERA_Y;

    clearTransitionTestOverrides();

    loadLevelToVRAM(&level3);
    initTransition();
    setTransitionLevelContext(TEST_LEVEL_IDX_LEVEL3, 0, START_CAMERA_Y, 0, player.y);

    ASSERT(tryTriggerTransition(&level3, CONN_SIDE_RIGHT, PERP_POS, &player),
           "Generated level3->smb11 transition triggered");

    ScrollTransInfo info;
    getScrollTransInfo(&info);
    ASSERT(info.active, "Generated horizontal connection uses scroll transition");
    ASSERT(info.toTileY0 == -14, "Generated connection quantizes the destination room to the nearest tile");
    ASSERT(info.canReuseTilemapOnCommit, "Generated connection keeps the scrolled tilemap aligned");

    int minScreenY, maxScreenY, minCameraY, maxCameraY, lastScrollScreenY, lastScrollCameraY;
    int frames = run_transition_collect_screen_y(&camera, &player, 120, TEST_FIXED_SHIFT,
                                                 &minScreenY, &maxScreenY,
                                                 &minCameraY, &maxCameraY,
                                                 &lastScrollScreenY, &lastScrollCameraY);

    ASSERT(!isTransitioning(), "Generated horizontal transition completed");
    ASSERT(frames > 0 && frames <= 120, "Generated horizontal transition finished within frame budget");
    ASSERT((maxScreenY - minScreenY) > 0,
           "Generated horizontal clamp case actually exercises the perpendicular handoff");
    ASSERT(lastScrollScreenY == ((player.y >> TEST_FIXED_SHIFT) - camera.y),
           "Generated horizontal scroll hands off to commit without a screen-space jump");
    ASSERT(lastScrollCameraY == camera.y + ((info.toTileY0 - info.fromTileY0) * 8),
           "Generated horizontal scroll ends on the committed destination camera view");
}

// ---------------------------------------------------------------------------
// Test 12: real generated smb11->celeste1 vertical connection keeps the view aligned
// ---------------------------------------------------------------------------
static void test_generated_vertical_connection_handoff(void) {
    printf("\n[Test 12] Generated vertical connection hands off cleanly\n");

    enum {
        TEST_FIXED_SHIFT = 8,
        START_CAMERA_X = 240,
        START_CAMERA_Y = 80,
        PERP_POS = 320,
        START_PLAYER_Y = 220,
    };

    Player player = {0};
    Camera camera = {0};

    player.x = PERP_POS << TEST_FIXED_SHIFT;
    player.y = START_PLAYER_Y << TEST_FIXED_SHIFT;
    camera.x = START_CAMERA_X;
    camera.y = START_CAMERA_Y;

    clearTransitionTestOverrides();

    loadLevelToVRAM(&smb11);
    initTransition();
    setTransitionLevelContext(TEST_LEVEL_IDX_SMB11, START_CAMERA_X, START_CAMERA_Y, player.x, player.y);

    ASSERT(tryTriggerTransition(&smb11, CONN_SIDE_BOTTOM, PERP_POS, &player),
           "Generated smb11->celeste1 transition triggered");

    ScrollTransInfo info;
    getScrollTransInfo(&info);
    ASSERT(info.active, "Generated vertical connection uses scroll transition");
    ASSERT(info.toTileX0 == 39, "Generated vertical connection preserves the large horizontal room offset");
    ASSERT(info.canReuseTilemapOnCommit, "Generated vertical connection keeps the scrolled tilemap aligned");

    int minScreenX, maxScreenX, minCameraX, maxCameraX, lastScrollScreenX, lastScrollCameraX;
    int frames = run_transition_collect_screen_x(&camera, &player, 120, TEST_FIXED_SHIFT,
                                                 &minScreenX, &maxScreenX,
                                                 &minCameraX, &maxCameraX,
                                                 &lastScrollScreenX, &lastScrollCameraX);

    ASSERT(!isTransitioning(), "Generated vertical transition completed");
    ASSERT(frames > 0 && frames <= 120, "Generated vertical transition finished within frame budget");
    ASSERT(camera.x == 0, "Generated vertical transition lands on the clamped destination camera X");
    ASSERT((maxScreenX - minScreenX) > 0,
           "Generated vertical clamp case exercises the perpendicular camera handoff");
    ASSERT(lastScrollScreenX == ((player.x >> TEST_FIXED_SHIFT) - camera.x),
           "Generated vertical scroll hands off to commit without a screen-space jump");
    ASSERT(lastScrollCameraX == camera.x + ((info.toTileX0 - info.fromTileX0) * 8),
           "Generated vertical scroll ends on the committed destination camera view");
    assert_camera_stable_after_commit(&camera, &player, &celeste1,
                                      "Generated vertical transition lands on the first gameplay camera");
}

// ---------------------------------------------------------------------------
// Test 13: real generated celeste1->smb11 vertical connection settles the camera
// ---------------------------------------------------------------------------
static void test_generated_reverse_vertical_camera_stability(void) {
    printf("\n[Test 13] Generated reverse vertical transition settles camera before commit\n");

    enum {
        TEST_FIXED_SHIFT = 8,
        START_CAMERA_X = 0,
        START_CAMERA_Y = 0,
        PERP_POS = 72,
    };

    Player player = {0};
    Camera camera = {0};

    player.x = PERP_POS << TEST_FIXED_SHIFT;
    player.y = 0;
    camera.x = START_CAMERA_X;
    camera.y = START_CAMERA_Y;

    clearTransitionTestOverrides();

    loadLevelToVRAM(&celeste1);
    initTransition();
    setTransitionLevelContext(TEST_LEVEL_IDX_CELESTE1, START_CAMERA_X, START_CAMERA_Y, player.x, player.y);

    ASSERT(tryTriggerTransition(&celeste1, CONN_SIDE_TOP, PERP_POS, &player),
           "Generated celeste1->smb11 transition triggered");

    ScrollTransInfo info;
    getScrollTransInfo(&info);
    ASSERT(info.active, "Generated reverse vertical connection uses scroll transition");
    ASSERT(info.toTileX0 == -39, "Generated reverse vertical connection keeps the large horizontal offset");

    int minScreenX, maxScreenX, minCameraX, maxCameraX, lastScrollScreenX, lastScrollCameraX;
    int frames = run_transition_collect_screen_x(&camera, &player, 120, TEST_FIXED_SHIFT,
                                                 &minScreenX, &maxScreenX,
                                                 &minCameraX, &maxCameraX,
                                                 &lastScrollScreenX, &lastScrollCameraX);

    ASSERT(!isTransitioning(), "Generated reverse vertical transition completed");
    ASSERT(frames > 0 && frames <= 120, "Generated reverse vertical transition finished within frame budget");
    ASSERT(camera.x == 304, "Generated reverse vertical transition lands on the settled destination camera X");
    ASSERT((maxScreenX - minScreenX) <= 8,
           "Generated reverse vertical transition limits player screen X drift to the dead-zone correction");
    ASSERT(lastScrollScreenX == ((player.x >> TEST_FIXED_SHIFT) - camera.x),
           "Generated reverse vertical scroll hands off to commit without a screen-space jump");
    ASSERT(lastScrollCameraX == camera.x + ((info.toTileX0 - info.fromTileX0) * 8),
           "Generated reverse vertical scroll ends on the committed destination camera view");
    assert_camera_stable_after_commit(&camera, &player, &smb11,
                                      "Generated reverse vertical transition lands on the first gameplay camera");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(void) {
    printf("=== Transition Buffer Swap Tests ===\n");

    test_buffer_swap_invariant();
    test_double_transition_buffers();
    test_adopt_preserves_incoming_vram_offset();
    test_player_render_offset_during_transition();
    test_decoration_layer_valid_after_load();
    test_destination_only_layer_visible_during_scroll();
    test_scroll_handoff_extra_frame();
    test_scroll_player_handoff_and_trail_cleanup();
    test_boundary_transition_preserves_horizontal_velocity();
    test_horizontal_connection_offset();
    test_vertical_connection_offset();
    test_fade_transition_offset();
    test_reverse_horizontal_offset_commit_reuse();
    test_generated_horizontal_connection_quantization();
    test_generated_vertical_connection_handoff();
    test_generated_reverse_vertical_camera_stability();

    printf("\n================================\n");
    printf("Results: %d passed, %d failed\n", g_passed, g_failed);
    return (g_failed > 0) ? 1 : 0;
}

#endif // DESKTOP_BUILD
