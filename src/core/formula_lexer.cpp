/**
 * Agentite Engine - Formula Lexer and Parser
 *
 * Tokenizes and parses mathematical expressions using recursive descent.
 *
 * Formula Grammar (EBNF):
 * =======================
 *
 * expression     = ternary ;
 * ternary        = or ( "?" expression ":" expression )? ;
 * or             = and ( "||" and )* ;
 * and            = equality ( "&&" equality )* ;
 * equality       = comparison ( ("==" | "!=") comparison )* ;
 * comparison     = additive ( ("<" | "<=" | ">" | ">=") additive )* ;
 * additive       = multiplicative ( ("+" | "-") multiplicative )* ;
 * multiplicative = unary ( ("*" | "/" | "%") unary )* ;
 * unary          = ("!" | "-" | "+") unary | power ;
 * power          = primary ( "^" unary )? ;
 * primary        = NUMBER | IDENTIFIER | IDENTIFIER "(" args? ")" | "(" expression ")" ;
 * args           = expression ( "," expression )* ;
 *
 * Operator Precedence (lowest to highest):
 * ========================================
 *
 *  1. Ternary (?:)           - Conditional, right-associative
 *  2. Logical OR (||)        - Short-circuit OR
 *  3. Logical AND (&&)       - Short-circuit AND
 *  4. Equality (==, !=)      - Comparison returns 1.0 or 0.0
 *  5. Comparison (<, <=, >, >=)
 *  6. Additive (+, -)
 *  7. Multiplicative (*, /, %)
 *  8. Unary (!, -, +)        - Prefix operators
 *  9. Power (^)              - Right-associative (2^3^2 = 2^9 = 512)
 * 10. Primary                - Atoms: numbers, variables, calls, parens
 *
 * Token Types:
 * ============
 *
 * TOK_NUMBER   - Floating point: 123, 3.14, .5, 1e10, 1.5e-3
 * TOK_IDENT    - Variable or function name: x, damage, MAX_HP
 * TOK_PLUS     - Addition or unary plus
 * TOK_MINUS    - Subtraction or unary negation
 * TOK_STAR     - Multiplication
 * TOK_SLASH    - Division (error on divide by zero)
 * TOK_PERCENT  - Modulo (error on modulo by zero)
 * TOK_CARET    - Power/exponentiation
 * TOK_LPAREN   - Left parenthesis
 * TOK_RPAREN   - Right parenthesis
 * TOK_COMMA    - Function argument separator
 * TOK_QUESTION - Ternary condition
 * TOK_COLON    - Ternary separator
 * TOK_EQ       - Equality (==)
 * TOK_NE       - Not equal (!=)
 * TOK_LT       - Less than
 * TOK_LE       - Less than or equal
 * TOK_GT       - Greater than
 * TOK_GE       - Greater than or equal
 * TOK_AND      - Logical AND (&&)
 * TOK_OR       - Logical OR (||)
 * TOK_NOT      - Logical NOT (!)
 */

#include "formula_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

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

/* ============================================================================
 * Lexer Implementation
 * ============================================================================ */

/**
 * Skip whitespace characters (space, tab, newline, carriage return).
 */
static void skip_whitespace(Parser *p) {
    while (p->expr[p->pos] && isspace((unsigned char)p->expr[p->pos])) {
        p->pos++;
    }
}

/**
 * Scan the next token from the input expression.
 *
 * Number format: integer or floating point, with optional exponent.
 *   Examples: 123, 3.14, .5, 1e10, 1.5e-3
 *
 * Identifier format: letter or underscore, followed by alphanumerics/underscores.
 *   Examples: x, damage, MAX_HP, _private
 */
