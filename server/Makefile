CC := gcc
CFLAGS := -Wall -Wextra -I../shared -MMD -MP
LDLIBS := -lsqlite3 -lpthread -lssl -lcrypto -lm

BUILD_DIR := ../build/server
TARGET := $(BUILD_DIR)/monopoly_server

SOURCES := server_main.c auth.c database.c ../shared/protocol.c ../shared/cJSON.c
OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SOURCES)))
DEPS := $(OBJECTS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJECTS) -o $@ $(LDLIBS)
	@echo "✓ Server built: $@"

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

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
	$(TARGET)

.PHONY: all clean run
