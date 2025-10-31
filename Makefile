CC = gcc
CFLAGS = $(shell sdl2-config --cflags) -Wall
LDFLAGS = $(shell sdl2-config --libs) -lSDL2_ttf -lm
TARGET = monopoly
SOURCES = Game.c Render.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run