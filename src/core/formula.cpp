#include "carbon/carbon.h"
#include "carbon/formula.h"
#include "carbon/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>

/*============================================================================
 * Internal Types
 *============================================================================*/

typedef struct {
    char name[CARBON_FORMULA_VAR_NAME_LEN];
    double value;
} FormulaVar;

typedef struct {
    char name[CARBON_FORMULA_VAR_NAME_LEN];
    Carbon_FormulaFunc func;
    int min_args;
    int max_args;
    void *userdata;
} FormulaCustomFunc;

#define CARBON_FORMULA_MAX_CUSTOM_FUNCS 32

struct Carbon_FormulaContext {
    FormulaVar vars[CARBON_FORMULA_MAX_VARS];
    int var_count;
    FormulaCustomFunc custom_funcs[CARBON_FORMULA_MAX_CUSTOM_FUNCS];
    int custom_func_count;
    char error[CARBON_FORMULA_ERROR_LEN];
};

/* Token types for lexer */
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

typedef struct {
    TokenType type;
    double number;
    char ident[CARBON_FORMULA_VAR_NAME_LEN];
} Token;

typedef struct {
    const char *expr;
    size_t pos;
    Token current;
    Carbon_FormulaContext *ctx;
    bool has_error;
} Parser;

/* Compiled formula bytecode */
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

typedef struct {
    OpCode op;
    union {
        double num;
        char var_name[CARBON_FORMULA_VAR_NAME_LEN];
        struct {
            char func_name[CARBON_FORMULA_VAR_NAME_LEN];
            int arg_count;
        } call;
    } data;
} Instruction;

#define CARBON_FORMULA_MAX_INSTRUCTIONS 256
#define CARBON_FORMULA_MAX_STACK 64
#define CARBON_FORMULA_MAX_VARS_USED 32

struct Carbon_Formula {
    char expr[CARBON_FORMULA_MAX_EXPR_LEN];
    Instruction code[CARBON_FORMULA_MAX_INSTRUCTIONS];
    int code_len;
    char vars_used[CARBON_FORMULA_MAX_VARS_USED][CARBON_FORMULA_VAR_NAME_LEN];
    int vars_used_count;
};

/*============================================================================
 * Forward Declarations
 *============================================================================*/

static void next_token(Parser *p);
static double parse_expression(Parser *p);
static double parse_ternary(Parser *p);
static double parse_or(Parser *p);
static double parse_and(Parser *p);
static double parse_equality(Parser *p);
static double parse_comparison(Parser *p);
static double parse_additive(Parser *p);
static double parse_multiplicative(Parser *p);
static double parse_unary(Parser *p);
static double parse_power(Parser *p);
static double parse_primary(Parser *p);
static double call_builtin(const char *name, double *args, int argc, Carbon_FormulaContext *ctx);

/*============================================================================
 * Lexer
 *============================================================================*/

static void skip_whitespace(Parser *p) {
    while (p->expr[p->pos] && isspace((unsigned char)p->expr[p->pos])) {
        p->pos++;
    }
}

static void next_token(Parser *p) {
    skip_whitespace(p);

    if (p->expr[p->pos] == '\0') {
        p->current.type = TOK_EOF;
        return;
    }

    char c = p->expr[p->pos];

    /* Number */
    if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)p->expr[p->pos + 1]))) {
        char *end;
        p->current.number = strtod(&p->expr[p->pos], &end);
        p->pos = end - p->expr;
        p->current.type = TOK_NUMBER;
        return;
    }

    /* Identifier or keyword */
    if (isalpha((unsigned char)c) || c == '_') {
        size_t start = p->pos;
        while (isalnum((unsigned char)p->expr[p->pos]) || p->expr[p->pos] == '_') {
            p->pos++;
        }
        size_t len = p->pos - start;
        if (len >= CARBON_FORMULA_VAR_NAME_LEN) {
            len = CARBON_FORMULA_VAR_NAME_LEN - 1;
        }
        memcpy(p->current.ident, &p->expr[start], len);
        p->current.ident[len] = '\0';
        p->current.type = TOK_IDENT;
        return;
    }

    /* Operators */
    p->pos++;
    switch (c) {
        case '+': p->current.type = TOK_PLUS; break;
        case '-': p->current.type = TOK_MINUS; break;
        case '*': p->current.type = TOK_STAR; break;
        case '/': p->current.type = TOK_SLASH; break;
        case '%': p->current.type = TOK_PERCENT; break;
        case '^': p->current.type = TOK_CARET; break;
        case '(': p->current.type = TOK_LPAREN; break;
        case ')': p->current.type = TOK_RPAREN; break;
        case ',': p->current.type = TOK_COMMA; break;
        case '?': p->current.type = TOK_QUESTION; break;
        case ':': p->current.type = TOK_COLON; break;
        case '!':
            if (p->expr[p->pos] == '=') {
                p->pos++;
                p->current.type = TOK_NE;
            } else {
                p->current.type = TOK_NOT;
            }
            break;
        case '=':
            if (p->expr[p->pos] == '=') {
                p->pos++;
                p->current.type = TOK_EQ;
            } else {
                p->current.type = TOK_ERROR;
                snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
                         "Expected '==' at position %zu", p->pos);
                p->has_error = true;
            }
            break;
        case '<':
            if (p->expr[p->pos] == '=') {
                p->pos++;
                p->current.type = TOK_LE;
            } else {
                p->current.type = TOK_LT;
            }
            break;
        case '>':
            if (p->expr[p->pos] == '=') {
                p->pos++;
                p->current.type = TOK_GE;
            } else {
                p->current.type = TOK_GT;
            }
            break;
        case '&':
            if (p->expr[p->pos] == '&') {
                p->pos++;
                p->current.type = TOK_AND;
            } else {
                p->current.type = TOK_ERROR;
                snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
                         "Expected '&&' at position %zu", p->pos);
                p->has_error = true;
            }
            break;
        case '|':
            if (p->expr[p->pos] == '|') {
                p->pos++;
                p->current.type = TOK_OR;
            } else {
                p->current.type = TOK_ERROR;
                snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
                         "Expected '||' at position %zu", p->pos);
                p->has_error = true;
            }
            break;
        default:
            p->current.type = TOK_ERROR;
            snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
                     "Unexpected character '%c' at position %zu", c, p->pos - 1);
            p->has_error = true;
            break;
    }
}

