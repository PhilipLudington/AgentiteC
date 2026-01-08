/**
 * Agentite Engine - Formula Built-in Functions
 *
 * Implements all built-in mathematical and utility functions for the
 * formula expression engine.
 *
 * Built-in Function Reference:
 * ============================
 *
 * Math Functions:
 *   min(a, b, ...)     - Returns minimum value (2+ args)
 *   max(a, b, ...)     - Returns maximum value (2+ args)
 *   clamp(x, lo, hi)   - Clamps x to range [lo, hi]
 *   lerp(a, b, t)      - Linear interpolation: a + (b - a) * t
 *   abs(x)             - Absolute value
 *   sign(x)            - Returns -1, 0, or 1 based on sign of x
 *   step(edge, x)      - Returns 1.0 if x >= edge, else 0.0
 *   smoothstep(e0, e1, x) - Smooth Hermite interpolation
 *
 * Rounding Functions:
 *   floor(x)           - Round down to nearest integer
 *   ceil(x)            - Round up to nearest integer
 *   round(x)           - Round to nearest integer
 *   trunc(x)           - Truncate toward zero
 *
 * Power/Logarithm Functions:
 *   sqrt(x)            - Square root (error if x < 0)
 *   pow(base, exp)     - Power function
 *   exp(x)             - e^x
 *   log(x)             - Natural logarithm (error if x <= 0)
 *   log2(x)            - Base-2 logarithm (error if x <= 0)
 *   log10(x)           - Base-10 logarithm (error if x <= 0)
 *
 * Trigonometric Functions (radians):
 *   sin(x)             - Sine
 *   cos(x)             - Cosine
 *   tan(x)             - Tangent
 *   asin(x)            - Arc sine (error if |x| > 1)
 *   acos(x)            - Arc cosine (error if |x| > 1)
 *   atan(x)            - Arc tangent
 *   atan2(y, x)        - Two-argument arc tangent
 *
 * Control Flow:
 *   if(cond, a, b)     - Returns a if cond != 0, else b
 */

#include "formula_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Built-in Function Implementation
 * ============================================================================ */

