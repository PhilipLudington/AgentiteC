# Carbide Security Review

Perform a security-focused review of C/C++ code.

## Arguments
- `$ARGUMENTS` - File path(s) to review (required)

## Instructions

Perform a security-focused code review targeting common C/C++ vulnerabilities. This review checks against OWASP, CWE, and Carbide security standards.

### Review Focus Areas

#### 1. Buffer Overflows (CWE-120, CWE-121, CWE-122)

Check for:
- [ ] Use of unsafe functions: `sprintf`, `strcpy`, `strcat`, `gets`, `scanf("%s")`
- [ ] Fixed-size buffer with unbounded input
- [ ] Missing length checks before copy operations
- [ ] Off-by-one errors in buffer sizing

**Safe alternatives**:
| Unsafe | Safe |
|--------|------|
| `sprintf` | `snprintf` |
| `strcpy` | `strncpy` + null-terminate, or `snprintf` |
| `strcat` | `strncat` or `snprintf` |
| `gets` | `fgets` |
| `scanf("%s")` | `scanf("%Ns")` with width specifier |

#### 2. Integer Overflow (CWE-190, CWE-191)

Check for:
- [ ] Arithmetic without overflow checks
- [ ] Size calculations that could overflow
- [ ] Signed/unsigned comparison issues
- [ ] Implicit truncation on assignment

**Pattern to find**:
```c
size_t size = count * element_size;  // Can overflow
void *p = malloc(size);              // Allocates wrong size
```

#### 3. Format String Vulnerabilities (CWE-134)

Check for:
- [ ] `printf(user_string)` - user controls format
- [ ] `sprintf(buf, user_string)` - user controls format
- [ ] Any printf-family with user-controlled format

**Fix**: Always use format specifier: `printf("%s", user_string)`

#### 4. Use After Free (CWE-416)

Check for:
- [ ] Pointer used after free()
- [ ] Pointer returned from function that freed it
- [ ] Double free potential
- [ ] Dangling pointer after scope exit

#### 5. Null Pointer Dereference (CWE-476)

Check for:
- [ ] Pointer used without NULL check
- [ ] NULL check after dereference
- [ ] Function that can return NULL with unchecked result

#### 6. Uninitialized Memory (CWE-457)

Check for:
- [ ] Local variables used before initialization
- [ ] Struct fields not initialized
- [ ] malloc() without initialization (use calloc())

#### 7. Race Conditions (CWE-362)

Check for:
- [ ] Shared data without synchronization
- [ ] Time-of-check-time-of-use (TOCTOU)
- [ ] Non-atomic operations on shared state

#### 8. Command Injection (CWE-78)

Check for:
- [ ] `system()` with user input
- [ ] `popen()` with user input
- [ ] `exec*()` with constructed strings

#### 9. Path Traversal (CWE-22)

Check for:
- [ ] File paths constructed from user input
- [ ] No validation of `..` in paths
- [ ] No canonicalization before use

#### 10. Memory Leaks (CWE-401)

Check for:
- [ ] Allocation without matching free
- [ ] Early return without cleanup
- [ ] Error paths that skip deallocation

### Severity Classification

**CRITICAL** - Exploitable vulnerability, immediate fix required:
- Buffer overflow with user input
- Format string with user input
- Command injection
- Use after free with user-controlled data

**HIGH** - Likely exploitable or causes crash:
- Null pointer dereference
- Unvalidated array index
- Integer overflow in size calculation

**MEDIUM** - Potential vulnerability or bad practice:
- Memory leak
- Uninitialized variable
- Missing bounds check on internal data

**LOW** - Code quality issue with security implications:
- Using deprecated unsafe functions
- Missing const correctness
- Overly permissive access

### Report Format

```
# Security Review Report

## Summary
- Files reviewed: X
- Critical: X
- High: X
- Medium: X
- Low: X

## Critical Issues
[Must fix immediately]

## High Issues
[Should fix before deployment]

## Medium Issues
[Fix when possible]

## Low Issues
[Consider fixing]

## Recommendations
[General security improvements]
```

### For Each Issue

Report with this format:
```
### [CRITICAL] CWE-120: Buffer Overflow
**File**: src/parser.c:127
**Code**:
```c
char buffer[64];
strcpy(buffer, user_input);  // No bounds check
```
**Risk**: Attacker can overflow buffer to execute arbitrary code
**Fix**:
```c
char buffer[64];
strncpy(buffer, user_input, sizeof(buffer) - 1);
buffer[sizeof(buffer) - 1] = '\0';
```

### Additional Checks

If the code handles:
- **Network data**: Extra scrutiny on parsing, validate all lengths
- **File I/O**: Check for path traversal, validate file sizes
- **User input**: Assume all input is malicious
- **Cryptography**: Check for weak algorithms, proper key handling

### What NOT to Flag

- Theoretical vulnerabilities with no attack vector
- Issues in test code (unless teaching bad patterns)
- Third-party library internals (note but don't count)
- Performance issues (unless they enable DoS)
