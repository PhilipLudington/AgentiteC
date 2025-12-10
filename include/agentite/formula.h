#ifndef AGENTITE_FORMULA_H
#define AGENTITE_FORMULA_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Carbon Formula Engine
 *
 * Runtime-configurable game balance through expression evaluation.
 * Supports mathematical expressions with variables and built-in functions.
 *
 * Features:
 * - Mathematical operators: +, -, *, /, %, ^ (power)
 * - Comparison operators: ==, !=, <, <=, >, >= (return 1.0 or 0.0)
 * - Logical operators: && (and), || (or), ! (not)
 * - Parentheses for grouping
 * - Named variables with runtime substitution
 * - Built-in functions: min, max, clamp, floor, ceil, round, sqrt, pow, log,
 *                       abs, sin, cos, tan, asin, acos, atan, atan2, exp, lerp
 * - Ternary operator: condition ? true_expr : false_expr
 * - Compiled formulas for repeated evaluation
 *
 * Example usage:
 *   Agentite_FormulaContext *ctx = agentite_formula_create();
 *   agentite_formula_set_var(ctx, "base_damage", 10.0);
 *   agentite_formula_set_var(ctx, "strength", 15.0);
 *   agentite_formula_set_var(ctx, "level", 5.0);
 *
 *   double damage = agentite_formula_eval(ctx, "base_damage + strength * 0.5 + level * 2");
 *   // damage = 10 + 7.5 + 10 = 27.5
 *
 *   // Compiled formula for repeated use
 *   Agentite_Formula *f = agentite_formula_compile(ctx, "clamp(health / max_health, 0, 1)");
 *   for (int i = 0; i < 100; i++) {
 *       agentite_formula_set_var(ctx, "health", entities[i].health);
 *       agentite_formula_set_var(ctx, "max_health", entities[i].max_health);
 *       double ratio = agentite_formula_exec(f, ctx);
 *   }
 *   agentite_formula_free(f);
 *
 *   agentite_formula_destroy(ctx);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_FORMULA_MAX_VARS       64
#define AGENTITE_FORMULA_VAR_NAME_LEN   32
#define AGENTITE_FORMULA_MAX_EXPR_LEN   1024
#define AGENTITE_FORMULA_ERROR_LEN      256

/*============================================================================
 * Types
 *============================================================================*/

/**
 * Formula context - holds variables and evaluation state.
 */
typedef struct Agentite_FormulaContext Agentite_FormulaContext;

/**
 * Compiled formula - pre-parsed for faster repeated evaluation.
 */
typedef struct Agentite_Formula Agentite_Formula;

/**
 * Custom function callback.
 * @param args Array of argument values
 * @param arg_count Number of arguments
 * @param userdata User-provided data
 * @return Function result
 */
typedef double (*Agentite_FormulaFunc)(const double *args, int arg_count, void *userdata);

/*============================================================================
 * Context Management
 *============================================================================*/

/**
 * Create a new formula context.
 * @return New context, or NULL on failure
 */
Agentite_FormulaContext *agentite_formula_create(void);

/**
 * Destroy a formula context.
 * Does not free any compiled formulas - those must be freed separately.
 * @param ctx Context to destroy
 */
void agentite_formula_destroy(Agentite_FormulaContext *ctx);

/**
 * Clone a formula context (copies all variables).
 * @param ctx Context to clone
 * @return New context with same variables, or NULL on failure
 */
Agentite_FormulaContext *agentite_formula_clone(const Agentite_FormulaContext *ctx);

/*============================================================================
 * Variable Management
 *============================================================================*/

/**
 * Set a variable value.
 * @param ctx Formula context
 * @param name Variable name (max 31 chars)
 * @param value Variable value
 * @return true on success, false if name too long or too many variables
 */
bool agentite_formula_set_var(Agentite_FormulaContext *ctx, const char *name, double value);

/**
 * Get a variable value.
 * @param ctx Formula context
 * @param name Variable name
 * @return Variable value, or 0.0 if not found
 */
double agentite_formula_get_var(const Agentite_FormulaContext *ctx, const char *name);

/**
 * Get a variable value with default.
 * @param ctx Formula context
 * @param name Variable name
 * @param default_val Value to return if variable not found
 * @return Variable value, or default_val if not found
 */
double agentite_formula_get_var_or(const Agentite_FormulaContext *ctx, const char *name, double default_val);

/**
 * Check if a variable exists.
 * @param ctx Formula context
 * @param name Variable name
 * @return true if variable exists
 */
bool agentite_formula_has_var(const Agentite_FormulaContext *ctx, const char *name);

/**
 * Remove a variable.
 * @param ctx Formula context
 * @param name Variable name
 * @return true if variable was removed, false if not found
 */
bool agentite_formula_remove_var(Agentite_FormulaContext *ctx, const char *name);

/**
 * Clear all variables.
 * @param ctx Formula context
 */
void agentite_formula_clear_vars(Agentite_FormulaContext *ctx);

/**
 * Get the number of variables.
 * @param ctx Formula context
 * @return Number of variables
 */
int agentite_formula_var_count(const Agentite_FormulaContext *ctx);

