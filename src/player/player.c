#include "player.h"
#include "state.h"
#include "util/calc.h"
#include "core/input.h"

void initPlayer(Player* player, const Level* level) {
    player->x = level->playerSpawnX << FIXED_SHIFT;
    player->y = level->playerSpawnY << FIXED_SHIFT;
    player->vx = 0;
    player->vy = 0;
    player->maxFall = MAX_FALL_SPEED;
    player->onGround = 0;
    player->wasOnGround = 0;
    player->coyoteTime = 0;
    player->jumpBuffer = 0;
    player->jumpHeld = 0;
    player->autoJump = 0;
    player->autoJumpTimer = 0;
    player->liftBoostX = 0;
    player->liftBoostY = 0;
    player->varJumpSpeed = 0;
    player->varJumpTimer = 0;
    player->dashing = 0;
    player->dashes = 1;  // Start with 1 dash
    player->maxDashes = 1;
    player->dashCooldownTimer = 0;
    player->dashRefillCooldownTimer = 0;
    player->facingRight = 1;
    player->prevKeys = 0;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->wallSlideDir = 0;
    player->dashAttackTimer = 0;
    player->dashDirX = 0;
    player->dashDirY = 0;
    player->beforeDashSpeedX = 0;
    player->ducking = 0;
    player->lastAimX = 1 << FIXED_SHIFT;  // Default to right (Celeste line 350)
    player->lastAimY = 0;
    clearPlayerDashTrail(player);

    // Initialize climb state
    player->stamina = CLIMB_MAX_STAMINA;
    player->climbNoMoveTimer = 0;
    player->lastClimbMove = 0;
    player->wallBoostTimer = 0;
    player->wallBoostDir = 0;

    // Initialize climb hop system (Celeste line 218-219)
    player->hopWaitX = 0;
    player->hopWaitXSpeed = 0;
    player->forceMoveX = 0;
    player->forceMoveXTimer = 0;

    // Initialize HitSquash state
    player->hitSquashNoMoveTimer = 0;

    // Initialize Boost state
    player->boostTargetX = 0;
    player->boostTargetY = 0;
    player->boostRed = 0;
    player->boostTimer = 0;
    player->currentBubbleX = -1000;  // Sentinel value for no current bubble
    player->currentBubbleY = -1000;

    // Initialize state machine (Celeste line 322-332)
    initStateMachine(&player->stateMachine);
    setStateCallbacks(&player->stateMachine, ST_NORMAL, normalUpdate, normalBegin, normalEnd);
    setStateCallbacks(&player->stateMachine, ST_DASH, dashUpdate, dashBegin, dashEnd);
    setStateCallbacks(&player->stateMachine, ST_CLIMB, climbUpdate, climbBegin, climbEnd);
    setStateCallbacks(&player->stateMachine, ST_BOOST, boostUpdate, boostBegin, boostEnd);
    setStateCallbacks(&player->stateMachine, ST_RED_DASH, redDashUpdate, redDashBegin, redDashEnd);
    setStateCallbacks(&player->stateMachine, ST_HIT_SQUASH, hitSquashUpdate, hitSquashBegin, hitSquashEnd);
    // TODO: Add more states as needed

    // Set initial state to Normal
    setState(&player->stateMachine, ST_NORMAL, player, level);
}

