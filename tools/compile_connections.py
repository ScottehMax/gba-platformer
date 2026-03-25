#!/usr/bin/env python3
"""
Connection Compiler - Compiles connections.json to a C header for GBA.
Usage: compile_connections.py <connections.json> <levels_dir> <output_connections.h>

Scans levels_dir for *.tmx files, reads their dimensions and names,
then generates a C header with level indices and connection data.
"""

import sys
import os
import json
import xml.etree.ElementTree as ET
from pathlib import Path


def sanitize_identifier(name: str) -> str:
    """Convert a string to a valid C identifier."""
    result = ''.join(c if c.isalnum() else '_' for c in name)
    if result and result[0].isdigit():
        result = '_' + result
    return result or 'unnamed'


SIDE_MAP = {
    'right':  ('CONN_SIDE_RIGHT',  0),
    'left':   ('CONN_SIDE_LEFT',   1),
    'bottom': ('CONN_SIDE_BOTTOM', 2),
    'top':    ('CONN_SIDE_TOP',    3),
}

OPPOSITE_SIDE = {
    'right':  'left',
    'left':   'right',
    'bottom': 'top',
    'top':    'bottom',
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

    # Extract display name from <properties>
    display_name = tmx_path.stem  # fallback
    props = root.find('properties')
    if props is not None:
        for prop in props.findall('property'):
            if prop.get('name') == 'name':
                display_name = prop.get('value', display_name)
                break

    return display_name, width, height


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
    # 4. Build level index lookup
    # -------------------------------------------------------------------------
    # stem -> index in sorted list
    stem_to_idx = {stem: i for i, (stem, _, _, _) in enumerate(level_info)}

    # -------------------------------------------------------------------------
    # 5. Parse connections array
    # -------------------------------------------------------------------------
    connections = conn_data.get('connections', [])

    # -------------------------------------------------------------------------
    # 6. Generate connections.h
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
    # ConnectionSide, ScreenConnection defined in transition.h - include it for those types
    lines.append('#include "transition/transition.h"')
    lines.append('#include "level/level.h"')
    lines.append('')

    # Individual level headers
    for stem, display_name, w, h in level_info:
        lines.append(f'#include "{stem}.h"')
    lines.append('')

    # Level index constants
    for i, (stem, display_name, w, h) in enumerate(level_info):
        lines.append(f'#define LEVEL_IDX_{stem} {i}')
    lines.append(f'#define LEVEL_COUNT {len(level_info)}')
    lines.append('')

    # g_levels array
    lines.append('static const Level* g_levels[LEVEL_COUNT] = {')
    for stem, display_name, w, h in level_info:
        c_var = sanitize_identifier(stem)  # use filename stem - guaranteed unique
        lines.append(f'    &{c_var},')
    lines.append('};')
    lines.append('')

    # g_levelNames array - used by menu.c; suppress unused warning in other TUs
    lines.append('static const char* g_levelNames[LEVEL_COUNT]')
    lines.append('    __attribute__((unused)) = {')
    for stem, display_name, w, h in level_info:
        escaped = display_name.replace('\\', '\\\\').replace('"', '\\"')
        lines.append(f'    "{escaped}",')
    lines.append('};')
    lines.append('')

    # Build connection entries (both directions)
    conn_entries = []
    errors = 0
    for conn in connections:
        from_obj  = conn.get('from', {})
        to_obj    = conn.get('to',   {})
        offset    = conn.get('offset', 0)

        from_stem = from_obj.get('level', '')
        to_stem   = to_obj.get('level',   '')
        from_side_str = from_obj.get('side', '').lower()
        to_side_str   = to_obj.get('side',   '').lower()

        # Validate
        ok = True
        if from_stem not in stem_to_idx:
            print(f"ERROR: Connection references unknown level {from_stem!r}", file=sys.stderr)
            ok = False
        if to_stem not in stem_to_idx:
            print(f"ERROR: Connection references unknown level {to_stem!r}", file=sys.stderr)
            ok = False
        if from_side_str not in SIDE_MAP:
            print(f"ERROR: Unknown side {from_side_str!r}", file=sys.stderr)
            ok = False
        if to_side_str not in SIDE_MAP:
            print(f"ERROR: Unknown side {to_side_str!r}", file=sys.stderr)
            ok = False
        if not ok:
            errors += 1
            continue

        from_idx = stem_to_idx[from_stem]
        to_idx   = stem_to_idx[to_stem]
        from_side_name, _ = SIDE_MAP[from_side_str]
        to_side_name,   _ = SIDE_MAP[to_side_str]

        # from -> to
        conn_entries.append(
            f'    {{ {from_idx}, {to_idx}, {from_side_name}, {to_side_name}, {offset} }},'
        )
        # to -> from (reversed, negated offset)
        conn_entries.append(
            f'    {{ {to_idx}, {from_idx}, {to_side_name}, {from_side_name}, {-offset} }},'
        )

    if errors:
        print(f"WARNING: {errors} connection(s) skipped due to errors.", file=sys.stderr)

    # g_connections array
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
