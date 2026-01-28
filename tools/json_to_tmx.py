#!/usr/bin/env python3
"""
JSON to TMX Converter - Converts old JSON level files to Tiled TMX format
Usage: python json_to_tmx.py input.json output.tmx
"""

import json
import sys
import os
import xml.etree.ElementTree as ET
from xml.dom import minidom


def json_to_tmx(json_path: str, tmx_path: str):
    """Convert a JSON level file to TMX format."""

    # Load JSON
    with open(json_path, 'r') as f:
        data = json.load(f)

    width = data['width']
    height = data['height']
    tile_width = data.get('tileWidth', 8)
    tile_height = data.get('tileHeight', 8)

    # Create TMX structure
    map_elem = ET.Element('map', {
        'version': '1.10',
        'tiledversion': '1.10.1',
        'orientation': 'orthogonal',
        'renderorder': 'right-down',
        'width': str(width),
        'height': str(height),
        'tilewidth': str(tile_width),
        'tileheight': str(tile_height),
        'infinite': '0',
        'nextlayerid': '3',
        'nextobjectid': str(len(data.get('objects', [])) + 2)
    })

    # Add map properties
    if 'name' in data or 'author' in data:
        props = ET.SubElement(map_elem, 'properties')
        if 'name' in data:
            ET.SubElement(props, 'property', {'name': 'name', 'value': data['name']})
        if 'author' in data:
            ET.SubElement(props, 'property', {'name': 'author', 'value': data['author']})

    # Add tilesets - TMX uses different ordering than game
    # Game: grassy_stone(1-55), plants(56-215), decals(216-1440)
    # TMX:  grassy_stone(1-55), decals(56-1280), plants(1281-1440)
    ET.SubElement(map_elem, 'tileset', {
        'firstgid': '1',
        'source': '../assets/grassy_stone.tsx'
    })
    ET.SubElement(map_elem, 'tileset', {
        'firstgid': '56',
        'source': '../assets/decals.tsx'
    })
    ET.SubElement(map_elem, 'tileset', {
        'firstgid': '1281',
        'source': '../assets/plants.tsx'
    })

    # Build game ID to TMX GID mapping
    game_to_tmx = {0: 0}

    # grassy_stone: 1-55 -> 1-55
    for i in range(55):
        game_to_tmx[1 + i] = 1 + i

    # plants: 56-215 -> 1281-1440
    for i in range(160):
        game_to_tmx[56 + i] = 1281 + i

    # decals: 216-1440 -> 56-1280
    for i in range(1225):
        game_to_tmx[216 + i] = 56 + i

    # Add tile layer
    layer = ET.SubElement(map_elem, 'layer', {
        'id': '1',
        'name': 'Tile Layer 1',
        'width': str(width),
        'height': str(height)
    })

    layer_data = ET.SubElement(layer, 'data', {'encoding': 'csv'})

    # Convert tiles to TMX format
    csv_lines = []
    tiles = data['tiles']
    for row in tiles:
        # Map game IDs to TMX GIDs
        tmx_row = [game_to_tmx.get(tile_id, 0) for tile_id in row]
        csv_lines.append(','.join(str(gid) for gid in tmx_row))

    # Add final row without trailing comma
    layer_data.text = '\n' + ',\n'.join(csv_lines) + '\n'

    # Add object layer if there are objects or player spawn
    objects = data.get('objects', [])
    spawn = data.get('playerSpawn', {})

    if objects or spawn:
        objectgroup = ET.SubElement(map_elem, 'objectgroup', {
            'id': '2',
            'name': 'Objects'
        })

        obj_id = 1

        # Add player spawn
        if spawn:
            spawn_obj = ET.SubElement(objectgroup, 'object', {
                'id': str(obj_id),
                'name': 'PlayerSpawn',
                'type': 'PlayerSpawn',
                'x': str(spawn.get('x', 0)),
                'y': str(spawn.get('y', 0))
            })
            ET.SubElement(spawn_obj, 'point')
            obj_id += 1

        # Add other objects
        for obj in objects:
            obj_elem = ET.SubElement(objectgroup, 'object', {
                'id': str(obj_id),
                'name': obj.get('type', 'object'),
                'type': obj.get('type', 'object'),
                'x': str(obj.get('x', 0)),
                'y': str(obj.get('y', 0))
            })

            # Add properties if any
            if 'properties' in obj and obj['properties']:
                obj_props = ET.SubElement(obj_elem, 'properties')
                for key, value in obj['properties'].items():
                    ET.SubElement(obj_props, 'property', {
                        'name': key,
                        'value': str(value)
                    })

            obj_id += 1

    # Pretty print XML
    xml_str = ET.tostring(map_elem, encoding='unicode')
    dom = minidom.parseString(xml_str)
    pretty_xml = dom.toprettyxml(indent=' ')

    # Remove extra blank lines
    lines = [line for line in pretty_xml.split('\n') if line.strip()]
    pretty_xml = '\n'.join(lines)

    # Write to file
    with open(tmx_path, 'w', encoding='utf-8') as f:
        f.write(pretty_xml)

    print(f"Successfully converted {json_path} to {tmx_path}")
    print(f"  Level: {data.get('name', 'Untitled')}")
    print(f"  Dimensions: {width}x{height}")
    print(f"  Objects: {len(objects)}")


def main():
    if len(sys.argv) != 3:
        print("Usage: python json_to_tmx.py input.json output.tmx")
        sys.exit(1)

    json_path = sys.argv[1]
    tmx_path = sys.argv[2]

    if not os.path.exists(json_path):
        print(f"Error: Input file '{json_path}' not found")
        sys.exit(1)

    try:
        json_to_tmx(json_path, tmx_path)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
