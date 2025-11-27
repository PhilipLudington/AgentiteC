# Carbon Game Engine - Cross-platform Makefile
# Supports macOS, Linux, and Windows (via MinGW)

# Project settings
PROJECT_NAME := carbon
BUILD_DIR := build
SRC_DIR := src
INCLUDE_DIR := include
LIB_DIR := lib

# Detect OS
UNAME_S := $(shell uname -s)

# Compiler settings
CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -I$(INCLUDE_DIR) -I$(LIB_DIR) -I$(LIB_DIR)/cglm/include
LDFLAGS :=

# Debug/Release builds
ifdef DEBUG
    CFLAGS += -g -O0 -DDEBUG
else
    CFLAGS += -O2 -DNDEBUG
endif

# SDL3 configuration (using pkg-config)
SDL3_CFLAGS := $(shell pkg-config --cflags sdl3 2>/dev/null)
SDL3_LDFLAGS := $(shell pkg-config --libs sdl3 2>/dev/null)

# Fallback if pkg-config fails
ifeq ($(SDL3_CFLAGS),)
    $(warning pkg-config failed for SDL3, using default paths)
    SDL3_CFLAGS := -I/usr/local/include -I/opt/homebrew/include
    SDL3_LDFLAGS := -L/usr/local/lib -L/opt/homebrew/lib -lSDL3
endif

CFLAGS += $(SDL3_CFLAGS)
LDFLAGS += $(SDL3_LDFLAGS)

# Platform-specific settings
ifeq ($(UNAME_S),Darwin)
    # macOS
    LDFLAGS += -framework Cocoa -framework Metal -framework QuartzCore
    EXECUTABLE := $(PROJECT_NAME)
else ifeq ($(UNAME_S),Linux)
    # Linux
    LDFLAGS += -lm -ldl -lpthread
    EXECUTABLE := $(PROJECT_NAME)
else
    # Windows (MinGW)
    LDFLAGS += -lmingw32 -lSDL3main -mwindows
    EXECUTABLE := $(PROJECT_NAME).exe
endif

# Source files
SRCS := $(wildcard $(SRC_DIR)/*.c) \
        $(wildcard $(SRC_DIR)/core/*.c) \
        $(wildcard $(SRC_DIR)/platform/*.c) \
        $(wildcard $(SRC_DIR)/graphics/*.c) \
        $(wildcard $(SRC_DIR)/audio/*.c) \
        $(wildcard $(SRC_DIR)/input/*.c) \
        $(wildcard $(SRC_DIR)/ui/*.c) \
        $(wildcard $(SRC_DIR)/ecs/*.c) \
        $(wildcard $(SRC_DIR)/ai/*.c) \
        $(wildcard $(SRC_DIR)/game/*.c)

# Flecs ECS library (compiled as separate object)
FLECS_SRC := $(LIB_DIR)/flecs.c
FLECS_OBJ := $(BUILD_DIR)/flecs.o

# Object files
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Default target
all: dirs $(BUILD_DIR)/$(EXECUTABLE)

# Create build directories
dirs:
	@mkdir -p $(BUILD_DIR)/core
	@mkdir -p $(BUILD_DIR)/platform
	@mkdir -p $(BUILD_DIR)/graphics
	@mkdir -p $(BUILD_DIR)/audio
	@mkdir -p $(BUILD_DIR)/input
	@mkdir -p $(BUILD_DIR)/ui
	@mkdir -p $(BUILD_DIR)/ecs
	@mkdir -p $(BUILD_DIR)/ai
	@mkdir -p $(BUILD_DIR)/game

# Link executable (include Flecs object)
$(BUILD_DIR)/$(EXECUTABLE): $(OBJS) $(FLECS_OBJ)
	$(CC) $(OBJS) $(FLECS_OBJ) -o $@ $(LDFLAGS)

# Compile Flecs (with relaxed warnings due to third-party code)
$(FLECS_OBJ): $(FLECS_SRC)
	$(CC) -std=c11 -O2 -I$(LIB_DIR) -c $< -o $@

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run the game
run: all
	./$(BUILD_DIR)/$(EXECUTABLE)

# Clean build files
clean:
	rm -rf $(BUILD_DIR)

# Install SDL3 (macOS only, informational)
install-deps-macos:
	@echo "Installing SDL3 via Homebrew..."
	brew install sdl3

# Install SDL3 (Linux, informational)
install-deps-linux:
	@echo "On Ubuntu/Debian:"
	@echo "  sudo apt install libsdl3-dev"
	@echo "On Fedora:"
	@echo "  sudo dnf install SDL3-devel"
	@echo "Or build from source: https://github.com/libsdl-org/SDL"

# Print configuration
info:
	@echo "Project: $(PROJECT_NAME)"
	@echo "OS: $(UNAME_S)"
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "Sources: $(SRCS)"

.PHONY: all dirs run clean install-deps-macos install-deps-linux info
