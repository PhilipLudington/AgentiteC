#include "agentite/agentite.h"
#include "agentite/game_event.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_EVENTS 128
#define MAX_TRIGGERED_IDS 256

struct Agentite_EventManager {
    // Registered events
    Agentite_EventDef events[MAX_EVENTS];
    int event_count;

    // Active event state
    Agentite_ActiveEvent pending;
    bool has_pending;

    // Tracking one-shot events
    char triggered_ids[MAX_TRIGGERED_IDS][64];
    int triggered_count;

    // Per-event cooldowns (turns remaining)
    int event_cooldowns[MAX_EVENTS];

    // Global cooldown between events
    int cooldown_between;
    int cooldown_remaining;
};

Agentite_EventManager *agentite_event_create(void) {
    Agentite_EventManager *em = AGENTITE_ALLOC(Agentite_EventManager);
    if (!em) return NULL;

    em->pending.choice_made = -1;
    return em;
}

void agentite_event_destroy(Agentite_EventManager *em) {
    free(em);
}

void agentite_event_register(Agentite_EventManager *em, const Agentite_EventDef *def) {
    if (!em || !def || em->event_count >= MAX_EVENTS) return;

    em->events[em->event_count] = *def;
    em->event_count++;
}

void agentite_event_set_cooldown_between(Agentite_EventManager *em, int turns) {
    if (!em) return;
    em->cooldown_between = turns;
}

// Check if event was already triggered (for one-shots)
static bool was_triggered(const Agentite_EventManager *em, const char *id) {
    for (int i = 0; i < em->triggered_count; i++) {
        if (strcmp(em->triggered_ids[i], id) == 0) {
            return true;
        }
    }
    return false;
}

static void mark_triggered(Agentite_EventManager *em, const char *id) {
    if (em->triggered_count >= MAX_TRIGGERED_IDS) return;

    strncpy(em->triggered_ids[em->triggered_count], id,
            sizeof(em->triggered_ids[em->triggered_count]) - 1);
    em->triggered_count++;
}

// Simple expression tokenizer
typedef enum {
    TOK_NUMBER,
    TOK_IDENTIFIER,
    TOK_OPERATOR,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_AND,
    TOK_OR,
    TOK_END,
    TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    float number;
    char text[64];
} Token;

typedef struct {
    const char *expr;
    int pos;
    Token current;
    const Agentite_TriggerContext *ctx;
} ExprParser;

static void skip_whitespace(ExprParser *p) {
    while (isspace((unsigned char)p->expr[p->pos])) {
        p->pos++;
    }
}

static float lookup_variable(const Agentite_TriggerContext *ctx, const char *name) {
    if (!ctx || !name) return 0.0f;

    for (int i = 0; i < ctx->var_count; i++) {
        if (ctx->var_names[i] && strcmp(ctx->var_names[i], name) == 0) {
            return ctx->var_values[i];
        }
    }
    return 0.0f;
}

