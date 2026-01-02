/**
 * Agentite Engine - Scene DSL Parser
 *
 * Parses scene/prefab DSL tokens into prefab structures.
 *
 * Grammar:
 *   prefab      = "Entity" [name] ["@" position] "{" body "}"
 *   position    = "(" number "," number ")"
 *   body        = (component | child)*
 *   component   = identifier ":" value
 *   child       = prefab
 *   value       = string | number | identifier | vector
 *   vector      = "(" number ("," number)* ")"
 */

#include "scene_internal.h"
#include "agentite/prefab.h"
#include "agentite/ecs_reflect.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Parser Structure
 * ============================================================================ */

typedef struct Agentite_Parser {
    Agentite_Lexer lexer;
    Agentite_Token current;
    Agentite_Token previous;
    const Agentite_ReflectRegistry *reflect;

    char error[512];
    bool has_error;
    bool panic_mode;
} Agentite_Parser;

/* Thread-local error storage */
static thread_local char s_last_error[512] = {0};

/* ============================================================================
 * Parser Helpers
 * ============================================================================ */

static void parser_advance(Agentite_Parser *p) {
    p->previous = p->current;

    for (;;) {
        p->current = agentite_lexer_next(&p->lexer);
        if (p->current.type != TOK_ERROR) break;

        /* Store error and continue */
        if (!p->has_error) {
            snprintf(p->error, sizeof(p->error), "%s", p->lexer.error);
            snprintf(s_last_error, sizeof(s_last_error), "%s", p->lexer.error);
            p->has_error = true;
        }
    }
}

static bool parser_check(Agentite_Parser *p, Agentite_TokenType type) {
    return p->current.type == type;
}

static bool parser_match(Agentite_Parser *p, Agentite_TokenType type) {
    if (!parser_check(p, type)) return false;
    parser_advance(p);
    return true;
}

static void parser_error(Agentite_Parser *p, const char *message) {
    if (p->panic_mode) return;
    p->panic_mode = true;
    p->has_error = true;

    snprintf(p->error, sizeof(p->error), "%s:%d:%d: %s",
             p->lexer.name ? p->lexer.name : "<source>",
             p->current.line, p->current.column, message);
    snprintf(s_last_error, sizeof(s_last_error), "%s", p->error);
}

static void parser_error_at_current(Agentite_Parser *p, const char *message) {
    parser_error(p, message);
}

static bool parser_consume(Agentite_Parser *p, Agentite_TokenType type,
                           const char *message) {
    if (p->current.type == type) {
        parser_advance(p);
        return true;
    }

    parser_error_at_current(p, message);
    return false;
}

static char *parser_copy_token(const Agentite_Token *token) {
    return agentite_token_to_string(token);
}

/* ============================================================================
 * Value Parsing
 * ============================================================================ */

static bool parse_number(Agentite_Parser *p, Agentite_PropValue *value) {
    bool negative = false;

    if (parser_match(p, TOK_MINUS)) {
        negative = true;
    }

    if (parser_check(p, TOK_INT)) {
        value->type = AGENTITE_PROP_INT;
        value->int_val = p->current.int_val;
        if (negative) value->int_val = -value->int_val;
        parser_advance(p);
        return true;
    }

    if (parser_check(p, TOK_FLOAT)) {
        value->type = AGENTITE_PROP_FLOAT;
        value->float_val = p->current.float_val;
        if (negative) value->float_val = -value->float_val;
        parser_advance(p);
        return true;
    }

    parser_error(p, "Expected number");
    return false;
}

