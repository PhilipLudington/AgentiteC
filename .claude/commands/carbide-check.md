# Carbide Validation Check

Run validation tooling on a Carbide project.

## Arguments
- `$ARGUMENTS` - Optional: specific targets or files to check

## Instructions

Run the Carbide validation toolchain on the current project. This command requires a Carbide-structured project with a Makefile.

### Prerequisites Check

1. **Verify Carbide project structure exists**:
   - `Makefile` with Carbide targets
   - `src/` directory
   - `include/` directory (optional but expected)

2. **Check for required tools**:
   - `clang-tidy` (for static analysis)
   - `clang-format` (for format checking)
   - C compiler (clang, gcc, or cl)

If prerequisites are missing, report what's needed and how to install.

### Validation Steps

Run these make targets in order and report results:

#### 1. Compile Check
```bash
make build 2>&1
```
- Report any compilation errors or warnings
- Count: errors, warnings

#### 2. Static Analysis
```bash
make check 2>&1
```
- Report clang-tidy findings
- Categorize by severity (error, warning, note)

#### 3. Format Check
```bash
make format-check 2>&1
```
- Report files that need formatting
- Show diff if available

#### 4. (If tests exist) Test with Sanitizers
```bash
make test 2>&1
```
- Report test results
- Report any sanitizer findings (ASAN, UBSAN)

### Report Format

```
# Carbide Validation Report

## Build
- Status: PASS/FAIL
- Errors: X
- Warnings: X

## Static Analysis (clang-tidy)
- Status: PASS/FAIL
- Issues found: X
  - Errors: X
  - Warnings: X
  - Notes: X

## Format
- Status: PASS/FAIL
- Files needing format: X

## Tests
- Status: PASS/FAIL/SKIPPED
- Tests run: X
- Tests passed: X
- Sanitizer issues: X

## Overall
- Status: PASS/FAIL
- Issues to fix: X
```

### Issue Details

For each issue found, report:
1. **Source**: Which check found it (compiler, clang-tidy, sanitizer)
2. **Location**: File:line
3. **Message**: The actual error/warning
4. **Severity**: Error/Warning/Note

### Fixing Issues

After reporting, offer to:
1. Auto-fix formatting issues (`make format`)
2. Show how to fix specific issues
3. Re-run validation after fixes

### If No Makefile

If the project doesn't have a Carbide Makefile:
1. Suggest running `/carbide-init` to set up the project
2. Or offer to run individual checks manually:
   - `clang-tidy src/*.c -- -Iinclude`
   - `clang-format --dry-run src/*.c`

### Exit Status Interpretation

- `make build`: 0 = success, non-zero = compile errors
- `make check`: 0 = no issues, non-zero = issues found
- `make test`: 0 = all pass, non-zero = failures