void updatePlayer(Player* player, u16 keys, const Level* level) {
    // === PRE-STATE UPDATE LOGIC ===
    // Timers that tick down every frame regardless of state

    // AutoJump timer (Celeste line 747-757)
    // Dash sets AutoJump=true and AutoJumpTimer=0, which stays active until landing
    if (player->autoJumpTimer > 0) {
        if (player->autoJump) {
            player->autoJumpTimer--;
            if (player->autoJumpTimer <= 0) {
                player->autoJump = 0;
            }
        } else {
            player->autoJumpTimer = 0;
        }
    }

    // Dash cooldowns (Celeste ticks these in engine update)
    if (player->dashCooldownTimer > 0) {
        player->dashCooldownTimer--;
    }
    if (player->dashRefillCooldownTimer > 0) {
        player->dashRefillCooldownTimer--;
    }

    // Jump buffer decay
    if (player->jumpBuffer > 0) {
        player->jumpBuffer--;
    }

    // Var jump timer decay
    if (player->varJumpTimer > 0) {
        player->varJumpTimer--;
    }

    // Dash attack timer decay
    if (player->dashAttackTimer > 0) {
        player->dashAttackTimer--;
    }

    // Wall slide timer decay when not actively wall sliding
    if (player->wallSlideDir == 0 && player->wallSlideTimer > 0) {
        player->wallSlideTimer--;
    }
    player->wallSlideDir = 0;  // Reset each frame, state will set if wall sliding

    // Trail fade timer (for dash trails after dash ends)
    if (player->dashing == 0 && player->trailFadeTimer < TRAIL_LENGTH * 8) {
        player->trailFadeTimer++;
    }

    // Climb no move timer
    if (player->climbNoMoveTimer > 0) {
        player->climbNoMoveTimer--;
    }

    // Wall boost timer
    if (player->wallBoostTimer > 0) {
        player->wallBoostTimer--;
        // Wall Boost (Celeste line 689-698)
        // After climb jump with no horizontal input, pressing away from wall converts to wall jump
        int moveX = inputMoveX(keys);
        if (moveX == player->wallBoostDir) {
            // Convert climb jump to wall jump and refund stamina
            player->vx = player->wallBoostDir * WALL_JUMP_H_SPEED;
            player->stamina += CLIMB_JUMP_COST;
            player->wallBoostTimer = 0;
        }
    }

    // Force move X timer (Celeste line 762)
    if (player->forceMoveXTimer > 0) {
        player->forceMoveXTimer--;
    }

    // Update facing based on input (Celeste line 786-794)
    // This allows reverse hypers: dash one direction, hold opposite direction, jump
    // NOTE: Does NOT update during climb, RedDash, HitSquash, or pickup
    int moveX = inputMoveX(keys);
    if (moveX != 0 &&
        player->stateMachine.state != ST_CLIMB &&
        player->stateMachine.state != ST_RED_DASH &&
        player->stateMachine.state != ST_HIT_SQUASH) {
        player->facingRight = moveX > 0 ? 1 : 0;
    }

    // Update lastAim (Celeste line 797)
    // Get aim direction from input, or default to facing direction
    int aimX = 0, aimY = 0;
    if (keys & BTN_LEFT) aimX = -1;
    if (keys & BTN_RIGHT) aimX = 1;
    if (keys & BTN_UP) aimY = -1;
    if (keys & BTN_DOWN) aimY = 1;

    // If no direction held, aim in facing direction
    if (aimX == 0 && aimY == 0) {
        aimX = player->facingRight ? 1 : -1;
    }

    // Normalize diagonal aim (same as dash diagonal normalization)
    if (aimX != 0 && aimY != 0) {
        player->lastAimX = aimX * FP_DIAG_NORMALIZE;
        player->lastAimY = aimY * FP_DIAG_NORMALIZE;
    } else {
        player->lastAimX = aimX << FIXED_SHIFT;
        player->lastAimY = aimY << FIXED_SHIFT;
    }

    // === STATE MACHINE UPDATE ===
    // This is where the state-specific logic runs (Celeste line 632: StateMachine.Update())
    updateStateMachine(&player->stateMachine, player, keys, level);

    // === POST-STATE UPDATE LOGIC ===
    // Physics and collision that happens after state logic

    // Apply collision (Celeste does this in Actor.Update via MoveH/MoveV)
    // Save velocity before collision to detect wall/ceiling hits
    int prevVx = player->vx;
    int prevVy = player->vy;

    collideHorizontal(player, level);

    // RedDash horizontal collision handling (Celeste line 2445-2449, 2711-2712)
    // If RedDash hits a wall → HitSquash, if hits bounds → Normal
    if (player->stateMachine.state == ST_RED_DASH) {
        int hitHorizontal = (prevVx != 0 && player->vx == 0);
        if (hitHorizontal) {
            int screenX = player->x >> FIXED_SHIFT;
            int halfWidth = PLAYER_WIDTH / 2;
            int levelWidthPx = level->width * 8;
            int atBounds = (screenX <= halfWidth || screenX >= levelWidthPx - halfWidth);

            if (atBounds) {
                // Hit level bounds (Celeste OnBoundsH line 2711-2712)
                setState(&player->stateMachine, ST_NORMAL, player, level);
            } else {
                // Hit wall (Celeste OnCollideH line 2445-2449)
                setState(&player->stateMachine, ST_HIT_SQUASH, player, level);
            }
        }
    }

    // Dash Floor Snapping (Celeste line 920-925)
    // During horizontal dash-attack, snap down up to 3px to stay grounded.
    // TIMING: Runs pre-vertical-physics to match Celeste (snap at line 920, MoveV at 935).
    // Uses previous frame's onGround (collideVertical hasn't reset it yet).
    // vy >= 0 guard: prevents snap from cancelling a jump set this frame by the state machine.
    if (!player->onGround && player->dashAttackTimer > 0 && player->dashDirY == 0 && player->vy >= 0) {
        int screenX = player->x >> FIXED_SHIFT;
        int screenY = player->y >> FIXED_SHIFT;
        int playerLeft = screenX - PLAYER_WIDTH / 2;
        int playerRight = screenX + PLAYER_WIDTH / 2;
        int playerBottom = PLAYER_BOTTOM(screenY);
        int tileMinX = playerLeft / 8;
        int tileMaxX = playerRight / 8;
        int tileMinY = (playerBottom + 1) / 8;
        int tileMaxY = (playerBottom + DASH_V_FLOOR_SNAP_DIST) / 8;
        int snapped = 0;
        for (int ty = tileMinY; ty <= tileMaxY && !snapped; ty++) {
            int tileTop = ty * 8;
            if (tileTop <= playerBottom) continue;  // Must be strictly below player bottom
            for (int tx = tileMinX; tx <= tileMaxX && !snapped; tx++) {
                CollisionType col = getTileCollision(level, tx, ty);
                if (col != COL_SOLID && col != COL_JUMPTHRU) continue;
                int tileLeft = tx * 8;
                int tileRight = (tx + 1) * 8;
                if (playerRight <= tileLeft || playerLeft >= tileRight) continue;
                // Snap to floor (matches MoveVExact(DashVFloorSnapDist) stopping at tile)
                player->y = (tileTop - PLAYER_HEIGHT / 2 - PLAYER_HITBOX_Y_SHIFT) << FIXED_SHIFT;
                player->vy = 0;
                player->onGround = 1;
                snapped = 1;
            }
        }
    }

    collideVertical(player, level);

    // RedDash vertical collision handling (Celeste line 2634-2638, 2719-2720)
    // If RedDash hits ceiling or floor → HitSquash, if hits bounds → Normal
    if (player->stateMachine.state == ST_RED_DASH) {
        int hitVertical = (prevVy != 0 && player->vy == 0);
        if (hitVertical) {
            int screenY = player->y >> FIXED_SHIFT;
            int atTopBounds = (PLAYER_TOP(screenY) <= 0);
            int atBottomBounds = (PLAYER_BOTTOM(screenY) >= level->height * 8);

            if (atTopBounds || atBottomBounds) {
                // Hit vertical bounds (Celeste OnBoundsV line 2719-2720)
                setState(&player->stateMachine, ST_NORMAL, player, level);
            } else {
                // Hit ceiling or floor (Celeste OnCollideV line 2634-2638)
                setState(&player->stateMachine, ST_HIT_SQUASH, player, level);
            }
        }
    }

    // Hop Wait X - delays horizontal movement after climb hop until above ledge (Celeste line 814-823)
    if (player->hopWaitX != 0) {
        // If moving away from wall OR moving down, cancel hopWaitX (Celeste line 816-817)
        int vxSign = player->vx > 0 ? 1 : (player->vx < 0 ? -1 : 0);
        if (vxSign == -player->hopWaitX || player->vy > 0) {
            player->hopWaitX = 0;
        } else {
            // Check if there's no solid at Position + UnitX * hopWaitX (Celeste line 818)
            // Check at center so full hitbox clears before applying velocity
            if (!checkWallAt(player, level, player->hopWaitX, 0, 1)) {
                // No wall ahead - apply the horizontal speed (Celeste line 820-821)
                player->vx = player->hopWaitXSpeed;
                player->hopWaitX = 0;
            }
        }
    }

    // LiftBoost application on walk-off (Celeste line 2787-2788)
    // When walking off a platform with upward velocity from moving platform
    if (player->liftBoostY < 0 && player->wasOnGround && !player->onGround && player->vy >= 0) {
        player->vy = player->liftBoostY;
    }

    // Dash coroutine resume (emulates Celeste's yield return null)
    // This sets the dash velocity AFTER collision has updated onGround
    if (player->stateMachine.state == ST_DASH) {
        dashCoroutineResume(player, keys);
    }

    // Variable jump height: cut upward velocity on release (Celeste line 1090)
    // This is shared across states
    if (player->stateMachine.state != ST_DASH && player->jumpHeld && !(keys & BTN_JUMP) && player->vy < 0) {
        player->vy /= JUMP_RELEASE_MULTIPLIER;
        player->jumpHeld = 0;
    }

    // After Dash / On Ground (Celeste line 702-707, 714-720)
    // These checks MUST happen before buffered jump execution, so they see the correct onGround state
    // Clear AutoJump when landing (not during climb)
    if (player->onGround && player->stateMachine.state != ST_CLIMB) {
        player->autoJump = 0;
        player->wallSlideTimer = WALL_SLIDE_TIME;  // Reset wall slide timer on landing
        player->stamina = CLIMB_MAX_STAMINA;  // Refill stamina on ground (Celeste line 3107-3108)
    }

    // Coyote time / Jump Grace (Celeste line 714-720)
    if (player->onGround) {
        player->coyoteTime = COYOTE_TIME;
        player->jumpHeld = 0;
    } else if (player->coyoteTime > 0) {
        player->coyoteTime--;
    }

    // Refill dash when on ground (Celeste line 735-738)
    // MUST happen before buffered jump execution!
    if (player->dashRefillCooldownTimer == 0 && player->onGround && player->dashes < player->maxDashes) {
        player->dashes = player->maxDashes;
    }

    // Execute buffered jump on landing (Celeste uses Input.Jump.Pressed logic)
    // This happens AFTER all onGround checks, because it will set onGround = 0
    if (player->jumpBuffer > 0 && player->onGround) {
        player->vy = JUMP_STRENGTH + player->liftBoostY;  // Apply lift boost
        player->vx += player->liftBoostX;  // Apply horizontal lift boost
        player->varJumpSpeed = JUMP_STRENGTH;
        player->varJumpTimer = VAR_JUMP_TIME;
        player->onGround = 0;
        player->coyoteTime = 0;
        player->jumpBuffer = 0;
        player->jumpHeld = 1;
        player->autoJump = 0;  // Clear AutoJump when manually jumping
    }

    // Dash slide check (Celeste OnCollideV line 2544-2552)
    // Applies to both Dash and RedDash when hitting floor diagonally
    if (player->stateMachine.state == ST_DASH || player->stateMachine.state == ST_RED_DASH) {
        dashSlideCheck(player);
    }

    // Consume dash when dash is triggered (handled by Normal state returning ST_DASH)
    // The actual dash consumption happens in dashBegin()

    // Update previous keys for next frame
    player->prevKeys = keys;

    // Track wasOnGround for next frame's walk-off detection (Celeste line 1082)
    // MUST be at the end after all processing, so it captures this frame's final onGround state
    player->wasOnGround = player->onGround;
}

