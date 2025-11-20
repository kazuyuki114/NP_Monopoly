CC := gcc
SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS   := $(shell pkg-config --libs sdl2) $(shell pkg-config --libs SDL2_ttf)

CFLAGS := $(SDL_CFLAGS) -Iinclude -Wall -Wextra -MMD -MP
LDLIBS := $(SDL_LIBS) -lm

SRC_DIR := src
BUILD_DIR := build
TARGET := $(BUILD_DIR)/monopoly

SOURCES := $(SRC_DIR)/main.c $(wildcard $(SRC_DIR)/game/*.c $(SRC_DIR)/render/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJECTS) -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