/*============================================================================
 * Parser - Recursive Descent
 *============================================================================*/

static double parse_expression(Parser *p) {
    return parse_ternary(p);
}

static double parse_ternary(Parser *p) {
    double cond = parse_or(p);
    if (p->has_error) return NAN;

    if (p->current.type == TOK_QUESTION) {
        next_token(p);
        double true_val = parse_expression(p);
        if (p->has_error) return NAN;

        if (p->current.type != TOK_COLON) {
            snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
                     "Expected ':' in ternary expression");
            p->has_error = true;
            return NAN;
        }
        next_token(p);
        double false_val = parse_expression(p);
        if (p->has_error) return NAN;

        return cond != 0.0 ? true_val : false_val;
    }

    return cond;
}

static double parse_or(Parser *p) {
    double left = parse_and(p);
    if (p->has_error) return NAN;

    while (p->current.type == TOK_OR) {
        next_token(p);
        double right = parse_and(p);
        if (p->has_error) return NAN;
        left = (left != 0.0 || right != 0.0) ? 1.0 : 0.0;
    }

    return left;
}

static double parse_and(Parser *p) {
    double left = parse_equality(p);
    if (p->has_error) return NAN;

    while (p->current.type == TOK_AND) {
        next_token(p);
        double right = parse_equality(p);
        if (p->has_error) return NAN;
        left = (left != 0.0 && right != 0.0) ? 1.0 : 0.0;
    }

    return left;
}

static double parse_equality(Parser *p) {
    double left = parse_comparison(p);
    if (p->has_error) return NAN;

    while (p->current.type == TOK_EQ || p->current.type == TOK_NE) {
        TokenType op = p->current.type;
        next_token(p);
        double right = parse_comparison(p);
        if (p->has_error) return NAN;

        if (op == TOK_EQ) {
            left = (left == right) ? 1.0 : 0.0;
        } else {
            left = (left != right) ? 1.0 : 0.0;
        }
    }

    return left;
}

static double parse_comparison(Parser *p) {
    double left = parse_additive(p);
    if (p->has_error) return NAN;

    while (p->current.type == TOK_LT || p->current.type == TOK_LE ||
           p->current.type == TOK_GT || p->current.type == TOK_GE) {
        TokenType op = p->current.type;
        next_token(p);
        double right = parse_additive(p);
        if (p->has_error) return NAN;

        switch (op) {
            case TOK_LT: left = (left < right) ? 1.0 : 0.0; break;
            case TOK_LE: left = (left <= right) ? 1.0 : 0.0; break;
            case TOK_GT: left = (left > right) ? 1.0 : 0.0; break;
            case TOK_GE: left = (left >= right) ? 1.0 : 0.0; break;
            default: break;
        }
    }

    return left;
}

static double parse_additive(Parser *p) {
    double left = parse_multiplicative(p);
    if (p->has_error) return NAN;

    while (p->current.type == TOK_PLUS || p->current.type == TOK_MINUS) {
        TokenType op = p->current.type;
        next_token(p);
        double right = parse_multiplicative(p);
        if (p->has_error) return NAN;

        if (op == TOK_PLUS) {
            left += right;
        } else {
            left -= right;
        }
    }

    return left;
}

static double parse_multiplicative(Parser *p) {
    double left = parse_unary(p);
    if (p->has_error) return NAN;

    while (p->current.type == TOK_STAR || p->current.type == TOK_SLASH ||
           p->current.type == TOK_PERCENT) {
        TokenType op = p->current.type;
        next_token(p);
        double right = parse_unary(p);
        if (p->has_error) return NAN;

        switch (op) {
            case TOK_STAR: left *= right; break;
            case TOK_SLASH:
                if (right == 0.0) {
                    snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN, "Division by zero");
                    p->has_error = true;
                    return NAN;
                }
                left /= right;
                break;
            case TOK_PERCENT:
                if (right == 0.0) {
                    snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN, "Modulo by zero");
                    p->has_error = true;
                    return NAN;
                }
                left = fmod(left, right);
                break;
            default: break;
        }
    }

    return left;
}