/**
 * Get variable name by index (for iteration).
 * @param ctx Formula context
 * @param index Variable index (0 to var_count-1)
 * @return Variable name, or NULL if index out of bounds
 */
const char *agentite_formula_var_name(const Agentite_FormulaContext *ctx, int index);

/**
 * Get variable value by index (for iteration).
 * @param ctx Formula context
 * @param index Variable index (0 to var_count-1)
 * @return Variable value, or 0.0 if index out of bounds
 */
double agentite_formula_var_value(const Agentite_FormulaContext *ctx, int index);

/*============================================================================
 * Custom Functions
 *============================================================================*/

/**
 * Register a custom function.
 * @param ctx Formula context
 * @param name Function name
 * @param func Function callback
 * @param min_args Minimum number of arguments
 * @param max_args Maximum number of arguments (-1 for variadic)
 * @param userdata User data passed to callback
 * @return true on success
 */
bool agentite_formula_register_func(Agentite_FormulaContext *ctx, const char *name,
                                   Agentite_FormulaFunc func, int min_args, int max_args,
                                   void *userdata);

/**
 * Unregister a custom function.
 * @param ctx Formula context
 * @param name Function name
 * @return true if function was removed
 */
bool agentite_formula_unregister_func(Agentite_FormulaContext *ctx, const char *name);

/*============================================================================
 * Expression Evaluation
 *============================================================================*/

/**
 * Evaluate an expression string.
 * @param ctx Formula context with variables
 * @param expression Expression string
 * @return Result of evaluation, or NaN on error
 */
double agentite_formula_eval(Agentite_FormulaContext *ctx, const char *expression);

/**
 * Check if an expression is valid (without evaluating).
 * @param ctx Formula context
 * @param expression Expression string
 * @return true if expression is syntactically valid
 */
bool agentite_formula_valid(Agentite_FormulaContext *ctx, const char *expression);

/**
 * Get the last error message.
 * @param ctx Formula context
 * @return Error message, or empty string if no error
 */
const char *agentite_formula_get_error(const Agentite_FormulaContext *ctx);

/**
 * Clear the last error.
 * @param ctx Formula context
 */
void agentite_formula_clear_error(Agentite_FormulaContext *ctx);

/**
 * Check if last evaluation had an error.
 * @param ctx Formula context
 * @return true if there was an error
 */
bool agentite_formula_has_error(const Agentite_FormulaContext *ctx);

/*============================================================================
 * Compiled Formulas
 *============================================================================*/

/**
 * Compile an expression for repeated evaluation.
 * Compiled formulas are faster for repeated use with different variable values.
 * @param ctx Formula context (used for syntax checking)
 * @param expression Expression string
 * @return Compiled formula, or NULL on error
 */
Agentite_Formula *agentite_formula_compile(Agentite_FormulaContext *ctx, const char *expression);

/**
 * Execute a compiled formula.
 * @param formula Compiled formula
 * @param ctx Formula context with current variable values
 * @return Result of evaluation, or NaN on error
 */
double agentite_formula_exec(Agentite_Formula *formula, Agentite_FormulaContext *ctx);

/**
 * Free a compiled formula.
 * @param formula Formula to free
 */
void agentite_formula_free(Agentite_Formula *formula);

/**
 * Get the original expression string from a compiled formula.
 * @param formula Compiled formula
 * @return Expression string (do not free)
 */
const char *agentite_formula_get_expr(const Agentite_Formula *formula);

/**
 * Get list of variable names used in a compiled formula.
 * @param formula Compiled formula
 * @param out_names Array to fill with variable names
 * @param max_names Maximum number of names to return
 * @return Number of variable names found
 */
int agentite_formula_get_vars(const Agentite_Formula *formula, const char **out_names, int max_names);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Evaluate a simple expression with inline variables.
 * Convenience function for one-off evaluations.
 * Example: agentite_formula_eval_simple("10 + x * 2", "x", 5.0, NULL)
 *
 * @param expression Expression string
 * @param ... Pairs of (const char *name, double value), terminated by NULL
 * @return Result of evaluation, or NaN on error
 */
double agentite_formula_eval_simple(const char *expression, ...);

/**
 * Format a formula result as string.
 * @param value Value to format
 * @param buf Output buffer
 * @param buf_size Buffer size
 * @param precision Decimal precision (-1 for auto)
 * @return Number of characters written
 */
int agentite_formula_format(double value, char *buf, size_t buf_size, int precision);

/**
 * Check if a value is NaN (indicates error).
 * @param value Value to check
 * @return true if value is NaN
 */
bool agentite_formula_is_nan(double value);

/**
 * Check if a value is infinite.
 * @param value Value to check
 * @return true if value is positive or negative infinity
 */
bool agentite_formula_is_inf(double value);

/*============================================================================
 * Built-in Constants
 *============================================================================*/

/**
 * Set common mathematical constants as variables.
 * Adds: pi, e, tau (2*pi), phi (golden ratio)
 * @param ctx Formula context
 */
void agentite_formula_set_constants(Agentite_FormulaContext *ctx);

#endif /* AGENTITE_FORMULA_H */
