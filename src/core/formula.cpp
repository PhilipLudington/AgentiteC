/**
 * Agentite Engine - Formula Expression Engine
 *
 * Runtime-configurable game balance through expression evaluation.
 * Supports mathematical expressions with variables and built-in functions.
 *
 * This file contains the public API implementation:
 * - Context management (create, destroy, clone)
 * - Variable management (set, get, remove, clear)
 * - Custom function registration
 * - Expression evaluation entry point
 * - Utility functions
 *
 * See also:
 * - formula_internal.h  - Shared types and function declarations
 * - formula_lexer.cpp   - Tokenizer and recursive descent parser
 * - formula_builtins.cpp - Built-in function implementations
 * - formula_compiler.cpp - Bytecode compiler and VM
 */

#include "formula_internal.h"
#include "agentite/agentite.h"
#include "agentite/profiler.h"
#include "agentite/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>

/* ============================================================================
 * Context Management
 * ============================================================================ */

Agentite_FormulaContext *agentite_formula_create(void) {
    Agentite_FormulaContext *ctx = AGENTITE_ALLOC(Agentite_FormulaContext);
    if (!ctx) {
        agentite_set_error("Failed to allocate formula context");
        return NULL;
    }
    memset(ctx, 0, sizeof(Agentite_FormulaContext));
    return ctx;
}

void agentite_formula_destroy(Agentite_FormulaContext *ctx) {
    free(ctx);
}

void agentite_formula_set_profiler(Agentite_FormulaContext *ctx, Agentite_Profiler *profiler) {
    if (ctx) {
        ctx->profiler = profiler;
    }
}

Agentite_FormulaContext *agentite_formula_clone(const Agentite_FormulaContext *ctx) {
    if (!ctx) return NULL;

    Agentite_FormulaContext *clone = AGENTITE_ALLOC(Agentite_FormulaContext);
    if (!clone) {
        agentite_set_error("Failed to allocate formula context");
        return NULL;
    }

    memcpy(clone, ctx, sizeof(Agentite_FormulaContext));
    clone->error[0] = '\0';
    return clone;
}

/* ============================================================================
 * Variable Management
 * ============================================================================ */

bool agentite_formula_set_var(Agentite_FormulaContext *ctx, const char *name, double value) {
    if (!ctx || !name) return false;

    size_t len = strlen(name);
    if (len == 0 || len >= AGENTITE_FORMULA_VAR_NAME_LEN) {
        agentite_set_error("Variable name too long or empty");
        return false;
    }

    /* Update existing variable */
    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            ctx->vars[i].value = value;
            return true;
        }
    }

    /* Add new variable */
    if (ctx->var_count >= AGENTITE_FORMULA_MAX_VARS) {
        agentite_set_error("Formula: Maximum variables exceeded (%d/%d) when adding '%s'",
                          ctx->var_count, AGENTITE_FORMULA_MAX_VARS, name);
        return false;
    }

    strncpy(ctx->vars[ctx->var_count].name, name, AGENTITE_FORMULA_VAR_NAME_LEN - 1);
    ctx->vars[ctx->var_count].name[AGENTITE_FORMULA_VAR_NAME_LEN - 1] = '\0';
    ctx->vars[ctx->var_count].value = value;
    ctx->var_count++;

    return true;
}

double agentite_formula_get_var(const Agentite_FormulaContext *ctx, const char *name) {
    return agentite_formula_get_var_or(ctx, name, 0.0);
}

double agentite_formula_get_var_or(const Agentite_FormulaContext *ctx, const char *name, double default_val) {
    if (!ctx || !name) return default_val;

    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            return ctx->vars[i].value;
        }
    }

    return default_val;
}

bool agentite_formula_has_var(const Agentite_FormulaContext *ctx, const char *name) {
    if (!ctx || !name) return false;

    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            return true;
        }
    }

    return false;
}

bool agentite_formula_remove_var(Agentite_FormulaContext *ctx, const char *name) {
    if (!ctx || !name) return false;

    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            /* Shift remaining variables down */
            for (int j = i; j < ctx->var_count - 1; j++) {
                ctx->vars[j] = ctx->vars[j + 1];
            }
            ctx->var_count--;
            return true;
        }
    }

    return false;
}

void agentite_formula_clear_vars(Agentite_FormulaContext *ctx) {
    if (!ctx) return;
    ctx->var_count = 0;
}

int agentite_formula_var_count(const Agentite_FormulaContext *ctx) {
    return ctx ? ctx->var_count : 0;
}

const char *agentite_formula_var_name(const Agentite_FormulaContext *ctx, int index) {
    if (!ctx || index < 0 || index >= ctx->var_count) return NULL;
    return ctx->vars[index].name;
}

double agentite_formula_var_value(const Agentite_FormulaContext *ctx, int index) {
    if (!ctx || index < 0 || index >= ctx->var_count) return 0.0;
    return ctx->vars[index].value;
}

/* ============================================================================
 * Custom Functions
 * ============================================================================ */