static double parse_unary(Parser *p) {
    if (p->current.type == TOK_MINUS) {
        next_token(p);
        return -parse_unary(p);
    }
    if (p->current.type == TOK_PLUS) {
        next_token(p);
        return parse_unary(p);
    }
    if (p->current.type == TOK_NOT) {
        next_token(p);
        double val = parse_unary(p);
        return (val == 0.0) ? 1.0 : 0.0;
    }
    return parse_power(p);
}

static double parse_power(Parser *p) {
    double base = parse_primary(p);
    if (p->has_error) return NAN;

    if (p->current.type == TOK_CARET) {
        next_token(p);
        double exp = parse_unary(p);  /* Right-associative */
        if (p->has_error) return NAN;
        return pow(base, exp);
    }

    return base;
}

static double parse_primary(Parser *p) {
    if (p->current.type == TOK_NUMBER) {
        double val = p->current.number;
        next_token(p);
        return val;
    }

    if (p->current.type == TOK_IDENT) {
        char name[CARBON_FORMULA_VAR_NAME_LEN];
        strncpy(name, p->current.ident, CARBON_FORMULA_VAR_NAME_LEN - 1);
        name[CARBON_FORMULA_VAR_NAME_LEN - 1] = '\0';
        next_token(p);

        /* Function call */
        if (p->current.type == TOK_LPAREN) {
            next_token(p);
            double args[16];
            int argc = 0;

            if (p->current.type != TOK_RPAREN) {
                do {
                    if (argc >= 16) {
                        snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
                                 "Too many arguments to function '%s'", name);
                        p->has_error = true;
                        return NAN;
                    }
                    args[argc++] = parse_expression(p);
                    if (p->has_error) return NAN;
                } while (p->current.type == TOK_COMMA && (next_token(p), 1));
            }

            if (p->current.type != TOK_RPAREN) {
                snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
                         "Expected ')' after function arguments");
                p->has_error = true;
                return NAN;
            }
            next_token(p);

            return call_builtin(name, args, argc, p->ctx);
        }

        /* Variable lookup */
        for (int i = 0; i < p->ctx->var_count; i++) {
            if (strcmp(p->ctx->vars[i].name, name) == 0) {
                return p->ctx->vars[i].value;
            }
        }

        snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
                 "Unknown variable '%s'", name);
        p->has_error = true;
        return NAN;
    }

    if (p->current.type == TOK_LPAREN) {
        next_token(p);
        double val = parse_expression(p);
        if (p->has_error) return NAN;

        if (p->current.type != TOK_RPAREN) {
            snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
                     "Expected closing parenthesis");
            p->has_error = true;
            return NAN;
        }
        next_token(p);
        return val;
    }

    snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
             "Unexpected token at position %zu", p->pos);
    p->has_error = true;
    return NAN;
}

/*============================================================================
 * Built-in Functions
 *============================================================================*/

