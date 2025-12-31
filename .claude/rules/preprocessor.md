---
paths: "**/*.{c,h,cpp,hpp}"
---

# Preprocessor Rules

## Macro Hygiene

- **P1**: Parenthesize all macro parameters
- **P2**: Parenthesize the entire macro expansion
- **P3**: Use `do { ... } while(0)` for multi-statement macros

```c
// GOOD
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SAFE_FREE(ptr) do { free(ptr); (ptr) = NULL; } while(0)

// BAD
#define MAX(a, b) a > b ? a : b
#define SAFE_FREE(ptr) free(ptr); ptr = NULL
```

## Macro Side Effects

- **P4**: Never use expressions with side effects in macros
- **P5**: Evaluate macro arguments exactly once when possible
- Document if a macro evaluates arguments multiple times

```c
// DANGER: MAX(i++, j++) increments twice!
// For side-effect-safe operations, use inline functions instead
static inline int max_int(int a, int b) { return a > b ? a : b; }
```

## When to Use Macros vs Functions

- Use macros for: constants, conditional compilation, generic operations
- Use `static inline` for: type-safe operations, debuggability, avoiding side-effect issues
- Use `enum` for: related integer constants (provides type checking)

## Conditional Compilation

- Use `#ifdef` for feature detection, not `#if defined()`
- Group platform-specific code in dedicated sections
- Provide fallbacks or clear errors for unsupported configurations

```c
#ifdef _WIN32
    // Windows implementation
#elif defined(__linux__)
    // Linux implementation
#else
    #error "Unsupported platform"
#endif
```

## Include Guards

- Use `#ifndef PROJECT_MODULE_H` format
- Guard name must match file path to avoid collisions
- Place guards as the very first and last lines (after license comment)

## Prohibited Practices

- Never `#undef` standard library macros
- Never use `#define` to rename standard functions
- Avoid function-like macros in public headers when inline functions work
- Never use macros to generate unbalanced braces or control flow
