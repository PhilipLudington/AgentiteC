/**
 * Agentite Engine - Scene Lexer/Parser Internal Types
 *
 * Internal header shared between scene_lexer.cpp and scene_parser.cpp.
 */

#ifndef AGENTITE_SCENE_INTERNAL_H
#define AGENTITE_SCENE_INTERNAL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Token Types
 * ============================================================================ */

typedef enum Agentite_TokenType {
    TOK_EOF = 0,
    TOK_ERROR,

    /* Literals */
    TOK_IDENTIFIER,    /* Entity, ComponentName, true, false, etc. */
    TOK_STRING,        /* "quoted string" */
    TOK_INT,           /* 123, -456 */
    TOK_FLOAT,         /* 1.5, -3.14 */

    /* Symbols */
    TOK_AT,            /* @ */
    TOK_LPAREN,        /* ( */
    TOK_RPAREN,        /* ) */
    TOK_LBRACE,        /* { */
    TOK_RBRACE,        /* } */
    TOK_COLON,         /* : */
    TOK_COMMA,         /* , */
    TOK_MINUS,         /* - (for negative numbers) */
} Agentite_TokenType;

/* ============================================================================
 * Token Structure
 * ============================================================================ */

typedef struct Agentite_Token {
    Agentite_TokenType type;
    const char *start;   /* Pointer into source (not null-terminated) */
    int length;          /* Token length in bytes */
    int line;            /* Source line number (1-based) */
    int column;          /* Source column (1-based) */

    /* Parsed values for literals */
    int64_t int_val;
    double float_val;
} Agentite_Token;

/* ============================================================================
 * Lexer Structure
 * ============================================================================ */

typedef struct Agentite_Lexer {
    const char *source;  /* Source text */
    const char *start;   /* Start of current token */
    const char *current; /* Current position */
    int line;            /* Current line number */
    int column;          /* Current column */
    const char *name;    /* Source name for errors */

    /* Error state */
    char error[256];
    bool has_error;
} Agentite_Lexer;

/* ============================================================================
 * Lexer Functions
 * ============================================================================ */

/**
 * Initialize lexer with source text.
 */
void agentite_lexer_init(Agentite_Lexer *lexer, const char *source,
                          size_t length, const char *name);

/**
 * Get next token from source.
 */
Agentite_Token agentite_lexer_next(Agentite_Lexer *lexer);

/**
 * Peek at next token without consuming it.
 */
Agentite_Token agentite_lexer_peek(Agentite_Lexer *lexer);

/**
 * Get token type name for debugging.
 */
const char *agentite_token_type_name(Agentite_TokenType type);

/**
 * Extract token text as null-terminated string.
 * Caller must free the returned string.
 */
char *agentite_token_to_string(const Agentite_Token *token);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_SCENE_INTERNAL_H */
