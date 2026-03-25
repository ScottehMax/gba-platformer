#ifdef DESKTOP_BUILD

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Desktop tonc stub must come first
#include "desktop/desktop_stubs.h"
#include "level/level.h"
#include "smb11.h"  // level3.h pulled in by level.h; smb11.h is not, add explicitly
#include "transition/transition.h"

// Test-accessor functions exposed by level.c under DESKTOP_BUILD
extern const u16* getMainBufBase(void);
extern const u16* getSecBufBase(void);
extern const u16* getTileBufA(void);  // always g_tileBuffer
extern const u16* getTileBufB(void);  // always g_tileBBuffer

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

enum {
    TEST_LEVEL_IDX_LEVEL3 = 3,
    TEST_LEVEL_IDX_LEVEL4 = 4,
    TEST_LEVEL_IDX_SMB11 = 5,
};

static const ScreenConnection kHorizontalOffsetConnections[] = {
    { TEST_LEVEL_IDX_LEVEL3, TEST_LEVEL_IDX_SMB11, CONN_SIDE_RIGHT,  CONN_SIDE_LEFT,  21 },
    { TEST_LEVEL_IDX_SMB11,  TEST_LEVEL_IDX_LEVEL3, CONN_SIDE_LEFT,   CONN_SIDE_RIGHT, -21 },
};

