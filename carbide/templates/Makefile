# Carbide Project Makefile
# Cross-platform build system supporting Clang, GCC, and MSVC
#
# Targets:
#   make build        - Compile the project
#   make check        - Run clang-tidy static analysis
#   make safety       - Run security-focused checks
#   make test         - Run tests with sanitizers
#   make format       - Auto-format code
#   make format-check - Check formatting without modifying
#   make clean        - Remove build artifacts
#   make info         - Show build configuration

# ============================================================
# Project Configuration
# ============================================================

PROJECT_NAME := {{PROJECT_NAME}}
VERSION := 0.1.0

# Directories
SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build
TEST_DIR := tests

# Output
TARGET := $(BUILD_DIR)/$(PROJECT_NAME)
TEST_TARGET := $(BUILD_DIR)/test_$(PROJECT_NAME)

# ============================================================
# Platform Detection
# ============================================================

ifeq ($(OS),Windows_NT)
    PLATFORM := windows
    TARGET := $(BUILD_DIR)/$(PROJECT_NAME).exe
    TEST_TARGET := $(BUILD_DIR)/test_$(PROJECT_NAME).exe
    RM := del /Q
    RMDIR := rmdir /S /Q
    MKDIR := mkdir
    PATHSEP := \\
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        PLATFORM := macos
    else
        PLATFORM := linux
    endif
    RM := rm -f
    RMDIR := rm -rf
    MKDIR := mkdir -p
    PATHSEP := /
endif

# ============================================================
# Compiler Detection
# ============================================================

# Allow override via CC environment variable
ifeq ($(origin CC),default)
    ifeq ($(PLATFORM),windows)
        # Try clang first, then cl, then gcc
        ifneq ($(shell where clang 2>NUL),)
            CC := clang
        else ifneq ($(shell where cl 2>NUL),)
            CC := cl
        else
            CC := gcc
        endif
    else
        # Try clang first, then gcc
        ifneq ($(shell which clang 2>/dev/null),)
            CC := clang
        else
            CC := gcc
        endif
    endif
endif

# Detect compiler type
ifeq ($(findstring clang,$(CC)),clang)
    COMPILER := clang
else ifeq ($(findstring gcc,$(CC)),gcc)
    COMPILER := gcc
else ifeq ($(findstring g++,$(CC)),gcc)
    COMPILER := gcc
else ifeq ($(CC),cl)
    COMPILER := msvc
else
    COMPILER := unknown
endif

# ============================================================
# Compiler Flags
# ============================================================

# Language standard
ifeq ($(COMPILER),msvc)
    CSTD := /std:c11
else
    CSTD := -std=c11
endif

# Include paths
ifeq ($(COMPILER),msvc)
    INCLUDES := /I$(INCLUDE_DIR)
else
    INCLUDES := -I$(INCLUDE_DIR)
endif

# Warning flags
ifeq ($(COMPILER),msvc)
    WARNINGS := /W4 /WX
else
    WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    WARNINGS += -Wdouble-promotion -Wformat=2 -Wundef
    WARNINGS += -Wno-unused-parameter
endif

# Optimization
ifeq ($(DEBUG),1)
    ifeq ($(COMPILER),msvc)
        OPT := /Od /Zi
        DEFINES := /DDEBUG
    else
        OPT := -O0 -g
        DEFINES := -DDEBUG
    endif
else
    ifeq ($(COMPILER),msvc)
        OPT := /O2
        DEFINES := /DNDEBUG
    else
        OPT := -O2
        DEFINES := -DNDEBUG
    endif
endif

# Sanitizers (not supported on MSVC)
ifneq ($(COMPILER),msvc)
    ifeq ($(SANITIZE),address)
        SANITIZE_FLAGS := -fsanitize=address -fno-omit-frame-pointer
    else ifeq ($(SANITIZE),undefined)
        SANITIZE_FLAGS := -fsanitize=undefined
    else ifeq ($(SANITIZE),thread)
        SANITIZE_FLAGS := -fsanitize=thread
    else ifeq ($(SANITIZE),memory)
        SANITIZE_FLAGS := -fsanitize=memory -fno-omit-frame-pointer
    else ifeq ($(SANITIZE),all)
        SANITIZE_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
    endif
endif

# Combine flags
ifeq ($(COMPILER),msvc)
    CFLAGS := $(CSTD) $(INCLUDES) $(WARNINGS) $(OPT) $(DEFINES)
    LDFLAGS :=
else
    CFLAGS := $(CSTD) $(INCLUDES) $(WARNINGS) $(OPT) $(DEFINES) $(SANITIZE_FLAGS)
    LDFLAGS := $(SANITIZE_FLAGS)
