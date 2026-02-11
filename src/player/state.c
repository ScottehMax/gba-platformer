#include "state.h"
#include <string.h>

void initStateMachine(StateMachine* sm) {
    sm->state = ST_NORMAL;
    sm->previousState = -1;
    memset(sm->callbacks, 0, sizeof(sm->callbacks));
}

void setStateCallbacks(StateMachine* sm, int state, StateUpdateFunc update, StateBeginFunc begin, StateEndFunc end) {
    if (state >= 0 && state < 10) {
        sm->callbacks[state].update = (void*)update;
        sm->callbacks[state].begin = (void*)begin;
        sm->callbacks[state].end = (void*)end;
    }
}

int updateStateMachine(StateMachine* sm, Player* player, u16 keys, const Level* level) {
    // Call the current state's update function
    if (sm->callbacks[sm->state].update) {
        StateUpdateFunc updateFunc = (StateUpdateFunc)sm->callbacks[sm->state].update;
        int nextState = updateFunc(player, keys, level);

        // If state changed, call end/begin callbacks
        if (nextState != sm->state) {
            setState(sm, nextState, player, level);
        }

        return nextState;
    }

    return sm->state;
}

void setState(StateMachine* sm, int newState, Player* player, const Level* level) {
    if (newState == sm->state) {
        return;
    }

    // Call current state's end callback
    if (sm->callbacks[sm->state].end) {
        StateEndFunc endFunc = (StateEndFunc)sm->callbacks[sm->state].end;
        endFunc(player);
    }

    sm->previousState = sm->state;
    sm->state = newState;

    // Call new state's begin callback
    if (sm->callbacks[sm->state].begin) {
        StateBeginFunc beginFunc = (StateBeginFunc)sm->callbacks[sm->state].begin;
        beginFunc(player, level);
    }
}
