#include "scroll_tilemap.h"

typedef struct {
    int startX;
    int endX;
    const u16* src;
    const u16* entryTable;
} RowSpan;

typedef struct {
    int startY;
    int endY;
    const u16* src;
    int stride;
    const u16* entryTable;
} ColumnSpan;

static u16 incomingTileEntryAt(const Level* level, u8 layerIdx, int localX, int localY) {
    if (layerIdx >= level->layerCount) {
        return 0;
    }
    if (localX < 0 || localX >= level->width || localY < 0 || localY >= level->height) {
        return 0;
    }
    if (!g_levelBLayerTiles[layerIdx]) {
        return 0;
    }

    u16 tid = g_levelBLayerTiles[layerIdx][localY * level->width + localX];
    return mapTileEntry(g_levelBTileEntries, tid);
}

u16 currentTileEntryAt(const Level* level, u8 layerIdx, int localX, int localY) {
    if (layerIdx >= level->layerCount) {
        return 0;
    }
    if (localX < 0 || localX >= level->width || localY < 0 || localY >= level->height) {
        return 0;
    }
    if (!g_levelLayerTiles[layerIdx]) {
        return 0;
    }

    u16 tid = g_levelLayerTiles[layerIdx][localY * level->width + localX];
    return mapTileEntry(g_levelTileEntries, tid);
}

u16 scrollTileEntryAt(const ScrollTransInfo* scrollInfo, u8 layerIdx, int virtualTileX, int virtualTileY) {
    const Level* fromLevel = scrollInfo->fromLevel;
    int fromLocalX = virtualTileX - scrollInfo->fromTileX0;
    int fromLocalY = virtualTileY - scrollInfo->fromTileY0;
    if (fromLocalX >= 0 && fromLocalX < fromLevel->width &&
        fromLocalY >= 0 && fromLocalY < fromLevel->height) {
        return currentTileEntryAt(fromLevel, layerIdx, fromLocalX, fromLocalY);
    }

    {
        const Level* toLevel = scrollInfo->toLevel;
        int toLocalX = virtualTileX - scrollInfo->toTileX0;
        int toLocalY = virtualTileY - scrollInfo->toTileY0;
        if (toLocalX >= 0 && toLocalX < toLevel->width &&
            toLocalY >= 0 && toLocalY < toLevel->height) {
            return incomingTileEntryAt(toLevel, layerIdx, toLocalX, toLocalY);
        }
    }

    return 0;
}

void prefillScrollSeam(volatile u16* bgMap, u8 layerIdx,
                       const ScrollTransInfo* scrollInfo,
                       int scrollBgOriginX, int scrollBgOriginY,
                       int cameraTileX, int cameraTileY) {
    const Level* toLevel = scrollInfo->toLevel;
    int incomingX0 = scrollBgOriginX + scrollInfo->toTileX0;
    int incomingY0 = scrollBgOriginY + scrollInfo->toTileY0;

    if (scrollInfo->seamPrefillAxis == 1) {
        int seamStartsOnRight = scrollInfo->toTileX0 > scrollInfo->fromTileX0;
        for (int s = 0; s < 2; s++) {
            int localX = seamStartsOnRight ? s : (toLevel->width - 1 - s);
            int mapX = incomingX0 + localX;
            int mx = mapX & 31;
            for (int ty = 0; ty < 32; ty++) {
                int mapY = cameraTileY + ty;
                int localY = mapY - incomingY0;
                bgMap[(mapY & 31) * 32 + mx] =
                    incomingTileEntryAt(toLevel, layerIdx, localX, localY);
            }
        }
    } else if (scrollInfo->seamPrefillAxis == 2) {
        int seamStartsBelow = scrollInfo->toTileY0 > scrollInfo->fromTileY0;
        for (int s = 0; s < 2; s++) {
            int localY = seamStartsBelow ? s : (toLevel->height - 1 - s);
            int mapY = incomingY0 + localY;
            int my = mapY & 31;
            for (int tx = 0; tx < 32; tx++) {
                int mapX = cameraTileX + tx;
                int localX = mapX - incomingX0;
                bgMap[my * 32 + (mapX & 31)] =
                    incomingTileEntryAt(toLevel, layerIdx, localX, localY);
            }
        }
    }
}

