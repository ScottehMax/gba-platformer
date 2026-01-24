#include "player.h"

void initPlayer(Player* player, const Level* level) {
    player->x = level->playerSpawnX << FIXED_SHIFT;
    player->y = level->playerSpawnY << FIXED_SHIFT;
    player->vx = 0;
    player->vy = 0;
    player->onGround = 0;
    player->coyoteTime = 0;
    player->dashing = 0;
    player->dashCooldown = 0;
    player->facingRight = 1;
    player->prevKeys = 0;
    player->trailIndex = 0;
    player->trailTimer = 0;
    player->trailFadeTimer = TRAIL_LENGTH * 2;  // Start fully faded

    // Initialize trail positions off-screen
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        player->trailX[i] = -1000 << FIXED_SHIFT;
        player->trailY[i] = -1000 << FIXED_SHIFT;
        player->trailFacing[i] = player->facingRight;
    }
}

void updatePlayer(Player* player, u16 keys, const Level* level) {
    // Detect button presses (pressed this frame but not last frame)
    u16 pressed = keys & ~player->prevKeys;

    // Dash cooldown
    if (player->dashCooldown > 0) {
        player->dashCooldown--;
    }

    // R button: Dash (8-directional or forward) - only on press, not hold
    if ((pressed & KEY_R) && player->dashCooldown == 0 && player->dashing == 0) {
        player->dashing = 8; // Dash lasts 8 frames
        player->dashCooldown = 30; // 30 frames cooldown
        player->trailFadeTimer = 0; // Reset fade timer for new dash

        // Clear old trail positions
        for (int i = 0; i < TRAIL_LENGTH; i++) {
            player->trailX[i] = -1000 << FIXED_SHIFT;
            player->trailY[i] = -1000 << FIXED_SHIFT;
        }

        // Check which directions are held
        int dashX = 0, dashY = 0;
        if (keys & KEY_LEFT) dashX = -1;
        if (keys & KEY_RIGHT) dashX = 1;
        if (keys & KEY_UP) dashY = -1;
        if (keys & KEY_DOWN) dashY = 1;

        // If no direction held, dash forward based on facing
        if (dashX == 0 && dashY == 0) {
            dashX = player->facingRight ? 1 : -1;
        }

        // Apply dash velocity (normalize diagonals to maintain speed)
        if (dashX != 0 && dashY != 0) {
            // Diagonal: multiply by ~0.707 (using 181/256 as approximation)
            player->vx = (dashX * DASH_SPEED * 181) >> 8;
            player->vy = (dashY * DASH_SPEED * 181) >> 8;
        } else {
            player->vx = dashX * DASH_SPEED;
            player->vy = dashY * DASH_SPEED;
        }
    }

    // Countdown dash timer
    if (player->dashing > 0) {
        player->dashing--;
        // Start fade timer when dash ends
        if (player->dashing == 0) {
            player->trailTimer = 0;
            player->trailFadeTimer = 0;
        }
    }

    // Update trail fade after dash ends (10 sprites * 2 frames each = 20 frames)
    if (player->dashing == 0 && player->trailFadeTimer < TRAIL_LENGTH * 2) {
        player->trailFadeTimer++;
    }

    // Horizontal movement (disabled during dash)
    if (player->dashing == 0) {
        if (keys & KEY_LEFT) {
            player->vx -= ACCELERATION;
            if (player->vx < -MAX_SPEED) {
                player->vx = -MAX_SPEED;
            }
            player->facingRight = 0;  // Face left
        } else if (keys & KEY_RIGHT) {
            player->vx += ACCELERATION;
            if (player->vx > MAX_SPEED) {
                player->vx = MAX_SPEED;
            }
            player->facingRight = 1;  // Face right
        } else {
            // Apply friction when no input
            int friction = player->onGround ? FRICTION : AIR_FRICTION;
            if (player->vx > 0) {
                player->vx -= friction;
                if (player->vx < 0) player->vx = 0;
            } else if (player->vx < 0) {
                player->vx += friction;
                if (player->vx > 0) player->vx = 0;
            }
        }
    }

    // A button: Jump - only on press, not hold
    // Allow jump if grounded OR within coyote time window
    if ((pressed & KEY_A) && (player->onGround || player->coyoteTime > 0)) {
        player->vy = -JUMP_STRENGTH;
        player->onGround = 0;
        player->coyoteTime = 0;  // Consume coyote time on jump
    }

    // Apply gravity (but not during dash, to preserve dash trajectory)
    if (!player->onGround && player->dashing == 0) {
        player->vy += GRAVITY;
    }

    // Swept collision - move along each axis and stop at first collision
    collideHorizontal(player, level);
    collideVertical(player, level);

    // Update coyote time
    if (player->onGround) {
        player->coyoteTime = COYOTE_TIME;  // Reset when grounded
    } else if (player->coyoteTime > 0) {
        player->coyoteTime--;  // Count down when airborne
    }

    // Update dash trail (record every 2 frames for spacing)
    // Continue recording for a bit after dash ends to fill the trail buffer
    if (player->dashing > 0 || player->trailFadeTimer < 10) {
        player->trailTimer++;
        if (player->trailTimer >= 2) {
            player->trailTimer = 0;
            player->trailIndex = (player->trailIndex + 1) % TRAIL_LENGTH;
            player->trailX[player->trailIndex] = player->x;
            player->trailY[player->trailIndex] = player->y;
            player->trailFacing[player->trailIndex] = player->facingRight;
        }
    }

    // Update previous keys for next frame
    player->prevKeys = keys;
}
