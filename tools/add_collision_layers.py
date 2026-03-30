#!/usr/bin/env python3
"""
One-off script: adds a Collision layer to existing TMX files based on the old
solid-tile rules (grassy_stone tiles with game IDs 1-55 are solid).

Uses string manipulation to insert new content without touching existing XML
formatting, so the files remain valid in Tiled.

Usage:
  python tools/add_collision_layers.py              # processes all levels/*.tmx
  python tools/add_collision_layers.py levels/foo.tmx levels/bar.tmx
"""

import xml.etree.ElementTree as ET
import re
import sys
import os
from glob import glob

SOLID_TILE_IDS = set(range(1, 56))

EXPECTED_RANGES = {
    'grassy_stone': 1,
    'plants': 56,
    'decals': 216,
}


def get_tileset_info(tsx_path):
    root = ET.parse(tsx_path).getroot()
    return root.get('name'), int(root.get('tilecount', 0))


def build_collision_csv(layer0_csv_text, gid_to_game_id, solid_gid):
    """Given layer 0 CSV text and GID mapping, return collision layer CSV text."""
    rows = []
    for row_str in layer0_csv_text.strip().split('\n'):
        gids = [int(v) for v in row_str.strip().rstrip(',').split(',') if v.strip()]
        if not gids:
            continue
        rows.append(','.join(
            str(solid_gid) if gid_to_game_id.get(gid, 0) in SOLID_TILE_IDS else '0'
            for gid in gids
        ))
    # Tiled requires trailing commas on all rows except the last
    for i in range(len(rows) - 1):
        rows[i] += ','
    return '\n' + '\n'.join(rows) + '\n'


def repair_et_corrupted(tmx_path):
    """
    ET-corrupted files have lost all Tiled formatting. Re-parse and write them
    using a Tiled-style formatter, WITHOUT the collision additions ET made.
    """
    tree = ET.parse(tmx_path)
    root = tree.getroot()

    # Remove collision_types tileset and Collision layer that ET added
    to_remove = []
    for ts in root.findall('tileset'):
        src = ts.get('source', '')
        if 'collision_types' in src:
            to_remove.append(ts)
    for layer in root.findall('layer'):
        if layer.get('name', '').lower() == 'collision':
            to_remove.append(layer)
    for elem in to_remove:
        root.remove(elem)

    # Fix nextlayerid (decrement back)
    nli = int(root.get('nextlayerid', '2'))
    root.set('nextlayerid', str(nli - 1))

    # Write using Tiled-compatible formatter
    content = tiled_format(root)
    with open(tmx_path, 'w', encoding='UTF-8') as f:
        f.write(content)
    print(f"  REPAIRED  {tmx_path}")


def tiled_format(root):
    """Serialize an ET element tree in Tiled's expected XML style."""
    lines = ['<?xml version="1.0" encoding="UTF-8"?>']
    _write_elem(root, lines, indent=0)
    return '\n'.join(lines) + '\n'


def _write_elem(elem, lines, indent):
    sp = ' ' * indent
    attrs = _format_attrs(elem.attrib)
    children = list(elem)
    text = elem.text or ''

    # <data encoding="csv"> gets special treatment to preserve multi-line CSV
    if elem.tag == 'data' and elem.get('encoding') == 'csv':
        lines.append(f'{sp}<data encoding="csv">{text}</data>')
        return

    if not children and not text.strip():
        lines.append(f'{sp}<{elem.tag}{attrs}/>')
    elif not children:
        stripped = text.strip()
        lines.append(f'{sp}<{elem.tag}{attrs}>{stripped}</{elem.tag}>')
    else:
        lines.append(f'{sp}<{elem.tag}{attrs}>')
        if text and text.strip():
            lines.append(text.rstrip())
        for child in children:
            _write_elem(child, lines, indent + 1)
            # preserve tail (text after closing tag of child)
            tail = child.tail or ''
            if tail.strip():
                lines.append(tail.rstrip())
        lines.append(f'{sp}</{elem.tag}>')


