---
paths: "**/*.{c,h,cpp,hpp}"
---

# Security Rules

## Input Validation

- **S1**: Validate all external input before use (files, network, user input, env vars)
- Check data sizes before processing
- Validate ranges, formats, and constraints
- Reject invalid input early with clear error messages

## Buffer Safety

- **S2**: Use bounded string functions only

| Unsafe | Safe Alternative |
|--------|------------------|
| `sprintf` | `snprintf` |
| `strcpy` | `strncpy` + null terminate, or `strlcpy` |
| `strcat` | `strncat` or `snprintf` |
| `gets` | `fgets` |
| `scanf("%s")` | `scanf("%Ns")` with width specifier |

## Integer Safety

- **S3**: Check for overflow before arithmetic operations
- Before multiply: `if (a > 0 && b > SIZE_MAX / a) { /* overflow */ }`
- Before add: `if (a > SIZE_MAX - b) { /* overflow */ }`
- Use `size_t` for sizes and counts, not `int`

## Array Bounds

- **S4**: Validate indices before every array access
- Check `index < count` before `array[index]`
- Return error or use safe default, never access out-of-bounds

## Format String Safety

- **S5**: Never pass user input as format string
- BAD: `printf(user_input)`
- GOOD: `printf("%s", user_input)`

## Path Safety

- **S6**: Validate file paths to prevent traversal attacks
- Reject paths containing `..`
- Use allowlists for permitted directories
- Canonicalize paths before validation

## Command Injection

- **S7**: Never pass user input directly to `system()` or `popen()`
- Use `exec*` family with explicit argument arrays instead
- Validate and sanitize all command components
