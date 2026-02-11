#include "replay.h"
#include <string.h>
#include <stdio.h>

void initReplay(ReplayState* replay) {
    replay->mode = REPLAY_MODE_OFF;
    replay->frameCount = 0;
    replay->currentFrame = 0;
    replay->startX = 0;
    replay->startY = 0;
    memset(replay->inputs, 0, sizeof(replay->inputs));
}

void startRecording(ReplayState* replay) {
    replay->mode = REPLAY_MODE_RECORDING;
    replay->frameCount = 0;
    replay->currentFrame = 0;
    memset(replay->inputs, 0, sizeof(replay->inputs));
}

void startPlayback(ReplayState* replay) {
    replay->mode = REPLAY_MODE_PLAYBACK;
    replay->currentFrame = 0;
}

void stopReplay(ReplayState* replay) {
    replay->mode = REPLAY_MODE_OFF;
}

void recordFrame(ReplayState* replay, u16 keys) {
    if (replay->mode != REPLAY_MODE_RECORDING) {
        return;
    }

    if (replay->frameCount < MAX_REPLAY_FRAMES) {
        replay->inputs[replay->frameCount] = keys;
        replay->frameCount++;
    } else {
        // Stop recording when buffer is full
        replay->mode = REPLAY_MODE_OFF;
    }
}

u16 getPlaybackInput(ReplayState* replay) {
    if (replay->mode != REPLAY_MODE_PLAYBACK) {
        return 0;
    }

    if (replay->currentFrame < replay->frameCount) {
        u16 keys = replay->inputs[replay->currentFrame];
        replay->currentFrame++;
        return keys;
    } else {
        // Reached end of replay
        replay->mode = REPLAY_MODE_OFF;
        return 0;
    }
}

int isReplayActive(ReplayState* replay) {
    return replay->mode != REPLAY_MODE_OFF;
}

#ifndef __GBA__
void saveReplayToFile(ReplayState* replay, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        printf("Failed to open file for writing: %s\n", filename);
        return;
    }

    fprintf(f, "# GBA Replay File\n");
    fprintf(f, "# Frame count: %d\n", replay->frameCount);
    fprintf(f, "FRAMES=%d\n", replay->frameCount);

    for (int i = 0; i < replay->frameCount; i++) {
        fprintf(f, "%04X\n", replay->inputs[i]);
    }

    fclose(f);
    printf("Replay saved to %s (%d frames)\n", filename, replay->frameCount);
}

void loadReplayFromFile(ReplayState* replay, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("Failed to open file for reading: %s\n", filename);
        return;
    }

    char line[256];
    int frameCount = 0;

    // Read header
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue; // Skip comments

        if (sscanf(line, "FRAMES=%d", &frameCount) == 1) {
            replay->frameCount = frameCount;
            break;
        }
    }

    // Read inputs
    int i = 0;
    while (fgets(line, sizeof(line), f) && i < MAX_REPLAY_FRAMES) {
        unsigned int keys;
        if (sscanf(line, "%X", &keys) == 1) {
            replay->inputs[i] = (u16)keys;
            i++;
        }
    }

    fclose(f);
    printf("Replay loaded from %s (%d frames)\n", filename, i);
}
#endif

void printReplayData(ReplayState* replay) {
    // Print in a format that can be copied to a C array
    printf("// Replay data: %d frames\n", replay->frameCount);
    printf("// To use: Copy this into src/replays/replay_data.c\n");
    printf("// Then call loadReplayFromArray(&replay, embeddedReplayInputs, embeddedReplayFrameCount);\n\n");
    printf("const u16 embeddedReplayInputs[] = {\n");

    for (int i = 0; i < replay->frameCount; i++) {
        if (i % 8 == 0) printf("    ");
        printf("0x%04X", replay->inputs[i]);
        if (i < replay->frameCount - 1) printf(", ");
        if (i % 8 == 7 || i == replay->frameCount - 1) printf("\n");
    }

    printf("};\n");
    printf("const int embeddedReplayFrameCount = %d;\n", replay->frameCount);
}