// === HELPER FUNCTIONS ===

// Celeste Player.cs line 2002-2011
int refillDash(Player* player) {
    if (player->dashes < player->maxDashes) {
        player->dashes = player->maxDashes;
        return 1;
    }
    return 0;
}

// Celeste Player.cs line 2025-2028
void refillStamina(Player* player) {
    player->stamina = CLIMB_MAX_STAMINA;
}

// === BOUNCE FUNCTIONS ===

// Celeste Player.cs line 1844-1876
void playerBounce(Player* player, int fromY) {
    // Move player to spring top (Celeste line 1855)
    // fromY is the spring top Y position in pixels
    // player->y is fixed-point (8 bits fractional)
    int playerBottom = (player->y >> FIXED_SHIFT) + PLAYER_RADIUS_Y;
    int offsetY = fromY - playerBottom;
    player->y += offsetY << FIXED_SHIFT;

    // Refill resources (Celeste line 1856-1858)
    refillDash(player);
    refillStamina(player);

    // Clear dashing flag so trails can fade (dash trail fade fix)
    player->dashing = 0;

    // Force to normal state (Celeste line 1859)
    setState(&player->stateMachine, ST_NORMAL, player, 0);

    // Reset timers (Celeste line 1861-1867)
    player->coyoteTime = 0;  // jumpGraceTimer
    player->varJumpTimer = BOUNCE_VAR_JUMP_TIME;
    player->autoJump = 1;
    player->autoJumpTimer = BOUNCE_AUTO_JUMP_TIME;
    player->dashAttackTimer = 0;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->wallBoostTimer = 0;

    // Set velocity (Celeste line 1869)
    player->varJumpSpeed = player->vy = BOUNCE_SPEED;
    // NOTE: Does NOT cancel horizontal velocity (Speed.X unchanged in Celeste)
}

