#include "player_render.h"

void drawPlayer(Player* player, Camera* camera) {
    // Draw dash trail (sprites 1-10)
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        // Calculate how many frames since dash ended (for gradual fade)
        int fadeSteps = player->trailFadeTimer / 2;  // Hide one sprite every 2 frames

        // Hide this sprite if it's been faded out (fade from back to front)
        // i=0 is newest (closest to player), i=9 is oldest (farthest)
        // We want to hide oldest first, so hide when i >= (TRAIL_LENGTH - fadeSteps)
        if (player->dashing == 0 && i >= (TRAIL_LENGTH - fadeSteps)) {
            *((volatile u16*)(0x07000000 + (i + 1) * 8)) = 160 << 0;  // Hide sprite
            continue;
        }

        // Only show trail if actively dashing or still fading
        if (player->dashing > 0 || player->trailFadeTimer < TRAIL_LENGTH * 2) {
            // Show every position to maximize visible trail
            int trailIdx = (player->trailIndex - i + TRAIL_LENGTH) % TRAIL_LENGTH;
            int trailScreenX = (player->trailX[trailIdx] >> FIXED_SHIFT) - camera->x - 8;
            int trailScreenY = (player->trailY[trailIdx] >> FIXED_SHIFT) - camera->y - 8;

            // Hide if off screen
            if (trailScreenX < -1000 || trailScreenX > 239 || trailScreenY < -1000 || trailScreenY > 159) {
                *((volatile u16*)(0x07000000 + (i + 1) * 8)) = 160 << 0;  // Hide sprite
            } else {
                // Calculate sprite "age" for palette selection (older = lighter/more transparent)
                // i=0 is newest, i=9 is oldest
                // fadeSteps adds extra age during fadeout for smooth transition
                int spriteAge = i + fadeSteps; // Combine position and fade
                int paletteNum = spriteAge; // 0-9 range for 10 palettes
                if (paletteNum > 9) paletteNum = 9; // Clamp to available palettes (1-10)
                if (paletteNum < 0) paletteNum = 0;

                // Use progressively lighter palettes (1-10) for very gradual fading effect
                *((volatile u16*)(0x07000000 + (i + 1) * 8)) = (trailScreenY & 0xFF) | (1 << 10);  // Semi-transparent mode
                *((volatile u16*)(0x07000000 + (i + 1) * 8 + 2)) = trailScreenX | (1 << 14) | (player->trailFacing[trailIdx] ? 0 : (1 << 12));
                *((volatile u16*)(0x07000000 + (i + 1) * 8 + 4)) = ((paletteNum + 1) << 12);  // tile 0, palette 1-10 based on age
            }
        } else {
            *((volatile u16*)(0x07000000 + (i + 1) * 8)) = 160 << 0;  // Hide sprite
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
    *((volatile u16*)0x07000000) = (screenY & 0xFF) | (0 << 10);   // attr0: Y position (8 bits), bits 10-11 = 00 (normal mode)
    *((volatile u16*)0x07000002) = screenX | (1 << 14) | (player->facingRight ? 0 : (1 << 12));   // attr1: X + size 16x16 + H-flip if facing left
    *((volatile u16*)0x07000004) = 0;                      // attr2: tile 0, palette 0
}
