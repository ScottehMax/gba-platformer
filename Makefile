TOOLCHAIN = arm-none-eabi-
CC = $(TOOLCHAIN)gcc
OBJCOPY = $(TOOLCHAIN)objcopy
GBAFIX = gbafix
GRIT = grit
PYTHON = python

GENDIR = generated
CFLAGS = -mthumb-interwork -mthumb -O2 -Wall -I. -I$(GENDIR)
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

OBJS = main.o $(GRIT_OBJS)

all: $(GENDIR) $(GRIT_HEADERS) $(LEVEL_HEADERS) $(TARGET).gba

# Create generated directory if it doesn't exist
$(GENDIR):
	mkdir -p $(GENDIR)

# Grit rules for sprite PNGs (with transparency, 16-color mode for palettes)
$(GENDIR)/skelly.h: assets/skelly.png | $(GENDIR)
	$(GRIT) $< -gB4 -gt -gTFF00FF -ftc -o$(GENDIR)/skelly

# Grit rules for tileset PNGs (no map)
$(GENDIR)/%.h: assets/%.png | $(GENDIR)
	$(GRIT) $< -gB8 -gt -m! -ftc -o$(GENDIR)/$*

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

# Main object depends on all generated headers
main.o: main.c gba.h $(GRIT_HEADERS) $(LEVEL_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.elf *.gba
	rm -rf $(GENDIR)

.PHONY: all clean
