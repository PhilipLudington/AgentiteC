#ifndef AGENTITE_MATH_SAFE_H
#define AGENTITE_MATH_SAFE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Carbon Safe Arithmetic Library
 *
 * Overflow-protected integer arithmetic for financial calculations,
 * scores, and resource systems. Operations clamp to INT_MAX/MIN on
 * overflow and optionally log warnings.
 *
 * Usage:
 *   // Check before operation
 *   if (agentite_would_multiply_overflow(price, quantity)) {
 *       // Handle overflow case
 *   }
 *
 *   // Or use safe operations (clamp on overflow)
 *   int32_t total = agentite_safe_multiply(price, quantity);
 *   int32_t balance = agentite_safe_add(balance, income);
 *   int32_t result = agentite_safe_subtract(funds, cost);
 */

/*============================================================================
 * 32-bit Overflow Detection
 *============================================================================*/

/**
 * Check if a * b would overflow int32_t.
 *
 * @param a First operand
 * @param b Second operand
 * @return true if multiplication would overflow
 */
bool agentite_would_multiply_overflow(int32_t a, int32_t b);

/**
 * Check if a + b would overflow int32_t.
 *
 * @param a First operand
 * @param b Second operand
 * @return true if addition would overflow
 */
bool agentite_would_add_overflow(int32_t a, int32_t b);

/**
 * Check if a - b would overflow int32_t.
 *
 * @param a First operand
 * @param b Second operand
 * @return true if subtraction would overflow
 */
bool agentite_would_subtract_overflow(int32_t a, int32_t b);

/*============================================================================
 * 32-bit Safe Operations
 *============================================================================*/

/**
 * Safe multiplication that clamps to INT32_MAX/MIN on overflow.
 * Logs a warning if overflow occurs.
 *
 * @param a First operand
 * @param b Second operand
 * @return Result clamped to int32_t range
 */
int32_t agentite_safe_multiply(int32_t a, int32_t b);

/**
 * Safe addition that clamps to INT32_MAX/MIN on overflow.
 * Logs a warning if overflow occurs.
 *
 * @param a First operand
 * @param b Second operand
 * @return Result clamped to int32_t range
 */
int32_t agentite_safe_add(int32_t a, int32_t b);

/**
 * Safe subtraction that clamps to INT32_MAX/MIN on overflow.
 * Logs a warning if overflow occurs.
 *
 * @param a First operand
 * @param b Second operand
 * @return Result clamped to int32_t range
 */
int32_t agentite_safe_subtract(int32_t a, int32_t b);

/**
 * Safe division that handles divide-by-zero.
 * Returns 0 and logs warning if b is 0.
 *
 * @param a Dividend
 * @param b Divisor
 * @return Result, or 0 if b is 0
 */
int32_t agentite_safe_divide(int32_t a, int32_t b);

/*============================================================================
 * 64-bit Overflow Detection
 *============================================================================*/

/**
 * Check if a * b would overflow int64_t.
 *
 * @param a First operand
 * @param b Second operand
 * @return true if multiplication would overflow
 */
bool agentite_would_multiply_overflow_i64(int64_t a, int64_t b);

/**
 * Check if a + b would overflow int64_t.
 *
 * @param a First operand
 * @param b Second operand
 * @return true if addition would overflow
 */
bool agentite_would_add_overflow_i64(int64_t a, int64_t b);

/**
 * Check if a - b would overflow int64_t.
 *
 * @param a First operand
 * @param b Second operand
 * @return true if subtraction would overflow
 */
bool agentite_would_subtract_overflow_i64(int64_t a, int64_t b);

/*============================================================================
 * 64-bit Safe Operations
 *============================================================================*/

/**
 * Safe 64-bit multiplication that clamps to INT64_MAX/MIN on overflow.
 *
 * @param a First operand
 * @param b Second operand
 * @return Result clamped to int64_t range
 */
int64_t agentite_safe_multiply_i64(int64_t a, int64_t b);

/**
 * Safe 64-bit addition that clamps to INT64_MAX/MIN on overflow.
 *
 * @param a First operand
 * @param b Second operand
 * @return Result clamped to int64_t range
 */
int64_t agentite_safe_add_i64(int64_t a, int64_t b);

/**
 * Safe 64-bit subtraction that clamps to INT64_MAX/MIN on overflow.
 *
 * @param a First operand
 * @param b Second operand
 * @return Result clamped to int64_t range
 */
int64_t agentite_safe_subtract_i64(int64_t a, int64_t b);

/**
 * Safe 64-bit division that handles divide-by-zero.
 *
 * @param a Dividend
 * @param b Divisor
 * @return Result, or 0 if b is 0
 */
int64_t agentite_safe_divide_i64(int64_t a, int64_t b);

/*============================================================================
 * Unsigned Safe Operations
 *============================================================================*/

/**
 * Check if a + b would overflow uint32_t.
 *
 * @param a First operand
 * @param b Second operand
 * @return true if addition would overflow
 */
bool agentite_would_add_overflow_u32(uint32_t a, uint32_t b);

/**
 * Check if a * b would overflow uint32_t.
 *
 * @param a First operand
 * @param b Second operand
 * @return true if multiplication would overflow
 */
bool agentite_would_multiply_overflow_u32(uint32_t a, uint32_t b);

/**
 * Safe unsigned addition that clamps to UINT32_MAX on overflow.
 *
 * @param a First operand
 * @param b Second operand
 * @return Result clamped to uint32_t range
 */
uint32_t agentite_safe_add_u32(uint32_t a, uint32_t b);

/**
 * Safe unsigned multiplication that clamps to UINT32_MAX on overflow.
 *
 * @param a First operand
 * @param b Second operand
 * @return Result clamped to uint32_t range
 */
uint32_t agentite_safe_multiply_u32(uint32_t a, uint32_t b);

/**
 * Safe unsigned subtraction that clamps to 0 on underflow.
 *
 * @param a First operand
 * @param b Second operand
 * @return Result, or 0 if b > a
 */
uint32_t agentite_safe_subtract_u32(uint32_t a, uint32_t b);

/*============================================================================
 * Configuration
 *============================================================================*/

/**
 * Enable or disable overflow warnings via the logging system.
 * Enabled by default if logging is initialized.
 *
 * @param enabled true to log overflow warnings, false to suppress
 */
void agentite_safe_math_set_warnings(bool enabled);

#endif /* AGENTITE_MATH_SAFE_H */
