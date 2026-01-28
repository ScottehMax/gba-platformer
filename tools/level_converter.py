#!/usr/bin/env python3
"""
Level Converter - Converts Tiled TMX level files to C header files for GBA
Usage: python level_converter.py input.tmx output.h

Supports multi-tileset levels with collision bitmap.
Parses Tiled TMX format and external TSX tileset files.
"""

import xml.etree.ElementTree as ET
import sys
import os
from typing import Dict, List, Any, Set, Tuple
from pathlib import Path

# Default collision tiles (grassy_stone tiles 1-55 are solid)
DEFAULT_COLLISION_TILES = list(range(1, 56))


def parse_tsx_tileset(tsx_path: str) -> Dict[str, Any]:
    """Parse an external TSX tileset file."""
    tree = ET.parse(tsx_path)
    root = tree.getroot()

    tileset_info = {
        'name': root.get('name'),
        'tileWidth': int(root.get('tilewidth', 8)),
        'tileHeight': int(root.get('tileheight', 8)),
        'tileCount': int(root.get('tilecount', 0)),
        'columns': int(root.get('columns', 0))
    }

    # Get image source
    image = root.find('image')
    if image is not None:
        tileset_info['imageSource'] = image.get('source')

    return tileset_info


def parse_tmx_file(tmx_path: str) -> Dict[str, Any]:
    """Parse a TMX file and return level data in the expected JSON format."""
    tree = ET.parse(tmx_path)
    root = tree.getroot()

    # Get map properties
    width = int(root.get('width'))
    height = int(root.get('height'))
    tile_width = int(root.get('tilewidth', 8))
    tile_height = int(root.get('tileheight', 8))

    data = {
        'name': 'Untitled Level',
        'width': width,
        'height': height,
        'tileWidth': tile_width,
        'tileHeight': tile_height,
        'layers': [],
        'objects': [],
        'playerSpawn': {'x': 0, 'y': 0},
        'tilesets': [],
        'collisionTiles': DEFAULT_COLLISION_TILES.copy()
    }

    # Expected tileset ranges for the game
    EXPECTED_RANGES = {
        'grassy_stone': 1,
        'plants': 56,
        'decals': 216
    }

    # Parse tilesets and build TMX GID -> Game ID mapping
    tmx_dir = os.path.dirname(os.path.abspath(tmx_path))
    gid_to_game_id = {0: 0}  # 0 always maps to 0 (empty tile)

    for tileset_elem in root.findall('tileset'):
        firstgid = int(tileset_elem.get('firstgid'))
        source = tileset_elem.get('source')

        if source:
            # External tileset
            tsx_path = os.path.join(tmx_dir, source)
            tsx_info = parse_tsx_tileset(tsx_path)

            tileset_name = tsx_info['name']
            tile_count = tsx_info['tileCount']

            # Get expected firstId for this tileset
            expected_first_id = EXPECTED_RANGES.get(tileset_name, firstgid)

            # Build mapping from TMX GID to game tile ID
            for i in range(tile_count):
                tmx_gid = firstgid + i
                game_id = expected_first_id + i
                gid_to_game_id[tmx_gid] = game_id

            # Convert image source path to be relative to assets directory
            image_source = tsx_info.get('imageSource', '')

            data['tilesets'].append({
                'name': tileset_name,
                'file': image_source,  # Keep the relative path from TSX
                'firstId': expected_first_id,
                'tileCount': tile_count
            })

    # Sort tilesets by firstId to ensure consistent ordering
    data['tilesets'].sort(key=lambda t: t['firstId'])

    # Parse tile layers
    layer_index = 0
    for layer in root.findall('layer'):
        layer_name = layer.get('name', f'Layer {layer_index}')
        layer_data = layer.find('data')

        if layer_data is not None:
            encoding = layer_data.get('encoding')

            if encoding == 'csv':
                # Parse layer properties for BG layer and priority
                bg_layer = 2  # Default to BG2
                priority = 1  # Default priority

                # Layer-specific defaults
                if layer_index == 0:
                    bg_layer = 2
                    priority = 1
                elif layer_index == 1:
                    bg_layer = 1
                    priority = 0

                # Check for custom properties
                properties = layer.find('properties')
                if properties is not None:
                    for prop in properties.findall('property'):
                        prop_name = prop.get('name')
                        prop_value = prop.get('value')
                        if prop_name == 'bgLayer':
                            bg_layer = int(prop_value)
                        elif prop_name == 'priority':
                            priority = int(prop_value)

                # Parse CSV data
                csv_text = layer_data.text.strip()
                rows = csv_text.split('\n')

                tiles = []
                for row in rows:
                    # Parse TMX GIDs and remap to game tile IDs
                    tmx_gids = [int(tile_id) for tile_id in row.strip().rstrip(',').split(',') if tile_id.strip()]
                    if tmx_gids:  # Only add non-empty rows
                        # Remap TMX GIDs to game tile IDs
                        game_tile_row = [gid_to_game_id.get(gid, 0) for gid in tmx_gids]
                        tiles.append(game_tile_row)

                # Add layer to layers array
                data['layers'].append({
                    'name': layer_name,
                    'bgLayer': bg_layer,
                    'priority': priority,
                    'tiles': tiles
                })
                layer_index += 1
            else:
                raise ValueError(f"Unsupported encoding: {encoding}. Only CSV encoding is supported.")

    # Parse object layers
    for objectgroup in root.findall('objectgroup'):
        layer_name = objectgroup.get('name', '').lower()

        for obj in objectgroup.findall('object'):
            obj_id = obj.get('id')
            obj_name = obj.get('name', '')
            obj_type = obj.get('type', '')
            obj_x = float(obj.get('x', 0))
            obj_y = float(obj.get('y', 0))

            # Check if this is the player spawn
            if obj_type.lower() == 'playerspawn' or obj_name.lower() == 'playerspawn':
                data['playerSpawn'] = {
                    'x': int(obj_x),
                    'y': int(obj_y)
                }
            else:
                # Regular object
                obj_data = {
                    'type': obj_type or obj_name or 'object',
                    'x': int(obj_x),
                    'y': int(obj_y),
                    'properties': {}
                }

                # Parse properties
                properties = obj.find('properties')
                if properties is not None:
                    for prop in properties.findall('property'):
                        prop_name = prop.get('name')
                        prop_value = prop.get('value')
                        obj_data['properties'][prop_name] = prop_value

                data['objects'].append(obj_data)

    # Get level name from map properties or filename
    properties = root.find('properties')
    if properties is not None:
        for prop in properties.findall('property'):
            if prop.get('name') == 'name':
                data['name'] = prop.get('value')
            elif prop.get('name') == 'author':
                data['author'] = prop.get('value')

    return data


