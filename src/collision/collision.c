#include "collision.h"

static int isPositionColliding(const Level* level, int screenX, int screenY) {
    // 8x11 hitbox with configurable Y shift (adjust PLAYER_HITBOX_Y_SHIFT to change sprite ground position)
    int playerLeft = screenX - PLAYER_WIDTH / 2;
    int playerRight = screenX + PLAYER_WIDTH / 2;
    int playerTop = PLAYER_TOP(screenY);
    int playerBottom = PLAYER_BOTTOM(screenY);

    int tileMinX = playerLeft / 8;
    int tileMaxX = playerRight / 8;
    int tileMinY = playerTop / 8;
    int tileMaxY = playerBottom / 8;

    for (int ty = tileMinY; ty <= tileMaxY; ty++) {
        for (int tx = tileMinX; tx <= tileMaxX; tx++) {
            if (getTileCollision(level, tx, ty) != COL_SOLID) continue;

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
    int halfWidth = PLAYER_WIDTH / 2;
    if (screenX < halfWidth) {
        player->x = halfWidth << FIXED_SHIFT;
        player->vx = 0;
    } else if (screenX > levelWidthPx - halfWidth) {
        player->x = (levelWidthPx - halfWidth) << FIXED_SHIFT;
        player->vx = 0;
    } else {
        // Check for tile collision at new X position
        screenX = player->x >> FIXED_SHIFT;
        int playerLeft = screenX - PLAYER_WIDTH / 2;
        int playerRight = screenX + PLAYER_WIDTH / 2;
        int playerTop = PLAYER_TOP(screenY);
        int playerBottom = PLAYER_BOTTOM(screenY);

        int tileMinX = playerLeft / 8;
        int tileMaxX = playerRight / 8;
        int tileMinY = playerTop / 8;
        int tileMaxY = playerBottom / 8;

        for (int ty = tileMinY; ty <= tileMaxY; ty++) {
            for (int tx = tileMinX; tx <= tileMaxX; tx++) {
                // JumpThru platforms don't block horizontal movement
                if (getTileCollision(level, tx, ty) != COL_SOLID) continue;

                int tileLeft = tx * 8;
                int tileRight = (tx + 1) * 8;
                int tileTop = ty * 8;
                int tileBottom = (ty + 1) * 8;

                if (playerRight > tileLeft && playerLeft < tileRight &&
                    playerBottom > tileTop && playerTop < tileBottom) {
                    // Collision - snap to tile edge instead of reverting
                    int snappedX = player->vx > 0
                        ? (tileLeft - PLAYER_WIDTH / 2) << FIXED_SHIFT
                        : (tileRight + PLAYER_WIDTH / 2) << FIXED_SHIFT;

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
    if (PLAYER_TOP(screenY) < 0) {
        player->y = -PLAYER_TOP(0) << FIXED_SHIFT;
        player->vy = 0;
    } else {
        // Check for tile collision at new Y position
        int playerLeft = screenX - PLAYER_WIDTH / 2;
        int playerRight = screenX + PLAYER_WIDTH / 2;
        int playerTop = PLAYER_TOP(screenY);
        int playerBottom = PLAYER_BOTTOM(screenY);

        int tileMinX = playerLeft / 8;
        int tileMaxX = playerRight / 8;
        int tileMinY = playerTop / 8;
        int tileMaxY = playerBottom / 8;

        for (int ty = tileMinY; ty <= tileMaxY; ty++) {
            for (int tx = tileMinX; tx <= tileMaxX; tx++) {
                CollisionType col = getTileCollision(level, tx, ty);

                // JumpThru: only block when falling (vy > 0), not when moving up
                if (col == COL_JUMPTHRU) {
                    if (player->vy <= 0) continue;  // Don't block upward movement
                } else if (col != COL_SOLID) {
                    continue;
                }

                int tileLeft = tx * 8;
                int tileRight = (tx + 1) * 8;
                int tileTop = ty * 8;
                int tileBottom = (ty + 1) * 8;

                if (playerRight > tileLeft && playerLeft < tileRight &&
                    playerBottom > tileTop && playerTop < tileBottom) {

                    if (player->vy > 0) {
                        // Moving down - snap to top of tile
                        // JumpThru: only snap if player was above the platform top before this frame
                        if (col == COL_JUMPTHRU) {
                            int prevBottom = playerBottom - (player->vy >> FIXED_SHIFT);
                            if (prevBottom > tileTop) continue;  // Was already below top, skip
                        }
                        player->y = (tileTop - PLAYER_HEIGHT / 2 - PLAYER_HITBOX_Y_SHIFT) << FIXED_SHIFT;
                        player->vy = 0;
                        player->onGround = 1;
                    } else {
                        // Moving up - only solid tiles block upward (JumpThru already filtered above)
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
                            // PLAYER_TOP(Y) = tileBottom, so solve for Y
                            player->y = (tileBottom + PLAYER_HEIGHT / 2 + 1 - PLAYER_HITBOX_Y_SHIFT) << FIXED_SHIFT;
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
        int playerBottom = PLAYER_BOTTOM(screenY);
        int playerLeft = screenX - PLAYER_WIDTH / 2;
        int playerRight = screenX + PLAYER_WIDTH / 2;
        int feetY = (playerBottom + 1) / 8;
        int tileMinX = playerLeft / 8;
        int tileMaxX = playerRight / 8;

        for (int tx = tileMinX; tx <= tileMaxX; tx++) {
            CollisionType col = getTileCollision(level, tx, feetY);
            if (col == COL_SOLID || col == COL_JUMPTHRU) {
                int tileTop = feetY * 8;
                int tileLeft = tx * 8;
                int tileRight = (tx + 1) * 8;
                if (playerRight > tileLeft && playerLeft < tileRight &&
                    playerBottom >= tileTop - 1 && playerBottom <= tileTop + 1) {
                    // Snap to ground position (same as normal landing)
                    player->y = (tileTop - PLAYER_HEIGHT / 2 - PLAYER_HITBOX_Y_SHIFT) << FIXED_SHIFT;
                    player->vy = 0;
                    player->onGround = 1;
                    break;
                }
            }
        }
    }
}

int isPositionCollidingAt(const Level* level, int screenX, int screenY) {
    return isPositionColliding(level, screenX, screenY);
}

int checkWallAt(const Player* player, const Level* level, int dir, int yAdd, int dist) {
    int screenX = player->x >> FIXED_SHIFT;
    int screenY = (player->y >> FIXED_SHIFT) + yAdd;

    // Check for wall at requested distance (from player center)
    int checkX = screenX + (dir * dist);
    int checkY = screenY;

    return isPositionCollidingAt(level, checkX, checkY);
}

int checkWall(const Player* player, const Level* level, int dir) {
    return checkWallAt(player, level, dir, 0, WALL_JUMP_CHECK_DIST);
}

int checkCeiling(const Player* player, const Level* level) {
    int screenX = player->x >> FIXED_SHIFT;
    int screenY = player->y >> FIXED_SHIFT;

    // When ducking, player has reduced height
    // Check if standing height would collide with ceiling
    // Standing uses full PLAYER_RADIUS_Y (8), ducking typically uses half
    // We need to check if the top of standing hitbox would hit anything

    int standingTop = PLAYER_TOP(screenY);

    // Check tiles above the player at standing height
    int tileMinX = (screenX - PLAYER_WIDTH / 2) / 8;
    int tileMaxX = (screenX + PLAYER_WIDTH / 2) / 8;
    int tileY = standingTop / 8;

    for (int tx = tileMinX; tx <= tileMaxX; tx++) {
        // Only solid tiles block unducking (JumpThru doesn't)
        if (getTileCollision(level, tx, tileY) == COL_SOLID) {
            return 1;  // Ceiling blocking
        }
    }

    return 0;  // Clear to unduck
}
