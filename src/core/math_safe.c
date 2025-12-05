#include "carbon/math_safe.h"
#include "carbon/log.h"
#include <limits.h>

static bool warnings_enabled = true;

/* Log overflow warning if enabled */
static void log_overflow(const char *operation) {
    if (warnings_enabled) {
        carbon_log_warning(CARBON_LOG_CORE, "Integer overflow in %s operation", operation);
    }
}

static void log_divide_by_zero(void) {
    if (warnings_enabled) {
        carbon_log_warning(CARBON_LOG_CORE, "Division by zero attempted");
    }
}

/*============================================================================
 * 32-bit Overflow Detection
 *============================================================================*/

bool carbon_would_multiply_overflow(int32_t a, int32_t b) {
    if (a == 0 || b == 0) return false;

    /* Check for overflow using division */
    if (a > 0) {
        if (b > 0) {
            return a > INT32_MAX / b;
        } else {
            return b < INT32_MIN / a;
        }
    } else {
        if (b > 0) {
            return a < INT32_MIN / b;
        } else {
            return a != 0 && b < INT32_MAX / a;
        }
    }
}

bool carbon_would_add_overflow(int32_t a, int32_t b) {
    if (b > 0 && a > INT32_MAX - b) return true;
    if (b < 0 && a < INT32_MIN - b) return true;
    return false;
}

bool carbon_would_subtract_overflow(int32_t a, int32_t b) {
    if (b < 0 && a > INT32_MAX + b) return true;
    if (b > 0 && a < INT32_MIN + b) return true;
    return false;
}

/*============================================================================
 * 32-bit Safe Operations
 *============================================================================*/

int32_t carbon_safe_multiply(int32_t a, int32_t b) {
    if (carbon_would_multiply_overflow(a, b)) {
        log_overflow("multiply");
        /* Determine sign of result and clamp appropriately */
        if ((a > 0 && b > 0) || (a < 0 && b < 0)) {
            return INT32_MAX;
        } else {
            return INT32_MIN;
        }
    }
    return a * b;
}

int32_t carbon_safe_add(int32_t a, int32_t b) {
    if (carbon_would_add_overflow(a, b)) {
        log_overflow("add");
        return (b > 0) ? INT32_MAX : INT32_MIN;
    }
    return a + b;
}

int32_t carbon_safe_subtract(int32_t a, int32_t b) {
    if (carbon_would_subtract_overflow(a, b)) {
        log_overflow("subtract");
        return (b < 0) ? INT32_MAX : INT32_MIN;
    }
    return a - b;
}

int32_t carbon_safe_divide(int32_t a, int32_t b) {
    if (b == 0) {
        log_divide_by_zero();
        return 0;
    }
    /* Handle INT32_MIN / -1 overflow case */
    if (a == INT32_MIN && b == -1) {
        log_overflow("divide");
        return INT32_MAX;
    }
    return a / b;
}

/*============================================================================
 * 64-bit Overflow Detection
 *============================================================================*/

bool carbon_would_multiply_overflow_i64(int64_t a, int64_t b) {
    if (a == 0 || b == 0) return false;

    if (a > 0) {
        if (b > 0) {
            return a > INT64_MAX / b;
        } else {
            return b < INT64_MIN / a;
        }
    } else {
        if (b > 0) {
            return a < INT64_MIN / b;
        } else {
            return a != 0 && b < INT64_MAX / a;
        }
    }
}

bool carbon_would_add_overflow_i64(int64_t a, int64_t b) {
    if (b > 0 && a > INT64_MAX - b) return true;
    if (b < 0 && a < INT64_MIN - b) return true;
    return false;
}

bool carbon_would_subtract_overflow_i64(int64_t a, int64_t b) {
    if (b < 0 && a > INT64_MAX + b) return true;
    if (b > 0 && a < INT64_MIN + b) return true;
    return false;
}

/*============================================================================
 * 64-bit Safe Operations
 *============================================================================*/

int64_t carbon_safe_multiply_i64(int64_t a, int64_t b) {
    if (carbon_would_multiply_overflow_i64(a, b)) {
        log_overflow("multiply_i64");
        if ((a > 0 && b > 0) || (a < 0 && b < 0)) {
            return INT64_MAX;
        } else {
            return INT64_MIN;
        }
    }
    return a * b;
}

int64_t carbon_safe_add_i64(int64_t a, int64_t b) {
    if (carbon_would_add_overflow_i64(a, b)) {
        log_overflow("add_i64");
        return (b > 0) ? INT64_MAX : INT64_MIN;
    }
    return a + b;
}

int64_t carbon_safe_subtract_i64(int64_t a, int64_t b) {
    if (carbon_would_subtract_overflow_i64(a, b)) {
        log_overflow("subtract_i64");
        return (b < 0) ? INT64_MAX : INT64_MIN;
    }
    return a - b;
}

int64_t carbon_safe_divide_i64(int64_t a, int64_t b) {
    if (b == 0) {
        log_divide_by_zero();
        return 0;
    }
    /* Handle INT64_MIN / -1 overflow case */
    if (a == INT64_MIN && b == -1) {
        log_overflow("divide_i64");
        return INT64_MAX;
    }
    return a / b;
}

/*============================================================================
 * Unsigned Safe Operations
 *============================================================================*/

bool carbon_would_add_overflow_u32(uint32_t a, uint32_t b) {
    return a > UINT32_MAX - b;
}

bool carbon_would_multiply_overflow_u32(uint32_t a, uint32_t b) {
    if (a == 0 || b == 0) return false;
    return a > UINT32_MAX / b;
}

uint32_t carbon_safe_add_u32(uint32_t a, uint32_t b) {
    if (carbon_would_add_overflow_u32(a, b)) {
        log_overflow("add_u32");
        return UINT32_MAX;
    }
    return a + b;
}

uint32_t carbon_safe_multiply_u32(uint32_t a, uint32_t b) {
    if (carbon_would_multiply_overflow_u32(a, b)) {
        log_overflow("multiply_u32");
        return UINT32_MAX;
    }
    return a * b;
}

uint32_t carbon_safe_subtract_u32(uint32_t a, uint32_t b) {
    if (b > a) {
        log_overflow("subtract_u32");
        return 0;
    }
    return a - b;
}

/*============================================================================
 * Configuration
 *============================================================================*/

void carbon_safe_math_set_warnings(bool enabled) {
    warnings_enabled = enabled;
}