// Celeste Player.cs line 1878-1914
void playerSuperBounce(Player* player, int fromY) {
    // Move player to spring top (Celeste line 1890)
    int playerBottom = (player->y >> FIXED_SHIFT) + PLAYER_RADIUS_Y;
    int offsetY = fromY - playerBottom;
    player->y += offsetY << FIXED_SHIFT;

    // Refill resources (Celeste line 1891-1893)
    refillDash(player);
    refillStamina(player);

    // Clear dashing flag so trails can fade (dash trail fade fix)
    player->dashing = 0;

    // Force to normal state (Celeste line 1894)
    setState(&player->stateMachine, ST_NORMAL, player, 0);

    // Reset timers (Celeste line 1896-1902)
    player->coyoteTime = 0;  // jumpGraceTimer
    player->varJumpTimer = SUPER_BOUNCE_VAR_JUMP_TIME;
    player->autoJump = 1;
    player->autoJumpTimer = 0;  // AutoJumpTimer = 0 (infinite until landing)
    player->dashAttackTimer = 0;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->wallBoostTimer = 0;

    // Set velocity (Celeste line 1904-1905)
    player->vx = 0;  // CANCEL horizontal momentum
    player->varJumpSpeed = player->vy = SUPER_BOUNCE_SPEED;
}

