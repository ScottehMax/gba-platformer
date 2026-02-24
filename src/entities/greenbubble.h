#ifndef GREENBUBBLE_H
#define GREENBUBBLE_H

#include "core/game_types.h"
#include "level/level.h"

#define MAX_GREEN_BUBBLES 32

typedef struct {
    int x;          // Center X in pixels
    int y;          // Center Y in pixels
    int width;      // Width in pixels
    int height;     // Height in pixels
    int active;     // 1 if active, 0 if collected/inactive
} GreenBubble;

typedef struct {
    GreenBubble bubbles[MAX_GREEN_BUBBLES];
    int count;
} GreenBubbleManager;

void initGreenBubbleManager(GreenBubbleManager* manager);
void loadGreenBubblesFromLevel(GreenBubbleManager* manager, const Level* level);
void updateGreenBubbles(GreenBubbleManager* manager, Player* player);
void renderGreenBubbles(const GreenBubbleManager* manager, int cameraX, int cameraY);

#endif