static void next_token(ExprParser *p) {
    skip_whitespace(p);

    if (p->expr[p->pos] == '\0') {
        p->current.type = TOK_END;
        return;
    }

    char c = p->expr[p->pos];

    // Check for number
    if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)p->expr[p->pos + 1]))) {
        char *end;
        p->current.number = strtof(p->expr + p->pos, &end);
        p->pos = (int)(end - p->expr);
        p->current.type = TOK_NUMBER;
        return;
    }

    // Check for identifier
    if (isalpha((unsigned char)c) || c == '_') {
        int start = p->pos;
        while (isalnum((unsigned char)p->expr[p->pos]) || p->expr[p->pos] == '_') {
            p->pos++;
        }
        int len = p->pos - start;
        if (len >= (int)sizeof(p->current.text)) len = sizeof(p->current.text) - 1;
        strncpy(p->current.text, p->expr + start, len);
        p->current.text[len] = '\0';

        // Check for logical operators
        if (strcmp(p->current.text, "and") == 0 || strcmp(p->current.text, "AND") == 0) {
            p->current.type = TOK_AND;
        } else if (strcmp(p->current.text, "or") == 0 || strcmp(p->current.text, "OR") == 0) {
            p->current.type = TOK_OR;
        } else if (strcmp(p->current.text, "true") == 0) {
            p->current.type = TOK_NUMBER;
            p->current.number = 1.0f;
        } else if (strcmp(p->current.text, "false") == 0) {
            p->current.type = TOK_NUMBER;
            p->current.number = 0.0f;
        } else {
            p->current.type = TOK_IDENTIFIER;
            // Look up variable value
            p->current.number = lookup_variable(p->ctx, p->current.text);
        }
        return;
    }

    // Check for operators
    if (c == '&' && p->expr[p->pos + 1] == '&') {
        p->pos += 2;
        p->current.type = TOK_AND;
        return;
    }
    if (c == '|' && p->expr[p->pos + 1] == '|') {
        p->pos += 2;
        p->current.type = TOK_OR;
        return;
    }
    if (c == '>' && p->expr[p->pos + 1] == '=') {
        p->pos += 2;
        p->current.type = TOK_OPERATOR;
        p->current.text[0] = '>'; p->current.text[1] = '='; p->current.text[2] = '\0';
        return;
    }
    if (c == '<' && p->expr[p->pos + 1] == '=') {
        p->pos += 2;
        p->current.type = TOK_OPERATOR;
        p->current.text[0] = '<'; p->current.text[1] = '='; p->current.text[2] = '\0';
        return;
    }
    if (c == '=' && p->expr[p->pos + 1] == '=') {
        p->pos += 2;
        p->current.type = TOK_OPERATOR;
        p->current.text[0] = '='; p->current.text[1] = '='; p->current.text[2] = '\0';
        return;
    }
    if (c == '!' && p->expr[p->pos + 1] == '=') {
        p->pos += 2;
        p->current.type = TOK_OPERATOR;
        p->current.text[0] = '!'; p->current.text[1] = '='; p->current.text[2] = '\0';
        return;
    }

    if (c == '>' || c == '<') {
        p->current.type = TOK_OPERATOR;
        p->current.text[0] = c;
        p->current.text[1] = '\0';
        p->pos++;
        return;
    }

    if (c == '(') {
        p->current.type = TOK_LPAREN;
        p->pos++;
        return;
    }
    if (c == ')') {
        p->current.type = TOK_RPAREN;
        p->pos++;
        return;
    }

    p->current.type = TOK_ERROR;
}

// Recursive descent parser for expressions
static bool parse_or_expr(ExprParser *p);

static bool parse_primary(ExprParser *p, float *value) {
    if (p->current.type == TOK_NUMBER || p->current.type == TOK_IDENTIFIER) {
        *value = p->current.number;
        next_token(p);
        return true;
    }
    if (p->current.type == TOK_LPAREN) {
        next_token(p);
        bool result = parse_or_expr(p);
        if (p->current.type == TOK_RPAREN) {
            next_token(p);
        }
        *value = result ? 1.0f : 0.0f;
        return result;
    }
    return false;
}

static bool parse_comparison(ExprParser *p) {
    float left;
    if (!parse_primary(p, &left)) return false;

    if (p->current.type == TOK_OPERATOR) {
        char op[3];
        strncpy(op, p->current.text, sizeof(op) - 1);
        op[sizeof(op) - 1] = '\0';
        next_token(p);

        float right;
        if (!parse_primary(p, &right)) return false;

        if (strcmp(op, ">") == 0) return left > right;
        if (strcmp(op, "<") == 0) return left < right;
        if (strcmp(op, ">=") == 0) return left >= right;
        if (strcmp(op, "<=") == 0) return left <= right;
        if (strcmp(op, "==") == 0) return fabsf(left - right) < 0.0001f;
        if (strcmp(op, "!=") == 0) return fabsf(left - right) >= 0.0001f;
    }

    // No operator - just check if non-zero
    return left != 0.0f;
}

static bool parse_and_expr(ExprParser *p) {
    bool result = parse_comparison(p);

    while (p->current.type == TOK_AND) {
        next_token(p);
        bool right = parse_comparison(p);
        result = result && right;
    }

    return result;
}

static bool parse_or_expr(ExprParser *p) {
    bool result = parse_and_expr(p);

    while (p->current.type == TOK_OR) {
        next_token(p);
        bool right = parse_and_expr(p);
        result = result || right;
    }

    return result;
}

