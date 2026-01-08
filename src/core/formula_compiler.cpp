/**
 * Agentite Engine - Formula Bytecode Compiler and VM
 *
 * Compiles formula expressions to bytecode for faster repeated evaluation.
 * The bytecode VM executes using a simple stack-based architecture.
 *
 * Bytecode Architecture:
 * ======================
 *
 * The compiler generates a sequence of instructions that operate on a stack.
 * Each instruction pushes results onto the stack or consumes values from it.
 *
 * Instruction Set:
 *   OP_PUSH_NUM    - Push literal number onto stack
 *   OP_PUSH_VAR    - Look up variable and push its value
 *   OP_ADD/SUB/MUL/DIV/MOD/POW - Binary arithmetic (pop 2, push 1)
 *   OP_NEG/NOT     - Unary operators (pop 1, push 1)
 *   OP_EQ/NE/LT/LE/GT/GE - Comparison (pop 2, push 0.0 or 1.0)
 *   OP_AND/OR      - Logical operators (pop 2, push 0.0 or 1.0)
 *   OP_TERNARY     - Conditional (pop 3: cond, true_val, false_val; push 1)
 *   OP_CALL        - Function call (pop N args, push result)
 *
 * Stack Usage:
 *   - Maximum stack depth: 64 elements
 *   - Final stack should have exactly 1 element (the result)
 *
 * Compilation:
 *   - Same grammar as the interpreter (see formula_lexer.cpp)
 *   - Generates bytecode instead of immediately evaluating
 *   - Tracks which variables are used for dependency analysis
 */

#include "formula_internal.h"
#include "agentite/agentite.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Compiler State
 * ============================================================================ */

typedef struct {
    const char *expr;
    size_t pos;
    Token current;
    Agentite_FormulaContext *ctx;
    bool has_error;
    Agentite_Formula *formula;
    int depth;
} CompileParser;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

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

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Emit an instruction to the bytecode array.
 * Returns false if the instruction limit is exceeded.
 */
static bool emit(CompileParser *p, Instruction instr) {
    if (p->formula->code_len >= AGENTITE_FORMULA_MAX_INSTRUCTIONS) {
        snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Formula too complex");
        p->has_error = true;
        return false;
    }
    p->formula->code[p->formula->code_len++] = instr;
    return true;
}

/**
 * Track a variable name used in the formula.
 * Used for dependency analysis (agentite_formula_get_vars).
 */
static void add_var_used(CompileParser *p, const char *name) {
    /* Check if already tracked */
    for (int i = 0; i < p->formula->vars_used_count; i++) {
        if (strcmp(p->formula->vars_used[i], name) == 0) {
            return;
        }
    }

    if (p->formula->vars_used_count < AGENTITE_FORMULA_MAX_VARS_USED) {
        strncpy(p->formula->vars_used[p->formula->vars_used_count], name,
                AGENTITE_FORMULA_VAR_NAME_LEN - 1);
        p->formula->vars_used[p->formula->vars_used_count][AGENTITE_FORMULA_VAR_NAME_LEN - 1] = '\0';
        p->formula->vars_used_count++;
    }
}

/**
 * Advance to the next token, reusing the lexer from formula_lexer.cpp.
 */
static void compile_next_token(CompileParser *p) {
    /* Reuse lexer logic via temporary Parser struct */
    Parser temp = {
        .expr = p->expr,
        .pos = p->pos,
        .ctx = p->ctx,
        .has_error = p->has_error,
        .depth = 0
    };
    formula_next_token(&temp);
    p->pos = temp.pos;
    p->current = temp.current;
    p->has_error = temp.has_error;
}

/* ============================================================================
 * Compiler Implementation - Generates Bytecode
 *
 * The compiler mirrors the structure of the interpreter's parser, but
 * instead of evaluating expressions, it emits bytecode instructions.
 * ============================================================================ */

/**
 * Compile expression with recursion depth limit.
 */
static bool compile_expression(CompileParser *p) {
    if (p->depth >= AGENTITE_FORMULA_MAX_DEPTH) {
        snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                 "Expression too deeply nested (max depth %d)", AGENTITE_FORMULA_MAX_DEPTH);
        p->has_error = true;
        return false;
    }
    p->depth++;
    bool result = compile_ternary(p);
    p->depth--;
    return result;
}

/**
 * Compile ternary conditional.
 *
 * Bytecode: [condition] [true_expr] [false_expr] OP_TERNARY
 * Stack effect: pushes condition, true, false, then OP_TERNARY pops 3 and pushes result
 */
