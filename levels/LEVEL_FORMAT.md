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
  },
  "tilesets": [
    {
      "name": "grassy_stone",
      "file": "grassy_stone.png",
      "firstId": 1,
      "tileCount": 55
    },
    {
      "name": "plants",
      "file": "plants.png",
      "firstId": 56,
      "tileCount": 160
    },
    {
      "name": "decals",
      "file": "decals.png",
      "firstId": 216,
      "tileCount": 1225
    }
  ],
  "collisionTiles": [1, 2, 3, ...]
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

- **tiles** (2D array of integers): Row-major tile grid where each value is a tile ID (u16)
  - Array dimensions: `height` rows × `width` columns
  - Tile ID 0 = empty/sky tile
  - Tile IDs 1-55 = grassy_stone tiles (solid terrain)
  - Tile IDs 56-215 = plants tiles (decorative, non-solid)
  - Tile IDs 216-1440 = decals tiles (decorative, non-solid)
  - Format: `[[row0_col0, row0_col1, ...], [row1_col0, row1_col1, ...], ...]`

### Tilesets Array (Optional)

- **tilesets** (array of objects): List of tilesets used in the level
  - Each tileset has:
    - **name** (string): Tileset identifier
    - **file** (string): Source PNG filename
    - **firstId** (number): First tile ID for this tileset
    - **tileCount** (number): Number of tiles in the tileset
  - If omitted, defaults to single grassy_stone tileset (IDs 1-55)

### Collision Tiles Array (Optional)

- **collisionTiles** (array of integers): List of tile IDs that are solid/collideable
  - If omitted, defaults to tiles 1-55 (grassy_stone) being solid
  - All other tiles are non-solid (decorative)

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

### Available Tilesets

| Tileset | Tile ID Range | Tile Count | Source | Default Collision |
|---------|---------------|------------|--------|-------------------|
| Sky | 0 | 1 | Built-in | Non-solid |
| grassy_stone | 1-55 | 55 | grassy_stone.png (88×40px) | Solid |
| plants | 56-215 | 160 | plants.png (160×64px) | Non-solid |
| decals | 216-1440 | 1225 | decals.png (392×200px) | Non-solid |

**Total available tiles:** 1441 (including sky)

### Tileset Details

#### grassy_stone (Terrain)
- 88×40 pixels = 11×5 tiles = 55 tiles
- Used for: Ground, walls, platforms
- Default: All tiles are solid/collideable

#### plants (Decorative)
- 160×64 pixels = 20×8 tiles = 160 tiles
- Used for: Vegetation, foliage decoration
- Default: All tiles are non-solid (pass-through)

#### decals (Decorative)
- 392×200 pixels = 49×25 tiles = 1225 tiles
- Used for: Background details, decorations
- Default: All tiles are non-solid (pass-through)

## Object Types

### Supported Object Types

- **player_spawn**: Player starting position
  - Properties: none

- **enemy**: Enemy entity (future implementation)
  - Properties: `{ "enemyType": "skeleton" }`

- **item**: Collectible item (future implementation)
  - Properties: `{ "itemType": "coin" }`

## Example Level

See `level3.json` for a complete example.

## Conversion to C

The Python converter (`tools/level_converter.py`) generates a C header file with:

```c
typedef struct {
    const char* name;
    u16 firstId;
    u16 tileCount;
    const u32* tileData;
    u32 tileDataLen;
    const u16* paletteData;
    u16 paletteLen;
} TilesetInfo;

typedef struct {
    const char* name;
    u16 width;
    u16 height;
    const u16* tiles;           // Note: u16 to support >255 tile IDs
    u16 objectCount;
    const LevelObject* objects;
    u16 playerSpawnX;
    u16 playerSpawnY;
    u8 tilesetCount;
    const TilesetInfo* tilesets;
    const u32* collisionBitmap; // Bit array for collision lookup
} Level;
```

### Collision Bitmap

The collision bitmap is a 256-byte (64 u32) array where each bit represents whether a tile ID is solid:
- Bit set (1) = tile is solid/collideable
- Bit clear (0) = tile is non-solid/pass-through

## Constraints

- Maximum level width: 256 tiles (2048 pixels)
- Maximum level height: 256 tiles (2048 pixels)
- Maximum tile ID: 65535 (u16 limit), but practical limit is ~1440
- Maximum objects per level: 256
- **VRAM limit**: Maximum ~1000 unique tiles per level (GBA hardware constraint)
- Total level data must fit in GBA memory (consider ~100KB reasonable limit)

## Validation Rules

1. Width and height must be positive integers
2. Tiles array must have exactly `height` rows
3. Each row must have exactly `width` columns
4. All tile IDs must be valid (0-65535)
5. Object positions must be within level bounds
6. Player spawn position must be within level bounds
7. Player spawn is required
8. **Warning**: Levels using >1000 unique tiles may exceed VRAM

## Backward Compatibility

Levels without `tilesets` or `collisionTiles` fields are automatically migrated:
- Default tileset: grassy_stone (IDs 1-55)
- Default collision: tiles 1-55 are solid, all others non-solid
