# GBA Level Editor

A complete level editing system for the GBA platformer game, including a web-based editor and build integration with multi-tileset support.

## Features

- **Web-based Level Editor**: Visual tile-based editor with multi-tile selection and placement
- **Multi-Tileset Support**: grassy_stone (terrain), plants (decorative), decals (backgrounds)
- **JSON Level Format**: Human-readable level files with tileset metadata
- **Build Integration**: Automatic conversion of JSON levels to C headers during compilation
- **Scrolling Levels**: Support for levels larger than the GBA screen (240x160)
- **Flexible Collision System**: Per-tile collision configuration with bitmap lookup
- **Camera System**: Smooth camera following with level bounds clamping

## Directory Structure

```
gbatest/
├── editor/               # Web-based level editor
│   ├── index.html       # Editor HTML
│   ├── editor.css       # Editor styles
│   ├── editor.js        # Editor logic
│   ├── Grassy_stone.png # Terrain tileset (88×40px)
│   ├── plants.png       # Plants tileset (160×64px)
│   ├── decals.png       # Decals tileset (392×200px)
│   └── skelly.png       # Player sprite preview
├── levels/              # Level JSON files
│   ├── LEVEL_FORMAT.md  # Level format specification
│   └── level*.json      # Level files
├── tools/               # Build tools
│   └── level_converter.py  # JSON to C converter
├── src/level/           # Level system code
│   ├── level.h          # Level API
│   └── level.c          # Level loading
└── [game files]         # Main GBA game source
```

## Using the Level Editor

### Opening the Editor

1. Open `editor/index.html` in a web browser (Chrome, Firefox, or Edge recommended)
2. The editor loads with a default empty level

### Editor Controls

**Tools:**
- **Brush (B)**: Paint tiles onto the level
- **Eraser (E)**: Remove tiles
- **Fill (F)**: Flood fill an area with the selected tile
- **Object (O)**: Place objects (player spawn, enemies, items)

**Mouse:**
- **Left Click**: Place tile/object
- **Right Click**: Erase tile
- **Click + Drag (Brush)**: Paint multiple tiles
- **Click + Drag (Tileset)**: Select multiple tiles for stamp placement
- **Middle Click / Shift+Drag**: Pan the view
- **Scroll Wheel**: Zoom in/out

**Keyboard Shortcuts:**
- `B` - Brush tool
- `E` - Eraser tool
- `F` - Fill tool
- `O` - Object tool
- `G` - Toggle grid display
- `Ctrl+Z` - Undo
- `Ctrl+Y` / `Ctrl+Shift+Z` - Redo
- `Space + Drag` - Pan view

### Multi-Tile Placement

The editor supports selecting and placing multiple tiles at once:

1. Click and drag in the tileset palette to select a rectangular region
2. Selected tiles are highlighted with a blue border
3. Click on the level to place the entire tile group
4. The stamp follows your cursor as you paint

This is useful for placing large structures or repeated patterns.

### Creating a Level

1. **Set Level Properties** (left panel):
   - Enter level name and author
   - Set dimensions (width and height in tiles)
   - Click "Resize Level" to apply changes

2. **Select Tiles** (left panel):
   - Click a tile in the palette to select it
   - Or drag to select multiple tiles for stamp placement
   - Tile 0 = sky (transparent/air)
   - Tiles 1-55 = grassy_stone terrain (solid)
   - Tiles 56-215 = plants decorations (non-solid)
   - Tiles 216-1440 = decals backgrounds (non-solid)

3. **Paint the Level**:
   - Select the Brush tool
   - Click and drag to place tiles
   - Use multi-tile selection for structures
   - Use the Fill tool for large areas
   - Right-click to erase

4. **Set Player Spawn**:
   - Switch to Object tool
   - Select "Player Spawn" from dropdown
   - Click on the level to set spawn point
   - Or use "Set Player Spawn Here" button

5. **Save the Level**:
   - Click "Save Level" button
   - Choose filename (e.g., `level2.json`)
   - Save to the `levels/` directory

### Loading a Level

1. Click "Load Level" button
2. Select a `.json` file from the `levels/` directory
3. The level will load into the editor with all tilesets

