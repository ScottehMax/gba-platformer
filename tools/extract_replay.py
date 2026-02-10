#!/usr/bin/env python3
"""
Extract replay data from GBA save file and convert to C array format.

Usage:
    python extract_replay.py game.sav
    python extract_replay.py game.sav > src/replays/replay_data.c
"""

import sys
import struct

REPLAY_MAGIC = 0x59  # 'Y' - first byte of "RPLY"
MAX_REPLAY_FRAMES = 3600

def extract_replay(sav_file):
    with open(sav_file, 'rb') as f:
        # Read magic byte (1 byte)
        magic_byte = f.read(1)
        if len(magic_byte) < 1:
            print("Error: Save file too small", file=sys.stderr)
            return None

        magic = magic_byte[0]

        if magic != REPLAY_MAGIC:
            print(f"Error: Invalid magic byte. Expected 0x{REPLAY_MAGIC:02X}, got 0x{magic:02X}", file=sys.stderr)
            print("No replay data found in save file.", file=sys.stderr)
            return None

        # Read frame count (4 bytes, little endian)
        frame_count_bytes = f.read(4)
        if len(frame_count_bytes) < 4:
            print("Error: Incomplete frame count", file=sys.stderr)
            return None
        frame_count = struct.unpack('<I', frame_count_bytes)[0]

        if frame_count > MAX_REPLAY_FRAMES:
            print(f"Warning: Frame count {frame_count} exceeds maximum {MAX_REPLAY_FRAMES}", file=sys.stderr)
            frame_count = MAX_REPLAY_FRAMES

        # Read input data (2 bytes per frame)
        inputs = []
        for i in range(frame_count):
            input_bytes = f.read(2)
            if len(input_bytes) < 2:
                print(f"Warning: Unexpected end of file at frame {i}", file=sys.stderr)
                break
            input_val = struct.unpack('<H', input_bytes)[0]
            inputs.append(input_val)

        return inputs

def format_as_c_array(inputs):
    """Format replay data as C array."""
    print("#include <tonc.h>")
    print()
    print("// Replay data extracted from save file")
    print(f"// Frame count: {len(inputs)}")
    print()
    print("const u16 embeddedReplayInputs[] = {")

    for i in range(0, len(inputs), 8):
        chunk = inputs[i:i+8]
        formatted = ", ".join(f"0x{val:04X}" for val in chunk)
        print(f"    {formatted},")

    print("};")
    print(f"const int embeddedReplayFrameCount = {len(inputs)};")

def main():
    if len(sys.argv) < 2:
        print("Usage: python extract_replay.py <save_file.sav>", file=sys.stderr)
        print("", file=sys.stderr)
        print("Example:", file=sys.stderr)
        print("  python extract_replay.py game.sav > src/replays/replay_data.c", file=sys.stderr)
        sys.exit(1)

    sav_file = sys.argv[1]

    try:
        inputs = extract_replay(sav_file)

        if inputs is None:
            sys.exit(1)

        if len(inputs) == 0:
            print("Error: No replay frames found", file=sys.stderr)
            sys.exit(1)

        format_as_c_array(inputs)
        print(f"// Successfully extracted {len(inputs)} frames", file=sys.stderr)

    except FileNotFoundError:
        print(f"Error: File not found: {sav_file}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
