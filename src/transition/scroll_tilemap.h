#ifndef SCROLL_TILEMAP_H
#define SCROLL_TILEMAP_H

#include "level/level.h"
#include "transition.h"

// Persistent tilemap bookkeeping for the incremental tile-writing algorithm.
// Tracks camera tile position, BG origin offsets, and scroll transition state
// so the game loop can do incremental column/row updates instead of full refreshes.
typedef struct {
    int oldCameraTileX;
    int oldCameraTileY;
    int oldCameraTileValid;
    int wasScrolling;
    int lastScrollToTileX0;
    int lastScrollToTileY0;
    int lastScrollBgOriginX;
    int lastScrollBgOriginY;
    int lastScrollCanReuseTilemapOnCommit;
    int bgTileOriginX;
    int bgTileOriginY;
} TilemapState;

void resetTilemapState(TilemapState* ts);

// Update BG scroll registers and write tile data for the current camera position.
// Handles both normal gameplay and scroll transitions.
// Returns 1 if a scroll transition just started this frame (caller needs this
// to decide whether to offset the render camera).
int updateTilemapForCamera(
    TilemapState* ts,
    const Level* currentLevel,
    const ScrollTransInfo* scrollInfo,
    int cameraX, int cameraY,
    int levelChanged);

u16 currentTileEntryAt(const Level* level, u8 layerIdx, int localX, int localY);
u16 scrollTileEntryAt(const ScrollTransInfo* scrollInfo, u8 layerIdx, int virtualTileX, int virtualTileY);

void prefillScrollSeam(volatile u16* bgMap, u8 layerIdx,
                       const ScrollTransInfo* scrollInfo,
                       int scrollBgOriginX, int scrollBgOriginY,
                       int cameraTileX, int cameraTileY);

void prefillScrollIncomingOverlap(const Level* currentLevel,
                                  const ScrollTransInfo* scrollInfo,
                                  int scrollBgOriginX, int scrollBgOriginY,
                                  int cameraTileX, int cameraTileY);

void writeHorizontalScrollRow(volatile u16* bgMap,
                              u8 layerIdx,
                              const ScrollTransInfo* scrollInfo,
                              int tileOriginX, int tileOriginY,
                              int cameraTileX,
                              int ly);

void writeVerticalScrollColumn(volatile u16* bgMap,
                               u8 layerIdx,
                               const ScrollTransInfo* scrollInfo,
                               int tileOriginX, int tileOriginY,
                               int cameraTileY,
                               int lx);

#endif // SCROLL_TILEMAP_H
