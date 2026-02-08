#include "player_render.h"
#include <tonc.h>

void drawPlayer(Player* player, Camera* camera) {
    // Draw dash trail (sprites 1-3)
    // Trail sprites: 0=first (oldest), 1=middle, 2=last (newest/closest to player)
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        // Only show trail if actively dashing or still fading
        if (player->dashing > 0 || player->trailFadeTimer < TRAIL_LENGTH * 8) {
            int trailScreenX = (player->trailX[i] >> FIXED_SHIFT) - camera->x - 8;
            int trailScreenY = (player->trailY[i] >> FIXED_SHIFT) - camera->y - 8;

            // Hide if not initialized or off screen (we use -1000 as sentinel value)
            if (trailScreenX <= -1000 || trailScreenX > 239 || trailScreenY <= -1000 || trailScreenY > 159) {
                oam_mem[i + 1].attr0 = 160 << 0;  // Hide sprite
            } else {
                // Calculate progressive fade: base age + fade progress
                // Base age: sprite 0 (oldest) = 3, sprite 1 = 2, sprite 2 (newest) = 1
                int baseAge = (TRAIL_LENGTH - i);

                // Add fade progress to make all sprites gradually fade together
                // Use finer granularity: each sprite gets 2 palette steps per fade unit
                int paletteNum = baseAge + (player->trailFadeTimer / 2);

                // Hide sprite only after it reaches max palette (fully faded)
                if (paletteNum > 10) {
                    oam_mem[i + 1].attr0 = 160 << 0;  // Hide sprite
                } else {
                    // Clamp to available palettes (1-10)
                    if (paletteNum < 1) paletteNum = 1;

                    // Use progressively lighter palettes for gradual fade effect
                    oam_mem[i + 1].attr0 = (trailScreenY & 0xFF) | (1 << 10);  // Semi-transparent mode
                    oam_mem[i + 1].attr1 = (trailScreenX & 0x1FF) | (1 << 14) | (player->trailFacing[i] ? 0 : (1 << 12));
                    oam_mem[i + 1].attr2 = (paletteNum << 12) | (1 << 10);  // tile 0, palette 1-10, priority 1
                }
            }
        } else {
            oam_mem[i + 1].attr0 = 160 << 0;  // Hide sprite
        }
    }

    // Update player sprite position - convert from world to screen coordinates
    int screenX = (player->x >> FIXED_SHIFT) - camera->x - 8;  // Center the 16x16 sprite
    int screenY = (player->y >> FIXED_SHIFT) - camera->y - 8;  // Center the 16x16 sprite

    // Clamp to visible area
    if (screenX < -16) screenX = -16;
    if (screenY < -16) screenY = -16;
    if (screenX > 239) screenX = 239;
    if (screenY > 159) screenY = 159;

    // Update sprite position (16x16, 16-color mode, palette 0, normal/opaque mode)
    // Clear bits 10-11 to ensure normal mode (not semi-transparent)
    oam_mem[0].attr0 = (screenY & 0xFF) | (0 << 10);   // attr0: Y position (8 bits), bits 10-11 = 00 (normal mode)
    oam_mem[0].attr1 = (screenX & 0x1FF) | (1 << 14) | (player->facingRight ? 0 : (1 << 12));   // attr1: X position (9 bits masked) + size 16x16 + H-flip if facing left
    oam_mem[0].attr2 = (1 << 10);              // attr2: tile 0, palette 0, priority 1
}
