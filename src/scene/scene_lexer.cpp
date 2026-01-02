/**
 * Agentite Engine - Scene DSL Lexer
 *
 * Tokenizes scene/prefab DSL into tokens for parsing.
 */

#include "scene_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static bool is_at_end(Agentite_Lexer *lexer) {
    return *lexer->current == '\0';
}

static char advance(Agentite_Lexer *lexer) {
    char c = *lexer->current++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static char peek(Agentite_Lexer *lexer) {
    return *lexer->current;
}

static char peek_next(Agentite_Lexer *lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

static bool match(Agentite_Lexer *lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    advance(lexer);
    return true;
}

static void skip_whitespace(Agentite_Lexer *lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                advance(lexer);
                break;
            case '/':
                /* Line comment // */
                if (peek_next(lexer) == '/') {
                    while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                        advance(lexer);
                    }
                } else {
                    return;
                }
                break;
            case '#':
                /* Line comment # (AI-friendly format) */
                while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                    advance(lexer);
                }
                break;
            default:
                return;
        }
    }
}

static Agentite_Token make_token(Agentite_Lexer *lexer, Agentite_TokenType type) {
    Agentite_Token token = {};
    token.type = type;
    token.start = lexer->start;
    token.length = (int)(lexer->current - lexer->start);
    token.line = lexer->line;
    token.column = lexer->column - token.length;
    return token;
}

static Agentite_Token error_token(Agentite_Lexer *lexer, const char *message) {
    snprintf(lexer->error, sizeof(lexer->error), "%s:%d:%d: %s",
             lexer->name ? lexer->name : "<source>",
             lexer->line, lexer->column, message);
    lexer->has_error = true;

    Agentite_Token token = {};
    token.type = TOK_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    return token;
}

/* ============================================================================
 * Token Scanning
 * ============================================================================ */

static Agentite_Token scan_string(Agentite_Lexer *lexer) {
    while (peek(lexer) != '"' && !is_at_end(lexer)) {
        if (peek(lexer) == '\\' && peek_next(lexer) != '\0') {
            /* Skip escape sequence */
            advance(lexer);
        }
        advance(lexer);
    }

    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated string");
    }

    /* Consume closing quote */
    advance(lexer);

    Agentite_Token token = make_token(lexer, TOK_STRING);
    /* Adjust start/length to exclude quotes */
    token.start++;
    token.length -= 2;
    return token;
}

static Agentite_Token scan_number(Agentite_Lexer *lexer) {
    bool is_float = false;

    while (isdigit(peek(lexer))) {
        advance(lexer);
    }

    /* Look for decimal part */
    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        is_float = true;
        advance(lexer);  /* Consume '.' */
        while (isdigit(peek(lexer))) {
            advance(lexer);
        }
    }

    /* Look for exponent */
    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        is_float = true;
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-') {
            advance(lexer);
        }
        if (!isdigit(peek(lexer))) {
            return error_token(lexer, "Invalid number exponent");
        }
        while (isdigit(peek(lexer))) {
            advance(lexer);
        }
    }

    Agentite_Token token = make_token(lexer, is_float ? TOK_FLOAT : TOK_INT);

    /* Parse the value */
    char *temp = agentite_token_to_string(&token);
    if (is_float) {
        token.float_val = strtod(temp, NULL);
    } else {
        token.int_val = strtoll(temp, NULL, 10);
    }
    free(temp);

    return token;
}

static Agentite_Token scan_identifier(Agentite_Lexer *lexer) {
    while (isalnum(peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }
    return make_token(lexer, TOK_IDENTIFIER);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void agentite_lexer_init(Agentite_Lexer *lexer, const char *source,
                          size_t length, const char *name) {
    memset(lexer, 0, sizeof(*lexer));
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->name = name;
    (void)length;  /* Currently unused - assume null-terminated */
}

Agentite_Token agentite_lexer_next(Agentite_Lexer *lexer) {
    skip_whitespace(lexer);

    lexer->start = lexer->current;

    if (is_at_end(lexer)) {
        return make_token(lexer, TOK_EOF);
    }

    char c = advance(lexer);

    /* Identifiers and keywords */
    if (isalpha(c) || c == '_') {
        return scan_identifier(lexer);
    }

    /* Numbers */
    if (isdigit(c)) {
        return scan_number(lexer);
    }

    /* Symbols */
    switch (c) {
        case '@': return make_token(lexer, TOK_AT);
        case '(': return make_token(lexer, TOK_LPAREN);
        case ')': return make_token(lexer, TOK_RPAREN);
        case '{': return make_token(lexer, TOK_LBRACE);
        case '}': return make_token(lexer, TOK_RBRACE);
        case ':': return make_token(lexer, TOK_COLON);
        case ',': return make_token(lexer, TOK_COMMA);
        case '-':
            /* Could be negative number or just minus */
            if (isdigit(peek(lexer))) {
                return scan_number(lexer);
            }
            return make_token(lexer, TOK_MINUS);
        case '"': return scan_string(lexer);
    }

    return error_token(lexer, "Unexpected character");
}

Agentite_Token agentite_lexer_peek(Agentite_Lexer *lexer) {
    /* Save state */
    const char *start = lexer->start;
    const char *current = lexer->current;
    int line = lexer->line;
    int column = lexer->column;

    Agentite_Token token = agentite_lexer_next(lexer);

    /* Restore state */
    lexer->start = start;
    lexer->current = current;
    lexer->line = line;
    lexer->column = column;

    return token;
}

const char *agentite_token_type_name(Agentite_TokenType type) {
    switch (type) {
        case TOK_EOF:        return "EOF";
        case TOK_ERROR:      return "ERROR";
        case TOK_IDENTIFIER: return "IDENTIFIER";
        case TOK_STRING:     return "STRING";
        case TOK_INT:        return "INT";
        case TOK_FLOAT:      return "FLOAT";
        case TOK_AT:         return "@";
        case TOK_LPAREN:     return "(";
        case TOK_RPAREN:     return ")";
        case TOK_LBRACE:     return "{";
        case TOK_RBRACE:     return "}";
        case TOK_COLON:      return ":";
        case TOK_COMMA:      return ",";
        case TOK_MINUS:      return "-";
        default:             return "UNKNOWN";
    }
}

char *agentite_token_to_string(const Agentite_Token *token) {
    if (!token || token->length <= 0) {
        return strdup("");
    }

    char *str = (char *)malloc(token->length + 1);
    if (str) {
        memcpy(str, token->start, token->length);
        str[token->length] = '\0';
    }
    return str;
}