static bool compile_ternary(CompileParser *p) {
    if (!compile_or(p)) return false;

    if (p->current.type == TOK_QUESTION) {
        compile_next_token(p);
        if (!compile_expression(p)) return false;

        if (p->current.type != TOK_COLON) {
            snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
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

/**
 * Compile logical OR.
 */
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

/**
 * Compile logical AND.
 */
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

/**
 * Compile equality comparison.
 */
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

/**
 * Compile relational comparison.
 */
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

/**
 * Compile addition and subtraction.
 */
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

/**
 * Compile multiplication, division, and modulo.
 */
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

/**
 * Compile unary operators.
 */
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

/**
 * Compile power operator (right-associative).
 */
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

/**
 * Compile primary expressions: numbers, variables, function calls, parentheses.
 */
static bool compile_primary(CompileParser *p) {
    /* Number literal */
    if (p->current.type == TOK_NUMBER) {
        Instruction instr = { .op = OP_PUSH_NUM };
        instr.data.num = p->current.number;
        emit(p, instr);
        compile_next_token(p);
        return true;
    }

    /* Identifier: variable or function call */
    if (p->current.type == TOK_IDENT) {
        char name[AGENTITE_FORMULA_VAR_NAME_LEN];
        strncpy(name, p->current.ident, AGENTITE_FORMULA_VAR_NAME_LEN - 1);
        name[AGENTITE_FORMULA_VAR_NAME_LEN - 1] = '\0';
        compile_next_token(p);

        /* Function call */
        if (p->current.type == TOK_LPAREN) {
            compile_next_token(p);
            int argc = 0;

            /* Compile arguments */
            if (p->current.type != TOK_RPAREN) {
                do {
                    if (!compile_expression(p)) return false;
                    argc++;
                } while (p->current.type == TOK_COMMA && (compile_next_token(p), 1));
            }

            if (p->current.type != TOK_RPAREN) {
                snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                         "Expected ')' after function arguments");
                p->has_error = true;
                return false;
            }
            compile_next_token(p);

            Instruction instr = { .op = OP_CALL };
            strncpy(instr.data.call.func_name, name, AGENTITE_FORMULA_VAR_NAME_LEN - 1);
            instr.data.call.func_name[AGENTITE_FORMULA_VAR_NAME_LEN - 1] = '\0';
            instr.data.call.arg_count = argc;
            emit(p, instr);
            return true;
        }

        /* Variable reference */
        add_var_used(p, name);
        Instruction instr = { .op = OP_PUSH_VAR };
        strncpy(instr.data.var_name, name, AGENTITE_FORMULA_VAR_NAME_LEN - 1);
        instr.data.var_name[AGENTITE_FORMULA_VAR_NAME_LEN - 1] = '\0';
        emit(p, instr);
        return true;
    }

    /* Parenthesized expression */
    if (p->current.type == TOK_LPAREN) {
        compile_next_token(p);
        if (!compile_expression(p)) return false;

        if (p->current.type != TOK_RPAREN) {
            snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                     "Expected closing parenthesis");
            p->has_error = true;
            return false;
        }
        compile_next_token(p);
        return true;
    }

    /* Unexpected token */
    snprintf(p->ctx->error, AGENTITE_FORMULA_ERROR_LEN,
             "Unexpected token at position %zu", p->pos);
    p->has_error = true;
    return false;
}

/* ============================================================================
 * Public API - Compilation
 * ============================================================================ */

Agentite_Formula *agentite_formula_compile(Agentite_FormulaContext *ctx, const char *expression) {
    if (!ctx || !expression) {
        if (ctx) {
            snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "NULL expression");
        }
        return NULL;
    }

    size_t len = strlen(expression);
    if (len >= AGENTITE_FORMULA_MAX_EXPR_LEN) {
        snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Expression too long");
        return NULL;
    }

    Agentite_Formula *f = AGENTITE_ALLOC(Agentite_Formula);
    if (!f) {
        snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Failed to allocate formula");
        return NULL;
    }

    /* Initialize formula struct */
    memset(f, 0, sizeof(Agentite_Formula));
    strncpy(f->expr, expression, AGENTITE_FORMULA_MAX_EXPR_LEN - 1);
    f->expr[AGENTITE_FORMULA_MAX_EXPR_LEN - 1] = '\0';

    ctx->error[0] = '\0';

    CompileParser p = {
        .expr = expression,
        .pos = 0,
        .ctx = ctx,
        .has_error = false,
        .formula = f,
        .depth = 0
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
        snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                 "Unexpected content after expression at position %zu", p.pos);
        free(f);
        return NULL;
    }

    return f;
}