bool agentite_event_evaluate(const char *expr, const Agentite_TriggerContext *ctx) {
    if (!expr || !expr[0]) return false;

    ExprParser parser = {0};
    parser.expr = expr;
    parser.ctx = ctx;

    next_token(&parser);
    return parse_or_expr(&parser);
}

// Compare events by priority for sorting (used for qsort if needed)
static int compare_events_by_priority(const void *a, const void *b) {
    const Agentite_EventDef *ea = (const Agentite_EventDef*)a;
    const Agentite_EventDef *eb = (const Agentite_EventDef*)b;
    return eb->priority - ea->priority;  // Higher priority first
}
// Suppress unused warning - function is available for future use
__attribute__((unused)) static void *_unused_compare = (void*)compare_events_by_priority;

bool agentite_event_check_triggers(Agentite_EventManager *em, const Agentite_TriggerContext *ctx) {
    if (!em || !ctx) return false;

    // Already have pending event
    if (em->has_pending) return false;

    // Check global cooldown
    if (em->cooldown_remaining > 0) {
        em->cooldown_remaining--;
        return false;
    }

    // Sort events by priority (in-place, could be optimized)
    // For now, just iterate and track best match
    const Agentite_EventDef *best = NULL;
    int best_index = -1;

    for (int i = 0; i < em->event_count; i++) {
        Agentite_EventDef *def = &em->events[i];

        // Skip if on cooldown
        if (em->event_cooldowns[i] > 0) {
            em->event_cooldowns[i]--;
            continue;
        }

        // Skip one-shot events that already fired
        if (def->one_shot && was_triggered(em, def->id)) {
            continue;
        }

        // Check trigger expression
        if (!agentite_event_evaluate(def->trigger, ctx)) {
            continue;
        }

        // Found a match - check priority
        if (!best || def->priority > best->priority) {
            best = def;
            best_index = i;
        }
    }

    if (best) {
        em->pending.def = best;
        em->pending.resolved = false;
        em->pending.choice_made = -1;
        em->has_pending = true;

        // Mark one-shot
        if (best->one_shot) {
            mark_triggered(em, best->id);
        }

        // Set event cooldown
        if (best->cooldown > 0) {
            em->event_cooldowns[best_index] = best->cooldown;
        }

        // Set global cooldown
        em->cooldown_remaining = em->cooldown_between;

        return true;
    }

    return false;
}

bool agentite_event_has_pending(const Agentite_EventManager *em) {
    if (!em) return false;
    return em->has_pending && !em->pending.resolved;
}

const Agentite_ActiveEvent *agentite_event_get_pending(const Agentite_EventManager *em) {
    if (!em || !em->has_pending) return NULL;
    return &em->pending;
}

bool agentite_event_choose(Agentite_EventManager *em, int choice_index) {
    if (!em || !em->has_pending || em->pending.resolved) return false;
    if (!em->pending.def) return false;
    if (choice_index < 0 || choice_index >= em->pending.def->choice_count) return false;

    em->pending.choice_made = choice_index;
    em->pending.resolved = true;
    return true;
}

const Agentite_EventChoice *agentite_event_get_chosen(const Agentite_EventManager *em) {
    if (!em || !em->has_pending || !em->pending.resolved) return NULL;
    if (em->pending.choice_made < 0) return NULL;

    return &em->pending.def->choices[em->pending.choice_made];
}

void agentite_event_clear_pending(Agentite_EventManager *em) {
    if (!em) return;

    em->has_pending = false;
    em->pending.def = NULL;
    em->pending.resolved = false;
    em->pending.choice_made = -1;
}

void agentite_event_reset(Agentite_EventManager *em) {
    if (!em) return;

    em->triggered_count = 0;
    em->cooldown_remaining = 0;
    memset(em->event_cooldowns, 0, sizeof(em->event_cooldowns));
    agentite_event_clear_pending(em);
}

void agentite_trigger_context_add(Agentite_TriggerContext *ctx, const char *name, float value) {
    if (!ctx || !name || ctx->var_count >= AGENTITE_EVENT_MAX_VARS) return;

    ctx->var_names[ctx->var_count] = name;
    ctx->var_values[ctx->var_count] = value;
    ctx->var_count++;
}

void agentite_trigger_context_clear(Agentite_TriggerContext *ctx) {
    if (!ctx) return;
    ctx->var_count = 0;
}
