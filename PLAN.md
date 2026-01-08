# Agentite Code Quality Improvement Plan

Based on the comprehensive code quality assessment (Overall Score: 8.2/10), this plan addresses all identified issues organized by priority.

## High Priority (Critical Issues)

### 1. Eliminate Unsafe strcpy Usage

**Status:** COMPLETED
**Severity:** HIGH
**Files Affected:** 14 files

**Resolution:** Upon investigation, the codebase already uses safe string functions (`strncpy`, `snprintf`) throughout. No `strcpy` calls were found in the source files. The initial review may have flagged false positives or the issues were previously resolved.

**Verification:** Searched all `.cpp` and `.c` files - no `strcpy` function calls found.

---

### 2. Fix Audio Callback Reallocation

**Status:** COMPLETED
**Severity:** MEDIUM
**Location:** `src/audio/audio.cpp:79-87`

**Problem:** Reallocating memory during audio callback can cause audio glitches or clicks.

**Resolution:**
- [x] Pre-allocated audio mix buffer with 16384 samples (~170ms at 48kHz stereo)
- [x] Removed realloc from audio callback path - now clamps to pre-allocated size
- [x] Added warning log (once) if requested size exceeds pre-allocated buffer
- [x] Added `AGENTITE_AUDIO_MAX_MIX_SAMPLES` constant to `include/agentite/audio.h`

**Changes Made:**
- `include/agentite/audio.h`: Added `AGENTITE_AUDIO_MAX_MIX_SAMPLES 16384` constant
- `src/audio/audio.cpp`:
  - Added `buffer_overflow_warned` flag to struct
  - Changed callback to clamp instead of realloc
  - Updated initialization to use constant

---

### 3. Add Path Traversal Protection

**Status:** PARTIALLY COMPLETED (Core functions implemented)
**Severity:** MEDIUM
**Location:** File loading functions throughout codebase

**Completed:**
- [x] Created `include/agentite/path.h` header with full API
- [x] Created `src/core/path.cpp` implementation with:
  - `agentite_path_component_is_safe()` - Validate single path component
  - `agentite_path_is_safe()` - Validate relative path (rejects "..")
  - `agentite_path_is_within()` - Verify path stays within base directory
  - `agentite_path_normalize()` - Normalize path separators
  - `agentite_path_join()` - Safely join path components
  - `agentite_path_canonicalize()` - Get absolute path (uses realpath/fullpath)
  - `agentite_path_is_absolute()` - Check if path is absolute
  - `agentite_path_filename()` - Extract filename from path
  - `agentite_path_dirname()` - Extract directory from path

**Remaining Tasks:**
- [ ] Add path validation to `agentite_texture_load()`
- [ ] Add path validation to `agentite_audio_load_sound()`
- [ ] Add path validation to `agentite_audio_load_music()`
- [ ] Add path validation to `agentite_font_load()`
- [ ] Add path validation to other file loading functions
- [ ] Add tests for path traversal rejection

---

### 4. Increase Test Coverage

**Status:** Not Started
**Severity:** MEDIUM
**Current Coverage:** ~50% estimated
**Target Coverage:** 70%+

**Tasks:**
- [ ] Add tests for graphics resource lifecycle
  - [ ] `tests/test_sprite.cpp` - Sprite creation/destruction
  - [ ] `tests/test_texture.cpp` - Texture loading/unloading
  - [ ] `tests/test_camera.cpp` - Camera transforms
- [ ] Add tests for audio system
  - [ ] `tests/test_audio.cpp` - Sound/music lifecycle
- [ ] Add tests for UI system
  - [ ] `tests/test_ui.cpp` - Widget creation/layout
- [ ] Add tests for ECS wrapper
  - [ ] `tests/test_ecs.cpp` - Component lifecycle, system registration
- [ ] Add tests for strategy systems
  - [ ] `tests/test_turn.cpp` - Turn management
  - [ ] `tests/test_resource.cpp` - Resource stockpiles
  - [ ] `tests/test_tech.cpp` - Tech tree traversal
- [ ] Add formula evaluation edge case tests
  - [ ] Division by zero handling
  - [ ] Max recursion depth
  - [ ] Invalid expressions
- [ ] Add memory allocation failure tests
  - [ ] Test graceful handling when allocation returns NULL

---

## Medium Priority

### 5. Document Lock Ordering in Async Loader

**Status:** Not Started
**Severity:** MEDIUM
**Location:** `src/core/async.cpp`

**Tasks:**
- [ ] Add lock ordering documentation at top of async.cpp
- [ ] Verify all lock acquisitions follow documented order
- [ ] Add comments at each lock acquisition explaining order

**Documentation to Add:**
```c
/*
 * LOCK ORDERING (to prevent deadlocks):
 * 1. work_mutex
 * 2. loaded_mutex
 * 3. complete_mutex
 * 4. region_mutex
 *
 * Always acquire locks in this order. Never hold a higher-numbered
 * lock while acquiring a lower-numbered one.
 */
```

---

### 6. Add Thread Safety Assertions

**Status:** Not Started
**Severity:** MEDIUM
**Location:** All GPU/SDL wrapper functions

