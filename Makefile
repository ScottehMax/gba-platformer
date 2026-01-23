TOOLCHAIN = arm-none-eabi-
CC = $(TOOLCHAIN)gcc
OBJCOPY = $(TOOLCHAIN)objcopy
GBAFIX = gbafix
GRIT = grit
PYTHON = python

CFLAGS = -mthumb-interwork -mthumb -O2 -Wall
LDFLAGS = -specs=gba.specs

TARGET = game
OBJS = main.o skelly.o ground.o ground2.o

# Level files
LEVEL_JSONS = $(wildcard levels/*.json)
LEVEL_HEADERS = $(patsubst levels/%.json,%.h,$(LEVEL_JSONS))

all: $(LEVEL_HEADERS) $(TARGET).gba

skelly.h: assets/skelly.png
	$(GRIT) assets/skelly.png -gB8 -gt -gTFF00FF -ftc

ground.h: assets/ground.png
	$(GRIT) assets/ground.png -gB8 -gt -m! -ftc

ground2.h: assets/ground2.png
	$(GRIT) assets/ground2.png -gB8 -gt -m! -ftc

%.h: levels/%.json
	$(PYTHON) tools/level_converter.py $< $@

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(GBAFIX) $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

skelly.o: skelly.c skelly.h
	$(CC) $(CFLAGS) -c $< -o $@

ground.o: ground.c ground.h
	$(CC) $(CFLAGS) -c $< -o $@

ground2.o: ground2.c ground2.h
	$(CC) $(CFLAGS) -c $< -o $@

main.o: main.c gba.h skelly.h ground.h ground2.h $(LEVEL_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c gba.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.elf *.gba skelly.h skelly.c ground.h ground.c ground2.h ground2.c $(LEVEL_HEADERS)

.PHONY: all clean