static double call_builtin(const char *name, double *args, int argc, Carbon_FormulaContext *ctx) {
    /* Check custom functions first */
    for (int i = 0; i < ctx->custom_func_count; i++) {
        if (strcmp(ctx->custom_funcs[i].name, name) == 0) {
            FormulaCustomFunc *f = &ctx->custom_funcs[i];
            if (argc < f->min_args) {
                snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN,
                         "Function '%s' requires at least %d arguments", name, f->min_args);
                return NAN;
            }
            if (f->max_args >= 0 && argc > f->max_args) {
                snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN,
                         "Function '%s' accepts at most %d arguments", name, f->max_args);
                return NAN;
            }
            return f->func(args, argc, f->userdata);
        }
    }

    /* Built-in functions */
    if (strcmp(name, "min") == 0) {
        if (argc < 2) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "min() requires at least 2 arguments");
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
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "max() requires at least 2 arguments");
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
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "clamp() requires 3 arguments");
            return NAN;
        }
        double val = args[0], lo = args[1], hi = args[2];
        if (val < lo) return lo;
        if (val > hi) return hi;
        return val;
    }

    if (strcmp(name, "lerp") == 0) {
        if (argc != 3) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "lerp() requires 3 arguments");
            return NAN;
        }
        return args[0] + (args[1] - args[0]) * args[2];
    }

    if (strcmp(name, "floor") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "floor() requires 1 argument");
            return NAN;
        }
        return floor(args[0]);
    }

    if (strcmp(name, "ceil") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "ceil() requires 1 argument");
            return NAN;
        }
        return ceil(args[0]);
    }

    if (strcmp(name, "round") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "round() requires 1 argument");
            return NAN;
        }
        return round(args[0]);
    }

    if (strcmp(name, "trunc") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "trunc() requires 1 argument");
            return NAN;
        }
        return trunc(args[0]);
    }

    if (strcmp(name, "abs") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "abs() requires 1 argument");
            return NAN;
        }
        return fabs(args[0]);
    }

    if (strcmp(name, "sqrt") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "sqrt() requires 1 argument");
            return NAN;
        }
        if (args[0] < 0) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "sqrt() of negative number");
            return NAN;
        }
        return sqrt(args[0]);
    }

    if (strcmp(name, "pow") == 0) {
        if (argc != 2) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "pow() requires 2 arguments");
            return NAN;
        }
        return pow(args[0], args[1]);
    }

    if (strcmp(name, "log") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "log() requires 1 argument");
            return NAN;
        }
        if (args[0] <= 0) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "log() of non-positive number");
            return NAN;
        }
        return log(args[0]);
    }

    if (strcmp(name, "log10") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "log10() requires 1 argument");
            return NAN;
        }
        if (args[0] <= 0) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "log10() of non-positive number");
            return NAN;
        }
        return log10(args[0]);
    }

    if (strcmp(name, "log2") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "log2() requires 1 argument");
            return NAN;
        }
        if (args[0] <= 0) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "log2() of non-positive number");
            return NAN;
        }
        return log2(args[0]);
    }

    if (strcmp(name, "exp") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "exp() requires 1 argument");
            return NAN;
        }
        return exp(args[0]);
    }

    if (strcmp(name, "sin") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "sin() requires 1 argument");
            return NAN;
        }
        return sin(args[0]);
    }

    if (strcmp(name, "cos") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "cos() requires 1 argument");
            return NAN;
        }
        return cos(args[0]);
    }

    if (strcmp(name, "tan") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "tan() requires 1 argument");
            return NAN;
        }
        return tan(args[0]);
    }

    if (strcmp(name, "asin") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "asin() requires 1 argument");
            return NAN;
        }
        if (args[0] < -1 || args[0] > 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "asin() argument out of range [-1, 1]");
            return NAN;
        }
        return asin(args[0]);
    }

    if (strcmp(name, "acos") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "acos() requires 1 argument");
            return NAN;
        }
        if (args[0] < -1 || args[0] > 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "acos() argument out of range [-1, 1]");
            return NAN;
        }
        return acos(args[0]);
    }

    if (strcmp(name, "atan") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "atan() requires 1 argument");
            return NAN;
        }
        return atan(args[0]);
    }

    if (strcmp(name, "atan2") == 0) {
        if (argc != 2) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "atan2() requires 2 arguments");
            return NAN;
        }
        return atan2(args[0], args[1]);
    }

    if (strcmp(name, "sign") == 0) {
        if (argc != 1) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "sign() requires 1 argument");
            return NAN;
        }
        if (args[0] > 0) return 1.0;
        if (args[0] < 0) return -1.0;
        return 0.0;
    }

    if (strcmp(name, "step") == 0) {
        if (argc != 2) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "step() requires 2 arguments");
            return NAN;
        }
        return args[1] >= args[0] ? 1.0 : 0.0;
    }

    if (strcmp(name, "smoothstep") == 0) {
        if (argc != 3) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "smoothstep() requires 3 arguments");
            return NAN;
        }
        double edge0 = args[0], edge1 = args[1], x = args[2];
        double t = (x - edge0) / (edge1 - edge0);
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        return t * t * (3 - 2 * t);
    }

    if (strcmp(name, "if") == 0) {
        if (argc != 3) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "if() requires 3 arguments");
            return NAN;
        }
        return args[0] != 0.0 ? args[1] : args[2];
    }

    snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Unknown function '%s'", name);
    return NAN;
}

/*============================================================================
 * Context Management
 *============================================================================*/

Carbon_FormulaContext *carbon_formula_create(void) {
    Carbon_FormulaContext *ctx = CARBON_ALLOC(Carbon_FormulaContext);
    if (!ctx) {
        carbon_set_error("Failed to allocate formula context");
        return NULL;
    }
    return ctx;
}

void carbon_formula_destroy(Carbon_FormulaContext *ctx) {
    free(ctx);
}

Carbon_FormulaContext *carbon_formula_clone(const Carbon_FormulaContext *ctx) {
    if (!ctx) return NULL;

    Carbon_FormulaContext *clone = CARBON_ALLOC(Carbon_FormulaContext);
    if (!clone) {
        carbon_set_error("Failed to allocate formula context");
        return NULL;
    }

    memcpy(clone, ctx, sizeof(Carbon_FormulaContext));
    clone->error[0] = '\0';
    return clone;
}

/*============================================================================
 * Variable Management
 *============================================================================*/

bool carbon_formula_set_var(Carbon_FormulaContext *ctx, const char *name, double value) {
    if (!ctx || !name) return false;

    size_t len = strlen(name);
    if (len == 0 || len >= CARBON_FORMULA_VAR_NAME_LEN) {
        carbon_set_error("Variable name too long or empty");
        return false;
    }

    /* Update existing */
    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            ctx->vars[i].value = value;
            return true;
        }
    }

    /* Add new */
    if (ctx->var_count >= CARBON_FORMULA_MAX_VARS) {
        carbon_set_error("Maximum variables exceeded");
        return false;
    }

    strncpy(ctx->vars[ctx->var_count].name, name, CARBON_FORMULA_VAR_NAME_LEN - 1);
    ctx->vars[ctx->var_count].name[CARBON_FORMULA_VAR_NAME_LEN - 1] = '\0';
    ctx->vars[ctx->var_count].value = value;
    ctx->var_count++;

    return true;
}

double carbon_formula_get_var(const Carbon_FormulaContext *ctx, const char *name) {
    return carbon_formula_get_var_or(ctx, name, 0.0);
}

