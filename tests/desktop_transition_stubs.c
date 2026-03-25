#ifdef DESKTOP_BUILD

#include "transition/transition.h"

void initTransition(void) {}

void setTransitionLevelContext(int levelIdx, int cameraX, int cameraY, int playerX, int playerY) {
    (void)levelIdx;
    (void)cameraX;
    (void)cameraY;
    (void)playerX;
    (void)playerY;
}

int tryTriggerTransition(const Level* level, int side, int perpPos, Player* player) {
    (void)level;
    (void)side;
    (void)perpPos;
    (void)player;
    return 0;
}

int updateTransition(Player* player, Camera* camera) {
    (void)player;
    (void)camera;
    return 0;
}

int isTransitioning(void) {
    return 0;
}

void getTransitionVirtualCamera(int* outX, int* outY) {
    if (outX) *outX = 0;
    if (outY) *outY = 0;
}

void getScrollTransInfo(ScrollTransInfo* out) {
    if (!out) return;
    out->active = 0;
    out->fromLevel = 0;
    out->toLevel = 0;
    out->fromTileX0 = 0;
    out->fromTileY0 = 0;
    out->toTileX0 = 0;
    out->toTileY0 = 0;
    out->tileVramOffset = 0;
    out->seamPrefillAxis = 0;
    out->canReuseTilemapOnCommit = 0;
}

u16 getScrollTileEntry(int layerIdx, int virtualTileX, int virtualTileY) {
    (void)layerIdx;
    (void)virtualTileX;
    (void)virtualTileY;
    return 0;
}

void consumeTransitionSeamPrefill(void) {}

void setTransitionTestOverrides(const Level* const* levels, int levelCount,
                                const ScreenConnection* connections, int connectionCount) {
    (void)levels;
    (void)levelCount;
    (void)connections;
    (void)connectionCount;
}

void clearTransitionTestOverrides(void) {}

#endif // DESKTOP_BUILD