void prefillScrollIncomingOverlap(const Level* currentLevel,
                                  const ScrollTransInfo* scrollInfo,
                                  int scrollBgOriginX, int scrollBgOriginY,
                                  int cameraTileX, int cameraTileY) {
    int incomingX0 = scrollBgOriginX + scrollInfo->toTileX0;
    int incomingY0 = scrollBgOriginY + scrollInfo->toTileY0;
    int incomingX1 = incomingX0 + scrollInfo->toLevel->width - 1;
    int incomingY1 = incomingY0 + scrollInfo->toLevel->height - 1;

    int windowX0 = cameraTileX;
    int windowY0 = cameraTileY;
    int windowX1 = cameraTileX + 31;
    int windowY1 = cameraTileY + 31;

    int startX = incomingX0 > windowX0 ? incomingX0 : windowX0;
    int startY = incomingY0 > windowY0 ? incomingY0 : windowY0;
    int endX = incomingX1 < windowX1 ? incomingX1 : windowX1;
    int endY = incomingY1 < windowY1 ? incomingY1 : windowY1;

    if (startX > endX || startY > endY) {
        return;
    }

    const u8 MAX_BG_LAYERS = 2;
    const Level* toLevel = scrollInfo->toLevel;
    for (u8 layerIdx = 0; layerIdx < MAX_BG_LAYERS; layerIdx++) {
        u8 bgLayer;
        if (layerIdx < currentLevel->layerCount) {
            bgLayer = currentLevel->layers[layerIdx].bgLayer;
        } else if (layerIdx < toLevel->layerCount) {
            bgLayer = toLevel->layers[layerIdx].bgLayer;
        } else {
            bgLayer = layerIdx;
        }

        {
            u8 screenBase = 24 + bgLayer;
            volatile u16* bgMap = (volatile u16*)(0x06000000 + (screenBase << 11));
            for (int mapY = startY; mapY <= endY; mapY++) {
                int rowBase = (mapY & 31) * 32;
                int localY = mapY - incomingY0;
                if (layerIdx < toLevel->layerCount && g_levelBLayerTiles[layerIdx]) {
                    const u16* src =
                        g_levelBLayerTiles[layerIdx] + localY * toLevel->width + (startX - incomingX0);
                    const u16* entryTable = g_levelBTileEntries;
                    for (int mapX = startX; mapX <= endX; mapX++) {
                        bgMap[rowBase + (mapX & 31)] = entryTable[*src++];
                    }
                } else {
                    for (int mapX = startX; mapX <= endX; mapX++) {
                        bgMap[rowBase + (mapX & 31)] = 0;
                    }
                }
            }
        }
    }
}

void writeHorizontalScrollRow(volatile u16* bgMap,
                              u8 layerIdx,
                              const ScrollTransInfo* scrollInfo,
                              int tileOriginX, int tileOriginY,
                              int cameraTileX,
                              int ly) {
    const int visibleX0 = cameraTileX;
    const int visibleX1 = cameraTileX + 31;
    const int rowBase = (ly & 31) * 32;
    RowSpan spans[2];
    int spanCount = 0;

    const Level* fromLevel = scrollInfo->fromLevel;
    int fromLocalY = ly - (tileOriginY + scrollInfo->fromTileY0);
    if (layerIdx < fromLevel->layerCount &&
        fromLocalY >= 0 && fromLocalY < fromLevel->height &&
        g_levelLayerTiles[layerIdx]) {
        int fromMapX0 = tileOriginX + scrollInfo->fromTileX0;
        int startX = fromMapX0 > visibleX0 ? fromMapX0 : visibleX0;
        int endX = fromMapX0 + fromLevel->width - 1;
        if (endX > visibleX1) {
            endX = visibleX1;
        }
        if (startX <= endX) {
            spans[spanCount].startX = startX;
            spans[spanCount].endX = endX;
            spans[spanCount].src =
                g_levelLayerTiles[layerIdx] + fromLocalY * fromLevel->width + (startX - fromMapX0);
            spans[spanCount].entryTable = g_levelTileEntries;
            spanCount++;
        }
    }

    {
        const Level* toLevel = scrollInfo->toLevel;
        int toLocalY = ly - (tileOriginY + scrollInfo->toTileY0);
        if (layerIdx < toLevel->layerCount &&
            toLocalY >= 0 && toLocalY < toLevel->height &&
            g_levelBLayerTiles[layerIdx]) {
            int toMapX0 = tileOriginX + scrollInfo->toTileX0;
            int startX = toMapX0 > visibleX0 ? toMapX0 : visibleX0;
            int endX = toMapX0 + toLevel->width - 1;
            if (endX > visibleX1) {
                endX = visibleX1;
            }
            if (startX <= endX) {
                spans[spanCount].startX = startX;
                spans[spanCount].endX = endX;
                spans[spanCount].src =
                    g_levelBLayerTiles[layerIdx] + toLocalY * toLevel->width + (startX - toMapX0);
                spans[spanCount].entryTable = g_levelBTileEntries;
                spanCount++;
            }
        }
    }

    if (spanCount == 2 && spans[1].startX < spans[0].startX) {
        RowSpan tmp = spans[0];
        spans[0] = spans[1];
        spans[1] = tmp;
    }

    {
        int cursor = visibleX0;
        for (int spanIdx = 0; spanIdx < spanCount; spanIdx++) {
            for (int x = cursor; x < spans[spanIdx].startX; x++) {
                bgMap[rowBase + (x & 31)] = 0;
            }

            {
                const u16* src = spans[spanIdx].src;
                const u16* entryTable = spans[spanIdx].entryTable;
                for (int x = spans[spanIdx].startX; x <= spans[spanIdx].endX; x++) {
                    bgMap[rowBase + (x & 31)] = entryTable[*src++];
                }
            }

            cursor = spans[spanIdx].endX + 1;
        }

        for (int x = cursor; x <= visibleX1; x++) {
            bgMap[rowBase + (x & 31)] = 0;
        }
    }
}

