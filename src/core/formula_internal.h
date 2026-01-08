/**
 * Agentite Engine - Formula Parser Internal Types
 *
 * Internal header shared between formula_lexer.cpp, formula_builtins.cpp,
 * formula_compiler.cpp, and formula.cpp.
 *
 * This header contains implementation details not exposed in the public API.
 */

#ifndef AGENTITE_FORMULA_INTERNAL_H
#define AGENTITE_FORMULA_INTERNAL_H

#include "agentite/formula.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Maximum recursion depth to prevent stack overflow */
#define AGENTITE_FORMULA_MAX_DEPTH 64

/* Maximum number of custom functions */
#define AGENTITE_FORMULA_MAX_CUSTOM_FUNCS 32

/* Bytecode limits */
#define AGENTITE_FORMULA_MAX_INSTRUCTIONS 256
#define AGENTITE_FORMULA_MAX_STACK 64
#define AGENTITE_FORMULA_MAX_VARS_USED 32

/* ============================================================================
 * Token Types
 * ============================================================================ */

typedef enum {
    TOK_EOF,
    TOK_NUMBER,
    TOK_IDENT,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_CARET,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_QUESTION,
    TOK_COLON,
    TOK_EQ,
    TOK_NE,
    TOK_LT,
    TOK_LE,
    TOK_GT,
    TOK_GE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_ERROR
} TokenType;

/* ============================================================================
 * Token Structure
 * ============================================================================ */

typedef struct {
    TokenType type;
    double number;
    char ident[AGENTITE_FORMULA_VAR_NAME_LEN];
} Token;

/* ============================================================================
 * Parser State
 * ============================================================================ */

typedef struct {
    const char *expr;
    size_t pos;
    Token current;
    Agentite_FormulaContext *ctx;
    bool has_error;
    int depth;
} Parser;

/* ============================================================================
 * Bytecode Opcodes
 * ============================================================================ */

typedef enum {
    OP_PUSH_NUM,
    OP_PUSH_VAR,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_POW,
    OP_NEG,
    OP_NOT,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_AND,
    OP_OR,
    OP_TERNARY,
    OP_CALL
} OpCode;

/* ============================================================================
 * Bytecode Instruction
 * ============================================================================ */

typedef struct {
    OpCode op;
    union {
        double num;
        char var_name[AGENTITE_FORMULA_VAR_NAME_LEN];
        struct {
            char func_name[AGENTITE_FORMULA_VAR_NAME_LEN];
            int arg_count;
        } call;
    } data;
} Instruction;

/* ============================================================================
 * Compiled Formula Structure
 *
 * Full definition of the opaque Agentite_Formula type.
 * ============================================================================ */

struct Agentite_Formula {
    char expr[AGENTITE_FORMULA_MAX_EXPR_LEN];
    Instruction code[AGENTITE_FORMULA_MAX_INSTRUCTIONS];
    int code_len;
    char vars_used[AGENTITE_FORMULA_MAX_VARS_USED][AGENTITE_FORMULA_VAR_NAME_LEN];
    int vars_used_count;
};

/* ============================================================================
 * Internal Variable Storage
 * ============================================================================ */

typedef struct {
    char name[AGENTITE_FORMULA_VAR_NAME_LEN];
    double value;
} FormulaVar;

/* ============================================================================
 * Custom Function Storage
 * ============================================================================ */

typedef struct {
    char name[AGENTITE_FORMULA_VAR_NAME_LEN];
    Agentite_FormulaFunc func;
    int min_args;
    int max_args;
    void *userdata;
} FormulaCustomFunc;

/* ============================================================================
 * Context Structure
 *
 * Full definition of the opaque Agentite_FormulaContext type.
 * ============================================================================ */

/* Forward declaration for profiler */
struct Agentite_Profiler;

struct Agentite_FormulaContext {
    FormulaVar vars[AGENTITE_FORMULA_MAX_VARS];
    int var_count;
    FormulaCustomFunc custom_funcs[AGENTITE_FORMULA_MAX_CUSTOM_FUNCS];
    int custom_func_count;
    char error[AGENTITE_FORMULA_ERROR_LEN];
    struct Agentite_Profiler *profiler;  /* Optional profiler for performance tracking */
};

/* ============================================================================
 * Lexer Functions (formula_lexer.cpp)
 * ============================================================================ */

/**
 * Advance to the next token in the input.
 * Updates p->current with the new token.
 * Sets p->has_error and writes to p->ctx->error on lexer errors.
 */
void formula_next_token(Parser *p);

/**
 * Parse a complete expression starting from the current token.
 * This is the entry point for the recursive descent parser.
 *
 * Operator precedence (lowest to highest):
 *   1. Ternary (?:)
 *   2. Logical OR (||)
 *   3. Logical AND (&&)
 *   4. Equality (==, !=)
 *   5. Comparison (<, <=, >, >=)
 *   6. Additive (+, -)
 *   7. Multiplicative (*, /, %)
 *   8. Unary (!, -, +)
 *   9. Power (^) - right associative
 *  10. Primary (numbers, variables, function calls, parentheses)
 */
double formula_parse_expression(Parser *p);

/* ============================================================================
 * Built-in Functions (formula_builtins.cpp)
 * ============================================================================ */

/**
 * Call a built-in or custom function by name.
 *
 * Built-in functions:
 *   Math: min, max, clamp, lerp, abs, sign, step, smoothstep
 *   Rounding: floor, ceil, round, trunc
 *   Power/Log: sqrt, pow, exp, log, log2, log10
 *   Trig: sin, cos, tan, asin, acos, atan, atan2
 *   Control: if
 *
 * Custom functions registered via agentite_formula_register_func() are
 * checked first, allowing overrides of built-in names.
 *
 * @param name Function name to call
 * @param args Array of argument values
 * @param argc Number of arguments
 * @param ctx Formula context (for custom functions and error reporting)
 * @return Function result, or NAN on error (with error message in ctx->error)
 */
double formula_call_builtin(const char *name, double *args, int argc, Agentite_FormulaContext *ctx);

/* ============================================================================
 * Compiler Functions (formula_compiler.cpp)
 * ============================================================================ */

/* Compiler functions are declared in the public header (formula.h):
 *   agentite_formula_compile()
 *   agentite_formula_exec()
 *   agentite_formula_free()
 *   agentite_formula_get_expr()
 *   agentite_formula_get_vars()
 */

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_FORMULA_INTERNAL_H */