def validate_level(data: Dict[str, Any], filename: str) -> None:
    """Validate level data structure and constraints."""
    errors = []

    # Required fields
    required_fields = ['name', 'width', 'height', 'layers', 'playerSpawn']
    for field in required_fields:
        if field not in data:
            errors.append(f"Missing required field: {field}")

    if errors:
        raise ValueError(f"Validation errors in {filename}:\n" + "\n".join(errors))

    # Validate dimensions
    width = data['width']
    height = data['height']

    if not isinstance(width, int) or width <= 0 or width > 256:
        errors.append(f"Invalid width: {width} (must be 1-256)")
    if not isinstance(height, int) or height <= 0 or height > 256:
        errors.append(f"Invalid height: {height} (must be 1-256)")

    # Validate layers
    layers = data['layers']
    if not layers:
        errors.append("Level must have at least one tile layer")

    # Validate each layer's tile array dimensions
    for layer_idx, layer in enumerate(layers):
        tiles = layer['tiles']
        if len(tiles) != height:
            errors.append(f"Layer {layer_idx} has {len(tiles)} rows, expected {height}")

        for i, row in enumerate(tiles):
            if len(row) != width:
                errors.append(f"Layer {layer_idx} row {i} has {len(row)} columns, expected {width}")
            for j, tile_id in enumerate(row):
                if not isinstance(tile_id, int) or tile_id < 0 or tile_id > 65535:
                    errors.append(f"Layer {layer_idx} invalid tile ID at ({i},{j}): {tile_id}")

    # Validate player spawn
    spawn = data['playerSpawn']
    if 'x' not in spawn or 'y' not in spawn:
        errors.append("playerSpawn must have x and y coordinates")
    else:
        spawn_x = spawn['x']
        spawn_y = spawn['y']
        level_width_px = width * data.get('tileWidth', 8)
        level_height_px = height * data.get('tileHeight', 8)

        if spawn_x < 0 or spawn_x >= level_width_px:
            errors.append(f"playerSpawn.x ({spawn_x}) out of bounds (0-{level_width_px-1})")
        if spawn_y < 0 or spawn_y >= level_height_px:
            errors.append(f"playerSpawn.y ({spawn_y}) out of bounds (0-{level_height_px-1})")

    # Validate objects
    if 'objects' in data:
        objects = data['objects']
        if not isinstance(objects, list):
            errors.append("objects must be an array")
        elif len(objects) > 256:
            errors.append(f"Too many objects: {len(objects)} (max 256)")
        else:
            for i, obj in enumerate(objects):
                if 'type' not in obj:
                    errors.append(f"Object {i} missing 'type' field")
                if 'x' not in obj or 'y' not in obj:
                    errors.append(f"Object {i} missing position fields")

    # Validate VRAM limit - count unique tiles used across all layers
    unique_tiles: Set[int] = set()
    for layer in layers:
        for row in layer['tiles']:
            for tile_id in row:
                if tile_id > 0:  # Skip sky tile
                    unique_tiles.add(tile_id)

    if len(unique_tiles) > 1000:
        print(f"WARNING: Level uses {len(unique_tiles)} unique tiles. GBA VRAM limit is ~1000 tiles per level.")

    if errors:
        raise ValueError(f"Validation errors in {filename}:\n" + "\n".join(errors))