void writeVerticalScrollColumn(volatile u16* bgMap,
                               u8 layerIdx,
                               const ScrollTransInfo* scrollInfo,
                               int tileOriginX, int tileOriginY,
                               int cameraTileY,
                               int lx) {
    const int visibleY0 = cameraTileY;
    const int visibleY1 = cameraTileY + 31;
    const int mx = lx & 31;
    ColumnSpan spans[2];
    int spanCount = 0;

    const Level* fromLevel = scrollInfo->fromLevel;
    int fromLocalX = lx - (tileOriginX + scrollInfo->fromTileX0);
    if (layerIdx < fromLevel->layerCount &&
        fromLocalX >= 0 && fromLocalX < fromLevel->width &&
        g_levelLayerTiles[layerIdx]) {
        int fromMapY0 = tileOriginY + scrollInfo->fromTileY0;
        int startY = fromMapY0 > visibleY0 ? fromMapY0 : visibleY0;
        int endY = fromMapY0 + fromLevel->height - 1;
        if (endY > visibleY1) {
            endY = visibleY1;
        }
        if (startY <= endY) {
            spans[spanCount].startY = startY;
            spans[spanCount].endY = endY;
            spans[spanCount].src =
                g_levelLayerTiles[layerIdx] + (startY - fromMapY0) * fromLevel->width + fromLocalX;
            spans[spanCount].stride = fromLevel->width;
            spans[spanCount].entryTable = g_levelTileEntries;
            spanCount++;
        }
    }

    {
        const Level* toLevel = scrollInfo->toLevel;
        int toLocalX = lx - (tileOriginX + scrollInfo->toTileX0);
        if (layerIdx < toLevel->layerCount &&
            toLocalX >= 0 && toLocalX < toLevel->width &&
            g_levelBLayerTiles[layerIdx]) {
            int toMapY0 = tileOriginY + scrollInfo->toTileY0;
            int startY = toMapY0 > visibleY0 ? toMapY0 : visibleY0;
            int endY = toMapY0 + toLevel->height - 1;
            if (endY > visibleY1) {
                endY = visibleY1;
            }
            if (startY <= endY) {
                spans[spanCount].startY = startY;
                spans[spanCount].endY = endY;
                spans[spanCount].src =
                    g_levelBLayerTiles[layerIdx] + (startY - toMapY0) * toLevel->width + toLocalX;
                spans[spanCount].stride = toLevel->width;
                spans[spanCount].entryTable = g_levelBTileEntries;
                spanCount++;
            }
        }
    }

    if (spanCount == 2 && spans[1].startY < spans[0].startY) {
        ColumnSpan tmp = spans[0];
        spans[0] = spans[1];
        spans[1] = tmp;
    }

    {
        int cursor = visibleY0;
        for (int spanIdx = 0; spanIdx < spanCount; spanIdx++) {
            for (int y = cursor; y < spans[spanIdx].startY; y++) {
                bgMap[(y & 31) * 32 + mx] = 0;
            }

            {
                const u16* src = spans[spanIdx].src;
                const u16* entryTable = spans[spanIdx].entryTable;
                int stride = spans[spanIdx].stride;
                for (int y = spans[spanIdx].startY; y <= spans[spanIdx].endY; y++) {
                    bgMap[(y & 31) * 32 + mx] = entryTable[*src];
                    src += stride;
                }
            }

            cursor = spans[spanIdx].endY + 1;
        }

        for (int y = cursor; y <= visibleY1; y++) {
            bgMap[(y & 31) * 32 + mx] = 0;
        }
    }
}
