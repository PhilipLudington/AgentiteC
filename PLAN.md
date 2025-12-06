# Carbon Engine Code Quality Improvement Plan

Based on comprehensive code quality review. Overall score: 7.5/10.

## Phase 1: Critical Security Fixes ✓ COMPLETED

### 1.1 Buffer Overflow in Save System ✓
- **File**: `src/strategy/save.cpp:116`
- **Issue**: `strcpy(writer.current_section, "game_state")` without bounds checking
- **Status**: Fixed - replaced with `strncpy` + null-termination

### 1.2 Path Traversal Vulnerability ✓
- **File**: `src/strategy/save.cpp`
- **Issue**: No validation of user-provided save names allows `../../` attacks
- **Status**: Fixed - added `is_valid_save_name()` validation to all save functions:
  - `carbon_save_game()`
  - `carbon_load_game()`
  - `carbon_save_delete()`
  - `carbon_save_exists()`

### 1.3 Unsafe String Functions Audit ✓
All `strcpy` calls replaced with `strncpy` + null-termination:
- [x] `src/strategy/game_event.cpp` - Fixed operator string copies in tokenizer
- [x] `src/strategy/biome.cpp` - Fixed `carbon_biome_default_def()`
- [x] `src/strategy/anomaly.cpp` - Fixed result message assignments and `carbon_anomaly_type_default()`
- [x] `src/strategy/save.cpp` - Fixed section name copy
- [x] `src/core/query.cpp` - Fixed tag array shifting in `carbon_query_remove_tag()`

---

## Phase 2: Memory Safety Improvements ✓ COMPLETED

### 2.1 Document Ownership Semantics ✓
- [x] Added comprehensive Memory Ownership Conventions documentation to `include/carbon/carbon.h`
- [x] Documented CREATE/DESTROY pairs, LOAD functions, GET functions, const char* returns
- [x] Added ownership documentation to key headers:
  - `event.h` - `carbon_event_dispatcher_create()`
  - `sprite.h` - `carbon_sprite_init()`, `carbon_texture_*()` functions
  - `text.h` - `carbon_text_init()`, `carbon_font_load()`, `carbon_sdf_font_load()`
  - `animation.h` - `carbon_animation_create()`, `carbon_animation_from_*()` functions
  - `pathfinding.h` - `carbon_pathfinder_create()`, `carbon_pathfinder_find()`
  - `blueprint.h` - `carbon_blueprint_create()`

### 2.2 RAII Wrappers - SKIPPED (Decision Made)
**Decision**: Keep C-style programming pattern.

Rationale:
- Codebase uses pure C patterns despite .cpp extension
- No existing std:: usage in source files
- Extensive refactoring would be needed with minimal benefit
- Current create/destroy pattern is well-documented and consistent

### 2.3 Consistent Allocation Pattern ✓
Audit completed:
- [x] All `malloc`/`realloc` calls checked for NULL handling
- [x] Only 2 missing NULL checks found in `text.cpp` (lines 1926, 1943) - FIXED
- [x] `CARBON_ALLOC` macros defined in `carbon.h` (underutilized but available)
- [x] 98% of allocations already have proper NULL checks

---

## Phase 3: Performance Optimization

### 3.1 Sprite Batch Auto-Flush ✓
- **File**: `src/graphics/sprite.cpp`
- **Issue**: Texture change mid-batch only logs warning
- **Status**: Fixed with multi-segment batch rendering
- **Solution**:
  - Added `SpriteBatchSegment` struct to track texture + index range
  - Store up to 64 sub-batches when textures change mid-batch
  - Modified `carbon_sprite_render()` to render all segments in order
  - No data loss on texture switches; proper draw order maintained

### 3.2 Memory Pools for Hot Paths (Deferred)
Implement object pools for frequently allocated objects:
- [ ] Command objects in `src/core/command.cpp`
- [ ] Event objects in `src/core/event.cpp`
- [ ] Formula nodes in `src/core/formula.cpp`

