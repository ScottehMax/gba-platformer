#include "collision.h"

static int isPositionColliding(const Level* level, int screenX, int screenY) {
    int tileMinX = (screenX - PLAYER_RADIUS) / 8;
    int tileMaxX = (screenX + PLAYER_RADIUS) / 8;
    int tileMinY = (screenY - PLAYER_RADIUS) / 8;
    int tileMaxY = (screenY + PLAYER_RADIUS) / 8;

    int playerLeft = screenX - PLAYER_RADIUS;
    int playerRight = screenX + PLAYER_RADIUS;
    int playerTop = screenY - PLAYER_RADIUS;
    int playerBottom = screenY + PLAYER_RADIUS;

    for (int ty = tileMinY; ty <= tileMaxY; ty++) {
        for (int tx = tileMinX; tx <= tileMaxX; tx++) {
            u16 tile = getTileAt(level, 0, tx, ty);
            if (!isTileSolid(level, tile)) continue;

            int tileLeft = tx * 8;
            int tileRight = (tx + 1) * 8;
            int tileTop = ty * 8;
            int tileBottom = (ty + 1) * 8;

            if (playerRight > tileLeft && playerLeft < tileRight &&
                playerBottom > tileTop && playerTop < tileBottom) {
                return 1;
            }
        }
    }

    return 0;
}


void collideHorizontal(Player* player, const Level* level) {
    // Horizontal sweep
    player->x += player->vx;
    int screenX = player->x >> FIXED_SHIFT;
    int screenY = player->y >> FIXED_SHIFT;

    // Level bounds
    int levelWidthPx = level->width * 8;
    if (screenX < PLAYER_RADIUS) {
        player->x = PLAYER_RADIUS << FIXED_SHIFT;
        player->vx = 0;
    } else if (screenX > levelWidthPx - PLAYER_RADIUS) {
        player->x = (levelWidthPx - PLAYER_RADIUS) << FIXED_SHIFT;
        player->vx = 0;
    } else {
        // Check for tile collision at new X position
        screenX = player->x >> FIXED_SHIFT;
        int tileMinX = (screenX - PLAYER_RADIUS) / 8;
        int tileMaxX = (screenX + PLAYER_RADIUS) / 8;
        int tileMinY = (screenY - PLAYER_RADIUS) / 8;
        int tileMaxY = (screenY + PLAYER_RADIUS) / 8;

        for (int ty = tileMinY; ty <= tileMaxY; ty++) {
            for (int tx = tileMinX; tx <= tileMaxX; tx++) {
                u16 tile = getTileAt(level, 0, tx, ty);
                if (!isTileSolid(level, tile)) continue;

                int tileLeft = tx * 8;
                int tileRight = (tx + 1) * 8;
                int tileTop = ty * 8;
                int tileBottom = (ty + 1) * 8;

                int playerLeft = screenX - PLAYER_RADIUS;
                int playerRight = screenX + PLAYER_RADIUS;
                int playerTop = screenY - PLAYER_RADIUS;
                int playerBottom = screenY + PLAYER_RADIUS;

                if (playerRight > tileLeft && playerLeft < tileRight &&
                    playerBottom > tileTop && playerTop < tileBottom) {
                    // Collision - snap to tile edge instead of reverting
                    int snappedX = player->vx > 0
                        ? (tileLeft - PLAYER_RADIUS) << FIXED_SHIFT
                        : (tileRight + PLAYER_RADIUS) << FIXED_SHIFT;

                    // Dash ledge pop: pop up only if overlap is within range
                    int popped = 0;
                    if (player->dashing > 0) {
                        int originalY = player->y;
                        int baseScreenX = snappedX >> FIXED_SHIFT;
                        int requiredPopPx = playerBottom - tileTop;
                        int requiredPop = requiredPopPx << FIXED_SHIFT;

                        if (requiredPopPx > 0 && requiredPop <= DASH_LEDGE_POP_HEIGHT) {
                            int newY = originalY - requiredPop;
                            int newScreenY = newY >> FIXED_SHIFT;
                            if (!isPositionColliding(level, baseScreenX, newScreenY)) {
                                player->y = newY;
                                popped = 1;
                            }
                        }
                    }

                    player->x = snappedX;
                    if (!popped) {
                        player->vx = 0;
                    }
                    return;
                }
            }
        }
    }
}