double carbon_formula_get_var_or(const Carbon_FormulaContext *ctx, const char *name, double default_val) {
    if (!ctx || !name) return default_val;

    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            return ctx->vars[i].value;
        }
    }

    return default_val;
}

bool carbon_formula_has_var(const Carbon_FormulaContext *ctx, const char *name) {
    if (!ctx || !name) return false;

    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            return true;
        }
    }

    return false;
}

bool carbon_formula_remove_var(Carbon_FormulaContext *ctx, const char *name) {
    if (!ctx || !name) return false;

    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            /* Shift remaining */
            for (int j = i; j < ctx->var_count - 1; j++) {
                ctx->vars[j] = ctx->vars[j + 1];
            }
            ctx->var_count--;
            return true;
        }
    }

    return false;
}

void carbon_formula_clear_vars(Carbon_FormulaContext *ctx) {
    if (!ctx) return;
    ctx->var_count = 0;
}

int carbon_formula_var_count(const Carbon_FormulaContext *ctx) {
    return ctx ? ctx->var_count : 0;
}

const char *carbon_formula_var_name(const Carbon_FormulaContext *ctx, int index) {
    if (!ctx || index < 0 || index >= ctx->var_count) return NULL;
    return ctx->vars[index].name;
}

double carbon_formula_var_value(const Carbon_FormulaContext *ctx, int index) {
    if (!ctx || index < 0 || index >= ctx->var_count) return 0.0;
    return ctx->vars[index].value;
}

/*============================================================================
 * Custom Functions
 *============================================================================*/

bool carbon_formula_register_func(Carbon_FormulaContext *ctx, const char *name,
                                   Carbon_FormulaFunc func, int min_args, int max_args,
                                   void *userdata) {
    if (!ctx || !name || !func) return false;

    if (ctx->custom_func_count >= CARBON_FORMULA_MAX_CUSTOM_FUNCS) {
        carbon_set_error("Maximum custom functions exceeded");
        return false;
    }

    /* Check for duplicate */
    for (int i = 0; i < ctx->custom_func_count; i++) {
        if (strcmp(ctx->custom_funcs[i].name, name) == 0) {
            /* Update existing */
            ctx->custom_funcs[i].func = func;
            ctx->custom_funcs[i].min_args = min_args;
            ctx->custom_funcs[i].max_args = max_args;
            ctx->custom_funcs[i].userdata = userdata;
            return true;
        }
    }

    FormulaCustomFunc *f = &ctx->custom_funcs[ctx->custom_func_count];
    strncpy(f->name, name, CARBON_FORMULA_VAR_NAME_LEN - 1);
    f->name[CARBON_FORMULA_VAR_NAME_LEN - 1] = '\0';
    f->func = func;
    f->min_args = min_args;
    f->max_args = max_args;
    f->userdata = userdata;
    ctx->custom_func_count++;

    return true;
}

bool carbon_formula_unregister_func(Carbon_FormulaContext *ctx, const char *name) {
    if (!ctx || !name) return false;

    for (int i = 0; i < ctx->custom_func_count; i++) {
        if (strcmp(ctx->custom_funcs[i].name, name) == 0) {
            for (int j = i; j < ctx->custom_func_count - 1; j++) {
                ctx->custom_funcs[j] = ctx->custom_funcs[j + 1];
            }
            ctx->custom_func_count--;
            return true;
        }
    }

    return false;
}

/*============================================================================
 * Expression Evaluation
 *============================================================================*/

double carbon_formula_eval(Carbon_FormulaContext *ctx, const char *expression) {
    if (!ctx || !expression) {
        if (ctx) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "NULL expression");
        }
        return NAN;
    }

    ctx->error[0] = '\0';

    Parser p = {
        .expr = expression,
        .pos = 0,
        .ctx = ctx,
        .has_error = false
    };

    next_token(&p);
    if (p.has_error) return NAN;

    double result = parse_expression(&p);

    if (!p.has_error && p.current.type != TOK_EOF) {
        snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN,
                 "Unexpected content after expression at position %zu", p.pos);
        return NAN;
    }

    return result;
}

bool carbon_formula_valid(Carbon_FormulaContext *ctx, const char *expression) {
    if (!ctx || !expression) return false;

    /* Save current error state */
    char saved_error[CARBON_FORMULA_ERROR_LEN];
    memcpy(saved_error, ctx->error, CARBON_FORMULA_ERROR_LEN);

    double result = carbon_formula_eval(ctx, expression);
    bool valid = !isnan(result) || ctx->error[0] == '\0';

    /* Restore error state */
    memcpy(ctx->error, saved_error, CARBON_FORMULA_ERROR_LEN);

    return valid;
}

const char *carbon_formula_get_error(const Carbon_FormulaContext *ctx) {
    if (!ctx) return "";
    return ctx->error;
}

void carbon_formula_clear_error(Carbon_FormulaContext *ctx) {
    if (ctx) {
        ctx->error[0] = '\0';
    }
}

bool carbon_formula_has_error(const Carbon_FormulaContext *ctx) {
    return ctx && ctx->error[0] != '\0';
}

/*============================================================================
 * Compiled Formulas
 *============================================================================*/