bool agentite_formula_register_func(Agentite_FormulaContext *ctx, const char *name,
                                   Agentite_FormulaFunc func, int min_args, int max_args,
                                   void *userdata) {
    if (!ctx || !name || !func) return false;

    if (ctx->custom_func_count >= AGENTITE_FORMULA_MAX_CUSTOM_FUNCS) {
        agentite_set_error("Formula: Maximum custom functions exceeded (%d/%d) when adding '%s'",
                          ctx->custom_func_count, AGENTITE_FORMULA_MAX_CUSTOM_FUNCS, name);
        return false;
    }

    /* Check for existing function with same name (update it) */
    for (int i = 0; i < ctx->custom_func_count; i++) {
        if (strcmp(ctx->custom_funcs[i].name, name) == 0) {
            ctx->custom_funcs[i].func = func;
            ctx->custom_funcs[i].min_args = min_args;
            ctx->custom_funcs[i].max_args = max_args;
            ctx->custom_funcs[i].userdata = userdata;
            return true;
        }
    }

    /* Add new function */
    FormulaCustomFunc *f = &ctx->custom_funcs[ctx->custom_func_count];
    strncpy(f->name, name, AGENTITE_FORMULA_VAR_NAME_LEN - 1);
    f->name[AGENTITE_FORMULA_VAR_NAME_LEN - 1] = '\0';
    f->func = func;
    f->min_args = min_args;
    f->max_args = max_args;
    f->userdata = userdata;
    ctx->custom_func_count++;

    return true;
}

bool agentite_formula_unregister_func(Agentite_FormulaContext *ctx, const char *name) {
    if (!ctx || !name) return false;

    for (int i = 0; i < ctx->custom_func_count; i++) {
        if (strcmp(ctx->custom_funcs[i].name, name) == 0) {
            /* Shift remaining functions down */
            for (int j = i; j < ctx->custom_func_count - 1; j++) {
                ctx->custom_funcs[j] = ctx->custom_funcs[j + 1];
            }
            ctx->custom_func_count--;
            return true;
        }
    }

    return false;
}

/* ============================================================================
 * Expression Evaluation
 * ============================================================================ */

double agentite_formula_eval(Agentite_FormulaContext *ctx, const char *expression) {
    if (!ctx || !expression) {
        if (ctx) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "NULL expression");
        }
        return NAN;
    }

    /* Profile formula evaluation if profiler is set */
    if (ctx->profiler) {
        agentite_profiler_begin_scope(ctx->profiler, "formula_eval");
    }

    ctx->error[0] = '\0';

    Parser p = {
        .expr = expression,
        .pos = 0,
        .ctx = ctx,
        .has_error = false,
        .depth = 0
    };

    formula_next_token(&p);
    if (p.has_error) {
        if (ctx->profiler) {
            agentite_profiler_end_scope(ctx->profiler);
        }
        return NAN;
    }

    double result = formula_parse_expression(&p);

    if (!p.has_error && p.current.type != TOK_EOF) {
        snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                 "Unexpected content after expression at position %zu", p.pos);
        if (ctx->profiler) {
            agentite_profiler_end_scope(ctx->profiler);
        }
        return NAN;
    }

    if (ctx->profiler) {
        agentite_profiler_end_scope(ctx->profiler);
    }
    return result;
}

bool agentite_formula_valid(Agentite_FormulaContext *ctx, const char *expression) {
    if (!ctx || !expression) return false;

    /* Save current error state */
    char saved_error[AGENTITE_FORMULA_ERROR_LEN];
    memcpy(saved_error, ctx->error, AGENTITE_FORMULA_ERROR_LEN);

    double result = agentite_formula_eval(ctx, expression);
    bool valid = !isnan(result) || ctx->error[0] == '\0';

    /* Restore error state */
    memcpy(ctx->error, saved_error, AGENTITE_FORMULA_ERROR_LEN);

    return valid;
}

const char *agentite_formula_get_error(const Agentite_FormulaContext *ctx) {
    if (!ctx) return "";
    return ctx->error;
}

void agentite_formula_clear_error(Agentite_FormulaContext *ctx) {
    if (ctx) {
        ctx->error[0] = '\0';
    }
}

bool agentite_formula_has_error(const Agentite_FormulaContext *ctx) {
    return ctx && ctx->error[0] != '\0';
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

double agentite_formula_eval_simple(const char *expression, ...) {
    Agentite_FormulaContext *ctx = agentite_formula_create();
    if (!ctx) return NAN;

    va_list args;
    va_start(args, expression);

    const char *name;
    while ((name = va_arg(args, const char *)) != NULL) {
        double value = va_arg(args, double);
        agentite_formula_set_var(ctx, name, value);
    }

    va_end(args);

    double result = agentite_formula_eval(ctx, expression);
    agentite_formula_destroy(ctx);

    return result;
}

int agentite_formula_format(double value, char *buf, size_t buf_size, int precision) {
    if (!buf || buf_size == 0) return 0;

    if (isnan(value)) {
        return snprintf(buf, buf_size, "NaN");
    }
    if (isinf(value)) {
        return snprintf(buf, buf_size, value > 0 ? "Inf" : "-Inf");
    }

    if (precision < 0) {
        /* Auto precision - show up to 6 decimals, trim trailing zeros */
        int len = snprintf(buf, buf_size, "%.6f", value);
        if (len < 0 || (size_t)len >= buf_size) return len;

        /* Trim trailing zeros */
        char *dot = strchr(buf, '.');
        if (dot) {
            char *end = buf + len - 1;
            while (end > dot && *end == '0') {
                *end-- = '\0';
                len--;
            }
            if (end == dot) {
                *end = '\0';
                len--;
            }
        }
        return len;
    }

    return snprintf(buf, buf_size, "%.*f", precision, value);
}

bool agentite_formula_is_nan(double value) {
    return isnan(value);
}

bool agentite_formula_is_inf(double value) {
    return isinf(value);
}

void agentite_formula_set_constants(Agentite_FormulaContext *ctx) {
    if (!ctx) return;

    agentite_formula_set_var(ctx, "pi", 3.14159265358979323846);
    agentite_formula_set_var(ctx, "e", 2.71828182845904523536);
    agentite_formula_set_var(ctx, "tau", 6.28318530717958647692);  /* 2*pi */
    agentite_formula_set_var(ctx, "phi", 1.61803398874989484820);  /* Golden ratio */
}