## Available Tilesets

| Tileset | Tile ID Range | Size | Tile Count | Default Collision |
|---------|---------------|------|------------|-------------------|
| Sky | 0 | 1×1 | 1 | Non-solid |
| grassy_stone | 1-55 | 11×5 tiles (88×40px) | 55 | Solid |
| plants | 56-215 | 20×8 tiles (160×64px) | 160 | Non-solid |
| decals | 216-1440 | 49×25 tiles (392×200px) | 1225 | Non-solid |

**Total available tiles:** 1441 (including sky)

### Tileset Details

#### grassy_stone (Terrain)
- Used for: Ground, walls, platforms, solid surfaces
- Default: All tiles are solid/collideable
- Appears in foreground layer (BG2)

#### plants (Decorative)
- Used for: Vegetation, grass, flowers, foliage
- Default: All tiles are non-solid (pass-through)
- Appears in foreground layer (BG2)

#### decals (Decorative)
- Used for: Background details, environmental decorations
- Default: All tiles are non-solid (pass-through)
- Appears in foreground layer (BG2)

## Level Format

Levels are stored as JSON files. See `levels/LEVEL_FORMAT.md` for complete specification.

**Example:**
```json
{
  "name": "Tutorial Level",
  "author": "Your Name",
  "width": 60,
  "height": 20,
  "tileWidth": 8,
  "tileHeight": 8,
  "playerSpawn": {
    "x": 40,
    "y": 96
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
  "collisionTiles": [1, 2, 3, 4, 5, ...],
  "tiles": [
    [0, 0, 0, ...],
    [0, 0, 0, ...],
    ...
  ],
  "objects": []
}
```

## Building the Game with Levels

### Automatic Build Integration

The build system automatically converts JSON levels to C headers:

1. Place your `.json` level file in the `levels/` directory
2. Run `make`
3. The level converter runs automatically
4. A corresponding `.h` file is generated (e.g., `level3.json` → `level3.h`)

The converter:
- Validates level format
- Converts tile array to C data
- Generates collision bitmap for fast lookup
- Embeds tileset metadata

### Manual Conversion

You can manually convert levels:

```bash
python tools/level_converter.py levels/level3.json level3.h
```

### Using Levels in Code

Include the level header and reference the level structure:

```c
#include "level3.h"

int main() {
    const Level* currentLevel = &Tutorial_Level;

    // Access level data
    int width = currentLevel->width;
    int height = currentLevel->height;
    u16 tile = getTileAt(currentLevel, x, y);
    int spawnX = currentLevel->playerSpawnX;
    int spawnY = currentLevel->playerSpawnY;

    // Load level tiles to VRAM
    loadLevelToVRAM(currentLevel);

    // Check tile collision
    if (isTileSolid(currentLevel, tile)) {
        // Handle collision
    }

    // ... game loop
}
```

## Level System Architecture

