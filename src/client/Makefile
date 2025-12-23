CC := gcc
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf)
SDL_LIBS := $(shell pkg-config --libs sdl2 SDL2_ttf)

# Include paths: shared
CFLAGS := -Wall -Wextra -I../shared $(SDL_CFLAGS) -MMD -MP
LDLIBS := $(SDL_LIBS) -lm

BUILD_DIR := ../../build/client
TARGET := $(BUILD_DIR)/monopoly_client

# Client sources (server handles game logic)
CLIENT_SOURCES := client_main.c client_network.c lobby.c game_network.c
SHARED_SOURCES := ../shared/protocol.c ../shared/cJSON.c

# All sources
SOURCES := $(CLIENT_SOURCES) $(SHARED_SOURCES)

# Object files
CLIENT_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(CLIENT_SOURCES)))
SHARED_OBJECTS := $(BUILD_DIR)/protocol.o $(BUILD_DIR)/cJSON.o

OBJECTS := $(CLIENT_OBJECTS) $(SHARED_OBJECTS)
DEPS := $(OBJECTS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJECTS) -o $@ $(LDLIBS)
	@echo "✓ Client GUI built: $@"

# Client source files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Shared source files
$(BUILD_DIR)/protocol.o: ../shared/protocol.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/cJSON.o: ../shared/cJSON.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

run: $(TARGET)
	cd ../.. && SDL_VIDEODRIVER=x11 ./build/client/monopoly_client 127.0.0.1 8888

.PHONY: all clean run