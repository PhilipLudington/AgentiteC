# Agentite Engine - Cross-platform Makefile
# Supports macOS, Linux, and Windows (via MinGW)

# Project settings
PROJECT_NAME := agentite
BUILD_DIR := build
SRC_DIR := src
INCLUDE_DIR := include
LIB_DIR := lib
EXAMPLES_DIR := examples

# Detect OS
UNAME_S := $(shell uname -s)

# Compiler settings
CXX := g++
CC := gcc
CXXFLAGS := -Wall -Wextra -Wdeprecated-declarations -Wno-missing-field-initializers -Wno-missing-braces -std=c++17 -I$(INCLUDE_DIR) -I$(LIB_DIR) -I$(LIB_DIR)/cglm/include -I$(LIB_DIR)/chipmunk2d/include -I$(SRC_DIR)
CFLAGS := -Wall -Wextra -Wdeprecated-declarations -Wno-missing-field-initializers -Wno-missing-braces -std=c11 -I$(INCLUDE_DIR) -I$(LIB_DIR) -I$(LIB_DIR)/cglm/include -I$(LIB_DIR)/chipmunk2d/include -I$(SRC_DIR)
LDFLAGS :=

# Debug/Release builds
ifdef DEBUG
    CXXFLAGS += -g -O0 -DDEBUG
    CFLAGS += -g -O0 -DDEBUG
else
    CXXFLAGS += -O2 -DNDEBUG
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

CXXFLAGS += $(SDL3_CFLAGS)
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