**Note**: Deferred - requires profiling to identify actual bottlenecks first.

### 3.3 Event System Optimization (Deferred)
- **File**: `src/core/event.cpp:161-170`
- **Issue**: O(n) linear scan through all listeners
- **Fix**: Hash table lookup by event type (only if profiling shows bottleneck)

**Note**: Deferred - current O(n) scan is likely sufficient for typical game event counts.

---

## Phase 4: Code Organization

### 4.1 Split Large Files
Break down files exceeding 1000 lines:

**text.cpp (2269 lines)**:
- [ ] `text_font.cpp` - Font loading and management
- [ ] `text_render.cpp` - Text rendering and batching
- [ ] `text_sdf.cpp` - SDF/MSDF specific code

**formula.cpp (1697 lines)**:
- [ ] `formula_lexer.cpp` - Tokenization
- [ ] `formula_parser.cpp` - AST construction
- [ ] `formula_eval.cpp` - Evaluation and bytecode
- [ ] `formula_builtins.cpp` - Built-in functions

**htn.cpp (1176 lines)**:
- [ ] `htn_planner.cpp` - Core planning algorithm
- [ ] `htn_tasks.cpp` - Task definitions and management
- [ ] `htn_conditions.cpp` - Condition evaluation

### 4.2 C++ Migration Decision
Choose one:
- **Option A**: Full C++ adoption - use std::unique_ptr, std::optional, RAII
- **Option B**: Revert to C - rename .cpp → .c, remove C++ includes

Current state is confusing: files are .cpp but code is pure C style.

### 4.3 Add Recursion Limits
- **File**: `src/core/formula.cpp`
- **Issue**: Deep nesting can cause stack overflow
- **Fix**: Add depth counter to parser, fail at reasonable limit (e.g., 64)

---

## Phase 5: Testing Infrastructure

### 5.1 Test Framework Setup
- [ ] Add test framework (Catch2 recommended for C++)
- [ ] Create `tests/` directory structure
- [ ] Add `make test` target to Makefile

### 5.2 Unit Tests Priority
Core systems (highest priority):
- [ ] `tests/core/test_error.cpp` - Error handling
- [ ] `tests/core/test_event.cpp` - Event dispatcher
- [ ] `tests/core/test_command.cpp` - Command queue
- [ ] `tests/core/test_formula.cpp` - Formula evaluation

Strategy systems:
- [ ] `tests/strategy/test_save.cpp` - Save/load (especially path validation)
- [ ] `tests/strategy/test_resource.cpp` - Resource management

AI systems:
- [ ] `tests/ai/test_pathfinding.cpp` - A* correctness

### 5.3 Integration Tests
- [ ] Graphics pipeline (sprite batching, texture handling)
- [ ] Full game loop (init → update → render → shutdown)

---

## Progress Tracking

| Phase | Status | Notes |
|-------|--------|-------|
| Phase 1: Security | Completed | All strcpy→strncpy fixes done, path traversal validation added |
| Phase 2: Memory Safety | Completed | Ownership docs added, NULL check fixes, RAII skipped (decision made) |
| Phase 3: Performance | Partial | Sprite auto-flush done; pools/event optimization deferred pending profiling |
| Phase 4: Organization | Not Started | |
| Phase 5: Testing | Not Started | |

---

## Files Reference

### Critical Priority
- `src/strategy/save.cpp` - Buffer overflow, path traversal
- `src/strategy/game_event.cpp` - Unsafe strings
- `src/strategy/biome.cpp` - Unsafe strings
- `src/strategy/anomaly.cpp` - Unsafe strings
- `src/core/query.cpp` - Unsafe strings

### Performance Priority
- `src/graphics/sprite.cpp` - Texture switching
- `src/core/event.cpp` - Listener lookup
- `src/core/command.cpp` - Allocation pooling

### Organization Priority
- `src/graphics/text.cpp` - Split (2269 lines)
- `src/core/formula.cpp` - Split (1697 lines)
- `src/ai/htn.cpp` - Split (1176 lines)
