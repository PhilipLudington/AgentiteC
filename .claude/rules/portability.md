---
paths: "**/*.{c,h,cpp,hpp}"
---

# Portability Rules

## Integer Types

- **PT1**: Use fixed-width types for data structures: `int32_t`, `uint64_t`, etc.
- **PT2**: Use `size_t` for sizes and array indices
- **PT3**: Use `ptrdiff_t` for pointer differences
- **PT4**: Use `intptr_t`/`uintptr_t` for integers that hold pointers
- Avoid `int` for anything that crosses API boundaries

## Endianness

- **PT5**: Never assume host byte order for serialized data
- Use explicit conversion functions: `htole32`, `le32toh`, etc.
- Document byte order in file format specifications
- Test on both little-endian and big-endian systems if possible

## Path Handling

- **PT6**: Use `/` as path separator (works on all platforms)
- **PT7**: Never hardcode absolute paths
- **PT8**: Use `PATH_MAX` or dynamic allocation for path buffers
- Handle both `/` and `\` when parsing paths on Windows

## Platform-Specific Code

- Isolate platform code in dedicated files: `platform_win32.c`, `platform_posix.c`
- Use a common header with platform-agnostic API
- Prefer feature detection over platform detection

```c
// Prefer this:
#ifdef HAVE_MMAP
// Use mmap
#else
// Fallback
#endif

// Over this:
#ifdef __linux__
// Assume mmap exists
#endif
```

## Compiler Extensions

- **PT9**: Avoid compiler-specific extensions in portable code
- If extensions are needed, provide fallbacks:

```c
#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#define PACKED __attribute__((packed))
#else
#define UNUSED
#define PACKED
#endif
```

## Data Alignment

- **PT10**: Never cast between pointer types with different alignment requirements
- Use `alignas` or `_Alignas` for explicit alignment
- Avoid unaligned memory access (undefined on some architectures)

## Standard Library

- Stick to C11 standard library functions
- Avoid POSIX-only or Windows-only functions in portable code
- When using platform functions, provide abstractions