# Engine source files (no main.cpp - that's for apps)
ENGINE_SRCS := $(wildcard $(SRC_DIR)/core/*.cpp) \
               $(wildcard $(SRC_DIR)/platform/*.cpp) \
               $(wildcard $(SRC_DIR)/graphics/*.cpp) \
               $(wildcard $(SRC_DIR)/audio/*.cpp) \
               $(wildcard $(SRC_DIR)/input/*.cpp) \
               $(wildcard $(SRC_DIR)/ui/*.cpp) \
               $(wildcard $(SRC_DIR)/ecs/*.cpp) \
               $(wildcard $(SRC_DIR)/ai/*.cpp) \
               $(wildcard $(SRC_DIR)/strategy/*.cpp) \
               $(wildcard $(SRC_DIR)/scene/*.cpp) \
               $(wildcard $(SRC_DIR)/debug/*.cpp)

# Game template source files
GAME_SRCS := $(wildcard $(SRC_DIR)/game/*.cpp) \
             $(wildcard $(SRC_DIR)/game/systems/*.cpp) \
             $(wildcard $(SRC_DIR)/game/states/*.cpp) \
             $(wildcard $(SRC_DIR)/game/data/*.cpp)

# Main application source (uses game template)
MAIN_SRC := $(SRC_DIR)/main.cpp

# All source files for main build
SRCS := $(MAIN_SRC) $(ENGINE_SRCS) $(GAME_SRCS)

# Flecs ECS library (compiled as separate object)
FLECS_SRC := $(LIB_DIR)/flecs.c
FLECS_OBJ := $(BUILD_DIR)/flecs.o

# TOML parser library (compiled as separate object)
TOML_SRC := $(LIB_DIR)/toml.c
TOML_OBJ := $(BUILD_DIR)/toml.o

# Chipmunk2D physics library (compiled as separate objects)
CHIPMUNK_DIR := $(LIB_DIR)/chipmunk2d
CHIPMUNK_SRCS := $(wildcard $(CHIPMUNK_DIR)/src/*.c)
CHIPMUNK_OBJS := $(patsubst $(CHIPMUNK_DIR)/src/%.c,$(BUILD_DIR)/chipmunk/%.o,$(CHIPMUNK_SRCS))

# Object files
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Demo example sources (standalone, doesn't use game template)
DEMO_SRC := $(EXAMPLES_DIR)/demo/main.cpp
DEMO_OBJS := $(BUILD_DIR)/examples/demo/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS))

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
	@mkdir -p $(BUILD_DIR)/scene
	@mkdir -p $(BUILD_DIR)/debug
	@mkdir -p $(BUILD_DIR)/chipmunk
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
	@mkdir -p $(BUILD_DIR)/examples/ui_node
	@mkdir -p $(BUILD_DIR)/examples/strategy
	@mkdir -p $(BUILD_DIR)/examples/strategy-sim
	@mkdir -p $(BUILD_DIR)/examples/msdf
	@mkdir -p $(BUILD_DIR)/examples/charts
	@mkdir -p $(BUILD_DIR)/examples/richtext
	@mkdir -p $(BUILD_DIR)/examples/pathfinding
	@mkdir -p $(BUILD_DIR)/examples/ecs_custom_system
	@mkdir -p $(BUILD_DIR)/examples/inspector
	@mkdir -p $(BUILD_DIR)/examples/gizmos
	@mkdir -p $(BUILD_DIR)/examples/async
	@mkdir -p $(BUILD_DIR)/examples/prefab
	@mkdir -p $(BUILD_DIR)/examples/scene
	@mkdir -p $(BUILD_DIR)/examples/profiler
	@mkdir -p $(BUILD_DIR)/examples/particles
	@mkdir -p $(BUILD_DIR)/examples/collision
	@mkdir -p $(BUILD_DIR)/examples/physics
	@mkdir -p $(BUILD_DIR)/examples/physics2d
	@mkdir -p $(BUILD_DIR)/examples/noise
	@mkdir -p $(BUILD_DIR)/examples/shaders
	@mkdir -p $(BUILD_DIR)/examples/transitions
	@mkdir -p $(BUILD_DIR)/examples/lighting
	@mkdir -p $(BUILD_DIR)/examples/hidpi
	@mkdir -p $(BUILD_DIR)/examples/debug
	@mkdir -p $(BUILD_DIR)/examples/replay
	@mkdir -p $(BUILD_DIR)/examples/hotreload
	@mkdir -p $(BUILD_DIR)/examples/mods

# Link main executable (game template)
$(BUILD_DIR)/$(EXECUTABLE): $(OBJS) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS)
	$(CXX) $(OBJS) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $@ $(LDFLAGS)

# Link demo executable
$(BUILD_DIR)/demo: $(DEMO_OBJS) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS)
	$(CXX) $(DEMO_OBJS) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $@ $(LDFLAGS)

# Compile Flecs (with relaxed warnings due to third-party code)
$(FLECS_OBJ): $(FLECS_SRC)
	$(CC) -std=c11 -O2 -I$(LIB_DIR) -c $< -o $@

# Compile TOML parser (with relaxed warnings due to third-party code)
$(TOML_OBJ): $(TOML_SRC)
	$(CC) -std=c11 -O2 -I$(LIB_DIR) -c $< -o $@

# Compile Chipmunk2D (with relaxed warnings due to third-party code)
# Use NDEBUG to disable debug assertions that abort() on soft errors
$(BUILD_DIR)/chipmunk/%.o: $(CHIPMUNK_DIR)/src/%.c
	$(CC) -std=c99 -O2 -DNDEBUG -I$(CHIPMUNK_DIR)/include -c $< -o $@

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile example files
$(BUILD_DIR)/examples/%.o: $(EXAMPLES_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

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
example-minimal: dirs $(BUILD_DIR)/examples/minimal/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/minimal/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-minimal $(LDFLAGS)
	./$(BUILD_DIR)/example-minimal

# Build and run sprites example
example-sprites: dirs $(BUILD_DIR)/examples/sprites/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/sprites/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-sprites $(LDFLAGS)
	./$(BUILD_DIR)/example-sprites

# Build and run animation example
example-animation: dirs $(BUILD_DIR)/examples/animation/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/animation/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-animation $(LDFLAGS)
	./$(BUILD_DIR)/example-animation

# Build and run tilemap example
example-tilemap: dirs $(BUILD_DIR)/examples/tilemap/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/tilemap/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-tilemap $(LDFLAGS)
	./$(BUILD_DIR)/example-tilemap

# Build and run UI example
example-ui: dirs $(BUILD_DIR)/examples/ui/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/ui/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-ui $(LDFLAGS)
	./$(BUILD_DIR)/example-ui

# Build and run UI node (retained-mode) example
example-ui-node: dirs $(BUILD_DIR)/examples/ui_node/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/ui_node/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-ui-node $(LDFLAGS)
	./$(BUILD_DIR)/example-ui-node

# Build and run strategy example
example-strategy: dirs $(BUILD_DIR)/examples/strategy/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/strategy/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-strategy $(LDFLAGS)
	./$(BUILD_DIR)/example-strategy

# Build and run strategy-sim example (demonstrates new strategy systems)
example-strategy-sim: dirs $(BUILD_DIR)/examples/strategy-sim/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/strategy-sim/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-strategy-sim $(LDFLAGS)
	./$(BUILD_DIR)/example-strategy-sim

# Build and run MSDF text rendering demo
example-msdf: dirs $(BUILD_DIR)/examples/msdf/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/msdf/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-msdf $(LDFLAGS)
	./$(BUILD_DIR)/example-msdf

# Build and run charts demo
example-charts: dirs $(BUILD_DIR)/examples/charts/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/charts/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-charts $(LDFLAGS)
	./$(BUILD_DIR)/example-charts

# Build and run rich text demo
example-richtext: dirs $(BUILD_DIR)/examples/richtext/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/richtext/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-richtext $(LDFLAGS)
	./$(BUILD_DIR)/example-richtext

# Build and run dialogs demo
example-dialogs: dirs $(BUILD_DIR)/examples/dialogs/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/dialogs/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-dialogs $(LDFLAGS)
	./$(BUILD_DIR)/example-dialogs

# Build and run pathfinding demo
example-pathfinding: dirs $(BUILD_DIR)/examples/pathfinding/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/pathfinding/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-pathfinding $(LDFLAGS)
	./$(BUILD_DIR)/example-pathfinding

# Build and run ECS custom system demo
example-ecs: dirs $(BUILD_DIR)/examples/ecs_custom_system/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/ecs_custom_system/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-ecs $(LDFLAGS)
	./$(BUILD_DIR)/example-ecs

# Build and run Entity Inspector demo
example-inspector: dirs $(BUILD_DIR)/examples/inspector/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(GAME_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/inspector/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(GAME_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-inspector $(LDFLAGS)
	./$(BUILD_DIR)/example-inspector

# Build and run Gizmos demo
example-gizmos: dirs $(BUILD_DIR)/examples/gizmos/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/gizmos/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-gizmos $(LDFLAGS)
	./$(BUILD_DIR)/example-gizmos

# Build and run Async Loading demo
example-async: dirs $(BUILD_DIR)/examples/async/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/async/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-async $(LDFLAGS)
	./$(BUILD_DIR)/example-async

# Build and run Prefab demo
example-prefab: dirs $(BUILD_DIR)/examples/prefab/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/prefab/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-prefab $(LDFLAGS)
	./$(BUILD_DIR)/example-prefab

# Build and run Scene demo
example-scene: dirs $(BUILD_DIR)/examples/scene/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/scene/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-scene $(LDFLAGS)
	./$(BUILD_DIR)/example-scene

# Build and run Profiler demo
example-profiler: dirs $(BUILD_DIR)/examples/profiler/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/profiler/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-profiler $(LDFLAGS)
	./$(BUILD_DIR)/example-profiler

# Build and run Particles demo
example-particles: dirs $(BUILD_DIR)/examples/particles/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/particles/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-particles $(LDFLAGS)
	./$(BUILD_DIR)/example-particles

# Build and run Collision demo
example-collision: dirs $(BUILD_DIR)/examples/collision/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/collision/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-collision $(LDFLAGS)
	./$(BUILD_DIR)/example-collision

# Build and run Physics demo
example-physics: dirs $(BUILD_DIR)/examples/physics/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/physics/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-physics $(LDFLAGS)
	./$(BUILD_DIR)/example-physics

# Build and run Physics2D (Chipmunk) demo
example-physics2d: dirs $(BUILD_DIR)/examples/physics2d/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/physics2d/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-physics2d $(LDFLAGS)
	./$(BUILD_DIR)/example-physics2d

# Build and run Noise demo
example-noise: dirs $(BUILD_DIR)/examples/noise/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/noise/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-noise $(LDFLAGS)
	./$(BUILD_DIR)/example-noise

# Build and run Shaders demo
example-shaders: dirs $(BUILD_DIR)/examples/shaders/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/shaders/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-shaders $(LDFLAGS)
	./$(BUILD_DIR)/example-shaders

# Build and run Transitions demo
example-transitions: dirs $(BUILD_DIR)/examples/transitions/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/transitions/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-transitions $(LDFLAGS)
	./$(BUILD_DIR)/example-transitions

# Build and run Lighting demo
example-lighting: dirs $(BUILD_DIR)/examples/lighting/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/lighting/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-lighting $(LDFLAGS)
	./$(BUILD_DIR)/example-lighting

# Build and run Localization demo
example-localization: dirs $(BUILD_DIR)/examples/localization/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/localization/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-localization $(LDFLAGS)
	./$(BUILD_DIR)/example-localization

# Build and run HiDPI test (minimal reproduction of postprocess positioning bug)
example-hidpi: dirs $(BUILD_DIR)/examples/hidpi/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/hidpi/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-hidpi $(LDFLAGS)
	./$(BUILD_DIR)/example-hidpi

# Build and run Debug Tools demo
example-debug: dirs $(BUILD_DIR)/examples/debug/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/debug/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-debug $(LDFLAGS)
	./$(BUILD_DIR)/example-debug

# Build and run Replay System demo
example-replay: dirs $(BUILD_DIR)/examples/replay/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/replay/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-replay $(LDFLAGS)
	./$(BUILD_DIR)/example-replay

# Build and run Hot Reload demo
example-hotreload: dirs $(BUILD_DIR)/examples/hotreload/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/hotreload/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-hotreload $(LDFLAGS)
	./$(BUILD_DIR)/example-hotreload

# Build and run Mod System demo
example-mods: dirs $(BUILD_DIR)/examples/mods/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ)
	$(CXX) $(BUILD_DIR)/examples/mods/main.o $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS)) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $(BUILD_DIR)/example-mods $(LDFLAGS)
	./$(BUILD_DIR)/example-mods

#============================================================================
# Test targets
#============================================================================

TESTS_DIR := tests

# Test source files
TEST_SRCS := $(wildcard $(TESTS_DIR)/core/*.cpp) \
             $(wildcard $(TESTS_DIR)/strategy/*.cpp) \
             $(wildcard $(TESTS_DIR)/ai/*.cpp) \
             $(wildcard $(TESTS_DIR)/graphics/*.cpp) \
             $(wildcard $(TESTS_DIR)/ecs/*.cpp) \
             $(wildcard $(TESTS_DIR)/audio/*.cpp) \
             $(wildcard $(TESTS_DIR)/ui/*.cpp) \
             $(wildcard $(TESTS_DIR)/scene/*.cpp) \
             $(wildcard $(TESTS_DIR)/security/*.cpp)

# Test objects (engine + test files + catch2)
TEST_OBJS := $(patsubst $(TESTS_DIR)/%.cpp,$(BUILD_DIR)/tests/%.o,$(TEST_SRCS)) \
             $(BUILD_DIR)/tests/test_main.o \
             $(BUILD_DIR)/catch2.o \
             $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SRCS))

# Catch2 object
$(BUILD_DIR)/catch2.o: $(LIB_DIR)/catch_amalgamated.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) -std=c++17 -O2 -I$(LIB_DIR) -c $< -o $@

# Test main object
$(BUILD_DIR)/tests/test_main.o: $(TESTS_DIR)/test_main.cpp
	@mkdir -p $(BUILD_DIR)/tests
	$(CXX) $(CXXFLAGS) -I$(LIB_DIR) -c $< -o $@

# Compile test files
$(BUILD_DIR)/tests/%.o: $(TESTS_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(LIB_DIR) -c $< -o $@

# Build test executable
$(BUILD_DIR)/test_runner: $(TEST_OBJS) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS)
	$(CXX) $(TEST_OBJS) $(FLECS_OBJ) $(TOML_OBJ) $(CHIPMUNK_OBJS) -o $@ $(LDFLAGS)

# Run tests
test: dirs $(BUILD_DIR)/test_runner
	./$(BUILD_DIR)/test_runner

# Run tests with verbose output
test-verbose: dirs $(BUILD_DIR)/test_runner
	./$(BUILD_DIR)/test_runner --success

#============================================================================
# Sanitizer builds (for testing)
#============================================================================

# AddressSanitizer + UndefinedBehaviorSanitizer build
# Detects: buffer overflows, use-after-free, memory leaks, undefined behavior
SANITIZER_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls

# Sanitizer test objects (need separate build dir to avoid mixing with regular builds)
ASAN_BUILD_DIR := $(BUILD_DIR)/asan
ASAN_TEST_OBJS := $(patsubst $(TESTS_DIR)/%.cpp,$(ASAN_BUILD_DIR)/tests/%.o,$(TEST_SRCS)) \
                  $(ASAN_BUILD_DIR)/tests/test_main.o \
                  $(ASAN_BUILD_DIR)/catch2.o \
                  $(patsubst $(SRC_DIR)/%.cpp,$(ASAN_BUILD_DIR)/%.o,$(ENGINE_SRCS))
ASAN_FLECS_OBJ := $(ASAN_BUILD_DIR)/flecs.o
ASAN_TOML_OBJ := $(ASAN_BUILD_DIR)/toml.o
ASAN_CHIPMUNK_OBJS := $(patsubst $(CHIPMUNK_DIR)/src/%.c,$(ASAN_BUILD_DIR)/chipmunk/%.o,$(CHIPMUNK_SRCS))

# Create asan build directories
asan-dirs:
	@mkdir -p $(ASAN_BUILD_DIR)/core
	@mkdir -p $(ASAN_BUILD_DIR)/platform
	@mkdir -p $(ASAN_BUILD_DIR)/graphics
	@mkdir -p $(ASAN_BUILD_DIR)/audio
	@mkdir -p $(ASAN_BUILD_DIR)/input
	@mkdir -p $(ASAN_BUILD_DIR)/ui
	@mkdir -p $(ASAN_BUILD_DIR)/ecs
	@mkdir -p $(ASAN_BUILD_DIR)/ai
	@mkdir -p $(ASAN_BUILD_DIR)/strategy
	@mkdir -p $(ASAN_BUILD_DIR)/scene
	@mkdir -p $(ASAN_BUILD_DIR)/debug
	@mkdir -p $(ASAN_BUILD_DIR)/chipmunk
	@mkdir -p $(ASAN_BUILD_DIR)/tests/core
	@mkdir -p $(ASAN_BUILD_DIR)/tests/strategy
	@mkdir -p $(ASAN_BUILD_DIR)/tests/ai
	@mkdir -p $(ASAN_BUILD_DIR)/tests/graphics
	@mkdir -p $(ASAN_BUILD_DIR)/tests/ecs
	@mkdir -p $(ASAN_BUILD_DIR)/tests/scene
	@mkdir -p $(ASAN_BUILD_DIR)/tests/security

# Compile engine sources with sanitizers
$(ASAN_BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(SANITIZER_FLAGS) -c $< -o $@

# Compile Flecs with sanitizers
$(ASAN_FLECS_OBJ): $(FLECS_SRC)
	$(CC) -std=c11 -O1 -g $(SANITIZER_FLAGS) -I$(LIB_DIR) -c $< -o $@

# Compile TOML parser with sanitizers
$(ASAN_TOML_OBJ): $(TOML_SRC)
	$(CC) -std=c11 -O1 -g $(SANITIZER_FLAGS) -I$(LIB_DIR) -c $< -o $@

# Compile Chipmunk2D with sanitizers
$(ASAN_BUILD_DIR)/chipmunk/%.o: $(CHIPMUNK_DIR)/src/%.c
	$(CC) -std=c99 -O1 -g $(SANITIZER_FLAGS) -DNDEBUG -I$(CHIPMUNK_DIR)/include -c $< -o $@

# Compile Catch2 with sanitizers
$(ASAN_BUILD_DIR)/catch2.o: $(LIB_DIR)/catch_amalgamated.cpp
	$(CXX) -std=c++17 -O1 -g $(SANITIZER_FLAGS) -I$(LIB_DIR) -c $< -o $@

# Compile test main with sanitizers
$(ASAN_BUILD_DIR)/tests/test_main.o: $(TESTS_DIR)/test_main.cpp
	$(CXX) $(CXXFLAGS) $(SANITIZER_FLAGS) -I$(LIB_DIR) -c $< -o $@

# Compile test files with sanitizers
$(ASAN_BUILD_DIR)/tests/%.o: $(TESTS_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SANITIZER_FLAGS) -I$(LIB_DIR) -c $< -o $@

# Build test runner with sanitizers
$(ASAN_BUILD_DIR)/test_runner: $(ASAN_TEST_OBJS) $(ASAN_FLECS_OBJ) $(ASAN_TOML_OBJ) $(ASAN_CHIPMUNK_OBJS)
	$(CXX) $(ASAN_TEST_OBJS) $(ASAN_FLECS_OBJ) $(ASAN_TOML_OBJ) $(ASAN_CHIPMUNK_OBJS) -o $@ $(LDFLAGS) $(SANITIZER_FLAGS)

# ASAN options (detect_leaks not supported on macOS)
ifeq ($(UNAME_S),Darwin)
    ASAN_OPTS := abort_on_error=1
else
    ASAN_OPTS := detect_leaks=1:abort_on_error=1
endif

# Run tests with AddressSanitizer + UndefinedBehaviorSanitizer
test-asan: asan-dirs $(ASAN_BUILD_DIR)/test_runner
	@echo "Running tests with AddressSanitizer + UndefinedBehaviorSanitizer..."
	ASAN_OPTIONS=$(ASAN_OPTS) ./$(ASAN_BUILD_DIR)/test_runner

# Run tests with sanitizers (verbose)
test-asan-verbose: asan-dirs $(ASAN_BUILD_DIR)/test_runner
	@echo "Running tests with AddressSanitizer + UndefinedBehaviorSanitizer (verbose)..."
	ASAN_OPTIONS=$(ASAN_OPTS) ./$(ASAN_BUILD_DIR)/test_runner --success

#============================================================================
# Code Coverage
#============================================================================

# Coverage build directory
COV_BUILD_DIR := $(BUILD_DIR)/coverage

# Coverage flags
COVERAGE_FLAGS := --coverage -fprofile-arcs -ftest-coverage -O0 -g

# Coverage test objects
COV_TEST_OBJS := $(patsubst $(TESTS_DIR)/%.cpp,$(COV_BUILD_DIR)/tests/%.o,$(TEST_SRCS)) \
                 $(COV_BUILD_DIR)/tests/test_main.o \
                 $(COV_BUILD_DIR)/catch2.o \
                 $(patsubst $(SRC_DIR)/%.cpp,$(COV_BUILD_DIR)/%.o,$(ENGINE_SRCS))
COV_FLECS_OBJ := $(COV_BUILD_DIR)/flecs.o
COV_TOML_OBJ := $(COV_BUILD_DIR)/toml.o
COV_CHIPMUNK_OBJS := $(patsubst $(CHIPMUNK_DIR)/src/%.c,$(COV_BUILD_DIR)/chipmunk/%.o,$(CHIPMUNK_SRCS))

# Create coverage build directories
cov-dirs:
	@mkdir -p $(COV_BUILD_DIR)/core
	@mkdir -p $(COV_BUILD_DIR)/platform
	@mkdir -p $(COV_BUILD_DIR)/graphics
	@mkdir -p $(COV_BUILD_DIR)/audio
	@mkdir -p $(COV_BUILD_DIR)/input
	@mkdir -p $(COV_BUILD_DIR)/ui
	@mkdir -p $(COV_BUILD_DIR)/ecs
	@mkdir -p $(COV_BUILD_DIR)/ai
	@mkdir -p $(COV_BUILD_DIR)/strategy
	@mkdir -p $(COV_BUILD_DIR)/scene
	@mkdir -p $(COV_BUILD_DIR)/debug
	@mkdir -p $(COV_BUILD_DIR)/chipmunk
	@mkdir -p $(COV_BUILD_DIR)/tests/core
	@mkdir -p $(COV_BUILD_DIR)/tests/strategy
	@mkdir -p $(COV_BUILD_DIR)/tests/ai
	@mkdir -p $(COV_BUILD_DIR)/tests/graphics
	@mkdir -p $(COV_BUILD_DIR)/tests/ecs
	@mkdir -p $(COV_BUILD_DIR)/tests/scene
	@mkdir -p $(COV_BUILD_DIR)/tests/security

# Compile engine sources with coverage
$(COV_BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(COVERAGE_FLAGS) -c $< -o $@

# Compile Flecs with coverage (minimal, just to link)
$(COV_FLECS_OBJ): $(FLECS_SRC)
	$(CC) -std=c11 -O0 -g -I$(LIB_DIR) -c $< -o $@

# Compile TOML parser with coverage (minimal, just to link)
$(COV_TOML_OBJ): $(TOML_SRC)
	$(CC) -std=c11 -O0 -g -I$(LIB_DIR) -c $< -o $@

# Compile Chipmunk2D without coverage (third-party)
$(COV_BUILD_DIR)/chipmunk/%.o: $(CHIPMUNK_DIR)/src/%.c
	$(CC) -std=c99 -O0 -g -DNDEBUG -I$(CHIPMUNK_DIR)/include -c $< -o $@

# Compile Catch2 without coverage (third-party)
$(COV_BUILD_DIR)/catch2.o: $(LIB_DIR)/catch_amalgamated.cpp
	$(CXX) -std=c++17 -O0 -g -I$(LIB_DIR) -c $< -o $@

# Compile test main with coverage
$(COV_BUILD_DIR)/tests/test_main.o: $(TESTS_DIR)/test_main.cpp
	$(CXX) $(CXXFLAGS) $(COVERAGE_FLAGS) -I$(LIB_DIR) -c $< -o $@

# Compile test files with coverage
$(COV_BUILD_DIR)/tests/%.o: $(TESTS_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(COVERAGE_FLAGS) -I$(LIB_DIR) -c $< -o $@

# Build coverage test runner
$(COV_BUILD_DIR)/test_runner: $(COV_TEST_OBJS) $(COV_FLECS_OBJ) $(COV_TOML_OBJ) $(COV_CHIPMUNK_OBJS)
	$(CXX) $(COV_TEST_OBJS) $(COV_FLECS_OBJ) $(COV_TOML_OBJ) $(COV_CHIPMUNK_OBJS) -o $@ $(LDFLAGS) $(COVERAGE_FLAGS)

# Run tests and generate coverage report
test-coverage: cov-dirs $(COV_BUILD_DIR)/test_runner
	@echo "Running tests with code coverage..."
	@./$(COV_BUILD_DIR)/test_runner
	@echo ""
	@echo "Generating coverage report..."
	@command -v lcov >/dev/null 2>&1 || { echo "lcov not found. Install with: brew install lcov"; exit 1; }
	@lcov --capture --directory $(COV_BUILD_DIR) --output-file $(COV_BUILD_DIR)/coverage.info --ignore-errors inconsistent
	@lcov --remove $(COV_BUILD_DIR)/coverage.info '*/lib/*' '*/tests/*' '/usr/*' '/opt/*' --output-file $(COV_BUILD_DIR)/coverage.info --ignore-errors unused
	@lcov --list $(COV_BUILD_DIR)/coverage.info
	@echo ""
	@echo "Coverage data saved to $(COV_BUILD_DIR)/coverage.info"
	@echo "To generate HTML report: genhtml $(COV_BUILD_DIR)/coverage.info --output-directory $(COV_BUILD_DIR)/html"

# Generate HTML coverage report
coverage-html: test-coverage
	@echo "Generating HTML coverage report..."
	@command -v genhtml >/dev/null 2>&1 || { echo "genhtml not found. Install with: brew install lcov"; exit 1; }
	@genhtml $(COV_BUILD_DIR)/coverage.info --output-directory $(COV_BUILD_DIR)/html
	@echo "HTML report generated at $(COV_BUILD_DIR)/html/index.html"
ifeq ($(UNAME_S),Darwin)
	@open $(COV_BUILD_DIR)/html/index.html
endif

# Clean coverage data
clean-coverage:
	rm -rf $(COV_BUILD_DIR)
	find . -name "*.gcda" -delete
	find . -name "*.gcno" -delete

#============================================================================
# Carbide Static Analysis & Formatting
#============================================================================

# Source files to analyze (excluding third-party libs)
ANALYZE_SRCS := $(ENGINE_SRCS) $(GAME_SRCS)
ANALYZE_HDRS := $(wildcard $(INCLUDE_DIR)/agentite/*.h)

# Find clang-tidy (check PATH first, then Homebrew LLVM)
CLANG_TIDY := $(shell command -v clang-tidy 2>/dev/null || echo "$$(brew --prefix llvm 2>/dev/null)/bin/clang-tidy")
CLANG_FORMAT := $(shell command -v clang-format 2>/dev/null || echo "$$(brew --prefix llvm 2>/dev/null)/bin/clang-format")

# macOS SDK path for clang-tidy system headers
MACOS_SDK := $(shell xcrun --show-sdk-path 2>/dev/null)
CLANG_TIDY_EXTRA_ARGS := $(if $(MACOS_SDK),--extra-arg=-isysroot$(MACOS_SDK),)

# Run clang-tidy static analysis (uses compile_commands.json if available)
check:
	@echo "Running clang-tidy static analysis..."
	@test -x "$(CLANG_TIDY)" || { echo "clang-tidy not found. Install with: brew install llvm"; exit 1; }
	@if [ -f compile_commands.json ]; then \
		$(CLANG_TIDY) -p . $(CLANG_TIDY_EXTRA_ARGS) $(ANALYZE_SRCS) 2>&1 || true; \
	else \
		echo "Note: Run 'make compile_commands.json' for better analysis"; \
		$(CLANG_TIDY) $(CLANG_TIDY_EXTRA_ARGS) $(ANALYZE_SRCS) -- -std=c++17 -I$(INCLUDE_DIR) -I$(LIB_DIR) -I$(LIB_DIR)/cglm/include -I$(SRC_DIR) $(SDL3_CFLAGS) 2>&1 || true; \
	fi
	@echo "Static analysis complete."

# Run security-focused checks (uses compile_commands.json if available)
safety:
	@echo "Running security-focused checks..."
	@test -x "$(CLANG_TIDY)" || { echo "clang-tidy not found. Install with: brew install llvm"; exit 1; }
	@if [ -f compile_commands.json ]; then \
		$(CLANG_TIDY) -p . $(CLANG_TIDY_EXTRA_ARGS) -checks='-*,clang-analyzer-security.*,bugprone-*,cert-*' $(ANALYZE_SRCS) 2>&1 || true; \
	else \
		echo "Note: Run 'make compile_commands.json' for better analysis"; \
		$(CLANG_TIDY) $(CLANG_TIDY_EXTRA_ARGS) $(ANALYZE_SRCS) -checks='-*,clang-analyzer-security.*,bugprone-*,cert-*' \
			-- -std=c++17 -I$(INCLUDE_DIR) -I$(LIB_DIR) -I$(LIB_DIR)/cglm/include -I$(SRC_DIR) $(SDL3_CFLAGS) 2>&1 || true; \
	fi
	@echo "Security checks complete."

# Auto-format code with clang-format
format:
	@echo "Formatting source files..."
	@test -x "$(CLANG_FORMAT)" || { echo "clang-format not found. Install with: brew install llvm"; exit 1; }
	@$(CLANG_FORMAT) -i $(ANALYZE_SRCS) $(ANALYZE_HDRS) 2>/dev/null || echo "No files to format"
	@echo "Formatting complete."

# Check formatting without modifying
format-check:
	@echo "Checking format..."
	@test -x "$(CLANG_FORMAT)" || { echo "clang-format not found. Install with: brew install llvm"; exit 1; }
	@$(CLANG_FORMAT) --dry-run --Werror $(ANALYZE_SRCS) $(ANALYZE_HDRS) 2>/dev/null && echo "All files formatted correctly." || echo "Some files need formatting. Run 'make format' to fix."

# Generate compile_commands.json for clang-tidy (requires bear: brew install bear)
compile_commands.json:
	@echo "Generating compile_commands.json..."
	@command -v bear >/dev/null 2>&1 || { echo "bear not found. Install with: brew install bear"; exit 1; }
	@bear -- $(MAKE) clean all
	@echo "compile_commands.json generated."

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
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "CC: $(CC) (for C libraries)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "Engine sources: $(ENGINE_SRCS)"
	@echo "Game sources: $(GAME_SRCS)"

# List available targets
help:
	@echo "Agentite Engine Build Targets:"
	@echo ""
	@echo "  make              - Build main game (uses game template)"
	@echo "  make run          - Build and run main game"
	@echo "  make run-demo     - Build and run comprehensive demo"
	@echo ""
	@echo "Testing:"
	@echo "  make test              - Build and run all tests"
	@echo "  make test-verbose      - Run tests with detailed output"
	@echo "  make test-asan         - Run tests with AddressSanitizer + UBSan"
	@echo "  make test-asan-verbose - Run sanitizer tests with detailed output"
	@echo "  make test-coverage     - Run tests and generate coverage report"
	@echo "  make coverage-html     - Generate HTML coverage report"
	@echo ""
	@echo "Code Quality (Carbide):"
	@echo "  make check        - Run clang-tidy static analysis"
	@echo "  make safety       - Run security-focused checks"
	@echo "  make format       - Auto-format code with clang-format"
	@echo "  make format-check - Check formatting without modifying"
	@echo ""
	@echo "Examples:"
	@echo "  make example-minimal   - Minimal window setup"
	@echo "  make example-sprites   - Sprite rendering demo"
	@echo "  make example-animation - Animation system demo"
	@echo "  make example-tilemap   - Tilemap rendering demo"
	@echo "  make example-ui        - UI system demo (immediate-mode)"
	@echo "  make example-ui-node   - UI node demo (retained-mode)"
	@echo "  make example-strategy  - Strategy game patterns"
	@echo "  make example-strategy-sim - Strategy systems demo"
	@echo "  make example-msdf      - MSDF text rendering demo"
	@echo "  make example-charts    - Data visualization charts"
	@echo "  make example-richtext  - BBCode rich text demo"
	@echo "  make example-dialogs   - Modal dialogs and popups"
	@echo "  make example-pathfinding - A* pathfinding demo"
	@echo "  make example-ecs       - Custom ECS systems demo"
	@echo "  make example-inspector - Entity inspector demo"
	@echo "  make example-gizmos    - Gizmos and debug drawing demo"
	@echo "  make example-async     - Async asset loading demo"
	@echo "  make example-prefab    - Prefab spawning demo"
	@echo "  make example-scene     - Scene loading and switching demo"
	@echo ""
	@echo "Utilities:"
	@echo "  make clean        - Remove build files"
	@echo "  make info         - Show build configuration"
	@echo "  make DEBUG=1      - Build with debug symbols"

.PHONY: all dirs run run-demo clean install-deps-macos install-deps-linux info help test test-verbose
.PHONY: asan-dirs test-asan test-asan-verbose
.PHONY: cov-dirs test-coverage coverage-html clean-coverage
.PHONY: check safety format format-check
.PHONY: example-minimal example-sprites example-animation example-tilemap example-ui example-ui-node example-strategy example-strategy-sim example-msdf example-charts example-richtext example-dialogs example-pathfinding example-ecs example-inspector example-gizmos example-async example-prefab example-scene example-debug example-replay example-hotreload example-mods