double formula_call_builtin(const char *name, double *args, int argc, Agentite_FormulaContext *ctx) {
    /* Check custom functions first (allows overriding built-ins) */
    for (int i = 0; i < ctx->custom_func_count; i++) {
        if (strcmp(ctx->custom_funcs[i].name, name) == 0) {
            FormulaCustomFunc *f = &ctx->custom_funcs[i];
            if (argc < f->min_args) {
                snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                         "Function '%s' requires at least %d arguments", name, f->min_args);
                return NAN;
            }
            if (f->max_args >= 0 && argc > f->max_args) {
                snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                         "Function '%s' accepts at most %d arguments", name, f->max_args);
                return NAN;
            }
            return f->func(args, argc, f->userdata);
        }
    }

    /* ========================================================================
     * Math Functions
     * ======================================================================== */

    if (strcmp(name, "min") == 0) {
        if (argc < 2) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "min() requires at least 2 arguments");
            return NAN;
        }
        double result = args[0];
        for (int i = 1; i < argc; i++) {
            if (args[i] < result) result = args[i];
        }
        return result;
    }

    if (strcmp(name, "max") == 0) {
        if (argc < 2) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "max() requires at least 2 arguments");
            return NAN;
        }
        double result = args[0];
        for (int i = 1; i < argc; i++) {
            if (args[i] > result) result = args[i];
        }
        return result;
    }

    if (strcmp(name, "clamp") == 0) {
        if (argc != 3) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "clamp() requires 3 arguments");
            return NAN;
        }
        double val = args[0], lo = args[1], hi = args[2];
        if (val < lo) return lo;
        if (val > hi) return hi;
        return val;
    }

    if (strcmp(name, "lerp") == 0) {
        if (argc != 3) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "lerp() requires 3 arguments");
            return NAN;
        }
        return args[0] + (args[1] - args[0]) * args[2];
    }

    if (strcmp(name, "abs") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "abs() requires 1 argument");
            return NAN;
        }
        return fabs(args[0]);
    }

    if (strcmp(name, "sign") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "sign() requires 1 argument");
            return NAN;
        }
        if (args[0] > 0) return 1.0;
        if (args[0] < 0) return -1.0;
        return 0.0;
    }

    if (strcmp(name, "step") == 0) {
        if (argc != 2) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "step() requires 2 arguments");
            return NAN;
        }
        return args[1] >= args[0] ? 1.0 : 0.0;
    }

    if (strcmp(name, "smoothstep") == 0) {
        if (argc != 3) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "smoothstep() requires 3 arguments");
            return NAN;
        }
        double edge0 = args[0], edge1 = args[1], x = args[2];
        double t = (x - edge0) / (edge1 - edge0);
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        return t * t * (3 - 2 * t);
    }

    /* ========================================================================
     * Rounding Functions
     * ======================================================================== */

    if (strcmp(name, "floor") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "floor() requires 1 argument");
            return NAN;
        }
        return floor(args[0]);
    }

    if (strcmp(name, "ceil") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "ceil() requires 1 argument");
            return NAN;
        }
        return ceil(args[0]);
    }

    if (strcmp(name, "round") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "round() requires 1 argument");
            return NAN;
        }
        return round(args[0]);
    }

    if (strcmp(name, "trunc") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "trunc() requires 1 argument");
            return NAN;
        }
        return trunc(args[0]);
    }

    /* ========================================================================
     * Power/Logarithm Functions
     * ======================================================================== */

    if (strcmp(name, "sqrt") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "sqrt() requires 1 argument");
            return NAN;
        }
        if (args[0] < 0) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "sqrt() of negative number");
            return NAN;
        }
        return sqrt(args[0]);
    }

    if (strcmp(name, "pow") == 0) {
        if (argc != 2) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "pow() requires 2 arguments");
            return NAN;
        }
        return pow(args[0], args[1]);
    }

    if (strcmp(name, "exp") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "exp() requires 1 argument");
            return NAN;
        }
        return exp(args[0]);
    }

    if (strcmp(name, "log") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "log() requires 1 argument");
            return NAN;
        }
        if (args[0] <= 0) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "log() of non-positive number");
            return NAN;
        }
        return log(args[0]);
    }

    if (strcmp(name, "log2") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "log2() requires 1 argument");
            return NAN;
        }
        if (args[0] <= 0) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "log2() of non-positive number");
            return NAN;
        }
        return log2(args[0]);
    }

    if (strcmp(name, "log10") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "log10() requires 1 argument");
            return NAN;
        }
        if (args[0] <= 0) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "log10() of non-positive number");
            return NAN;
        }
        return log10(args[0]);
    }

    /* ========================================================================
     * Trigonometric Functions
     * ======================================================================== */

    if (strcmp(name, "sin") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "sin() requires 1 argument");
            return NAN;
        }
        return sin(args[0]);
    }

    if (strcmp(name, "cos") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "cos() requires 1 argument");
            return NAN;
        }
        return cos(args[0]);
    }

    if (strcmp(name, "tan") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "tan() requires 1 argument");
            return NAN;
        }
        return tan(args[0]);
    }

    if (strcmp(name, "asin") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "asin() requires 1 argument");
            return NAN;
        }
        if (args[0] < -1 || args[0] > 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "asin() argument out of range [-1, 1]");
            return NAN;
        }
        return asin(args[0]);
    }

    if (strcmp(name, "acos") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "acos() requires 1 argument");
            return NAN;
        }
        if (args[0] < -1 || args[0] > 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "acos() argument out of range [-1, 1]");
            return NAN;
        }
        return acos(args[0]);
    }

    if (strcmp(name, "atan") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "atan() requires 1 argument");
            return NAN;
        }
        return atan(args[0]);
    }

    if (strcmp(name, "atan2") == 0) {
        if (argc != 2) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "atan2() requires 2 arguments");
            return NAN;
        }
        return atan2(args[0], args[1]);
    }

    /* ========================================================================
     * Control Flow
     * ======================================================================== */

    if (strcmp(name, "if") == 0) {
        if (argc != 3) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "if() requires 3 arguments");
            return NAN;
        }
        return args[0] != 0.0 ? args[1] : args[2];
    }

    /* Unknown function */
    snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Unknown function '%s'", name);
    return NAN;
}
