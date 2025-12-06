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

## Phase 2: Memory Safety Improvements

### 2.1 Document Ownership Semantics
Add comments to all pointer-returning functions indicating:
- Who owns the returned memory
- Whether caller must free
- Lifetime expectations

### 2.2 RAII Wrappers (Optional C++ Enhancement)
If committing to C++, create wrappers for:
- [ ] `Carbon_Engine` - auto-cleanup on scope exit
- [ ] `Carbon_Texture` - reference counting or unique ownership
- [ ] `Carbon_EventDispatcher` - automatic listener cleanup

### 2.3 Consistent Allocation Pattern
Audit all `malloc`/`realloc` calls to ensure:
- [ ] NULL check after allocation
- [ ] Use `CARBON_ALLOC` macro consistently
- [ ] Document realloc failure handling (original pointer preserved)

---

## Phase 3: Performance Optimization

### 3.1 Sprite Batch Auto-Flush
- **File**: `src/graphics/sprite.cpp:686-690`
- **Issue**: Texture change mid-batch only logs warning
- **Fix**: Automatically flush batch when texture changes
```cpp
if (sr->current_texture && sr->current_texture != sprite->texture) {
    carbon_sprite_upload(sr, cmd);  // Flush current batch
    carbon_sprite_begin(sr, NULL);  // Start new batch
}
sr->current_texture = sprite->texture;
```

### 3.2 Memory Pools for Hot Paths
Implement object pools for frequently allocated objects:
- [ ] Command objects in `src/core/command.cpp`
- [ ] Event objects in `src/core/event.cpp`
- [ ] Formula nodes in `src/core/formula.cpp`

### 3.3 Event System Optimization (If Needed)
- **File**: `src/core/event.cpp:161-170`
- **Issue**: O(n) linear scan through all listeners
- **Fix**: Hash table lookup by event type (only if profiling shows bottleneck)

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
| Phase 2: Memory Safety | Not Started | |
| Phase 3: Performance | Not Started | |
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
