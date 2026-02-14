#include "spring.h"
#include "player/player.h"
#include "core/game_math.h"
#include <string.h>  // For memset

void initSpringManager(SpringManager* manager) {
    manager->count = 0;
    memset(manager->springs, 0, sizeof(manager->springs));
}

void loadSpringsFromLevel(SpringManager* manager, const Level* level) {
    manager->count = 0;

    // Scan level objects for springs
    for (int i = 0; i < level->objectCount && manager->count < MAX_SPRINGS; i++) {
        const LevelObject* obj = &level->objects[i];

        // Check if this is a Spring object (compile-time enum check, no strcmp!)
        if (obj->type == OBJ_SPRING || obj->type == OBJ_SPRING_SUPER ||
            obj->type == OBJ_SPRING_WALL_LEFT || obj->type == OBJ_SPRING_WALL_RIGHT) {

            Spring* spring = &manager->springs[manager->count];

            // Map object type to spring type
            switch (obj->type) {
                case OBJ_SPRING:
                    spring->type = SPRING_NORMAL;
                    break;
                case OBJ_SPRING_SUPER:
                    spring->type = SPRING_SUPER;
                    break;
                case OBJ_SPRING_WALL_LEFT:
                    spring->type = SPRING_WALL_LEFT;
                    break;
                case OBJ_SPRING_WALL_RIGHT:
                    spring->type = SPRING_WALL_RIGHT;
                    break;
                default:
                    spring->type = SPRING_NORMAL;
                    break;
            }

            spring->x = obj->x;
            spring->y = obj->y;
            spring->width = 8;   // Default 8x8 hitbox
            spring->height = 8;
            spring->active = 1;

            manager->count++;
        }
    }
}

void updateSprings(SpringManager* manager, Player* player) {
    // Get player hitbox in pixels
    int playerX = player->x >> FIXED_SHIFT;
    int playerY = player->y >> FIXED_SHIFT;
    int playerLeft = playerX - PLAYER_RADIUS_X;
    int playerRight = playerX + PLAYER_RADIUS_X;
    int playerTop = playerY - PLAYER_RADIUS_Y;
    int playerBottom = playerY + PLAYER_RADIUS_Y;

    // Check collision with each spring
    for (int i = 0; i < manager->count; i++) {
        Spring* spring = &manager->springs[i];
        if (!spring->active) continue;

        // Spring hitbox
        int springLeft = spring->x;
        int springRight = spring->x + spring->width;
        int springTop = spring->y;
        int springBottom = spring->y + spring->height;

        // AABB collision check
        if (playerRight > springLeft && playerLeft < springRight &&
            playerBottom > springTop && playerTop < springBottom) {

            // Player collided with spring - trigger bounce
            switch (spring->type) {
                case SPRING_NORMAL:
                    // Normal spring bounces from top
                    playerBounce(player, (float)springTop);
                    break;

                case SPRING_SUPER:
                    // Super spring bounces from top
                    playerSuperBounce(player, (float)springTop);
                    break;

                case SPRING_WALL_LEFT:
                    // Left wall spring (pushes player left)
                    playerSideBounce(player, -1, (float)springLeft, (float)(springTop + spring->height / 2));
                    break;

                case SPRING_WALL_RIGHT:
                    // Right wall spring (pushes player right)
                    playerSideBounce(player, 1, (float)springRight, (float)(springTop + spring->height / 2));
                    break;
            }

            // Spring triggered - could add animation/sound here
        }
    }
}

void renderSprings(const SpringManager* manager, int cameraX, int cameraY) {
#ifndef DESKTOP_BUILD
    // Render all springs using hardware OBJ sprites (sprites 16-47)
    volatile u16* oam = (volatile u16*)MEM_OAM;

    for (int i = 0; i < manager->count; i++) {
        const Spring* spring = &manager->springs[i];
        if (i >= 32) break;  // Only 32 spring sprites available

        int spriteIndex = 16 + i;  // Sprites 16-47 for springs
        volatile u16* spriteAttrs = &oam[spriteIndex * 4];

        // Check if spring is active and onscreen
        if (!spring->active) {
            // Hide inactive springs
            spriteAttrs[0] = 160;  // Y offscreen
            continue;
        }

        // Convert to screen coordinates
        int screenX = spring->x - cameraX;
        int screenY = spring->y - cameraY;

        // Check if offscreen (with margin for sprite size)
        if (screenX + spring->width < 0 || screenX >= 240 ||
            screenY + spring->height < 0 || screenY >= 160) {
            // Hide offscreen springs
            spriteAttrs[0] = 160;  // Y offscreen
            continue;
        }

        // OAM Attribute 0: Y position and shape
        // Bits 0-7: Y coordinate (0-255)
        // Bits 8-9: Object mode (0=normal, 1=semi-transparent, 2=object window)
        // Bits 10-11: GFX mode (0=normal)
        // Bits 12-13: Mosaic and OBJ disable flags
        // Bits 14-15: Color mode (0=16 colors, 1=256 colors) and Shape (0=square, 1=horizontal, 2=vertical)
        spriteAttrs[0] = (screenY & 0xFF) | (0 << 14);  // Y position, square shape

        // OAM Attribute 1: X position and size
        // Bits 0-8: X coordinate (0-511)
        // Bits 12-13: H flip, V flip
        // Bits 14-15: Size (0=8x8, 1=16x16, 2=32x32, 3=64x64 for square)
        spriteAttrs[1] = (screenX & 0x1FF) | (0 << 14);  // X position, 8x8 size

        // OAM Attribute 2: Tile index and palette
        // Bits 0-9: Tile index
        // Bits 10-11: Priority (0=highest)
        // Bits 12-15: Palette bank
        spriteAttrs[2] = 4 | (0 << 10) | (11 << 12);  // Tile 4, priority 0, palette bank 11 (spring palette)
    }

    // Hide remaining unused spring sprite slots
    for (int i = manager->count; i < 32; i++) {
        int spriteIndex = 16 + i;
        oam[spriteIndex * 4] = 160;  // Y offscreen
    }
#endif // !DESKTOP_BUILD
}