static bool parse_value(Agentite_Parser *p, Agentite_PropValue *value) {
    memset(value, 0, sizeof(*value));

    /* String literal */
    if (parser_check(p, TOK_STRING)) {
        value->type = AGENTITE_PROP_STRING;
        value->string_val = parser_copy_token(&p->current);
        parser_advance(p);
        return true;
    }

    /* Identifier (could be true/false or enum value) */
    if (parser_check(p, TOK_IDENTIFIER)) {
        char *name = parser_copy_token(&p->current);
        parser_advance(p);

        if (strcmp(name, "true") == 0) {
            value->type = AGENTITE_PROP_BOOL;
            value->bool_val = true;
            free(name);
        } else if (strcmp(name, "false") == 0) {
            value->type = AGENTITE_PROP_BOOL;
            value->bool_val = false;
            free(name);
        } else {
            value->type = AGENTITE_PROP_IDENTIFIER;
            value->string_val = name;
        }
        return true;
    }

    /* Negative number */
    if (parser_check(p, TOK_MINUS)) {
        return parse_number(p, value);
    }

    /* Positive number */
    if (parser_check(p, TOK_INT) || parser_check(p, TOK_FLOAT)) {
        return parse_number(p, value);
    }

    /* Vector: (x, y) or (x, y, z) or (x, y, z, w) */
    if (parser_match(p, TOK_LPAREN)) {
        float components[4] = {0};
        int count = 0;

        do {
            if (count >= 4) {
                parser_error(p, "Vector has too many components (max 4)");
                return false;
            }

            Agentite_PropValue num;
            if (!parse_number(p, &num)) return false;

            if (num.type == AGENTITE_PROP_INT) {
                components[count++] = (float)num.int_val;
            } else {
                components[count++] = (float)num.float_val;
            }
        } while (parser_match(p, TOK_COMMA));

        if (!parser_consume(p, TOK_RPAREN, "Expected ')' after vector")) {
            return false;
        }

        switch (count) {
            case 2:
                value->type = AGENTITE_PROP_VEC2;
                value->vec2_val[0] = components[0];
                value->vec2_val[1] = components[1];
                break;
            case 3:
                value->type = AGENTITE_PROP_VEC3;
                value->vec3_val[0] = components[0];
                value->vec3_val[1] = components[1];
                value->vec3_val[2] = components[2];
                break;
            case 4:
                value->type = AGENTITE_PROP_VEC4;
                value->vec4_val[0] = components[0];
                value->vec4_val[1] = components[1];
                value->vec4_val[2] = components[2];
                value->vec4_val[3] = components[3];
                break;
            default:
                parser_error(p, "Vector must have 2-4 components");
                return false;
        }
        return true;
    }

    parser_error(p, "Expected value");
    return false;
}

/* ============================================================================
 * Prefab Parsing
 * ============================================================================ */

static Agentite_Prefab *create_prefab(void) {
    Agentite_Prefab *prefab = (Agentite_Prefab *)calloc(1, sizeof(Agentite_Prefab));
    return prefab;
}

static void destroy_prop_value(Agentite_PropValue *value) {
    if (value->type == AGENTITE_PROP_STRING ||
        value->type == AGENTITE_PROP_IDENTIFIER) {
        free(value->string_val);
        value->string_val = NULL;
    }
}

static void destroy_component_config(Agentite_ComponentConfig *config) {
    free(config->component_name);
    for (int i = 0; i < config->field_count; i++) {
        free(config->fields[i].field_name);
        destroy_prop_value(&config->fields[i].value);
    }
}

void agentite_prefab_destroy(Agentite_Prefab *prefab) {
    if (!prefab) return;

    free(prefab->name);
    free(prefab->path);
    free(prefab->base_prefab_name);

    for (int i = 0; i < prefab->component_count; i++) {
        destroy_component_config(&prefab->components[i]);
    }

    for (int i = 0; i < prefab->child_count; i++) {
        agentite_prefab_destroy(prefab->children[i]);
    }

    free(prefab);
}

static bool parse_position(Agentite_Parser *p, float *x, float *y) {
    if (!parser_consume(p, TOK_LPAREN, "Expected '(' after '@'")) return false;

    Agentite_PropValue vx, vy;
    if (!parse_number(p, &vx)) return false;

    if (!parser_consume(p, TOK_COMMA, "Expected ',' in position")) return false;

    if (!parse_number(p, &vy)) return false;

    if (!parser_consume(p, TOK_RPAREN, "Expected ')' after position")) return false;

    *x = (vx.type == AGENTITE_PROP_INT) ? (float)vx.int_val : (float)vx.float_val;
    *y = (vy.type == AGENTITE_PROP_INT) ? (float)vy.int_val : (float)vy.float_val;

    return true;
}

static bool parse_component(Agentite_Parser *p, Agentite_ComponentConfig *config) {
    /* Component syntax:
     *   ComponentName: value
     *   OR
     *   ComponentName: { field: value, field: value }
     */

    config->component_name = parser_copy_token(&p->previous);
    config->field_count = 0;

    if (!parser_consume(p, TOK_COLON, "Expected ':' after component name")) {
        return false;
    }

    /* Check for block syntax { field: value, ... } */
    if (parser_match(p, TOK_LBRACE)) {
        while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
            if (config->field_count >= AGENTITE_PREFAB_MAX_FIELDS) {
                parser_error(p, "Too many fields in component");
                return false;
            }

            if (!parser_check(p, TOK_IDENTIFIER)) {
                parser_error(p, "Expected field name");
                return false;
            }

            Agentite_FieldAssign *field = &config->fields[config->field_count];
            field->field_name = parser_copy_token(&p->current);
            parser_advance(p);

            if (!parser_consume(p, TOK_COLON, "Expected ':' after field name")) {
                return false;
            }

            if (!parse_value(p, &field->value)) {
                return false;
            }

            config->field_count++;

            /* Optional comma between fields */
            parser_match(p, TOK_COMMA);
        }

        if (!parser_consume(p, TOK_RBRACE, "Expected '}' after component fields")) {
            return false;
        }
    } else {
        /* Simple single-value syntax: ComponentName: value
         * The value becomes a field named after the component (lowercase first char)
         * or we use a convention like "value" as the field name
         */
        if (config->field_count >= AGENTITE_PREFAB_MAX_FIELDS) {
            parser_error(p, "Too many fields in component");
            return false;
        }

        Agentite_FieldAssign *field = &config->fields[0];

        /* Use first field or a default name based on value type */
        field->field_name = strdup("value");

        if (!parse_value(p, &field->value)) {
            return false;
        }

        config->field_count = 1;
    }

    return true;
}

