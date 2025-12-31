---
paths: "**/*.{c,h,cpp,hpp}"
---

# Memory Management Rules

## Ownership

- Every pointer has exactly one owner - document ownership in function signatures
- `_create` functions transfer ownership to caller (caller must call `_destroy`)
- `_get_*` functions return borrowed references (valid only while source lives)
- `_init/_deinit` pairs work on caller-owned memory

## Allocation

- **M1**: Always check allocation results - never assume malloc/calloc succeeds
- **M2**: Initialize all allocated memory - prefer `calloc` over `malloc` for zero-init
- **M3**: Match every allocation with deallocation (`malloc`/`free`, `fopen`/`fclose`, `_create`/`_destroy`)
- **M4**: Set pointers to NULL after free to prevent use-after-free
- **M5**: Destroy functions must accept NULL safely (check and return early)
- **M6**: Prefer `calloc(count, size)` over `malloc(count * size)` - calloc checks overflow

## Prohibited Patterns

- Never use `alloca()` - stack overflow risk
- Never return pointers to stack-allocated memory
- Never store pointers to temporaries or string literals that may be modified
- Avoid realloc in loops without size limits

## Opaque Types

- Hide struct definitions in .c files, expose only forward declarations in headers
- Use accessor functions (`_get_*`, `_set_*`) for field access
