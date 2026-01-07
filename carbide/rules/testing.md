---
paths: "**/*.{c,h,cpp,hpp}"
---

# Testing Rules

## Test Organization

- One test file per module: `tests/test_<module>.c`
- Test files should be self-contained and independently runnable
- Group related tests in clearly named functions

## Test Naming

- Test functions: `test_<module>_<function>_<scenario>`
- Examples:
  - `test_player_create_success`
  - `test_player_create_null_name`
  - `test_buffer_write_overflow`

## Required Test Categories

- **Happy path**: Normal successful operation
- **Edge cases**: Empty input, zero values, maximum values
- **Error conditions**: NULL pointers, invalid input, allocation failures
- **Boundary conditions**: Buffer limits, integer boundaries

## Test Structure

```c
void test_function_scenario(void) {
    // Arrange - set up test data
    // Act - call function under test
    // Assert - verify results
    // Cleanup - free resources
}
```

## Assertions

- Use clear assertion macros that report failures meaningfully
- One logical assertion per test when possible
- Test one behavior per test function

## Sanitizer Testing

- Run tests with AddressSanitizer: `-fsanitize=address`
- Run tests with UndefinedBehaviorSanitizer: `-fsanitize=undefined`
- Run tests with LeakSanitizer: `-fsanitize=leak`
- CI builds MUST run all sanitizers

## Test Coverage

- All public API functions must have tests
- Error paths must be tested, not just happy paths
- Document any intentionally untested code

## Mocking

- Prefer dependency injection over global mocks
- Use function pointers in structs for mockable dependencies
- Document mock behavior clearly
