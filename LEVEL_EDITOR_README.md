# GBA Level Editor

A complete level editing system for the GBA platformer game, including a web-based editor and build integration.

## Features

- **Web-based Level Editor**: Visual tile-based editor with object placement
- **JSON Level Format**: Human-readable level files
- **Build Integration**: Automatic conversion of JSON levels to C headers during compilation
- **Scrolling Levels**: Support for levels larger than the GBA screen (240x160)
- **Tile-based Collision**: Automatic collision detection with level tiles
- **Camera System**: Smooth camera following with dead zones

## Directory Structure

```
gbatest/
├── editor/               # Web-based level editor
│   ├── index.html       # Editor HTML
│   ├── editor.css       # Editor styles
│   ├── editor.js        # Editor logic
│   └── *.png            # Tile assets
├── levels/              # Level JSON files
│   ├── LEVEL_FORMAT.md  # Level format specification
│   └── level1.json      # Example level
├── tools/               # Build tools
│   └── level_converter.py  # JSON to C converter
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

### Creating a Level

1. **Set Level Properties** (left panel):
   - Enter level name and author
   - Set dimensions (width and height in tiles)
   - Click "Resize Level" to apply changes

2. **Select Tiles** (left panel):
   - Click a tile in the palette to select it
   - Tile 0 = sky (transparent)
   - Tiles 1-4 = ground surface
   - Tiles 5-8 = underground

3. **Paint the Level**:
   - Select the Brush tool
   - Click and drag to place tiles
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
3. The level will load into the editor

## Level Format

Levels are stored as JSON files. See `levels/LEVEL_FORMAT.md` for complete specification.

**Example:**
```json
{
  "name": "Test Level 1",
  "author": "Your Name",
  "width": 60,
  "height": 20,
  "tileWidth": 8,
  "tileHeight": 8,
  "playerSpawn": {
    "x": 40,
    "y": 96
  },
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
4. A corresponding `.h` file is generated (e.g., `level1.json` → `level1.h`)

### Manual Conversion

You can manually convert levels:

```bash
python tools/level_converter.py levels/level1.json level1.h
```

### Using Levels in Code

Include the level header and reference the level structure:

```c
#include "level1.h"

int main() {
    const Level* currentLevel = &Test_Level_1;
    
    // Access level data
    int width = currentLevel->width;
    int height = currentLevel->height;
    u8 tile = currentLevel->tiles[y * width + x];
    int spawnX = currentLevel->playerSpawnX;
    int spawnY = currentLevel->playerSpawnY;
    
    // ... game loop
}
```

## Level System Architecture

### Level Structure (C)

```c
typedef struct {
    const char* name;
    u16 width;           // Width in tiles
    u16 height;          // Height in tiles
    const u8* tiles;     // Flat array of tile IDs
    u16 objectCount;     // Number of objects
    const LevelObject* objects;
    u16 playerSpawnX;    // Spawn X in pixels
    u16 playerSpawnY;    // Spawn Y in pixels
} Level;
```

### Camera System

The camera follows the player with a dead zone system:
- Camera scrolls when player moves outside 1/3 to 2/3 of screen
- Camera is clamped to level bounds
- Background scrolling uses hardware registers for smooth movement

### Collision Detection

Tile-based collision system:
- Player checks tiles in a radius around their position
- Solid tiles (IDs 1-8) block movement
- Collision response resolves on minimum overlap axis
- Supports floor, ceiling, and wall collisions

## Tips for Level Design

1. **Start Small**: Begin with a 60x20 tile level (480x160 pixels, 2 screens wide)
2. **Test Early**: Save and test your level in the game frequently
3. **Use Layers**: Ground surface (tiles 1-4) on top, underground (5-8) below
4. **Spawn Placement**: Place player spawn with enough clearance above and solid ground below
5. **Challenge Progression**: Design levels to gradually introduce mechanics
6. **Visual Variety**: Mix tile types to create visual interest

## Tile Reference

| ID | Type | Description |
|----|------|-------------|
| 0 | Sky | Transparent/air, no collision |
| 1-4 | Ground | Surface tiles with collision |
| 5-8 | Underground | Underground tiles with collision |

## Troubleshooting

**Editor doesn't load tiles:**
- Ensure tile image files (ground.png, ground2.png) are in `editor/` directory

**Level doesn't appear in game:**
- Check that the JSON file is in `levels/` directory
- Run `make clean` then `make` to force rebuild
- Verify JSON syntax is valid

**Player falls through floor:**
- Ensure tiles 1-8 are used for solid ground
- Check player spawn Y coordinate isn't inside solid tiles

**Camera doesn't scroll:**
- Level must be larger than 240x160 pixels (30x20 tiles minimum)
- Check camera bounds in code

## Extending the System

### Adding New Tiles

1. Create tile graphics (8x8 pixels, indexed color)
2. Use `grit` to convert to GBA format
3. Update palette setup in `main.c`
4. Add tiles to VRAM in tile setup code
5. Update tile palette in editor

### Adding New Object Types

1. Add object type to editor dropdown (`index.html`)
2. Update level converter validation
3. Implement object behavior in game code
4. Add object rendering in game loop

### Multiple Levels

To support multiple levels:
1. Create multiple JSON files in `levels/`
2. Include all level headers in `main.c`
3. Add level selection logic
4. Implement level transitions

## License

This level editor and tooling are provided as part of the GBA platformer project.
