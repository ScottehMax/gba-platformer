#!/usr/bin/env python3
"""
Connection Compiler - Compiles connections.json to a C header for GBA.
Usage: compile_connections.py <connections.json> <levels_dir> <output_connections.h>

Scans levels_dir for *.tmx files, reads their dimensions and names,
then generates a C header with level indices and connection data.

Connection format in connections.json:
  "levels": { "stem": { "worldX": int, "worldY": int } }
  "connections": [ { "a": "stemA", "b": "stemB" } ]

The shared edge and valid range are derived from the world positions.
fromStart/fromEnd define the valid perpendicular exit range (local px).
newPerpPos = perpPos - fromStart + toStart
"""

import sys
import os
import json
import xml.etree.ElementTree as ET
from pathlib import Path

SNAP_TOLERANCE = 2  # pixels — how close edges must be to count as adjacent


def sanitize_identifier(name: str) -> str:
    """Convert a string to a valid C identifier."""
    result = ''.join(c if c.isalnum() else '_' for c in name)
    if result and result[0].isdigit():
        result = '_' + result
    return result or 'unnamed'


SIDE_C_NAME = {
    'right':  'CONN_SIDE_RIGHT',
    'left':   'CONN_SIDE_LEFT',
    'bottom': 'CONN_SIDE_BOTTOM',
    'top':    'CONN_SIDE_TOP',
}


def parse_tmx(tmx_path: Path):
    """Parse a TMX file, returning (display_name, width_tiles, height_tiles)."""
    try:
        tree = ET.parse(tmx_path)
    except ET.ParseError as e:
        print(f"ERROR: Failed to parse {tmx_path}: {e}", file=sys.stderr)
        return None

    root = tree.getroot()
    width  = int(root.get('width',  0))
    height = int(root.get('height', 0))

    display_name = tmx_path.stem  # fallback
    props = root.find('properties')
    if props is not None:
        for prop in props.findall('property'):
            if prop.get('name') == 'name':
                display_name = prop.get('value', display_name)
                break

    return display_name, width, height