/* Compilation parser that generates bytecode instead of evaluating */
typedef struct {
    const char *expr;
    size_t pos;
    Token current;
    Carbon_FormulaContext *ctx;
    bool has_error;
    Carbon_Formula *formula;
} CompileParser;

static void compile_next_token(CompileParser *p);
static bool compile_expression(CompileParser *p);
static bool compile_ternary(CompileParser *p);
static bool compile_or(CompileParser *p);
static bool compile_and(CompileParser *p);
static bool compile_equality(CompileParser *p);
static bool compile_comparison(CompileParser *p);
static bool compile_additive(CompileParser *p);
static bool compile_multiplicative(CompileParser *p);
static bool compile_unary(CompileParser *p);
static bool compile_power(CompileParser *p);
static bool compile_primary(CompileParser *p);

static bool emit(CompileParser *p, Instruction instr) {
    if (p->formula->code_len >= CARBON_FORMULA_MAX_INSTRUCTIONS) {
        snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN, "Formula too complex");
        p->has_error = true;
        return false;
    }
    p->formula->code[p->formula->code_len++] = instr;
    return true;
}

static void add_var_used(CompileParser *p, const char *name) {
    /* Check if already tracked */
    for (int i = 0; i < p->formula->vars_used_count; i++) {
        if (strcmp(p->formula->vars_used[i], name) == 0) {
            return;
        }
    }

    if (p->formula->vars_used_count < CARBON_FORMULA_MAX_VARS_USED) {
        strncpy(p->formula->vars_used[p->formula->vars_used_count], name,
                CARBON_FORMULA_VAR_NAME_LEN - 1);
        p->formula->vars_used[p->formula->vars_used_count][CARBON_FORMULA_VAR_NAME_LEN - 1] = '\0';
        p->formula->vars_used_count++;
    }
}

static void compile_next_token(CompileParser *p) {
    /* Reuse lexer logic */
    Parser temp = {
        .expr = p->expr,
        .pos = p->pos,
        .ctx = p->ctx,
        .has_error = p->has_error
    };
    next_token(&temp);
    p->pos = temp.pos;
    p->current = temp.current;
    p->has_error = temp.has_error;
}

static bool compile_expression(CompileParser *p) {
    return compile_ternary(p);
}

static bool compile_ternary(CompileParser *p) {
    if (!compile_or(p)) return false;

    if (p->current.type == TOK_QUESTION) {
        compile_next_token(p);
        if (!compile_expression(p)) return false;

        if (p->current.type != TOK_COLON) {
            snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
                     "Expected ':' in ternary expression");
            p->has_error = true;
            return false;
        }
        compile_next_token(p);
        if (!compile_expression(p)) return false;

        Instruction instr = { .op = OP_TERNARY };
        emit(p, instr);
    }

    return true;
}

static bool compile_or(CompileParser *p) {
    if (!compile_and(p)) return false;

    while (p->current.type == TOK_OR) {
        compile_next_token(p);
        if (!compile_and(p)) return false;
        Instruction instr = { .op = OP_OR };
        emit(p, instr);
    }

    return true;
}

static bool compile_and(CompileParser *p) {
    if (!compile_equality(p)) return false;

    while (p->current.type == TOK_AND) {
        compile_next_token(p);
        if (!compile_equality(p)) return false;
        Instruction instr = { .op = OP_AND };
        emit(p, instr);
    }

    return true;
}

static bool compile_equality(CompileParser *p) {
    if (!compile_comparison(p)) return false;

    while (p->current.type == TOK_EQ || p->current.type == TOK_NE) {
        TokenType op = p->current.type;
        compile_next_token(p);
        if (!compile_comparison(p)) return false;
        Instruction instr = { .op = (op == TOK_EQ) ? OP_EQ : OP_NE };
        emit(p, instr);
    }

    return true;
}

static bool compile_comparison(CompileParser *p) {
    if (!compile_additive(p)) return false;

    while (p->current.type == TOK_LT || p->current.type == TOK_LE ||
           p->current.type == TOK_GT || p->current.type == TOK_GE) {
        TokenType op = p->current.type;
        compile_next_token(p);
        if (!compile_additive(p)) return false;

        Instruction instr;
        switch (op) {
            case TOK_LT: instr.op = OP_LT; break;
            case TOK_LE: instr.op = OP_LE; break;
            case TOK_GT: instr.op = OP_GT; break;
            case TOK_GE: instr.op = OP_GE; break;
            default: instr.op = OP_LT; break;
        }
        emit(p, instr);
    }

    return true;
}

static bool compile_additive(CompileParser *p) {
    if (!compile_multiplicative(p)) return false;

    while (p->current.type == TOK_PLUS || p->current.type == TOK_MINUS) {
        TokenType op = p->current.type;
        compile_next_token(p);
        if (!compile_multiplicative(p)) return false;
        Instruction instr = { .op = (op == TOK_PLUS) ? OP_ADD : OP_SUB };
        emit(p, instr);
    }

    return true;
}