void loadReplayFromArray(ReplayState* replay, const u16* inputs, int frameCount) {
    replay->frameCount = frameCount < MAX_REPLAY_FRAMES ? frameCount : MAX_REPLAY_FRAMES;

    for (int i = 0; i < replay->frameCount; i++) {
        replay->inputs[i] = inputs[i];
    }

    replay->currentFrame = 0;
    printf("Loaded replay from array: %d frames\n", replay->frameCount);
}

void setReplayStartPosition(ReplayState* replay, int x, int y) {
    replay->startX = x;
    replay->startY = y;
}

void getReplayStartPosition(ReplayState* replay, int* x, int* y) {
    *x = replay->startX;
    *y = replay->startY;
}

// SRAM save/load - must use byte writes for GBA SRAM
#ifndef DESKTOP_BUILD
#define SRAM_START ((u8*)0x0E000000)
#endif
#define REPLAY_MAGIC 0x59  // Just first byte 'Y' from "RPLY"

void saveReplayToSRAM(ReplayState* replay) {
#ifndef DESKTOP_BUILD
    u8* sram = SRAM_START;

    // Write magic byte
    sram[0] = REPLAY_MAGIC;

    // Write frame count (4 bytes, little endian)
    sram[1] = (replay->frameCount >> 0) & 0xFF;
    sram[2] = (replay->frameCount >> 8) & 0xFF;
    sram[3] = (replay->frameCount >> 16) & 0xFF;
    sram[4] = (replay->frameCount >> 24) & 0xFF;

    // Write starting position (8 bytes: 4 for X, 4 for Y, little endian)
    sram[5] = (replay->startX >> 0) & 0xFF;
    sram[6] = (replay->startX >> 8) & 0xFF;
    sram[7] = (replay->startX >> 16) & 0xFF;
    sram[8] = (replay->startX >> 24) & 0xFF;
    sram[9] = (replay->startY >> 0) & 0xFF;
    sram[10] = (replay->startY >> 8) & 0xFF;
    sram[11] = (replay->startY >> 16) & 0xFF;
    sram[12] = (replay->startY >> 24) & 0xFF;

    // Write input data (2 bytes per frame, little endian)
    int offset = 13;
    for (int i = 0; i < replay->frameCount && i < MAX_REPLAY_FRAMES; i++) {
        sram[offset++] = (replay->inputs[i] >> 0) & 0xFF;  // Low byte
        sram[offset++] = (replay->inputs[i] >> 8) & 0xFF;  // High byte
    }
#endif
}

void loadReplayFromSRAM(ReplayState* replay) {
#ifndef DESKTOP_BUILD
    u8* sram = SRAM_START;

    // Check magic byte
    if (sram[0] != REPLAY_MAGIC) {
        replay->frameCount = 0;
        return;
    }

    // Read frame count (4 bytes, little endian)
    replay->frameCount = sram[1] | (sram[2] << 8) | (sram[3] << 16) | (sram[4] << 24);

    if (replay->frameCount > MAX_REPLAY_FRAMES) {
        replay->frameCount = MAX_REPLAY_FRAMES;
    }

    // Read starting position (8 bytes: 4 for X, 4 for Y, little endian)
    replay->startX = sram[5] | (sram[6] << 8) | (sram[7] << 16) | (sram[8] << 24);
    replay->startY = sram[9] | (sram[10] << 8) | (sram[11] << 16) | (sram[12] << 24);

    // Read input data (2 bytes per frame, little endian)
    int offset = 13;
    for (int i = 0; i < replay->frameCount; i++) {
        u8 low = sram[offset++];
        u8 high = sram[offset++];
        replay->inputs[i] = low | (high << 8);
    }

    replay->currentFrame = 0;
#else
    // Desktop: no SRAM
    replay->frameCount = 0;
#endif
}
