# Carbide Code Review

Review C/C++ code against Carbide standards.

## Arguments
- `$ARGUMENTS` - File path(s) to review (required)

## Instructions

Perform a comprehensive code review of the specified C/C++ file(s) against Carbide standards defined in STANDARDS.md and CARBIDE.md.

### Review Process

1. **Read the file(s)** specified in arguments
2. **Check against each standard category** below
3. **Report findings** organized by severity
4. **Suggest fixes** for each issue

### Categories to Check

#### 1. Naming Conventions (STANDARDS.md Section 2)
- [ ] Types use PascalCase
- [ ] Functions use snake_case
- [ ] Macros use UPPER_SNAKE_CASE
- [ ] Public symbols have project prefix
- [ ] Boolean variables use is_/has_/can_ prefix
- [ ] Function names follow patterns (_create/_destroy, _get/_set, etc.)

#### 2. Memory Management (STANDARDS.md Section 3)
- [ ] All allocations are checked for NULL
- [ ] All allocated memory is initialized
- [ ] Create/destroy functions are paired
- [ ] Pointers set to NULL after free
- [ ] Clear ownership (who allocates, who frees)
- [ ] No use-after-free patterns
- [ ] No double-free patterns

#### 3. Error Handling (STANDARDS.md Section 4)
- [ ] Return values follow conventions (NULL/false/negative for failure)
- [ ] Errors checked immediately after calls
- [ ] Errors propagated, not swallowed
- [ ] Cleanup on error paths
- [ ] Error messages set via set_error()

#### 4. API Design (STANDARDS.md Section 5)
- [ ] Config structs for 3+ parameters
- [ ] DEFAULT macros for config structs
- [ ] Const correctness (const for read-only params)
- [ ] Header guards present and correct format
- [ ] extern "C" guards for C headers

#### 5. Code Organization (STANDARDS.md Section 7)
- [ ] Header includes in correct order
- [ ] Section comments used appropriately
- [ ] No overly long functions (suggest 50 lines max)

### Report Format

Organize findings by severity:

```
## Critical Issues
Issues that will cause crashes, memory corruption, or security vulnerabilities.

## Warnings
Issues that violate Carbide standards but may not cause immediate problems.

## Suggestions
Style improvements and best practice recommendations.

## Summary
- Files reviewed: X
- Critical: X
- Warnings: X
- Suggestions: X
- Standards compliance: X%
```

### For Each Issue

Report:
1. **Location**: File and line number
2. **Rule violated**: Reference to STANDARDS.md section
3. **Problem**: What's wrong
4. **Fix**: How to correct it

Example:
```
### [WARNING] src/player.c:45
**Rule**: STANDARDS.md 3.1 - Check all allocation results
**Problem**: malloc result not checked
**Code**: `Player *p = malloc(sizeof(Player));`
**Fix**:
```c
Player *p = malloc(sizeof(Player));
if (!p) {
    set_error("Failed to allocate Player");
    return NULL;
}
```

### What NOT to Flag

- Style preferences not covered by STANDARDS.md
- Third-party code patterns (unless security issue)
- Performance optimizations (unless requested)
- Missing features or functionality
