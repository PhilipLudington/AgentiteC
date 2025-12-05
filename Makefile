# Carbon Game Engine - Cross-platform Makefile
# Supports macOS, Linux, and Windows (via MinGW)

# Project settings
PROJECT_NAME := carbon
BUILD_DIR := build
SRC_DIR := src
INCLUDE_DIR := include
LIB_DIR := lib
EXAMPLES_DIR := examples

# Detect OS
UNAME_S := $(shell uname -s)

# Compiler settings
CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -I$(INCLUDE_DIR) -I$(LIB_DIR) -I$(LIB_DIR)/cglm/include -I$(SRC_DIR)
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

# Engine source files (no main.c - that's for apps)
ENGINE_SRCS := $(wildcard $(SRC_DIR)/core/*.c) \
               $(wildcard $(SRC_DIR)/platform/*.c) \
               $(wildcard $(SRC_DIR)/graphics/*.c) \
               $(wildcard $(SRC_DIR)/audio/*.c) \
               $(wildcard $(SRC_DIR)/input/*.c) \
               $(wildcard $(SRC_DIR)/ui/*.c) \
               $(wildcard $(SRC_DIR)/ecs/*.c) \
               $(wildcard $(SRC_DIR)/ai/*.c) \
               $(wildcard $(SRC_DIR)/strategy/*.c)

# Game template source files
GAME_SRCS := $(wildcard $(SRC_DIR)/game/*.c) \
             $(wildcard $(SRC_DIR)/game/systems/*.c) \
             $(wildcard $(SRC_DIR)/game/states/*.c) \
             $(wildcard $(SRC_DIR)/game/data/*.c)

# Main application source (uses game template)
MAIN_SRC := $(SRC_DIR)/main.c

# All source files for main build
SRCS := $(MAIN_SRC) $(ENGINE_SRCS) $(GAME_SRCS)

# Flecs ECS library (compiled as separate object)
FLECS_SRC := $(LIB_DIR)/flecs.c
FLECS_OBJ := $(BUILD_DIR)/flecs.o

# TOML parser library (compiled as separate object)
TOML_SRC := $(LIB_DIR)/toml.c
TOML_OBJ := $(BUILD_DIR)/toml.o

# Object files
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Demo example sources (standalone, doesn't use game template)
DEMO_SRC := $(EXAMPLES_DIR)/demo/main.c
DEMO_OBJS := $(BUILD_DIR)/examples/demo/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS))

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
	@mkdir -p $(BUILD_DIR)/strategy
	@mkdir -p $(BUILD_DIR)/game
	@mkdir -p $(BUILD_DIR)/game/systems
	@mkdir -p $(BUILD_DIR)/game/states
	@mkdir -p $(BUILD_DIR)/game/data
	@mkdir -p $(BUILD_DIR)/examples/demo
	@mkdir -p $(BUILD_DIR)/examples/minimal
	@mkdir -p $(BUILD_DIR)/examples/sprites
	@mkdir -p $(BUILD_DIR)/examples/animation
	@mkdir -p $(BUILD_DIR)/examples/tilemap
	@mkdir -p $(BUILD_DIR)/examples/ui
	@mkdir -p $(BUILD_DIR)/examples/strategy
	@mkdir -p $(BUILD_DIR)/examples/strategy-sim

# Link main executable (game template)
$(BUILD_DIR)/$(EXECUTABLE): $(OBJS) $(FLECS_OBJ) $(TOML_OBJ)
	$(CC) $(OBJS) $(FLECS_OBJ) $(TOML_OBJ) -o $@ $(LDFLAGS)

# Link demo executable
$(BUILD_DIR)/demo: $(DEMO_OBJS) $(FLECS_OBJ) $(TOML_OBJ)
	$(CC) $(DEMO_OBJS) $(FLECS_OBJ) $(TOML_OBJ) -o $@ $(LDFLAGS)

# Compile Flecs (with relaxed warnings due to third-party code)
$(FLECS_OBJ): $(FLECS_SRC)
	$(CC) -std=c11 -O2 -I$(LIB_DIR) -c $< -o $@

# Compile TOML parser (with relaxed warnings due to third-party code)
$(TOML_OBJ): $(TOML_SRC)
	$(CC) -std=c11 -O2 -I$(LIB_DIR) -c $< -o $@

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile example files
$(BUILD_DIR)/examples/%.o: $(EXAMPLES_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

#============================================================================
# Run targets
#============================================================================

# Run the main game (uses game template)
run: all
	./$(BUILD_DIR)/$(EXECUTABLE)

# Run the comprehensive demo
run-demo: dirs $(BUILD_DIR)/demo
	./$(BUILD_DIR)/demo

#============================================================================
# Example targets
#============================================================================

# Build and run minimal example
example-minimal: dirs $(BUILD_DIR)/examples/minimal/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CC) $(BUILD_DIR)/examples/minimal/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) -o $(BUILD_DIR)/example-minimal $(LDFLAGS)
	./$(BUILD_DIR)/example-minimal

# Build and run sprites example
example-sprites: dirs $(BUILD_DIR)/examples/sprites/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CC) $(BUILD_DIR)/examples/sprites/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) -o $(BUILD_DIR)/example-sprites $(LDFLAGS)
	./$(BUILD_DIR)/example-sprites

# Build and run animation example
example-animation: dirs $(BUILD_DIR)/examples/animation/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CC) $(BUILD_DIR)/examples/animation/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) -o $(BUILD_DIR)/example-animation $(LDFLAGS)
	./$(BUILD_DIR)/example-animation

# Build and run tilemap example
example-tilemap: dirs $(BUILD_DIR)/examples/tilemap/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CC) $(BUILD_DIR)/examples/tilemap/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) -o $(BUILD_DIR)/example-tilemap $(LDFLAGS)
	./$(BUILD_DIR)/example-tilemap

# Build and run UI example
example-ui: dirs $(BUILD_DIR)/examples/ui/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CC) $(BUILD_DIR)/examples/ui/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) -o $(BUILD_DIR)/example-ui $(LDFLAGS)
	./$(BUILD_DIR)/example-ui

# Build and run strategy example
example-strategy: dirs $(BUILD_DIR)/examples/strategy/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CC) $(BUILD_DIR)/examples/strategy/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) -o $(BUILD_DIR)/example-strategy $(LDFLAGS)
	./$(BUILD_DIR)/example-strategy

# Build and run strategy-sim example (demonstrates new strategy systems)
example-strategy-sim: dirs $(BUILD_DIR)/examples/strategy-sim/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CC) $(BUILD_DIR)/examples/strategy-sim/main.o $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) -o $(BUILD_DIR)/example-strategy-sim $(LDFLAGS)
	./$(BUILD_DIR)/example-strategy-sim

#============================================================================
# Utility targets
#============================================================================

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
	@echo "Engine sources: $(ENGINE_SRCS)"
	@echo "Game sources: $(GAME_SRCS)"

# List available targets
help:
	@echo "Carbon Engine Build Targets:"
	@echo ""
	@echo "  make              - Build main game (uses game template)"
	@echo "  make run          - Build and run main game"
	@echo "  make run-demo     - Build and run comprehensive demo"
	@echo ""
	@echo "Examples:"
	@echo "  make example-minimal   - Minimal window setup"
	@echo "  make example-sprites   - Sprite rendering demo"
	@echo "  make example-animation - Animation system demo"
	@echo "  make example-tilemap   - Tilemap rendering demo"
	@echo "  make example-ui        - UI system demo"
	@echo "  make example-strategy  - Strategy game patterns"
	@echo "  make example-strategy-sim - Strategy systems demo"
	@echo ""
	@echo "Utilities:"
	@echo "  make clean        - Remove build files"
	@echo "  make info         - Show build configuration"
	@echo "  make DEBUG=1      - Build with debug symbols"

.PHONY: all dirs run run-demo clean install-deps-macos install-deps-linux info help
.PHONY: example-minimal example-sprites example-animation example-tilemap example-ui example-strategy example-strategy-sim
