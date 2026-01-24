#include "camera.h"

void updateCamera(Camera* camera, Player* player, const Level* level) {
    // Camera follows player with dead zone
    int playerScreenX = (player->x >> FIXED_SHIFT) - camera->x;
    int playerScreenY = (player->y >> FIXED_SHIFT) - camera->y;

    // Horizontal camera
    int deadZoneLeft = SCREEN_WIDTH / 3;
    int deadZoneRight = 2 * SCREEN_WIDTH / 3;

    if (playerScreenX < deadZoneLeft) {
        camera->x += playerScreenX - deadZoneLeft;
    } else if (playerScreenX > deadZoneRight) {
        camera->x += playerScreenX - deadZoneRight;
    }

    // Vertical camera
    int deadZoneTop = SCREEN_HEIGHT / 3;
    int deadZoneBottom = 2 * SCREEN_HEIGHT / 3;

    if (playerScreenY < deadZoneTop) {
        camera->y += playerScreenY - deadZoneTop;
    } else if (playerScreenY > deadZoneBottom) {
        camera->y += playerScreenY - deadZoneBottom;
    }

    // Clamp camera to level bounds
    int maxCameraX = level->width * 8 - SCREEN_WIDTH;
    int maxCameraY = level->height * 8 - SCREEN_HEIGHT;

    if (camera->x < 0) camera->x = 0;
    if (camera->x > maxCameraX) camera->x = maxCameraX;
    if (camera->y < 0) camera->y = 0;
    if (camera->y > maxCameraY) camera->y = maxCameraY;
}