static bool compile_multiplicative(CompileParser *p) {
    if (!compile_unary(p)) return false;

    while (p->current.type == TOK_STAR || p->current.type == TOK_SLASH ||
           p->current.type == TOK_PERCENT) {
        TokenType op = p->current.type;
        compile_next_token(p);
        if (!compile_unary(p)) return false;

        Instruction instr;
        switch (op) {
            case TOK_STAR: instr.op = OP_MUL; break;
            case TOK_SLASH: instr.op = OP_DIV; break;
            case TOK_PERCENT: instr.op = OP_MOD; break;
            default: instr.op = OP_MUL; break;
        }
        emit(p, instr);
    }

    return true;
}

static bool compile_unary(CompileParser *p) {
    if (p->current.type == TOK_MINUS) {
        compile_next_token(p);
        if (!compile_unary(p)) return false;
        Instruction instr = { .op = OP_NEG };
        emit(p, instr);
        return true;
    }
    if (p->current.type == TOK_PLUS) {
        compile_next_token(p);
        return compile_unary(p);
    }
    if (p->current.type == TOK_NOT) {
        compile_next_token(p);
        if (!compile_unary(p)) return false;
        Instruction instr = { .op = OP_NOT };
        emit(p, instr);
        return true;
    }
    return compile_power(p);
}

static bool compile_power(CompileParser *p) {
    if (!compile_primary(p)) return false;

    if (p->current.type == TOK_CARET) {
        compile_next_token(p);
        if (!compile_unary(p)) return false;
        Instruction instr = { .op = OP_POW };
        emit(p, instr);
    }

    return true;
}

static bool compile_primary(CompileParser *p) {
    if (p->current.type == TOK_NUMBER) {
        Instruction instr = { .op = OP_PUSH_NUM };
        instr.data.num = p->current.number;
        emit(p, instr);
        compile_next_token(p);
        return true;
    }

    if (p->current.type == TOK_IDENT) {
        char name[CARBON_FORMULA_VAR_NAME_LEN];
        strncpy(name, p->current.ident, CARBON_FORMULA_VAR_NAME_LEN - 1);
        name[CARBON_FORMULA_VAR_NAME_LEN - 1] = '\0';
        compile_next_token(p);

        /* Function call */
        if (p->current.type == TOK_LPAREN) {
            compile_next_token(p);
            int argc = 0;

            if (p->current.type != TOK_RPAREN) {
                do {
                    if (!compile_expression(p)) return false;
                    argc++;
                } while (p->current.type == TOK_COMMA && (compile_next_token(p), 1));
            }

            if (p->current.type != TOK_RPAREN) {
                snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
                         "Expected ')' after function arguments");
                p->has_error = true;
                return false;
            }
            compile_next_token(p);

            Instruction instr = { .op = OP_CALL };
            strncpy(instr.data.call.func_name, name, CARBON_FORMULA_VAR_NAME_LEN - 1);
            instr.data.call.func_name[CARBON_FORMULA_VAR_NAME_LEN - 1] = '\0';
            instr.data.call.arg_count = argc;
            emit(p, instr);
            return true;
        }

        /* Variable */
        add_var_used(p, name);
        Instruction instr = { .op = OP_PUSH_VAR };
        strncpy(instr.data.var_name, name, CARBON_FORMULA_VAR_NAME_LEN - 1);
        instr.data.var_name[CARBON_FORMULA_VAR_NAME_LEN - 1] = '\0';
        emit(p, instr);
        return true;
    }

    if (p->current.type == TOK_LPAREN) {
        compile_next_token(p);
        if (!compile_expression(p)) return false;

        if (p->current.type != TOK_RPAREN) {
            snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
                     "Expected closing parenthesis");
            p->has_error = true;
            return false;
        }
        compile_next_token(p);
        return true;
    }

    snprintf(p->ctx->error, CARBON_FORMULA_ERROR_LEN,
             "Unexpected token at position %zu", p->pos);
    p->has_error = true;
    return false;
}

Carbon_Formula *carbon_formula_compile(Carbon_FormulaContext *ctx, const char *expression) {
    if (!ctx || !expression) {
        if (ctx) {
            snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "NULL expression");
        }
        return NULL;
    }

    size_t len = strlen(expression);
    if (len >= CARBON_FORMULA_MAX_EXPR_LEN) {
        snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Expression too long");
        return NULL;
    }

    Carbon_Formula *f = CARBON_ALLOC(Carbon_Formula);
    if (!f) {
        snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Failed to allocate formula");
        return NULL;
    }

    strncpy(f->expr, expression, CARBON_FORMULA_MAX_EXPR_LEN - 1);
    f->expr[CARBON_FORMULA_MAX_EXPR_LEN - 1] = '\0';

    ctx->error[0] = '\0';

    CompileParser p = {
        .expr = expression,
        .pos = 0,
        .ctx = ctx,
        .has_error = false,
        .formula = f
    };

    compile_next_token(&p);
    if (p.has_error) {
        free(f);
        return NULL;
    }

    if (!compile_expression(&p)) {
        free(f);
        return NULL;
    }

    if (p.current.type != TOK_EOF) {
        snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN,
                 "Unexpected content after expression at position %zu", p.pos);
        free(f);
        return NULL;
    }

    return f;
}