void formula_next_token(Parser *p) {
    skip_whitespace(p);

    if (p->expr[p->pos] == '\0') {
        p->current.type = TOK_EOF;
        return;
    }

    char c = p->expr[p->pos];

    /* Number: starts with digit, or '.' followed by digit */
    if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)p->expr[p->pos + 1]))) {
        char *end;
        p->current.number = strtod(&p->expr[p->pos], &end);
        p->pos = end - p->expr;
        p->current.type = TOK_NUMBER;
        return;
    }

    /* Identifier: starts with letter or underscore */
    if (isalpha((unsigned char)c) || c == '_') {
        size_t start = p->pos;
        while (isalnum((unsigned char)p->expr[p->pos]) || p->expr[p->pos] == '_') {
            p->pos++;
        }
        size_t len = p->pos - start;
        if (len >= AGENTITE_FORMULA_VAR_NAME_LEN) {
            len = AGENTITE_FORMULA_VAR_NAME_LEN - 1;
        }
        memcpy(p->current.ident, &p->expr[start], len);
        p->current.ident[len] = '\0';
        p->current.type = TOK_IDENT;
        return;
    }

    /* Single and multi-character operators */
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

        /* != and ! */
        case '!':
            if (p->expr[p->pos] == '=') {
                p->pos++;
                p->current.type = TOK_NE;
            } else {
                p->current.type = TOK_NOT;
            }
            break;

        /* == (assignment = is not supported) */
        case '=':
            if (p->expr[p->pos] == '=') {
                p->pos++;
                p->current.type = TOK_EQ;
            } else {
                p->current.type = TOK_ERROR;
                snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                         "Expected '==' at position %zu", p->pos);
                p->has_error = true;
            }
            break;

        /* < and <= */
        case '<':
            if (p->expr[p->pos] == '=') {
                p->pos++;
                p->current.type = TOK_LE;
            } else {
                p->current.type = TOK_LT;
            }
            break;

        /* > and >= */
        case '>':
            if (p->expr[p->pos] == '=') {
                p->pos++;
                p->current.type = TOK_GE;
            } else {
                p->current.type = TOK_GT;
            }
            break;

        /* && (bitwise & is not supported) */
        case '&':
            if (p->expr[p->pos] == '&') {
                p->pos++;
                p->current.type = TOK_AND;
            } else {
                p->current.type = TOK_ERROR;
                snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                         "Expected '&&' at position %zu", p->pos);
                p->has_error = true;
            }
            break;

        /* || (bitwise | is not supported) */
        case '|':
            if (p->expr[p->pos] == '|') {
                p->pos++;
                p->current.type = TOK_OR;
            } else {
                p->current.type = TOK_ERROR;
                snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                         "Expected '||' at position %zu", p->pos);
                p->has_error = true;
            }
            break;

        default:
            p->current.type = TOK_ERROR;
            snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                     "Unexpected character '%c' at position %zu", c, p->pos - 1);
            p->has_error = true;
            break;
    }
}

/* ============================================================================
 * Parser Implementation - Recursive Descent
 *
 * Each parse_* function handles one precedence level.
 * Lower precedence = called first (outer), higher precedence = called last (inner).
 * This ensures correct operator precedence: 1 + 2 * 3 = 1 + (2 * 3) = 7
 * ============================================================================ */

/**
 * Entry point for expression parsing.
 * Enforces maximum recursion depth to prevent stack overflow on malicious input.
 */
double formula_parse_expression(Parser *p) {
    if (p->depth >= AGENTITE_FORMULA_MAX_DEPTH) {
        snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                 "Expression too deeply nested (max depth %d)", AGENTITE_FORMULA_MAX_DEPTH);
        p->has_error = true;
        return NAN;
    }
    p->depth++;
    double result = parse_ternary(p);
    p->depth--;
    return result;
}

/**
 * Parse ternary conditional: condition ? true_value : false_value
 *
 * The ternary operator is right-associative:
 *   a ? b : c ? d : e  =  a ? b : (c ? d : e)
 *
 * Both branches are always evaluated (no short-circuit for ternary).
 */
