# ============================================================================
# Connect Four Online - C Server Makefile (Group 11)
# ============================================================================

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11 -D_GNU_SOURCE -O2 -g
INCLUDES := -I. -I/usr/include/postgresql -I/usr/include/cjson
LDLIBS   := -lpq -lcjson -lcrypt -lm

TARGET   := connect4_server
BUILD_DIR := build

# Automatically pick up existing .c source files in server_c/
SRCS     := $(wildcard *.c)
OBJS     := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean check-protocol test

all: $(TARGET)

$(TARGET): $(OBJS)
	@if [ -z "$(OBJS)" ]; then \
		echo "No .c files found yet. Checking protocol.h syntax..."; \
		$(CC) $(CFLAGS) -fsyntax-only protocol.h && echo "protocol.h syntax OK."; \
	else \
		$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS); \
	fi

$(BUILD_DIR)/%.o: %.c protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

check-protocol:
	$(CC) $(CFLAGS) -fsyntax-only protocol.h
	@echo "protocol.h syntax check passed."

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