### Level Structure (C)

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
    u16 width;                    // Width in tiles
    u16 height;                   // Height in tiles
    const u16* tiles;             // Flat array of tile IDs (u16 for >255 IDs)
    u16 objectCount;              // Number of objects
    const LevelObject* objects;
    u16 playerSpawnX;             // Spawn X in pixels
    u16 playerSpawnY;             // Spawn Y in pixels
    u8 tilesetCount;              // Number of tilesets
    const TilesetInfo* tilesets;  // Tileset metadata
    const u32* collisionBitmap;   // Bit array for collision (256 bytes)
    const u8* tilePaletteBanks;   // Palette bank per tile (for VRAM)
} Level;
```

### Camera System

The camera follows the player and is clamped to level bounds:
- Camera tracks player position
- Clamped to prevent showing area outside level
- Background scrolling uses hardware registers (REG_BG2HOFS/VOFS)
- Smooth pixel-perfect scrolling

### Collision Detection

Tile-based collision system with bitmap lookup:
- `isTileSolid()` uses collision bitmap for O(1) lookup
- Bitmap: 1 bit per tile ID (bit set = solid, bit clear = non-solid)
- Default: tiles 1-55 are solid (grassy_stone), others non-solid
- Player checks tiles in collision radius
- Collision response resolves on minimum overlap axis
- Supports floor, ceiling, and wall collisions

### VRAM Tile Loading

The `loadLevelToVRAM()` function:
- Scans level data to find unique tiles used
- Loads only used tiles to VRAM (saves memory)
- Remaps tile IDs to VRAM indices
- Assigns palette banks based on tileset

**VRAM Layout:**
- Char block 0: Game tiles (terrain + decorations)
- Char block 1: Text system dynamic tiles
- Char block 2: Background (nightsky)
- Screen block 26: BG2 tilemap (game)
- Screen block 28: BG3 tilemap (text)

## Tips for Level Design

1. **Start Small**: Begin with a 60x20 tile level (480x160 pixels, 2 screens wide)
2. **Test Early**: Save and test your level in the game frequently
3. **Use Tilesets Appropriately**: grassy_stone for platforms, plants/decals for decoration
4. **Spawn Placement**: Place player spawn with clearance above and solid ground below
5. **Challenge Progression**: Design levels to gradually introduce mechanics
6. **Visual Variety**: Mix tilesets to create visual interest
7. **Multi-Tile Stamps**: Use multi-tile selection for repeated structures
8. **VRAM Budget**: Keep unique tile count under ~1000 to avoid VRAM overflow

## Constraints and Limits

- **Maximum level size**: 256×256 tiles (2048×2048 pixels)
- **Maximum tile IDs**: 65535 (u16), but practical limit is ~1441
- **VRAM capacity**: ~1000 unique tiles per level (GBA hardware constraint)
- **Total tile IDs**: 1441 (0 + 55 + 160 + 1225)
- **Collision bitmap**: 256 bytes (covers tile IDs 0-2047)
- **Memory budget**: Keep total level data under ~100KB

## Troubleshooting

### Editor doesn't load tiles
- Ensure tileset PNG files are in `editor/` directory
- Check browser console for errors
- Verify PNG files match expected names/sizes

### Level doesn't appear in game
- Check that the JSON file is in `levels/` directory
- Run `make clean` then `make` to force rebuild
- Verify JSON syntax is valid (use JSON validator)
- Check for errors in build output

### Player falls through floor
- Ensure tiles 1-55 (grassy_stone) are used for solid ground
- Check `collisionTiles` array includes the tile IDs
- Verify player spawn Y coordinate isn't inside solid tiles

### Camera doesn't scroll
- Level must be larger than 240×160 pixels for horizontal/vertical scrolling
- Check that camera bounds are set correctly in code

### Tiles appear corrupted in game
- Level may exceed VRAM capacity (~1000 unique tiles)
- Reduce number of unique tiles used
- Check that tileset PNGs are 4bpp indexed color format

### Wrong tiles appear
- Verify tile ID ranges match tileset assignments
- Check that level converter ran successfully
- Ensure tileset metadata in JSON is correct

## Extending the System

### Adding New Tilesets

1. Create tileset graphics as PNG (multiple of 8×8 pixels, indexed color)
2. Use `grit` to convert to GBA format (see Makefile for examples)
3. Add tileset to `editor/` directory
4. Update editor.js to load the new tileset
5. Add tileset metadata to level JSON
6. Update palette setup in `main.c`

### Adding New Object Types

1. Add object type to editor dropdown (`editor/index.html`)
2. Update level converter validation (`tools/level_converter.py`)
3. Define object behavior in game code
4. Add object rendering to game loop
5. Handle object collisions/interactions

### Multiple Levels

To support multiple levels:
1. Create multiple JSON files in `levels/`
2. Include all level headers in `main.c`
3. Add level selection/transition logic
4. Implement checkpoints or level gates

### Custom Collision Shapes

To add per-tile collision shapes:
1. Extend `TilesetInfo` to include collision data
2. Update level converter to embed collision shapes
3. Modify `isTileSolid()` to use shape data
4. Implement shape-based collision detection

## Performance Notes

- **Level loading**: O(width × height) to scan for unique tiles
- **Collision checking**: O(1) bitmap lookup per tile
- **VRAM uploads**: Only unique tiles are loaded
- **Rendering**: Hardware-accelerated background scrolling
- **Memory usage**: Level data stored in ROM, not RAM