static Agentite_Prefab *parse_entity(Agentite_Parser *p);

static bool parse_body(Agentite_Parser *p, Agentite_Prefab *prefab) {
    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        if (p->has_error) return false;

        if (!parser_check(p, TOK_IDENTIFIER)) {
            parser_error(p, "Expected component name or 'Entity'");
            return false;
        }

        char *name = parser_copy_token(&p->current);
        parser_advance(p);

        if (strcmp(name, "Entity") == 0) {
            free(name);
            /* Nested entity/child */
            if (prefab->child_count >= AGENTITE_PREFAB_MAX_CHILDREN) {
                parser_error(p, "Too many child entities");
                return false;
            }

            Agentite_Prefab *child = parse_entity(p);
            if (!child) return false;

            prefab->children[prefab->child_count++] = child;
        } else if (strcmp(name, "prefab") == 0) {
            free(name);
            /* Reference to base prefab: prefab: "path/to/prefab" */
            if (!parser_consume(p, TOK_COLON, "Expected ':' after 'prefab'")) {
                return false;
            }

            if (!parser_check(p, TOK_STRING)) {
                parser_error(p, "Expected string path after 'prefab:'");
                return false;
            }

            prefab->base_prefab_name = parser_copy_token(&p->current);
            parser_advance(p);
        } else {
            /* Component configuration */
            if (prefab->component_count >= AGENTITE_PREFAB_MAX_COMPONENTS) {
                parser_error(p, "Too many components");
                free(name);
                return false;
            }

            /* Put name back for parse_component */
            p->previous.start = p->current.start - strlen(name);
            p->previous.length = (int)strlen(name);
            p->previous.type = TOK_IDENTIFIER;
            free(name);

            Agentite_ComponentConfig *config = &prefab->components[prefab->component_count];
            if (!parse_component(p, config)) {
                return false;
            }
            prefab->component_count++;
        }
    }

    return true;
}

static Agentite_Prefab *parse_entity(Agentite_Parser *p) {
    /* "Entity" was already consumed */
    Agentite_Prefab *prefab = create_prefab();
    if (!prefab) {
        parser_error(p, "Failed to allocate prefab");
        return NULL;
    }

    /* Optional name */
    if (parser_check(p, TOK_IDENTIFIER)) {
        prefab->name = parser_copy_token(&p->current);
        parser_advance(p);
    }

    /* Optional position @(x, y) */
    if (parser_match(p, TOK_AT)) {
        if (!parse_position(p, &prefab->position[0], &prefab->position[1])) {
            agentite_prefab_destroy(prefab);
            return NULL;
        }
    }

    /* Body { ... } */
    if (!parser_consume(p, TOK_LBRACE, "Expected '{' after entity header")) {
        agentite_prefab_destroy(prefab);
        return NULL;
    }

    if (!parse_body(p, prefab)) {
        agentite_prefab_destroy(prefab);
        return NULL;
    }

    if (!parser_consume(p, TOK_RBRACE, "Expected '}' after entity body")) {
        agentite_prefab_destroy(prefab);
        return NULL;
    }

    return prefab;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

Agentite_Prefab *agentite_prefab_load_string(const char *source,
                                              size_t length,
                                              const char *name,
                                              const Agentite_ReflectRegistry *reflect) {
    if (!source) {
        snprintf(s_last_error, sizeof(s_last_error), "Source is NULL");
        return NULL;
    }

    Agentite_Parser parser;
    memset(&parser, 0, sizeof(parser));

    agentite_lexer_init(&parser.lexer, source, length, name);
    parser.reflect = reflect;

    /* Prime the parser */
    parser_advance(&parser);

    /* Must start with "Entity" */
    if (!parser_check(&parser, TOK_IDENTIFIER)) {
        parser_error(&parser, "Expected 'Entity' keyword");
        return NULL;
    }

    char *keyword = parser_copy_token(&parser.current);
    if (strcmp(keyword, "Entity") != 0) {
        free(keyword);
        parser_error(&parser, "Expected 'Entity' keyword");
        return NULL;
    }
    free(keyword);
    parser_advance(&parser);

    Agentite_Prefab *prefab = parse_entity(&parser);

    if (parser.has_error) {
        agentite_prefab_destroy(prefab);
        return NULL;
    }

    return prefab;
}

const char *agentite_prefab_get_error(void) {
    return s_last_error;
}
