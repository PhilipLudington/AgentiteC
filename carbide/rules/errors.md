---
paths: "**/*.{c,h,cpp,hpp}"
---

# Error Handling Rules

## Return Value Conventions

- Pointer-returning functions: return `NULL` on failure
- Bool-returning functions: return `false` on failure
- Int status functions: return `0` on success, negative on failure
- Size functions: return `0` or `SIZE_MAX` on failure (context-dependent)

## Error Checking

- **E1**: Check errors immediately after the call - never defer
- **E2**: Propagate errors up the call stack - never swallow silently
- **E3**: Clean up all allocated resources on error paths
- **E4**: Use `goto cleanup` pattern for functions with multiple failure points

## Thread-Local Error Messages

- Use `set_error(fmt, ...)` to record detailed error messages
- Use `get_last_error()` to retrieve the last error
- Error messages should include context (file paths, values, indices)
- Clear errors at function entry points where appropriate

## Assertions

- Use `assert()` only for internal invariants that should never be false
- Never put side effects inside assert expressions (disabled in release builds)
- Use `static_assert` for compile-time checks (struct sizes, alignments)
- Prefer explicit error handling over assertions for recoverable conditions

## Cleanup Pattern

```c
bool func(void) {
    Resource *a = NULL, *b = NULL;
    bool result = false;

    a = resource_create();
    if (!a) goto cleanup;

    b = resource_create();
    if (!b) goto cleanup;

    result = true;
cleanup:
    resource_destroy(b);
    resource_destroy(a);
    return result;
}
```
