#ifndef SCROLL_TILEMAP_H
#define SCROLL_TILEMAP_H

#include "level/level.h"
#include "transition.h"

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
