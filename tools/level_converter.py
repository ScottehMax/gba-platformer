#!/usr/bin/env python3
"""
Level Converter - Converts JSON level files to C header files for GBA
Usage: python level_converter.py input.json output.h
"""

import json
import sys
import os
from typing import Dict, List, Any


def validate_level(data: Dict[str, Any], filename: str) -> None:
    """Validate level data structure and constraints."""
    errors = []
    
    # Required fields
    required_fields = ['name', 'width', 'height', 'tiles', 'playerSpawn']
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
    
    # Validate tile array dimensions
    tiles = data['tiles']
    if len(tiles) != height:
        errors.append(f"Tiles array has {len(tiles)} rows, expected {height}")
    
    for i, row in enumerate(tiles):
        if len(row) != width:
            errors.append(f"Row {i} has {len(row)} columns, expected {width}")
        for j, tile_id in enumerate(row):
            if not isinstance(tile_id, int) or tile_id < 0 or tile_id > 255:
                errors.append(f"Invalid tile ID at ({i},{j}): {tile_id}")
    
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


def generate_header(data: Dict[str, Any], output_name: str) -> str:
    """Generate C header file content from level data."""
    level_name = sanitize_identifier(data['name'])
    guard_name = f"{output_name.upper().replace('.', '_')}_H"
    
    width = data['width']
    height = data['height']
    tiles = data['tiles']
    objects = data.get('objects', [])
    spawn = data['playerSpawn']
    
    # Flatten tile array
    flat_tiles = [tile for row in tiles for tile in row]
    
    lines = []
    lines.append(f"#ifndef {guard_name}")
    lines.append(f"#define {guard_name}")
    lines.append("")
    lines.append("#include \"gba.h\"")
    lines.append("")
    
    # Level metadata
    lines.append(f"// Level: {data['name']}")
    if 'author' in data:
        lines.append(f"// Author: {data['author']}")
    lines.append(f"// Dimensions: {width}x{height} tiles ({width * 8}x{height * 8} pixels)")
    lines.append("")
    
    # Tile data
    lines.append(f"static const u8 {level_name}_tiles[{len(flat_tiles)}] = {{")
    
    # Format tiles in rows of 16 for readability
    for i in range(0, len(flat_tiles), 16):
        chunk = flat_tiles[i:i+16]
        line = "    " + ", ".join(f"{tile:3d}" for tile in chunk)
        if i + 16 < len(flat_tiles):
            line += ","
        lines.append(line)
    
    lines.append("};")
    lines.append("")
    
    # Object structure definition (if not already defined elsewhere)
    lines.append("#ifndef LEVEL_OBJECT_DEFINED")
    lines.append("#define LEVEL_OBJECT_DEFINED")
    lines.append("typedef struct {")
    lines.append("    const char* type;")
    lines.append("    u16 x;")
    lines.append("    u16 y;")
    lines.append("} LevelObject;")
    lines.append("#endif")
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
    
    # Level structure definition
    lines.append("#ifndef LEVEL_STRUCT_DEFINED")
    lines.append("#define LEVEL_STRUCT_DEFINED")
    lines.append("typedef struct {")
    lines.append("    const char* name;")
    lines.append("    u16 width;")
    lines.append("    u16 height;")
    lines.append("    const u8* tiles;")
    lines.append("    u16 objectCount;")
    lines.append("    const LevelObject* objects;")
    lines.append("    u16 playerSpawnX;")
    lines.append("    u16 playerSpawnY;")
    lines.append("} Level;")
    lines.append("#endif")
    lines.append("")
    
    # Level instance
    lines.append(f"static const Level {level_name} = {{")
    lines.append(f'    "{data["name"]}",')
    lines.append(f"    {width},")
    lines.append(f"    {height},")
    lines.append(f"    {level_name}_tiles,")
    lines.append(f"    {len(objects)},")
    lines.append(f"    {level_name}_objects,")
    lines.append(f"    {spawn['x']},")
    lines.append(f"    {spawn['y']}")
    lines.append("};")
    lines.append("")
    lines.append(f"#endif // {guard_name}")
    lines.append("")
    
    return "\n".join(lines)


def main():
    if len(sys.argv) != 3:
        print("Usage: python level_converter.py input.json output.h")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)
    
    try:
        # Load and validate JSON
        with open(input_file, 'r') as f:
            data = json.load(f)
        
        validate_level(data, input_file)
        
        # Generate header
        output_name = os.path.basename(output_file)
        header_content = generate_header(data, output_name)
        
        # Write output
        with open(output_file, 'w') as f:
            f.write(header_content)
        
        print(f"Successfully converted {input_file} to {output_file}")
        
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in {input_file}: {e}")
        sys.exit(1)
    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