double carbon_formula_exec(Carbon_Formula *formula, Carbon_FormulaContext *ctx) {
    if (!formula || !ctx) return NAN;

    double stack[CARBON_FORMULA_MAX_STACK];
    int sp = 0;

    ctx->error[0] = '\0';

    for (int ip = 0; ip < formula->code_len; ip++) {
        Instruction *instr = &formula->code[ip];

        switch (instr->op) {
            case OP_PUSH_NUM:
                if (sp >= CARBON_FORMULA_MAX_STACK) {
                    snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack overflow");
                    return NAN;
                }
                stack[sp++] = instr->data.num;
                break;

            case OP_PUSH_VAR: {
                if (sp >= CARBON_FORMULA_MAX_STACK) {
                    snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack overflow");
                    return NAN;
                }
                bool found = false;
                for (int i = 0; i < ctx->var_count; i++) {
                    if (strcmp(ctx->vars[i].name, instr->data.var_name) == 0) {
                        stack[sp++] = ctx->vars[i].value;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN,
                             "Unknown variable '%s'", instr->data.var_name);
                    return NAN;
                }
                break;
            }

            case OP_ADD:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] += stack[sp-1];
                sp--;
                break;

            case OP_SUB:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] -= stack[sp-1];
                sp--;
                break;

            case OP_MUL:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] *= stack[sp-1];
                sp--;
                break;

            case OP_DIV:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                if (stack[sp-1] == 0.0) {
                    snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Division by zero");
                    return NAN;
                }
                stack[sp-2] /= stack[sp-1];
                sp--;
                break;

            case OP_MOD:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                if (stack[sp-1] == 0.0) {
                    snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Modulo by zero");
                    return NAN;
                }
                stack[sp-2] = fmod(stack[sp-2], stack[sp-1]);
                sp--;
                break;

            case OP_POW:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = pow(stack[sp-2], stack[sp-1]);
                sp--;
                break;

            case OP_NEG:
                if (sp < 1) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-1] = -stack[sp-1];
                break;

            case OP_NOT:
                if (sp < 1) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-1] = (stack[sp-1] == 0.0) ? 1.0 : 0.0;
                break;

            case OP_EQ:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] == stack[sp-1]) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_NE:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] != stack[sp-1]) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_LT:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] < stack[sp-1]) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_LE:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] <= stack[sp-1]) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_GT:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] > stack[sp-1]) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_GE:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] >= stack[sp-1]) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_AND:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] != 0.0 && stack[sp-1] != 0.0) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_OR:
                if (sp < 2) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] != 0.0 || stack[sp-1] != 0.0) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_TERNARY:
                if (sp < 3) { snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-3] = (stack[sp-3] != 0.0) ? stack[sp-2] : stack[sp-1];
                sp -= 2;
                break;

            case OP_CALL: {
                int argc = instr->data.call.arg_count;
                if (sp < argc) {
                    snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack underflow");
                    return NAN;
                }
                double args[16];
                for (int i = 0; i < argc; i++) {
                    args[i] = stack[sp - argc + i];
                }
                sp -= argc;
                double result = call_builtin(instr->data.call.func_name, args, argc, ctx);
                if (isnan(result) && ctx->error[0] != '\0') {
                    return NAN;
                }
                if (sp >= CARBON_FORMULA_MAX_STACK) {
                    snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Stack overflow");
                    return NAN;
                }
                stack[sp++] = result;
                break;
            }
        }
    }

    if (sp != 1) {
        snprintf(ctx->error, CARBON_FORMULA_ERROR_LEN, "Invalid expression");
        return NAN;
    }

    return stack[0];
}

void carbon_formula_free(Carbon_Formula *formula) {
    free(formula);
}

const char *carbon_formula_get_expr(const Carbon_Formula *formula) {
    return formula ? formula->expr : "";
}

int carbon_formula_get_vars(const Carbon_Formula *formula, const char **out_names, int max_names) {
    if (!formula || !out_names || max_names <= 0) return 0;

    int count = formula->vars_used_count;
    if (count > max_names) count = max_names;

    for (int i = 0; i < count; i++) {
        out_names[i] = formula->vars_used[i];
    }

    return count;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

double carbon_formula_eval_simple(const char *expression, ...) {
    Carbon_FormulaContext *ctx = carbon_formula_create();
    if (!ctx) return NAN;

    va_list args;
    va_start(args, expression);

    const char *name;
    while ((name = va_arg(args, const char *)) != NULL) {
        double value = va_arg(args, double);
        carbon_formula_set_var(ctx, name, value);
    }

    va_end(args);

    double result = carbon_formula_eval(ctx, expression);
    carbon_formula_destroy(ctx);

    return result;
}

int carbon_formula_format(double value, char *buf, size_t buf_size, int precision) {
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

bool carbon_formula_is_nan(double value) {
    return isnan(value);
}

bool carbon_formula_is_inf(double value) {
    return isinf(value);
}

void carbon_formula_set_constants(Carbon_FormulaContext *ctx) {
    if (!ctx) return;

    carbon_formula_set_var(ctx, "pi", 3.14159265358979323846);
    carbon_formula_set_var(ctx, "e", 2.71828182845904523536);
    carbon_formula_set_var(ctx, "tau", 6.28318530717958647692);  /* 2*pi */
    carbon_formula_set_var(ctx, "phi", 1.61803398874989484820);  /* Golden ratio */
}
