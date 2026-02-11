#ifndef REPLAY_H
#define REPLAY_H

#ifdef DESKTOP_BUILD
#include "desktop/desktop_stubs.h"
#else
#include <tonc.h>
#endif

// Maximum frames to record (60fps * 60 seconds = 3600 frames)
#define MAX_REPLAY_FRAMES 3600

// Replay modes
typedef enum {
    REPLAY_MODE_OFF,
    REPLAY_MODE_RECORDING,
    REPLAY_MODE_PLAYBACK
} ReplayMode;

// Replay state
typedef struct {
    ReplayMode mode;
    int frameCount;
    int currentFrame;
    int startX;  // Starting X position (fixed-point)
    int startY;  // Starting Y position (fixed-point)
    int levelIndex;  // Level index (for switching levels on load)
    u16 inputs[MAX_REPLAY_FRAMES];
} ReplayState;

// Initialize replay system
void initReplay(ReplayState* replay);

// Start recording inputs
void startRecording(ReplayState* replay);

// Start playback
void startPlayback(ReplayState* replay);

// Stop recording/playback
void stopReplay(ReplayState* replay);

// Record a frame's input (call this each frame when recording)
void recordFrame(ReplayState* replay, u16 keys);

// Get input for playback (call this instead of key_poll() when playing back)
u16 getPlaybackInput(ReplayState* replay);

// Check if replay is active
int isReplayActive(ReplayState* replay);

// Save replay to file (desktop only)
#ifndef __GBA__
void saveReplayToFile(ReplayState* replay, const char* filename);
void loadReplayFromFile(ReplayState* replay, const char* filename);
#endif

// Print replay data to console for copying (desktop only)
void printReplayData(ReplayState* replay);

// Save/load replay to/from SRAM (GBA save file)
void saveReplayToSRAM(ReplayState* replay);
void loadReplayFromSRAM(ReplayState* replay);

// Load replay from a hardcoded array (for embedded replays)
void loadReplayFromArray(ReplayState* replay, const u16* inputs, int frameCount);

// Set/get starting position for replay
void setReplayStartPosition(ReplayState* replay, int x, int y);
void getReplayStartPosition(ReplayState* replay, int* x, int* y);

// Set/get level index for replay
void setReplayLevel(ReplayState* replay, int levelIndex);
int getReplayLevel(ReplayState* replay);

#endif