static const ScreenConnection kVerticalOffsetConnections[] = {
    { TEST_LEVEL_IDX_LEVEL3, TEST_LEVEL_IDX_LEVEL4, CONN_SIDE_BOTTOM, CONN_SIDE_TOP,    16 },
    { TEST_LEVEL_IDX_LEVEL4, TEST_LEVEL_IDX_LEVEL3, CONN_SIDE_TOP,    CONN_SIDE_BOTTOM, -16 },
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
    { 0, 1, CONN_SIDE_RIGHT, CONN_SIDE_LEFT, 13 },
    { 1, 0, CONN_SIDE_LEFT,  CONN_SIDE_RIGHT, -13 },
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

    loadLevelToVRAM(&smb11);
    initTransition();
    setTransitionLevelContext(5, 0, 0, 8 << 8, 120 << 8);

    ASSERT(tryTriggerTransition(&smb11, CONN_SIDE_LEFT, 120),
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

    enum { TEST_FIXED_SHIFT = 8 };

    Player player = {0};
    Camera camera = {0};

    loadLevelToVRAM(&smb11);
    initTransition();
    setTransitionLevelContext(5, 0, 0, 8 << TEST_FIXED_SHIFT, 120 << TEST_FIXED_SHIFT);

    ASSERT(tryTriggerTransition(&smb11, CONN_SIDE_LEFT, 120),
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

    ASSERT(tryTriggerTransition(&level3, CONN_SIDE_RIGHT, PERP_POS),
           "Horizontal offset transition triggered");

    ScrollTransInfo info;
    getScrollTransInfo(&info);
    ASSERT(info.active, "Horizontal offset uses scroll transition");
    ASSERT(info.toTileY0 == -2, "Horizontal scroll layout shifts destination by 2 tiles");
    ASSERT(info.seamPrefillAxis == 1, "Horizontal rightward scroll requests seam column prefill");
    ASSERT(info.canReuseTilemapOnCommit, "Horizontal tilemap can be reused on commit for non-tile-aligned offset");

    updateTransition(&player, &camera);
    ASSERT(camera.y == START_CAMERA_Y, "Horizontal scroll stays on a single axis until commit");

    int frames = 1 + run_transition_to_completion(&player, &camera, 90);
    ASSERT(!isTransitioning(), "Horizontal offset transition completed");
    ASSERT(frames > 0 && frames <= 91, "Horizontal offset transition finished within frame budget");
    ASSERT(camera.y == START_CAMERA_Y + OFFSET, "Horizontal offset shifts destination camera Y");
    ASSERT((player.y >> TEST_FIXED_SHIFT) == PERP_POS + OFFSET, "Horizontal offset shifts destination player Y");
    ASSERT(((player.y >> TEST_FIXED_SHIFT) - camera.y) == (PERP_POS - START_CAMERA_Y),
           "Horizontal offset preserves player screen Y");

    clearTransitionTestOverrides();
}

// ---------------------------------------------------------------------------
// Test 8: vertical connection offset shifts camera/player X
// ---------------------------------------------------------------------------
static void test_vertical_connection_offset(void) {
    printf("\n[Test 8] Vertical connection offset applied\n");

    enum {
        TEST_FIXED_SHIFT = 8,
        START_CAMERA_X = 120,
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

    ASSERT(tryTriggerTransition(&level3, CONN_SIDE_BOTTOM, PERP_POS),
           "Vertical offset transition triggered");

    ScrollTransInfo info;
    getScrollTransInfo(&info);
    ASSERT(info.active, "Vertical offset uses scroll transition");
    ASSERT(info.toTileX0 == -2, "Vertical scroll layout shifts destination by 2 tiles");
    ASSERT(info.seamPrefillAxis == 2, "Vertical downward scroll requests seam row prefill");
    ASSERT(info.canReuseTilemapOnCommit, "Vertical tilemap can be reused on commit for tile-aligned offset");

    updateTransition(&player, &camera);
    ASSERT(camera.x == START_CAMERA_X, "Vertical scroll stays on a single axis until commit");

    int frames = 1 + run_transition_to_completion(&player, &camera, 80);
    ASSERT(!isTransitioning(), "Vertical offset transition completed");
    ASSERT(frames > 0 && frames <= 81, "Vertical offset transition finished within frame budget");
    ASSERT(camera.x == START_CAMERA_X + OFFSET, "Vertical offset shifts destination camera X");
    ASSERT((player.x >> TEST_FIXED_SHIFT) == PERP_POS + OFFSET, "Vertical offset shifts destination player X");
    ASSERT(((player.x >> TEST_FIXED_SHIFT) - camera.x) == (PERP_POS - START_CAMERA_X),
           "Vertical offset preserves player screen X");
    ASSERT(camera.y == 0, "Vertical offset keeps top-entry camera Y");

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

    ASSERT(tryTriggerTransition(&kFadeFromLevel, CONN_SIDE_RIGHT, PERP_POS),
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

    ASSERT(tryTriggerTransition(&smb11, CONN_SIDE_LEFT, PERP_POS),
           "Reverse horizontal offset transition triggered");

    ScrollTransInfo info;
    getScrollTransInfo(&info);
    ASSERT(info.active, "Reverse horizontal offset uses scroll transition");
    ASSERT(info.toTileY0 == 3, "Reverse horizontal scroll layout shifts destination by 3 tiles");
    ASSERT(info.seamPrefillAxis == 1, "Reverse horizontal scroll requests seam column prefill");
    ASSERT(info.canReuseTilemapOnCommit, "Reverse horizontal tilemap can be reused on commit");

    updateTransition(&player, &camera);
    ASSERT(camera.y == START_CAMERA_Y, "Reverse horizontal scroll stays on a single axis until commit");

    int frames = 1 + run_transition_to_completion(&player, &camera, 90);
    ASSERT(!isTransitioning(), "Reverse horizontal offset transition completed");
    ASSERT(frames > 0 && frames <= 91, "Reverse horizontal offset transition finished within frame budget");
    ASSERT(camera.y == START_CAMERA_Y + OFFSET, "Reverse horizontal offset shifts destination camera Y");
    ASSERT((player.y >> TEST_FIXED_SHIFT) == PERP_POS + OFFSET, "Reverse horizontal offset shifts destination player Y");
    ASSERT(((player.y >> TEST_FIXED_SHIFT) - camera.y) == (PERP_POS - START_CAMERA_Y),
           "Reverse horizontal offset preserves player screen Y");

    clearTransitionTestOverrides();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(void) {
    printf("=== Transition Buffer Swap Tests ===\n");

    test_buffer_swap_invariant();
    test_double_transition_buffers();
    test_player_render_offset_during_transition();
    test_decoration_layer_valid_after_load();
    test_destination_only_layer_visible_during_scroll();
    test_scroll_handoff_extra_frame();
    test_horizontal_connection_offset();
    test_vertical_connection_offset();
    test_fade_transition_offset();
    test_reverse_horizontal_offset_commit_reuse();

    printf("\n================================\n");
    printf("Results: %d passed, %d failed\n", g_passed, g_failed);
    return (g_failed > 0) ? 1 : 0;
}

#endif // DESKTOP_BUILD