def _format_attrs(attrib):
    if not attrib:
        return ''
    return ''.join(f' {k}="{v}"' for k, v in attrib.items())


def process_tmx(tmx_path):
    with open(tmx_path, 'r', encoding='UTF-8') as f:
        content = f.read()

    # Detect if file was corrupted by ElementTree (single-quote XML declaration)
    if content.startswith("<?xml version='"):
        print(f"  Detected ET-corrupted format in {tmx_path}, repairing first...")
        repair_et_corrupted(tmx_path)
        with open(tmx_path, 'r', encoding='UTF-8') as f:
            content = f.read()

    # Check if Collision layer already exists
    if re.search(r'<layer[^>]+name="Collision"', content, re.IGNORECASE):
        print(f"  SKIP  {tmx_path}  (Collision layer already present)")
        return

    tmx_dir = os.path.dirname(os.path.abspath(tmx_path))

    # Parse with ET (read-only) to get GID mapping and tile data
    root = ET.fromstring(content)
    gid_to_game_id = {0: 0}
    max_end_gid = 0

    for ts_elem in root.findall('tileset'):
        firstgid = int(ts_elem.get('firstgid'))
        source = ts_elem.get('source', '')
        if not source or 'collision_types' in source:
            continue
        tsx_full = os.path.join(tmx_dir, source)
        name, tilecount = get_tileset_info(tsx_full)
        expected_first = EXPECTED_RANGES.get(name, firstgid)
        for i in range(tilecount):
            gid_to_game_id[firstgid + i] = expected_first + i
        max_end_gid = max(max_end_gid, firstgid + tilecount)

    col_firstgid = max_end_gid + 1
    solid_gid = col_firstgid  # tile index 0 = solid

    # Get layer 0 CSV data
    layers = root.findall('layer')
    if not layers:
        print(f"  SKIP  {tmx_path}  (no tile layers)")
        return
    layer_data = layers[0].find('data')
    if layer_data is None or layer_data.get('encoding') != 'csv':
        print(f"  SKIP  {tmx_path}  (layer 0 not CSV)")
        return

    width = int(root.get('width'))
    height = int(root.get('height'))
    next_layer_id = int(root.get('nextlayerid', str(len(layers) + 1)))
    csv_text = layer_data.text or ''
    col_csv = build_collision_csv(csv_text, gid_to_game_id, solid_gid)
    solid_count = col_csv.count(str(solid_gid))

    # --- String surgery: insert tileset ref before first <layer ---
    tileset_line = f' <tileset firstgid="{col_firstgid}" source="../assets/collision_types.tsx"/>'
    # Find position of first <layer (or <objectgroup)
    m = re.search(r'^[ \t]*<layer[ \t]', content, re.MULTILINE)
    if not m:
        print(f"  SKIP  {tmx_path}  (could not find layer insertion point)")
        return
    ins_pos = m.start()
    content = content[:ins_pos] + tileset_line + '\n' + content[ins_pos:]

    # --- String surgery: insert Collision layer before </map> ---
    collision_layer = (
        f' <layer id="{next_layer_id}" name="Collision" width="{width}" height="{height}" opacity="0.5">\n'
        f'  <data encoding="csv">{col_csv}</data>\n'
        f' </layer>\n'
    )
    content = content.replace('</map>', collision_layer + '</map>', 1)

    # --- Update nextlayerid on the <map ...> opening tag ---
    content = re.sub(
        r'(nextlayerid=")[^"]*(")',
        lambda m2: m2.group(1) + str(next_layer_id + 1) + m2.group(2),
        content,
        count=1
    )

    with open(tmx_path, 'w', encoding='UTF-8') as f:
        f.write(content)

    print(f"  OK    {tmx_path}  ({width}x{height}, {solid_count} solid tiles, col firstgid={col_firstgid})")


def main():
    paths = sys.argv[1:] or sorted(glob('levels/*.tmx'))
    if not paths:
        print("No TMX files found.")
        return
    print(f"Processing {len(paths)} file(s)...")
    for p in paths:
        process_tmx(p)
    print("Done.")


if __name__ == '__main__':
    main()
