TOOLCHAIN = arm-none-eabi-
CC = $(TOOLCHAIN)gcc
OBJCOPY = $(TOOLCHAIN)objcopy
GBAFIX = gbafix
GRIT = grit
PYTHON = python

GENDIR = generated
SRCDIR = src
CFLAGS = -mthumb-interwork -mthumb -O2 -Wall -I. -I$(GENDIR) -I$(SRCDIR)
LDFLAGS = -specs=gba.specs

TARGET = game

# Automatically find all PNG assets and create grit targets
ASSET_PNGS = $(wildcard assets/*.png)
GRIT_HEADERS = $(patsubst assets/%.png,$(GENDIR)/%.h,$(ASSET_PNGS))
GRIT_SOURCES = $(patsubst assets/%.png,$(GENDIR)/%.c,$(ASSET_PNGS))
GRIT_OBJS = $(patsubst assets/%.png,%.o,$(ASSET_PNGS))

# Level files
LEVEL_JSONS = $(wildcard levels/*.json)
LEVEL_HEADERS = $(patsubst levels/%.json,$(GENDIR)/%.h,$(LEVEL_JSONS))

OBJS = main.o text.o debug_utils.o level.o camera.o collision.o player.o player_render.o $(GRIT_OBJS)

all: $(GENDIR) $(GRIT_HEADERS) $(LEVEL_HEADERS) $(TARGET).gba

# Create generated directory if it doesn't exist
$(GENDIR):
	mkdir -p $(GENDIR)

# Grit rules for sprite PNGs (with transparency, 16-color mode for palettes)
$(GENDIR)/skelly.c $(GENDIR)/skelly.h: assets/skelly.png | $(GENDIR)
	$(GRIT) $< -gB4 -gt -gTFF00FF -ftc -o$(GENDIR)/skelly

# Font spritesheet (8x8 tiles, transparent magenta)
$(GENDIR)/tinypixie.c $(GENDIR)/tinypixie.h: assets/tinypixie.png | $(GENDIR)
	$(GRIT) $< -gB4 -gt -gTFF00FF -ftc -o$(GENDIR)/tinypixie

# Tileset rules - use 4-bit mode with separate palettes for each
# Terrain tileset - no transparency
$(GENDIR)/grassy_stone.c $(GENDIR)/grassy_stone.h: assets/grassy_stone.png | $(GENDIR)
	$(GRIT) $< -gB4 -gt -m! -ftc -o$(GENDIR)/grassy_stone

# Decorative tilesets - with transparency (magenta = FF00FF)
$(GENDIR)/plants.c $(GENDIR)/plants.h: assets/plants.png | $(GENDIR)
	$(GRIT) $< -gB4 -gt -gTFF00FF -m! -ftc -o$(GENDIR)/plants

$(GENDIR)/decals.c $(GENDIR)/decals.h: assets/decals.png | $(GENDIR)
	$(GRIT) $< -gB4 -gt -gTFF00FF -m! -ftc -o$(GENDIR)/decals

# Level converter
$(GENDIR)/%.h: levels/%.json | $(GENDIR)
	$(PYTHON) tools/level_converter.py $< $@

# Build targets
$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(GBAFIX) $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Object files from generated sources
%.o: $(GENDIR)/%.c $(GENDIR)/%.h
	$(CC) $(CFLAGS) -c $< -o $@

# Text module
text.o: text.c text.h gba.h $(GENDIR)/tinypixie.h assets/tinypixie_widths.h
	$(CC) $(CFLAGS) -c $< -o $@

# Debug utilities module
debug_utils.o: $(SRCDIR)/core/debug_utils.c $(SRCDIR)/core/debug_utils.h
	$(CC) $(CFLAGS) -c $< -o $@

# Level module
level.o: $(SRCDIR)/level/level.c $(SRCDIR)/level/level.h gba.h $(LEVEL_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Camera module
camera.o: $(SRCDIR)/camera/camera.c $(SRCDIR)/camera/camera.h $(SRCDIR)/core/game_types.h $(SRCDIR)/core/game_math.h $(SRCDIR)/level/level.h gba.h $(LEVEL_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Collision module
collision.o: $(SRCDIR)/collision/collision.c $(SRCDIR)/collision/collision.h $(SRCDIR)/core/game_types.h $(SRCDIR)/core/game_math.h $(SRCDIR)/level/level.h gba.h $(LEVEL_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Player module
player.o: $(SRCDIR)/player/player.c $(SRCDIR)/player/player.h $(SRCDIR)/core/game_types.h $(SRCDIR)/core/game_math.h $(SRCDIR)/collision/collision.h gba.h $(LEVEL_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Player rendering module
player_render.o: $(SRCDIR)/player/player_render.c $(SRCDIR)/player/player_render.h $(SRCDIR)/core/game_types.h $(SRCDIR)/core/game_math.h gba.h
	$(CC) $(CFLAGS) -c $< -o $@

# Main object depends on all headers
main.o: main.c gba.h text.h \
	$(SRCDIR)/core/game_math.h $(SRCDIR)/core/game_types.h $(SRCDIR)/core/debug_utils.h \
	$(SRCDIR)/level/level.h $(SRCDIR)/camera/camera.h $(SRCDIR)/collision/collision.h \
	$(SRCDIR)/player/player.h $(SRCDIR)/player/player_render.h \
	$(GRIT_HEADERS) $(LEVEL_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.elf *.gba
	rm -rf $(GENDIR)

.PHONY: all clean
