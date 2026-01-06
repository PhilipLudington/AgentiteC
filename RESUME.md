# SDL 3.4.0 Upgrade Plan

## Overview

Upgrade Agentite from current SDL3 version to SDL 3.4.0 (released January 1, 2026).

**Risk Level**: Low
**Estimated Effort**: Small
**Breaking Changes**: None identified

## Pre-Upgrade Checklist

- [x] Verify current SDL3 version: `pkg-config --modversion sdl3`
- [x] Ensure all tests pass: `./scripts/run-tests.sh`
- [x] Ensure build succeeds: `./scripts/run-build.sh`
- [x] Test key examples work (particles, shaders, lighting)

## Phase 1: Upgrade SDL3

### Step 1.1: Update SDL3 Installation

**macOS:**
```bash
brew upgrade sdl3
# or if not installed:
brew install sdl3
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt update
sudo apt upgrade libsdl3-dev
```

**Linux (Fedora):**
```bash
sudo dnf upgrade SDL3-devel
```

**From source (if needed):**
```bash
git clone https://github.com/libsdl-org/SDL.git
cd SDL
git checkout release-3.4.0
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
sudo cmake --install .
```

### Step 1.2: Verify Installation

```bash
pkg-config --modversion sdl3
# Should output: 3.4.0
```

### Step 1.3: Rebuild and Test

```bash
make clean
./scripts/run-build.sh
./scripts/run-tests.sh
```

### Step 1.4: Test Examples

```bash
make example-particles
make example-shaders
make example-lighting
make example-sprites
```

## Phase 2: Adopt New Features

### Priority 1: Pixel-Art Scale Mode

**File**: `src/graphics/sprite.cpp`

Add support for `SDL_SCALEMODE_PIXELART` for crisp pixel-art rendering.

```cpp
// In texture creation or sprite renderer config
typedef enum {
    AGENTITE_SCALE_LINEAR,
    AGENTITE_SCALE_NEAREST,
    AGENTITE_SCALE_PIXELART  // NEW: SDL 3.4.0
} Agentite_ScaleMode;

// Implementation maps to SDL:
// AGENTITE_SCALE_PIXELART -> SDL_SCALEMODE_PIXELART
```

**Tasks:**
- [x] Add `Agentite_ScaleMode` enum to `include/agentite/sprite.h`
- [x] Add scale mode parameter to textures (per-texture mode)
- [x] Implement in `agentite_texture_create()` / `agentite_texture_load()`
- [ ] Update documentation
- [ ] Add to sprite example

### Priority 2: Native PNG Support (Optional)

**File**: `src/graphics/texture.cpp`

SDL 3.4.0 adds native PNG loading without stb_image dependency.

```cpp
// Option A: Use SDL native PNG (simpler, fewer dependencies)
SDL_Surface *surface = SDL_LoadPNG_IO(SDL_IOFromFile(path, "rb"), true);

// Option B: Keep stb_image (more format support)
// Current implementation - no change needed
```

**Decision**: Keep stb_image as primary (supports PNG/JPEG/BMP/TGA). Use SDL_SavePNG() for screenshot saving.

**Tasks:**
- [x] Evaluate if native PNG reduces binary size - keeping stb_image for multi-format support
- [x] Implement screenshot saving via `SDL_SavePNG()` - Added `agentite_save_screenshot()` and `agentite_save_screenshot_auto()`
- [ ] Document decision in code comments

### Priority 3: Window Progress State

**File**: `src/core/engine.cpp` or new `src/platform/taskbar.cpp`

Add taskbar progress indication for loading screens.

```cpp
// New API
void agentite_set_loading_progress(Agentite_Engine *engine, float progress);
void agentite_clear_loading_progress(Agentite_Engine *engine);

// Implementation
void agentite_set_loading_progress(Agentite_Engine *engine, float progress) {
    SDL_SetWindowProgressState(engine->window, SDL_WINDOW_PROGRESS_NORMAL, progress);
}
```

**Tasks:**
- [x] Add progress API to `include/agentite/agentite.h`
- [x] Implement using `SDL_SetWindowProgressState()`
- [ ] Integrate with async loading system
- [ ] Test on Windows and Linux (not supported on macOS)

### Priority 4: Texture Address Mode

**File**: `src/graphics/sprite.cpp`

Add texture wrapping mode support for tiling effects.

```cpp
// New config option
typedef enum {
    AGENTITE_ADDRESS_CLAMP,
    AGENTITE_ADDRESS_WRAP  // NEW: SDL 3.4.0
} Agentite_TextureAddressMode;
```

**Tasks:**
- [x] Add address mode to texture config
- [x] Useful for tilemap seamless tiling
- [ ] Document in API docs

### Priority 5: Event Debugging

**File**: `src/input/input.cpp`

Use `SDL_GetEventDescription()` for better debug logging.

```cpp
// In debug builds, log events with descriptions
#ifdef DEBUG
void log_event(const SDL_Event *event) {
    SDL_Log("Event: %s", SDL_GetEventDescription(event));
}
#endif
```

**Tasks:**
- [x] Add event description logging in debug mode
- [ ] Integrate with existing debug console system

## Phase 3: Future Considerations

### Emscripten/Web Builds

When adding web support, use new SDL 3.4.0 features:
- `SDL_WINDOW_FILL_DOCUMENT` for full browser window
- Canvas ID configuration properties

### Touch/Mobile Support

When adding mobile support, leverage:
- Pinch gesture events (`SDL_EVENT_PINCH_*`)
- Enhanced pen device support

### GPU/2D Interop

The new `SDL_CreateGPURenderer()` could enable:
- Simpler 2D fallback path
- Mixed SDL_Renderer + SDL_GPU rendering
- Evaluate if useful for UI rendering optimization

## Rollback Plan

If issues occur after upgrade:

**macOS:**
```bash
brew uninstall sdl3
brew install sdl3@3.2  # or previous version
```

**From source:**
```bash
cd SDL
git checkout release-3.2.0  # previous version
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
sudo cmake --install .
```

## Verification Checklist

After upgrade, verify:

- [x] `make clean && make` succeeds
- [x] `./scripts/run-tests.sh` passes (235/235)
- [x] `make example-sprites` renders correctly
- [x] `make example-particles` renders correctly
- [x] `make example-shaders` post-processing works
- [x] `make example-lighting` shadows render correctly
- [x] `make example-physics2d` Chipmunk debug draw works
- [x] No new compiler warnings related to SDL

## References

- [SDL 3.4.0 Release Notes](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.0)
- [SDL3 Migration Guide](https://wiki.libsdl.org/SDL3/README/migration)
- [SDL_GPU Documentation](https://wiki.libsdl.org/SDL3/CategoryGPU)
