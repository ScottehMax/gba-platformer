#ifndef INPUT_H
#define INPUT_H

#include "core/game_types.h"

// Game action button mappings
// Change these defines to remap controls

// Primary actions
#define BTN_JUMP         KEY_A
#define BTN_DASH         KEY_R
#define BTN_GRAB         KEY_L

// Movement
#define BTN_LEFT         KEY_LEFT
#define BTN_RIGHT        KEY_RIGHT
#define BTN_UP           KEY_UP
#define BTN_DOWN         KEY_DOWN

// System/Menu
#define BTN_MENU         KEY_START
#define BTN_SELECT       KEY_SELECT
#define BTN_CONFIRM      KEY_A
#define BTN_CANCEL       KEY_B

// Replay controls (used with SELECT modifier)
#define BTN_REPLAY_START  KEY_L
#define BTN_REPLAY_PLAY   KEY_R
#define BTN_REPLAY_SAVE   KEY_B
#define BTN_REPLAY_LOAD   KEY_DOWN

// Helpers to reduce boilerplate in state update functions
static inline u16 inputPressed(u16 keys, u16 prevKeys) {
    return keys & ~prevKeys;
}

static inline int inputMoveX(u16 keys) {
    return (keys & BTN_RIGHT) ? 1 : ((keys & BTN_LEFT) ? -1 : 0);
}

static inline int inputMoveY(u16 keys) {
    return (keys & BTN_DOWN) ? 1 : ((keys & BTN_UP) ? -1 : 0);
}

#endif // INPUT_H