**Tasks:**
- [ ] Audit all functions in `src/graphics/` for main-thread requirement
- [ ] Add `AGENTITE_ASSERT_MAIN_THREAD()` to:
  - [ ] `agentite_sprite_*` functions
  - [ ] `agentite_texture_*` functions
  - [ ] `agentite_tilemap_*` functions
  - [ ] `agentite_camera_*` functions
  - [ ] `agentite_text_*` functions
  - [ ] `agentite_font_*` functions
- [ ] Add assertions to audio playback functions
- [ ] Add assertions to window/input functions
- [ ] Verify macro compiles out in release builds

---

### 7. Integrate Static Analysis in CI

**Status:** Not Started
**Severity:** MEDIUM

**Tasks:**
- [ ] Create `.github/workflows/static-analysis.yml` (or equivalent CI config)
- [ ] Run `make check` (clang-tidy) on every PR
- [ ] Run `make safety` on every PR
- [ ] Fail build on new warnings
- [ ] Document expected warnings in `.clang-tidy` config

---

### 8. Add Regular Sanitizer Testing

**Status:** Not Started
**Severity:** MEDIUM

**Tasks:**
- [ ] Add `make asan` target for AddressSanitizer build
- [ ] Add `make ubsan` target for UndefinedBehaviorSanitizer build
- [ ] Add `make lsan` target for LeakSanitizer build (or combined with asan)
- [ ] Create `scripts/run-sanitizers.sh` wrapper
- [ ] Add sanitizer runs to CI pipeline
- [ ] Fix any issues found by sanitizers

**Makefile additions:**
```makefile
asan: CFLAGS += -fsanitize=address -fno-omit-frame-pointer
asan: LDFLAGS += -fsanitize=address
asan: $(TARGET)

ubsan: CFLAGS += -fsanitize=undefined
ubsan: LDFLAGS += -fsanitize=undefined
ubsan: $(TARGET)
```

---

## Low Priority

### 9. Improve API Documentation

**Status:** Not Started
**Severity:** LOW

**Tasks:**
- [ ] Add Doxygen-style comments to public headers in `include/agentite/`
- [ ] Document function parameters, return values, and ownership
- [ ] Add usage examples in header comments
- [ ] Generate documentation with Doxygen
- [ ] Priority headers:
  - [ ] `include/agentite/sprite.h`
  - [ ] `include/agentite/ecs.h`
  - [ ] `include/agentite/input.h`
  - [ ] `include/agentite/audio.h`

---

### 10. Add Performance Profiling

**Status:** Not Started
**Severity:** LOW

**Tasks:**
- [ ] Integrate Tracy profiler (or similar)
- [ ] Add profiling zones to hot paths:
  - [ ] Render loop
  - [ ] Sprite batching
  - [ ] ECS system iteration
  - [ ] Pathfinding
  - [ ] Formula evaluation
- [ ] Document profiling usage in CLAUDE.md
- [ ] Create performance baseline measurements

---

### 11. Reduce Formula Parser Complexity

**Status:** Not Started
**Severity:** LOW
**Location:** `src/core/formula.cpp` (1,721 lines)

**Tasks:**
- [ ] Extract lexer into `src/core/formula_lexer.cpp`
- [ ] Extract bytecode compiler into `src/core/formula_compiler.cpp`
- [ ] Keep VM/evaluator in `src/core/formula.cpp`
- [ ] Add grammar documentation as comments
- [ ] Add more inline comments explaining parser decisions

---

### 12. Add Integration Tests

**Status:** Not Started
**Severity:** LOW

**Tasks:**
- [ ] Create `tests/integration/` directory
- [ ] Add full game loop test (init → update → render → shutdown)
- [ ] Add resource lifecycle test (load → use → unload)
- [ ] Add ECS integration test (create world → add entities → run systems → cleanup)
- [ ] Add headless rendering test (if possible with SDL3)

---

## Additional Fixes Identified

### 13. Consistent Allocation Macro Usage

**Status:** Not Started
**Severity:** LOW
**Location:** `src/core/containers.cpp:171`

**Tasks:**
- [ ] Replace raw `malloc` with `AGENTITE_MALLOC_ARRAY` macro
- [ ] Audit other files for raw malloc/calloc/realloc usage
- [ ] Ensure all allocations use safe macros

---

## Progress Tracking

| Priority | Total Tasks | Completed | Percentage |
|----------|-------------|-----------|------------|
| High     | 4 items     | 2.5       | 63%        |
| Medium   | 4 items     | 0         | 0%         |
| Low      | 5 items     | 0         | 0%         |
| **Total**| **13 items**| **2.5**   | **19%**    |

### Recent Changes (Session)
- **Task 1 (strcpy)**: COMPLETED - Already using safe functions
- **Task 2 (audio)**: COMPLETED - Pre-allocated buffer, removed realloc from callback
- **Task 3 (path)**: PARTIALLY COMPLETED - Core validation functions implemented

---

## Notes

- Run `./scripts/run-tests.sh` after each change to verify no regressions
- Run `./scripts/run-build.sh` to update GitStat status
- Use `/carbide-review <file>` to verify changes meet Carbide standards
- Use `/carbide-safety <file>` for security-focused review of changes

## References

- [carbide/CARBIDE.md](carbide/CARBIDE.md) - AI development guide
- [carbide/STANDARDS.md](carbide/STANDARDS.md) - Coding rules and conventions
- [docs/README.md](docs/README.md) - API documentation
