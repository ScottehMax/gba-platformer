#include "collision.h"

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
                u8 tile = getTileAt(level, tx, ty);
                if (!isTileSolid(tile)) continue;

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
                    if (player->vx > 0) {
                        // Moving right, snap to left edge of tile
                        player->x = (tileLeft - PLAYER_RADIUS) << FIXED_SHIFT;
                    } else {
                        // Moving left, snap to right edge of tile
                        player->x = (tileRight + PLAYER_RADIUS) << FIXED_SHIFT;
                    }
                    player->vx = 0;
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
                u8 tile = getTileAt(level, tx, ty);
                if (!isTileSolid(tile)) continue;

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
                        // Moving up, snap to bottom of tile
                        player->y = (tileBottom + PLAYER_RADIUS) << FIXED_SHIFT;
                        player->vy = 0;
                    }
                    return;
                }
            }
        }
    }

    // Ground check for standing still
    if (!player->onGround) {
        screenX = player->x >> FIXED_SHIFT;
        screenY = player->y >> FIXED_SHIFT;
        int feetY = (screenY + PLAYER_RADIUS + 1) / 8;
        int checkX = screenX / 8;

        u8 tile = getTileAt(level, checkX, feetY);
        if (isTileSolid(tile)) {
            int tileTop = feetY * 8;
            if (screenY + PLAYER_RADIUS >= tileTop - 1) {
                player->onGround = 1;
            }
        }
    }
}
