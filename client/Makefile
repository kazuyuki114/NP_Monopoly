CC := gcc
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf)
SDL_LIBS := $(shell pkg-config --libs sdl2 SDL2_ttf)

CFLAGS := -Wall -Wextra -I../shared $(SDL_CFLAGS) -MMD -MP
LDLIBS := $(SDL_LIBS) -lm

BUILD_DIR := ../build/client
TARGET := $(BUILD_DIR)/monopoly_client

# Main GUI client
SOURCES := client_main.c client_network.c lobby.c ../shared/protocol.c ../shared/cJSON.c
OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SOURCES)))
DEPS := $(OBJECTS:.o=.d)

# Test client (command line)
TEST_TARGET := $(BUILD_DIR)/monopoly_client_test
TEST_SOURCES := client_test.c client_network.c ../shared/protocol.c ../shared/cJSON.c
TEST_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/test_%.o,$(notdir $(TEST_SOURCES)))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJECTS) -o $@ $(LDLIBS)
	@echo "✓ Client GUI built: $@"

# Test client without SDL
test: $(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(TEST_OBJECTS) -o $@ -lm
	@echo "✓ Test client built: $@"

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -Wall -Wextra -I../shared -c $< -o $@

$(BUILD_DIR)/protocol.o: ../shared/protocol.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/cJSON.o: ../shared/cJSON.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_protocol.o: ../shared/protocol.c
	@mkdir -p $(dir $@)
	$(CC) -Wall -Wextra -I../shared -c $< -o $@

$(BUILD_DIR)/test_cJSON.o: ../shared/cJSON.c
	@mkdir -p $(dir $@)
	$(CC) -Wall -Wextra -I../shared -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

run: $(TARGET)
	cd .. && ./build/client/monopoly_client 127.0.0.1 8888

run-test: $(TEST_TARGET)
	$(TEST_TARGET) 127.0.0.1 8888

.PHONY: all test clean run run-test