void collideVertical(Player* player, const Level* level) {
    // Vertical sweep
    player->y += player->vy;
    int screenX = player->x >> FIXED_SHIFT;
    int screenY = player->y >> FIXED_SHIFT;

    player->onGround = 0;

    // Ceiling bounds
    if (screenY - PLAYER_RADIUS < 0) {
        player->y = PLAYER_RADIUS << FIXED_SHIFT;
        player->vy = 0;
    } else {
        // Check for tile collision at new Y position
        int tileMinX = (screenX - PLAYER_RADIUS) / 8;
        int tileMaxX = (screenX + PLAYER_RADIUS) / 8;
        int tileMinY = (screenY - PLAYER_RADIUS) / 8;
        int tileMaxY = (screenY + PLAYER_RADIUS) / 8;

        for (int ty = tileMinY; ty <= tileMaxY; ty++) {
            for (int tx = tileMinX; tx <= tileMaxX; tx++) {
                u16 tile = getTileAt(level, 0, tx, ty);
                if (!isTileSolid(level, tile)) continue;

                int tileLeft = tx * 8;
                int tileRight = (tx + 1) * 8;
                int tileTop = ty * 8;
                int tileBottom = (ty + 1) * 8;

                int playerLeft = screenX - PLAYER_RADIUS;
                int playerRight = screenX + PLAYER_RADIUS;
                int playerTop = screenY - PLAYER_RADIUS;
                int playerBottom = screenY + PLAYER_RADIUS;

                if (playerRight > tileLeft && playerLeft < tileRight &&
                    playerBottom > tileTop && playerTop < tileBottom) {
                    // Collision - snap to tile edge
                    if (player->vy > 0) {
                        // Moving down, snap to top of tile
                        player->y = (tileTop - PLAYER_RADIUS) << FIXED_SHIFT;
                        player->vy = 0;
                        player->onGround = 1;
                        if (player->dashing > 0) player->dashing = 0;
                    } else {
                        // Moving up, try corner correction before snapping
                        int originalX = player->x;
                        int nudged = 0;

                        for (int nudge = FIXED_ONE; nudge <= BONK_NUDGE_RANGE; nudge += FIXED_ONE) {
                            int newXRight = originalX + nudge;
                            int newScreenXRight = newXRight >> FIXED_SHIFT;
                            int clearRight = !isPositionColliding(level, newScreenXRight, screenY);

                            int newXLeft = originalX - nudge;
                            int newScreenXLeft = newXLeft >> FIXED_SHIFT;
                            int clearLeft = !isPositionColliding(level, newScreenXLeft, screenY);

                            if (clearRight ^ clearLeft) {
                                if (clearRight) {
                                    player->x = newXRight;
                                } else {
                                    player->x = newXLeft;
                                }
                                nudged = 1;
                                break;
                            }
                        }

                        if (!nudged) {
                            player->x = originalX;
                            player->y = (tileBottom + PLAYER_RADIUS) << FIXED_SHIFT;
                            player->vy = 0;
                        }
                    }
                    return;
                }
            }
        }
    }

    // Ground check for standing still
    if (!player->onGround && player->vy >= 0) {
        screenX = player->x >> FIXED_SHIFT;
        screenY = player->y >> FIXED_SHIFT;
        int playerBottom = screenY + PLAYER_RADIUS;
        int playerLeft = screenX - PLAYER_RADIUS;
        int playerRight = screenX + PLAYER_RADIUS;
        int feetY = (screenY + PLAYER_RADIUS + 1) / 8;
        int tileMinX = (screenX - PLAYER_RADIUS) / 8;
        int tileMaxX = (screenX + PLAYER_RADIUS) / 8;

        for (int tx = tileMinX; tx <= tileMaxX; tx++) {
            u16 tile = getTileAt(level, 0, tx, feetY);
            if (isTileSolid(level, tile)) {
                int tileTop = feetY * 8;
                int tileLeft = tx * 8;
                int tileRight = (tx + 1) * 8;
                if (playerRight > tileLeft && playerLeft < tileRight &&
                    playerBottom >= tileTop - 1 && playerBottom <= tileTop + 1) {
                    player->onGround = 1;
                    break;
                }
            }
        }
    }
}
