CC     = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

SRCS = main.c emu.c ../../core/world.c ../../core/character.c
TARGET = emu

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: clean