// Celeste Player.cs line 1919-1953
void playerSideBounce(Player* player, int dir, int fromX, int fromY) {
    // Move player to spring position (Celeste line 1924-1928)
    // Vertical: clamp offset to ±4 pixels (Celeste line 1924)
    int playerBottom = (player->y >> FIXED_SHIFT) + PLAYER_RADIUS_Y;
    int offsetY = fromY - playerBottom;
    if (offsetY > 4) offsetY = 4;
    if (offsetY < -4) offsetY = -4;
    player->y += offsetY << FIXED_SHIFT;

    // Horizontal: align to spring edge based on direction
    int playerX = player->x >> FIXED_SHIFT;
    int offsetX = 0;
    if (dir > 0) {
        // Push from left side (Celeste line 1926)
        offsetX = fromX - (playerX - PLAYER_RADIUS_X);
    } else if (dir < 0) {
        // Push from right side (Celeste line 1928)
        offsetX = fromX - (playerX + PLAYER_RADIUS_X);
    }
    player->x += offsetX << FIXED_SHIFT;

    // Refill resources (Celeste line 1929-1931)
    refillDash(player);
    refillStamina(player);

    // Clear dashing flag so trails can fade (dash trail fade fix)
    player->dashing = 0;

    // Force to normal state (Celeste line 1932)
    setState(&player->stateMachine, ST_NORMAL, player, 0);

    // Reset timers (Celeste line 1934-1942)
    player->coyoteTime = 0;  // jumpGraceTimer
    player->varJumpTimer = BOUNCE_VAR_JUMP_TIME;
    player->autoJump = 1;
    player->autoJumpTimer = 0;  // AutoJumpTimer = 0 (infinite until landing)
    player->dashAttackTimer = 0;
    player->wallSlideTimer = WALL_SLIDE_TIME;
    player->forceMoveX = dir;
    player->forceMoveXTimer = SIDE_BOUNCE_FORCE_MOVE_TIME;
    player->wallBoostTimer = 0;

    // Set velocity (Celeste line 1945-1946)
    player->vx = SIDE_BOUNCE_SPEED * dir;
    player->varJumpSpeed = player->vy = BOUNCE_SPEED;
}

void playerRedBoost(Player* player, int centerX, int centerY) {
    // Celeste RedBoost() (line 3779-3786)
    // Player enters Boost state, physically moves to bubble center, then starts RedDash

    // Set target position where player will move to (Celeste line 3774, 3783-3784)
    player->boostTargetX = centerX << FIXED_SHIFT;
    player->boostTargetY = centerY << FIXED_SHIFT;
    player->boostRed = 1;  // Red bubble triggers RedDash (not regular Dash)
    player->boostTimer = BOOST_TIME;  // Auto-dash after 0.25s

    // Track which bubble player is using (Celeste CurrentBooster line 3776/3785)
    player->currentBubbleX = centerX;
    player->currentBubbleY = centerY;

    // Transition to boost state (Celeste line 3772, 3781)
    // During Boost state, player moves to center, then transitions to RedDash
    setState(&player->stateMachine, ST_BOOST, player, 0);
}

void playerGreenBoost(Player* player, int centerX, int centerY) {
    // Celeste Boost() (line 3770-3777)
    // Player enters Boost state, physically moves to bubble center, then starts regular Dash

    // Set target position where player will move to (Celeste line 3774)
    player->boostTargetX = centerX << FIXED_SHIFT;
    player->boostTargetY = centerY << FIXED_SHIFT;
    player->boostRed = 0;  // Green bubble triggers regular Dash (not RedDash)
    player->boostTimer = BOOST_TIME;  // Auto-dash after 0.25s

    // Track which bubble player is using (Celeste CurrentBooster line 3776)
    player->currentBubbleX = centerX;
    player->currentBubbleY = centerY;

    // Transition to boost state (Celeste line 3772)
    // During Boost state, player moves to center, then transitions to regular Dash
    setState(&player->stateMachine, ST_BOOST, player, 0);
}

