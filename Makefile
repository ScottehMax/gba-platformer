TOOLCHAIN = arm-none-eabi-
CC = $(TOOLCHAIN)gcc
OBJCOPY = $(TOOLCHAIN)objcopy
GBAFIX = gbafix
GRIT = grit

CFLAGS = -mthumb-interwork -mthumb -O2 -Wall
LDFLAGS = -specs=gba.specs

TARGET = game
OBJS = main.o skelly.o ground.o ground2.o

all: $(TARGET).gba

skelly.h: assets/skelly.png
	$(GRIT) assets/skelly.png -gB8 -gt -gTFF00FF -ftc

ground.h: assets/ground.png
	$(GRIT) assets/ground.png -gB8 -gt -m! -ftc

ground2.h: assets/ground2.png
	$(GRIT) assets/ground2.png -gB8 -gt -m! -ftc

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

main.o: main.c gba.h skelly.h ground.h ground2.h
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c gba.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.elf *.gba skelly.h skelly.c ground.h ground.c ground2.h ground2.c

.PHONY: all clean