def derive_connection(a_stem, b_stem, meta, world_pos):
    """
    Given two level stems and their metadata + world positions, determine
    which side they share and compute the overlap range.

    Returns a list of two dicts (A→B and B→A), each with:
      { from_stem, to_stem, from_side, to_side, from_start, from_end, to_start }
    Returns None if the levels are not adjacent on any side.
    """
    if a_stem not in meta or b_stem not in meta:
        return None
    if a_stem not in world_pos or b_stem not in world_pos:
        return None

    ax = world_pos[a_stem]['worldX']
    ay = world_pos[a_stem]['worldY']
    aw = meta[a_stem]['widthPx']
    ah = meta[a_stem]['heightPx']

    bx = world_pos[b_stem]['worldX']
    by = world_pos[b_stem]['worldY']
    bw = meta[b_stem]['widthPx']
    bh = meta[b_stem]['heightPx']

    entries = []

    # Check A-right / B-left
    if abs((ax + aw) - bx) <= SNAP_TOLERANCE:
        # Perpendicular axis: Y
        overlap_start = max(ay, by)
        overlap_end   = min(ay + ah, by + bh)
        if overlap_end > overlap_start:
            from_start = overlap_start - ay
            to_start   = overlap_start - by
            from_end   = overlap_end   - ay
            entries.append({
                'from_stem': a_stem, 'to_stem': b_stem,
                'from_side': 'right', 'to_side': 'left',
                'from_start': from_start, 'from_end': from_end, 'to_start': to_start,
            })
            entries.append({
                'from_stem': b_stem, 'to_stem': a_stem,
                'from_side': 'left', 'to_side': 'right',
                'from_start': to_start, 'from_end': to_start + (from_end - from_start), 'to_start': from_start,
            })
            return entries

    # Check B-right / A-left
    if abs((bx + bw) - ax) <= SNAP_TOLERANCE:
        overlap_start = max(ay, by)
        overlap_end   = min(ay + ah, by + bh)
        if overlap_end > overlap_start:
            from_start = overlap_start - by
            to_start   = overlap_start - ay
            from_end   = overlap_end   - by
            entries.append({
                'from_stem': b_stem, 'to_stem': a_stem,
                'from_side': 'right', 'to_side': 'left',
                'from_start': from_start, 'from_end': from_end, 'to_start': to_start,
            })
            entries.append({
                'from_stem': a_stem, 'to_stem': b_stem,
                'from_side': 'left', 'to_side': 'right',
                'from_start': to_start, 'from_end': to_start + (from_end - from_start), 'to_start': from_start,
            })
            return entries

    # Check A-bottom / B-top
    if abs((ay + ah) - by) <= SNAP_TOLERANCE:
        # Perpendicular axis: X
        overlap_start = max(ax, bx)
        overlap_end   = min(ax + aw, bx + bw)
        if overlap_end > overlap_start:
            from_start = overlap_start - ax
            to_start   = overlap_start - bx
            from_end   = overlap_end   - ax
            entries.append({
                'from_stem': a_stem, 'to_stem': b_stem,
                'from_side': 'bottom', 'to_side': 'top',
                'from_start': from_start, 'from_end': from_end, 'to_start': to_start,
            })
            entries.append({
                'from_stem': b_stem, 'to_stem': a_stem,
                'from_side': 'top', 'to_side': 'bottom',
                'from_start': to_start, 'from_end': to_start + (from_end - from_start), 'to_start': from_start,
            })
            return entries

    # Check B-bottom / A-top
    if abs((by + bh) - ay) <= SNAP_TOLERANCE:
        overlap_start = max(ax, bx)
        overlap_end   = min(ax + aw, bx + bw)
        if overlap_end > overlap_start:
            from_start = overlap_start - bx
            to_start   = overlap_start - ax
            from_end   = overlap_end   - bx
            entries.append({
                'from_stem': b_stem, 'to_stem': a_stem,
                'from_side': 'bottom', 'to_side': 'top',
                'from_start': from_start, 'from_end': from_end, 'to_start': to_start,
            })
            entries.append({
                'from_stem': a_stem, 'to_stem': b_stem,
                'from_side': 'top', 'to_side': 'bottom',
                'from_start': to_start, 'from_end': to_start + (from_end - from_start), 'to_start': from_start,
            })
            return entries

    return None


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <connections.json> <levels_dir> <output_connections.h>",
              file=sys.stderr)
        sys.exit(1)

    conn_json_path = Path(sys.argv[1])
    levels_dir     = Path(sys.argv[2])
    output_path    = Path(sys.argv[3])

    # -------------------------------------------------------------------------
    # 1. Load connections.json
    # -------------------------------------------------------------------------
    if not conn_json_path.exists():
        print(f"ERROR: {conn_json_path} not found", file=sys.stderr)
        sys.exit(1)

    with open(conn_json_path, 'r', encoding='utf-8') as f:
        conn_data = json.load(f)

    # -------------------------------------------------------------------------
    # 2. Scan levels directory for TMX files (sorted alphabetically by filename)
    # -------------------------------------------------------------------------
    tmx_files = sorted(levels_dir.glob('*.tmx'), key=lambda p: p.name.lower())
    if not tmx_files:
        print(f"WARNING: No .tmx files found in {levels_dir}", file=sys.stderr)

    level_info = []  # list of (filename_stem, display_name, width_tiles, height_tiles)
    for tmx_path in tmx_files:
        result = parse_tmx(tmx_path)
        if result is None:
            continue
        display_name, w, h = result
        stem = tmx_path.stem
        print(f"  Level: {stem!r}  name={display_name!r}  size={w}x{h} tiles")
        level_info.append((stem, display_name, w, h))

    # -------------------------------------------------------------------------
    # 3. Update _metadata in connections.json and write back
    # -------------------------------------------------------------------------
    metadata = conn_data.setdefault('_metadata', {})
    for stem, display_name, w, h in level_info:
        metadata[stem] = {
            'displayName': display_name,
            'widthPx':  w * 8,
            'heightPx': h * 8,
        }

    with open(conn_json_path, 'w', encoding='utf-8') as f:
        json.dump(conn_data, f, indent=2)
    print(f"Updated metadata in {conn_json_path}")

    # -------------------------------------------------------------------------
    # 4. Build level index lookup and metadata dict
    # -------------------------------------------------------------------------
    stem_to_idx = {stem: i for i, (stem, _, _, _) in enumerate(level_info)}
    meta = {stem: {'widthPx': w * 8, 'heightPx': h * 8}
            for stem, _, w, h in level_info}

    # -------------------------------------------------------------------------
    # 5. Parse world positions and connections
    # -------------------------------------------------------------------------
    world_pos = conn_data.get('levels', {})
    connections = conn_data.get('connections', [])

    # -------------------------------------------------------------------------
    # 6. Derive connection entries from world positions
    # -------------------------------------------------------------------------
    conn_entries = []
    errors = 0
    for conn in connections:
        a_stem = conn.get('a', '')
        b_stem = conn.get('b', '')

        ok = True
        if a_stem not in stem_to_idx:
            print(f"ERROR: Connection references unknown level {a_stem!r}", file=sys.stderr)
            ok = False
        if b_stem not in stem_to_idx:
            print(f"ERROR: Connection references unknown level {b_stem!r}", file=sys.stderr)
            ok = False
        if a_stem not in world_pos:
            print(f"ERROR: Level {a_stem!r} has no world position in 'levels'", file=sys.stderr)
            ok = False
        if b_stem not in world_pos:
            print(f"ERROR: Level {b_stem!r} has no world position in 'levels'", file=sys.stderr)
            ok = False
        if not ok:
            errors += 1
            continue

        derived = derive_connection(a_stem, b_stem, meta, world_pos)
        if derived is None:
            print(f"ERROR: Levels {a_stem!r} and {b_stem!r} are not adjacent "
                  f"(no shared edge within {SNAP_TOLERANCE}px tolerance). "
                  f"Check their worldX/worldY positions.", file=sys.stderr)
            errors += 1
            continue

        for e in derived:
            from_idx = stem_to_idx[e['from_stem']]
            to_idx   = stem_to_idx[e['to_stem']]
            fs = SIDE_C_NAME[e['from_side']]
            ts = SIDE_C_NAME[e['to_side']]
            conn_entries.append(
                f'    {{ {from_idx}, {to_idx}, {fs}, {ts}, '
                f'{e["from_start"]}, {e["from_end"]}, {e["to_start"]} }},'
            )
            print(f"  {e['from_stem']} ({e['from_side']}) -> {e['to_stem']} ({e['to_side']})  "
                  f"range [{e['from_start']}, {e['from_end']}) -> [{e['to_start']}, "
                  f"{e['to_start'] + e['from_end'] - e['from_start']})")

    if errors:
        print(f"WARNING: {errors} connection(s) skipped due to errors.", file=sys.stderr)

    # -------------------------------------------------------------------------
    # 7. Generate connections.h
    # -------------------------------------------------------------------------
    output_path.parent.mkdir(parents=True, exist_ok=True)

    lines = []
    lines.append('#ifndef CONNECTIONS_H')
    lines.append('#define CONNECTIONS_H')
    lines.append('')
    lines.append('#ifdef DESKTOP_BUILD')
    lines.append('#include "desktop/desktop_stubs.h"')
    lines.append('#else')
    lines.append('#include <tonc.h>')
    lines.append('#endif')
    lines.append('')
    lines.append('#include "transition/transition.h"')
    lines.append('#include "level/level.h"')
    lines.append('')

    for stem, display_name, w, h in level_info:
        lines.append(f'#include "{stem}.h"')
    lines.append('')

    for i, (stem, display_name, w, h) in enumerate(level_info):
        lines.append(f'#define LEVEL_IDX_{stem} {i}')
    lines.append(f'#define LEVEL_COUNT {len(level_info)}')
    lines.append('')

    lines.append('static const Level* g_levels[LEVEL_COUNT] = {')
    for stem, display_name, w, h in level_info:
        c_var = sanitize_identifier(stem)
        lines.append(f'    &{c_var},')
    lines.append('};')
    lines.append('')

    lines.append('static const char* g_levelNames[LEVEL_COUNT]')
    lines.append('    __attribute__((unused)) = {')
    for stem, display_name, w, h in level_info:
        escaped = display_name.replace('\\', '\\\\').replace('"', '\\"')
        lines.append(f'    "{escaped}",')
    lines.append('};')
    lines.append('')

    lines.append('static const ScreenConnection g_connections[] = {')
    if conn_entries:
        lines.extend(conn_entries)
    else:
        lines.append('    /* no connections defined */')
    lines.append('};')
    lines.append(f'static const int g_connectionCount = {len(conn_entries)};')
    lines.append('')
    lines.append('#endif /* CONNECTIONS_H */')

    content = '\n'.join(lines) + '\n'
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"Generated {output_path}  ({len(level_info)} levels, {len(conn_entries)} connection entries)")


if __name__ == '__main__':
    main()