endif

# ============================================================
# Source Files
# ============================================================

# Find all source files
SRCS := $(wildcard $(SRC_DIR)/*.c)
TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)

# Generate object file names
ifeq ($(COMPILER),msvc)
    OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.obj)
    TEST_OBJS := $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/test_%.obj)
else
    OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
    TEST_OBJS := $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/test_%.o)
endif

# Separate main from library objects (for tests)
MAIN_OBJ := $(BUILD_DIR)/main.o
LIB_OBJS := $(filter-out $(MAIN_OBJ),$(OBJS))

# ============================================================
# Targets
# ============================================================

.PHONY: all build check safety test format format-check clean info dirs

all: build

# Create build directory
dirs:
	@$(MKDIR) $(BUILD_DIR)

# Build the main executable
build: dirs $(TARGET)

$(TARGET): $(OBJS)
ifeq ($(COMPILER),msvc)
	$(CC) $(OBJS) /Fe:$@ $(LDFLAGS)
else
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
endif

# Compile source files
ifeq ($(COMPILER),msvc)
$(BUILD_DIR)/%.obj: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) /c $< /Fo:$@
else
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@
endif

# ============================================================
# Static Analysis
# ============================================================

# Run clang-tidy
check:
	@echo "Running clang-tidy..."
	@clang-tidy $(SRCS) -- $(CSTD) $(INCLUDES) $(DEFINES) 2>&1 || true
	@echo "Static analysis complete."

# Run security-focused checks
safety:
	@echo "Running security checks..."
	@clang-tidy $(SRCS) \
		-checks='-*,clang-analyzer-security.*,bugprone-*,cert-*' \
		-- $(CSTD) $(INCLUDES) $(DEFINES) 2>&1 || true
	@echo "Security checks complete."

# ============================================================
# Formatting
# ============================================================

# Auto-format code
format:
	@echo "Formatting source files..."
	@clang-format -i $(SRCS) $(wildcard $(INCLUDE_DIR)/$(PROJECT_NAME)/*.h) 2>/dev/null || \
		clang-format -i $(SRCS) $(wildcard $(INCLUDE_DIR)/*.h) 2>/dev/null || \
		echo "clang-format not found or no files to format"
	@echo "Formatting complete."

# Check formatting without modifying
format-check:
	@echo "Checking format..."
	@clang-format --dry-run --Werror $(SRCS) $(wildcard $(INCLUDE_DIR)/$(PROJECT_NAME)/*.h) 2>/dev/null || \
		clang-format --dry-run --Werror $(SRCS) $(wildcard $(INCLUDE_DIR)/*.h) 2>/dev/null || \
		echo "clang-format not found or no files to check"

# ============================================================
# Testing
# ============================================================

# Build and run tests with sanitizers
test: SANITIZE_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
test: dirs $(TEST_TARGET)
	@echo "Running tests..."
	@./$(TEST_TARGET)

$(TEST_TARGET): $(LIB_OBJS) $(TEST_OBJS)
ifeq ($(COMPILER),msvc)
	$(CC) $(LIB_OBJS) $(TEST_OBJS) /Fe:$@ $(LDFLAGS)
else
	$(CC) $(LIB_OBJS) $(TEST_OBJS) -o $@ $(LDFLAGS)
endif

# Compile test files
ifeq ($(COMPILER),msvc)
$(BUILD_DIR)/test_%.obj: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) /c $< /Fo:$@
else
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@
endif

# ============================================================
# Utilities
# ============================================================

# Clean build artifacts
clean:
ifeq ($(PLATFORM),windows)
	@if exist $(BUILD_DIR) $(RMDIR) $(BUILD_DIR)
else
	$(RMDIR) $(BUILD_DIR)
endif
	@echo "Clean complete."

# Show build configuration
info:
	@echo "Project: $(PROJECT_NAME) v$(VERSION)"
	@echo "Platform: $(PLATFORM)"
	@echo "Compiler: $(CC) ($(COMPILER))"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "Sources: $(SRCS)"
	@echo "Objects: $(OBJS)"
	@echo "Target: $(TARGET)"

# ============================================================
# Dependencies
# ============================================================

# Auto-generate dependencies (GCC/Clang only)
ifneq ($(COMPILER),msvc)
-include $(OBJS:.o=.d)

$(BUILD_DIR)/%.d: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@$(CC) $(CSTD) $(INCLUDES) -MM -MT '$(@:.d=.o)' $< -MF $@
endif
