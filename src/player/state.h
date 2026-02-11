#ifndef PLAYER_STATE_H
#define PLAYER_STATE_H

#include "core/game_types.h"
#include "level/level.h"

// State constants (matching Celeste's Player.cs)
#define ST_NORMAL 0
#define ST_CLIMB 1
#define ST_DASH 2
#define ST_SWIM 3
#define ST_BOOST 4
#define ST_RED_DASH 5
#define ST_HIT_SQUASH 6
#define ST_LAUNCH 7
#define ST_PICKUP 8
#define ST_DREAM_DASH 9

// State function typedefs (actual types, not void*)
typedef int (*StateUpdateFunc)(Player* player, u16 keys, const Level* level);
typedef void (*StateBeginFunc)(Player* player, const Level* level);
typedef void (*StateEndFunc)(Player* player);

// State machine functions
void initStateMachine(StateMachine* sm);
void setStateCallbacks(StateMachine* sm, int state, StateUpdateFunc update, StateBeginFunc begin, StateEndFunc end);
int updateStateMachine(StateMachine* sm, Player* player, u16 keys, const Level* level);
void setState(StateMachine* sm, int newState, Player* player, const Level* level);

// Individual state update functions (defined in state/*.c)
int normalUpdate(Player* player, u16 keys, const Level* level);
void normalBegin(Player* player, const Level* level);
void normalEnd(Player* player);

int dashUpdate(Player* player, u16 keys, const Level* level);
void dashBegin(Player* player, const Level* level);
void dashEnd(Player* player);
void dashCoroutineResume(Player* player, u16 keys);
void dashSlideCheck(Player* player);

int climbUpdate(Player* player, u16 keys, const Level* level);
void climbBegin(Player* player, const Level* level);
void climbEnd(Player* player);

#endif