def sanitize_identifier(name: str) -> str:
    """Convert a string to a valid C identifier."""
    # Replace spaces and special chars with underscores
    result = ''.join(c if c.isalnum() else '_' for c in name)
    # Ensure it doesn't start with a digit
    if result and result[0].isdigit():
        result = '_' + result
    return result or 'unnamed'


def generate_collision_bitmap(collision_tiles: List[int]) -> List[int]:
    """Generate a collision bitmap as an array of u32 values.

    Each bit represents whether a tile ID is solid.
    Supports up to 2048 tile IDs (256 bytes = 64 u32 values).
    """
    # Initialize 256 bytes (2048 bits) as 64 u32 values
    bitmap = [0] * 64

    for tile_id in collision_tiles:
        if tile_id < 0 or tile_id >= 2048:
            continue
        byte_index = tile_id // 8
        bit_index = tile_id % 8
        u32_index = byte_index // 4
        byte_offset = byte_index % 4
        bitmap[u32_index] |= (1 << bit_index) << (byte_offset * 8)

    return bitmap


def generate_header(data: Dict[str, Any], output_name: str) -> str:
    """Generate C header file content from level data."""
    level_name = sanitize_identifier(data['name'])
    guard_name = f"{output_name.upper().replace('.', '_')}_H"

    width = data['width']
    height = data['height']
    layers = data['layers']
    objects = data.get('objects', [])
    spawn = data['playerSpawn']

    # Get tilesets
    tilesets = data.get('tilesets', [])

    # Get collision tiles
    collision_tiles = data.get('collisionTiles', DEFAULT_COLLISION_TILES)

    # Collect all unique tiles used across ALL layers
    all_tiles = []
    for layer in layers:
        flat_tiles = [tile for row in layer['tiles'] for tile in row]
        all_tiles.extend(flat_tiles)

    # Build compact tile mapping at build time
    # Find all unique tiles used in the level (across all layers)
    unique_tiles = sorted(set(all_tiles))

    # Define tileset ranges and palette banks dynamically
    # Palette bank mapping based on tileset name
    PALETTE_MAP = {
        'grassy_stone': 0,
        'plants': 2,
        'decals': 3
    }

    def get_tile_palette_bank(tile_id):
        if tile_id == 0:
            return 0

        # Find which tileset this tile belongs to
        for tileset in tilesets:
            first_id = tileset['firstId']
            tile_count = tileset['tileCount']
            if first_id <= tile_id < first_id + tile_count:
                tileset_name = tileset['name']
                return PALETTE_MAP.get(tileset_name, 0)

        return 0

    # Create pre-computed metadata for each unique tile
    tile_palette_banks = [get_tile_palette_bank(tid) for tid in unique_tiles]

    # Create mapping: original tile ID -> VRAM index
    tile_id_to_vram = {}
    for vram_index, tile_id in enumerate(unique_tiles):
        tile_id_to_vram[tile_id] = vram_index

    # Remap tiles for each layer to use VRAM indices directly
    remapped_layers = []
    for layer in layers:
        flat_tiles = [tile for row in layer['tiles'] for tile in row]
        remapped_tiles = [tile_id_to_vram[tile] for tile in flat_tiles]
        remapped_layers.append({
            'name': layer['name'],
            'bgLayer': layer['bgLayer'],
            'priority': layer['priority'],
            'tiles': remapped_tiles
        })

    # Remap collision bitmap from original tile IDs to VRAM indices
    # Build new collision set based on which VRAM indices are solid
    remapped_collision_tiles = []
    for original_tile_id in collision_tiles:
        if original_tile_id in tile_id_to_vram:
            vram_index = tile_id_to_vram[original_tile_id]
            remapped_collision_tiles.append(vram_index)

    # Generate collision bitmap with remapped VRAM indices
    collision_bitmap = generate_collision_bitmap(remapped_collision_tiles)

    lines = []
    lines.append(f"#ifndef {guard_name}")
    lines.append(f"#define {guard_name}")
    lines.append("")
    lines.append("#include <tonc.h>")
    lines.append("")

    # Level metadata
    lines.append(f"// Level: {data['name']}")
    if 'author' in data:
        lines.append(f"// Author: {data['author']}")
    lines.append(f"// Dimensions: {width}x{height} tiles ({width * 8}x{height * 8} pixels)")
    lines.append(f"// Tilesets: {len(tilesets)}")
    lines.append(f"// Layers: {len(layers)}")
    lines.append(f"// Unique tiles: {len(unique_tiles)}")
    lines.append("")

    # Tile mapping data (original tile ID list for loading)
    lines.append(f"// Unique tile IDs used in this level ({len(unique_tiles)} tiles)")
    lines.append(f"static const u16 {level_name}_unique_tile_ids[{len(unique_tiles)}] = {{")
    for i in range(0, len(unique_tiles), 16):
        chunk = unique_tiles[i:i+16]
        line = "    " + ", ".join(f"{tile:5d}" for tile in chunk)
        if i + 16 < len(unique_tiles):
            line += ","
        lines.append(line)
    lines.append("};")
    lines.append("")

    # Pre-computed palette banks for each unique tile
    lines.append(f"// Pre-computed palette banks for unique tiles")
    lines.append(f"static const u8 {level_name}_tile_palette_banks[{len(tile_palette_banks)}] = {{")
    for i in range(0, len(tile_palette_banks), 32):
        chunk = tile_palette_banks[i:i+32]
        line = "    " + ", ".join(f"{pb}" for pb in chunk)
        if i + 32 < len(tile_palette_banks):
            line += ","
        lines.append(line)
    lines.append("};")
    lines.append("")

    # Tile data for each layer (u16 VRAM indices, not original tile IDs!)
    for layer_idx, layer in enumerate(remapped_layers):
        layer_tiles = layer['tiles']
        lines.append(f"// Layer {layer_idx}: {layer['name']} (BG{layer['bgLayer']}, priority {layer['priority']})")
        lines.append(f"static const u16 {level_name}_layer{layer_idx}_tiles[{len(layer_tiles)}] = {{")

        # Format tiles in rows of 16 for readability
        for i in range(0, len(layer_tiles), 16):
            chunk = layer_tiles[i:i+16]
            line = "    " + ", ".join(f"{tile:5d}" for tile in chunk)
            if i + 16 < len(layer_tiles):
                line += ","
            lines.append(line)

        lines.append("};")
        lines.append("")

    # Collision bitmap
    lines.append(f"static const u32 {level_name}_collision[64] = {{")
    for i in range(0, 64, 8):
        chunk = collision_bitmap[i:i+8]
        line = "    " + ", ".join(f"0x{val:08X}" for val in chunk)
        if i + 8 < 64:
            line += ","
        lines.append(line)
    lines.append("};")
    lines.append("")

    # Layer metadata array
    lines.append(f"static const TileLayer {level_name}_layers[{len(remapped_layers)}] = {{")
    for layer_idx, layer in enumerate(remapped_layers):
        comma = "," if layer_idx < len(remapped_layers) - 1 else ""
        lines.append(f'    {{"{layer["name"]}", {layer["bgLayer"]}, {layer["priority"]}, {level_name}_layer{layer_idx}_tiles}}{comma}')
    lines.append("};")
    lines.append("")

    # Object data
    if objects:
        lines.append(f"static const LevelObject {level_name}_objects[{len(objects)}] = {{")
        for i, obj in enumerate(objects):
            obj_type = obj.get('type', 'unknown')
            obj_x = obj.get('x', 0)
            obj_y = obj.get('y', 0)
            comma = "," if i < len(objects) - 1 else ""
            lines.append(f'    {{"{obj_type}", {obj_x}, {obj_y}}}{comma}')
        lines.append("};")
    else:
        # Empty array for no objects
        lines.append(f"static const LevelObject {level_name}_objects[1] = {{{{\"none\", 0, 0}}}};")
    lines.append("")

    # Level instance
    lines.append(f"static const Level {level_name} = {{")
    lines.append(f'    "{data["name"]}",')
    lines.append(f"    {width},")
    lines.append(f"    {height},")
    lines.append(f"    {len(remapped_layers)},  // layerCount")
    lines.append(f"    {level_name}_layers,")
    lines.append(f"    {len(objects)},")
    lines.append(f"    {level_name}_objects,")
    lines.append(f"    {spawn['x']},")
    lines.append(f"    {spawn['y']},")
    lines.append(f"    0,  // tilesetCount (set at runtime)")
    lines.append(f"    0,  // tilesets (set at runtime)")
    lines.append(f"    {level_name}_collision,")
    lines.append(f"    {len(unique_tiles)},  // uniqueTileCount")
    lines.append(f"    {level_name}_unique_tile_ids,  // uniqueTileIds")
    lines.append(f"    {level_name}_tile_palette_banks  // tilePaletteBanks")
    lines.append("};")
    lines.append("")
    lines.append(f"#endif // {guard_name}")
    lines.append("")

    return "\n".join(lines)


def main():
    if len(sys.argv) != 3:
        print("Usage: python level_converter.py input.tmx output.h")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)

    try:
        # Parse TMX file
        data = parse_tmx_file(input_file)

        # Validate level data
        validate_level(data, input_file)

        # Generate header
        output_name = os.path.basename(output_file)
        header_content = generate_header(data, output_name)

        # Write output
        with open(output_file, 'w') as f:
            f.write(header_content)

        print(f"Successfully converted {input_file} to {output_file}")
        print(f"  Level: {data['name']}")
        print(f"  Dimensions: {data['width']}x{data['height']}")
        print(f"  Tilesets: {len(data['tilesets'])}")
        print(f"  Objects: {len(data['objects'])}")

    except ET.ParseError as e:
        print(f"Error: Invalid XML in {input_file}: {e}")
        sys.exit(1)
    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