/* ============================================================================
 * Public API - VM Execution
 * ============================================================================ */

double agentite_formula_exec(Agentite_Formula *formula, Agentite_FormulaContext *ctx) {
    if (!formula || !ctx) return NAN;

    double stack[AGENTITE_FORMULA_MAX_STACK];
    int sp = 0;

    ctx->error[0] = '\0';

    /* Execute bytecode */
    for (int ip = 0; ip < formula->code_len; ip++) {
        Instruction *instr = &formula->code[ip];

        switch (instr->op) {
            case OP_PUSH_NUM:
                if (sp >= AGENTITE_FORMULA_MAX_STACK) {
                    snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack overflow");
                    return NAN;
                }
                stack[sp++] = instr->data.num;
                break;

            case OP_PUSH_VAR: {
                if (sp >= AGENTITE_FORMULA_MAX_STACK) {
                    snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack overflow");
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
                    snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN,
                             "Unknown variable '%s'", instr->data.var_name);
                    return NAN;
                }
                break;
            }

            case OP_ADD:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] += stack[sp-1];
                sp--;
                break;

            case OP_SUB:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] -= stack[sp-1];
                sp--;
                break;

            case OP_MUL:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] *= stack[sp-1];
                sp--;
                break;

            case OP_DIV:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                if (stack[sp-1] == 0.0) {
                    snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Division by zero");
                    return NAN;
                }
                stack[sp-2] /= stack[sp-1];
                sp--;
                break;

            case OP_MOD:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                if (stack[sp-1] == 0.0) {
                    snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Modulo by zero");
                    return NAN;
                }
                stack[sp-2] = fmod(stack[sp-2], stack[sp-1]);
                sp--;
                break;

            case OP_POW:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = pow(stack[sp-2], stack[sp-1]);
                sp--;
                break;

            case OP_NEG:
                if (sp < 1) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-1] = -stack[sp-1];
                break;

            case OP_NOT:
                if (sp < 1) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-1] = (stack[sp-1] == 0.0) ? 1.0 : 0.0;
                break;

            case OP_EQ:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] == stack[sp-1]) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_NE:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] != stack[sp-1]) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_LT:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] < stack[sp-1]) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_LE:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] <= stack[sp-1]) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_GT:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] > stack[sp-1]) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_GE:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] >= stack[sp-1]) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_AND:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] != 0.0 && stack[sp-1] != 0.0) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_OR:
                if (sp < 2) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-2] = (stack[sp-2] != 0.0 || stack[sp-1] != 0.0) ? 1.0 : 0.0;
                sp--;
                break;

            case OP_TERNARY:
                if (sp < 3) { snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow"); return NAN; }
                stack[sp-3] = (stack[sp-3] != 0.0) ? stack[sp-2] : stack[sp-1];
                sp -= 2;
                break;

            case OP_CALL: {
                int argc = instr->data.call.arg_count;
                if (sp < argc) {
                    snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack underflow");
                    return NAN;
                }
                double args[16];
                for (int i = 0; i < argc; i++) {
                    args[i] = stack[sp - argc + i];
                }
                sp -= argc;
                double result = formula_call_builtin(instr->data.call.func_name, args, argc, ctx);
                if (isnan(result) && ctx->error[0] != '\0') {
                    return NAN;
                }
                if (sp >= AGENTITE_FORMULA_MAX_STACK) {
                    snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Stack overflow");
                    return NAN;
                }
                stack[sp++] = result;
                break;
            }
        }
    }

    if (sp != 1) {
        snprintf(ctx->error, AGENTITE_FORMULA_ERROR_LEN, "Invalid expression");
        return NAN;
    }

    return stack[0];
}

/* ============================================================================
 * Public API - Accessors
 * ============================================================================ */

void agentite_formula_free(Agentite_Formula *formula) {
    free(formula);
}

const char *agentite_formula_get_expr(const Agentite_Formula *formula) {
    return formula ? formula->expr : "";
}

int agentite_formula_get_vars(const Agentite_Formula *formula, const char **out_names, int max_names) {
    if (!formula || !out_names || max_names <= 0) return 0;

    int count = formula->vars_used_count;
    if (count > max_names) count = max_names;

    for (int i = 0; i < count; i++) {
        out_names[i] = formula->vars_used[i];
    }

    return count;
}