static double parse_ternary(Parser *p) {
    double cond = parse_or(p);
    if (p->has_error) return NAN;

    if (p->current.type == TOK_QUESTION) {
        formula_next_token(p);
        double true_val = formula_parse_expression(p);  /* Recursive for right-associativity */
        if (p->has_error) return NAN;

        if (p->current.type != TOK_COLON) {
            snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                     "Expected ':' in ternary expression");
            p->has_error = true;
            return NAN;
        }
        formula_next_token(p);
        double false_val = formula_parse_expression(p);
        if (p->has_error) return NAN;

        return cond != 0.0 ? true_val : false_val;
    }

    return cond;
}

/**
 * Parse logical OR: left || right
 *
 * Returns 1.0 if either operand is non-zero, 0.0 otherwise.
 * Note: Both sides are always evaluated (no short-circuit in this implementation).
 */
static double parse_or(Parser *p) {
    double left = parse_and(p);
    if (p->has_error) return NAN;

    while (p->current.type == TOK_OR) {
        formula_next_token(p);
        double right = parse_and(p);
        if (p->has_error) return NAN;
        left = (left != 0.0 || right != 0.0) ? 1.0 : 0.0;
    }

    return left;
}

/**
 * Parse logical AND: left && right
 *
 * Returns 1.0 if both operands are non-zero, 0.0 otherwise.
 * Note: Both sides are always evaluated (no short-circuit in this implementation).
 */
static double parse_and(Parser *p) {
    double left = parse_equality(p);
    if (p->has_error) return NAN;

    while (p->current.type == TOK_AND) {
        formula_next_token(p);
        double right = parse_equality(p);
        if (p->has_error) return NAN;
        left = (left != 0.0 && right != 0.0) ? 1.0 : 0.0;
    }

    return left;
}

/**
 * Parse equality comparison: left == right, left != right
 *
 * Returns 1.0 if condition is true, 0.0 otherwise.
 * Uses exact floating-point comparison (no epsilon tolerance).
 */
