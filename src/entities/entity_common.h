#ifndef ENTITY_COMMON_H
#define ENTITY_COMMON_H

#include "core/game_types.h"
#include "core/game_math.h"

// Axis-aligned bounding box (pixel coordinates)
typedef struct {
    int left, right, top, bottom;
} AABB;

// Build player AABB from fixed-point position
static inline AABB playerAABB(const Player* player) {
    int px = player->x >> FIXED_SHIFT;
    int py = player->y >> FIXED_SHIFT;
    AABB box;
    box.left   = px - PLAYER_RADIUS_X;
    box.right  = px + PLAYER_RADIUS_X;
    box.top    = py - PLAYER_RADIUS_Y;
    box.bottom = py + PLAYER_RADIUS_Y;
    return box;
}

// Build entity AABB from a center position and half-extents
static inline AABB entityAABBCentered(int cx, int cy, int halfW, int halfH) {
    AABB box;
    box.left   = cx - halfW;
    box.right  = cx + halfW;
    box.top    = cy - halfH;
    box.bottom = cy + halfH;
    return box;
}

// Build entity AABB from a top-left position and size
static inline AABB entityAABBTopLeft(int x, int y, int w, int h) {
    AABB box;
    box.left   = x;
    box.right  = x + w;
    box.top    = y;
    box.bottom = y + h;
    return box;
}

// Test whether two AABBs overlap
static inline int aabbOverlap(const AABB* a, const AABB* b) {
    return a->right > b->left && a->left < b->right &&
           a->bottom > b->top && a->top < b->bottom;
}

#endif // ENTITY_COMMON_H
