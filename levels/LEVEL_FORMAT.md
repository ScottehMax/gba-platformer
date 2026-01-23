# Level Format Specification

## Overview
Levels are stored as JSON files that define tile grids and object placements. During the build process, a Python script converts these JSON files into C header files that can be compiled into the GBA game.

## JSON Structure

```json
{
  "name": "Level Name",
  "author": "Author Name",
  "width": 60,
  "height": 20,
  "tileWidth": 8,
  "tileHeight": 8,
  "tiles": [],
  "objects": [],
  "playerSpawn": {
    "x": 120,
    "y": 100
  }
}
```

## Field Definitions

### Metadata Fields

- **name** (string): Human-readable level name
- **author** (string, optional): Level creator's name
- **width** (integer): Level width in tiles
- **height** (integer): Level height in tiles
- **tileWidth** (integer): Width of each tile in pixels (default: 8)
- **tileHeight** (integer): Height of each tile in pixels (default: 8)

### Tile Layer

- **tiles** (2D array of integers): Row-major tile grid where each value is a tile ID
  - Array dimensions: `height` rows × `width` columns
  - Tile ID 0 = empty/sky tile
  - Tile IDs 1-4 = ground.png tiles (16x16, stored as 4 8x8 tiles)
  - Tile IDs 5-8 = ground2.png tiles (16x16, stored as 4 8x8 tiles)
  - Format: `[[row0_col0, row0_col1, ...], [row1_col0, row1_col1, ...], ...]`

### Object Layer

- **objects** (array of objects): List of dynamic entities in the level
  - Each object has:
    - **type** (string): Object type identifier (e.g., "player_spawn", "enemy", "item")
    - **x** (number): X position in pixels
    - **y** (number): Y position in pixels
    - **properties** (object, optional): Type-specific properties

### Player Spawn

- **playerSpawn** (object): Starting position for the player
  - **x** (number): X position in pixels
  - **y** (number): Y position in pixels

## Tile ID Reference

### Current Tile Set

| Tile ID | Description | Source |
|---------|-------------|--------|
| 0 | Sky/Empty | Built-in |
| 1 | Ground (top-left) | ground.png |
| 2 | Ground (top-right) | ground.png |
| 3 | Ground (bottom-left) | ground.png |
| 4 | Ground (bottom-right) | ground.png |
| 5 | Underground (top-left) | ground2.png |
| 6 | Underground (top-right) | ground2.png |
| 7 | Underground (bottom-left) | ground2.png |
| 8 | Underground (bottom-right) | ground2.png |

**Note**: 16x16 tiles are stored as four 8x8 tiles in this order:
```
[1, 2]  (top row)
[3, 4]  (bottom row)
```

## Object Types

### Supported Object Types

- **player_spawn**: Player starting position
  - Properties: none

- **enemy**: Enemy entity (future implementation)
  - Properties: `{ "enemyType": "skeleton" }`

- **item**: Collectible item (future implementation)
  - Properties: `{ "itemType": "coin" }`

## Example Level

See `level1.json` for a complete example.

## Conversion to C

The Python converter (`tools/level_converter.py`) generates a C header file with:

```c
typedef struct {
    const char* name;
    u16 width;
    u16 height;
    const u8* tiles;
    u16 objectCount;
    const LevelObject* objects;
    u16 playerSpawnX;
    u16 playerSpawnY;
} Level;
```

## Constraints

- Maximum level width: 256 tiles (2048 pixels)
- Maximum level height: 256 tiles (2048 pixels)
- Maximum tile ID: 255 (u8 limit)
- Maximum objects per level: 256
- Total level data must fit in GBA memory (consider ~100KB reasonable limit)

## Validation Rules

1. Width and height must be positive integers
2. Tiles array must have exactly `height` rows
3. Each row must have exactly `width` columns
4. All tile IDs must be valid (0-255)
5. Object positions must be within level bounds
6. Player spawn position must be within level bounds
7. Player spawn is required