static double parse_equality(Parser *p) {
    double left = parse_comparison(p);
    if (p->has_error) return NAN;

    while (p->current.type == TOK_EQ || p->current.type == TOK_NE) {
        TokenType op = p->current.type;
        formula_next_token(p);
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

/**
 * Parse relational comparison: <, <=, >, >=
 *
 * Returns 1.0 if condition is true, 0.0 otherwise.
 */
static double parse_comparison(Parser *p) {
    double left = parse_additive(p);
    if (p->has_error) return NAN;

    while (p->current.type == TOK_LT || p->current.type == TOK_LE ||
           p->current.type == TOK_GT || p->current.type == TOK_GE) {
        TokenType op = p->current.type;
        formula_next_token(p);
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

/**
 * Parse addition and subtraction: left + right, left - right
 *
 * Left-associative: 1 - 2 - 3 = (1 - 2) - 3 = -4
 */
static double parse_additive(Parser *p) {
    double left = parse_multiplicative(p);
    if (p->has_error) return NAN;

    while (p->current.type == TOK_PLUS || p->current.type == TOK_MINUS) {
        TokenType op = p->current.type;
        formula_next_token(p);
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

/**
 * Parse multiplication, division, and modulo: *, /, %
 *
 * Left-associative: 12 / 3 / 2 = (12 / 3) / 2 = 2
 * Division and modulo by zero return NAN with error message.
 */
static double parse_multiplicative(Parser *p) {
    double left = parse_unary(p);
    if (p->has_error) return NAN;

    while (p->current.type == TOK_STAR || p->current.type == TOK_SLASH ||
           p->current.type == TOK_PERCENT) {
        TokenType op = p->current.type;
        formula_next_token(p);
        double right = parse_unary(p);
        if (p->has_error) return NAN;

        switch (op) {
            case TOK_STAR:
                left *= right;
                break;
            case TOK_SLASH:
                if (right == 0.0) {
                    snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Division by zero");
                    p->has_error = true;
                    return NAN;
                }
                left /= right;
                break;
            case TOK_PERCENT:
                if (right == 0.0) {
                    snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Modulo by zero");
                    p->has_error = true;
                    return NAN;
                }
                left = fmod(left, right);
                break;
            default:
                break;
        }
    }

    return left;
}

/**
 * Parse unary operators: -, +, !
 *
 * Unary operators are right-associative and recursive:
 *   --x = -(-x)
 *   !!x = !(!x)
 *
 * Logical NOT returns 1.0 if operand is zero, 0.0 otherwise.
 */
static double parse_unary(Parser *p) {
    if (p->current.type == TOK_MINUS) {
        formula_next_token(p);
        return -parse_unary(p);
    }
    if (p->current.type == TOK_PLUS) {
        formula_next_token(p);
        return parse_unary(p);
    }
    if (p->current.type == TOK_NOT) {
        formula_next_token(p);
        double val = parse_unary(p);
        return (val == 0.0) ? 1.0 : 0.0;
    }
    return parse_power(p);
}

/**
 * Parse power/exponentiation: base ^ exponent
 *
 * Right-associative: 2^3^2 = 2^(3^2) = 2^9 = 512
 * This matches mathematical convention.
 */
static double parse_power(Parser *p) {
    double base = parse_primary(p);
    if (p->has_error) return NAN;

    if (p->current.type == TOK_CARET) {
        formula_next_token(p);
        double exp = parse_unary(p);  /* Right-associative: recurse to unary, not power */
        if (p->has_error) return NAN;
        return pow(base, exp);
    }

    return base;
}

/**
 * Parse primary expressions: numbers, variables, function calls, parentheses.
 *
 * Number: numeric literal (123, 3.14, etc.)
 * Variable: identifier that doesn't have '(' following it
 * Function call: identifier followed by '(' arguments ')' - max 16 arguments
 * Parentheses: '(' expression ')'
 */
static double parse_primary(Parser *p) {
    /* Number literal */
    if (p->current.type == TOK_NUMBER) {
        double val = p->current.number;
        formula_next_token(p);
        return val;
    }

    /* Identifier: variable or function call */
    if (p->current.type == TOK_IDENT) {
        char name[AGENTITE_FORMULA_VAR_NAME_LEN];
        strncpy(name, p->current.ident, AGENTITE_FORMULA_VAR_NAME_LEN - 1);
        name[AGENTITE_FORMULA_VAR_NAME_LEN - 1] = '\0';
        formula_next_token(p);

        /* Function call: name followed by '(' */
        if (p->current.type == TOK_LPAREN) {
            formula_next_token(p);
            double args[16];
            int argc = 0;

            /* Parse arguments (if any) */
            if (p->current.type != TOK_RPAREN) {
                do {
                    if (argc >= 16) {
                        snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                                 "Too many arguments to function '%s'", name);
                        p->has_error = true;
                        return NAN;
                    }
                    args[argc++] = formula_parse_expression(p);
                    if (p->has_error) return NAN;
                } while (p->current.type == TOK_COMMA && (formula_next_token(p), 1));
            }

            if (p->current.type != TOK_RPAREN) {
                snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                         "Expected ')' after function arguments");
                p->has_error = true;
                return NAN;
            }
            formula_next_token(p);

            return formula_call_builtin(name, args, argc, p->ctx);
        }

        /* Variable lookup */
        for (int i = 0; i < p->ctx->var_count; i++) {
            if (strcmp(p->ctx->vars[i].name, name) == 0) {
                return p->ctx->vars[i].value;
            }
        }

        snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                 "Unknown variable '%s'", name);
        p->has_error = true;
        return NAN;
    }

    /* Parenthesized expression */
    if (p->current.type == TOK_LPAREN) {
        formula_next_token(p);
        double val = formula_parse_expression(p);
        if (p->has_error) return NAN;

        if (p->current.type != TOK_RPAREN) {
            snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                     "Expected closing parenthesis");
            p->has_error = true;
            return NAN;
        }
        formula_next_token(p);
        return val;
    }

    /* Unexpected token */
    snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
             "Unexpected token at position %zu", p->pos);
    p->has_error = true;
    return NAN;
}
