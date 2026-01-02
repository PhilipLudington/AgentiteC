/**
 * Agentite Engine - Scene DSL Writer
 *
 * Serializes scene data structures and ECS entities back to DSL format.
 * Inverse of scene_parser.cpp.
 *
 * Output Format:
 *   Entity Name @(x, y) {
 *       ComponentName: value
 *       ComponentName: { field: value, field: value }
 *
 *       Entity Child @(local_x, local_y) {
 *           ...
 *       }
 *   }
 */

#include "scene_internal.h"
#include "agentite/prefab.h"
#include "agentite/scene.h"
#include "agentite/ecs_reflect.h"
#include "agentite/error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "flecs.h"

/* ============================================================================
 * String Builder
 * ============================================================================ */

typedef struct StringBuilder {
    char *data;
    size_t length;
    size_t capacity;
} StringBuilder;

static void sb_init(StringBuilder *sb) {
    sb->capacity = 1024;
    sb->data = (char *)malloc(sb->capacity);
    sb->length = 0;
    if (sb->data) {
        sb->data[0] = '\0';
    }
}

static void sb_destroy(StringBuilder *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->length = 0;
    sb->capacity = 0;
}

static bool sb_ensure_capacity(StringBuilder *sb, size_t additional) {
    size_t required = sb->length + additional + 1;
    if (required <= sb->capacity) return true;

    size_t new_capacity = sb->capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    char *new_data = (char *)realloc(sb->data, new_capacity);
    if (!new_data) return false;

    sb->data = new_data;
    sb->capacity = new_capacity;
    return true;
}

static bool sb_append(StringBuilder *sb, const char *str) {
    if (!str) return true;
    size_t len = strlen(str);
    if (!sb_ensure_capacity(sb, len)) return false;

    memcpy(sb->data + sb->length, str, len);
    sb->length += len;
    sb->data[sb->length] = '\0';
    return true;
}

static bool sb_append_char(StringBuilder *sb, char c) {
    if (!sb_ensure_capacity(sb, 1)) return false;
    sb->data[sb->length++] = c;
    sb->data[sb->length] = '\0';
    return true;
}

static bool sb_append_indent(StringBuilder *sb, int depth) {
    for (int i = 0; i < depth; i++) {
        if (!sb_append(sb, "    ")) return false;
    }
    return true;
}

static bool sb_appendf(StringBuilder *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    /* First pass: determine required size */
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return false;
    }

    if (!sb_ensure_capacity(sb, (size_t)needed)) {
        va_end(args);
        return false;
    }

    vsnprintf(sb->data + sb->length, needed + 1, fmt, args);
    sb->length += needed;
    va_end(args);

    return true;
}

static char *sb_detach(StringBuilder *sb) {
    char *result = sb->data;
    sb->data = NULL;
    sb->length = 0;
    sb->capacity = 0;
    return result;
}

/* ============================================================================
 * Value Writing
 * ============================================================================ */

static bool needs_escape(const char *str) {
    for (const char *p = str; *p; p++) {
        if (*p == '"' || *p == '\\' || *p == '\n' || *p == '\r' || *p == '\t') {
            return true;
        }
    }
    return false;
}

static bool write_escaped_string(StringBuilder *sb, const char *str) {
    if (!sb_append_char(sb, '"')) return false;

    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':  if (!sb_append(sb, "\\\"")) return false; break;
            case '\\': if (!sb_append(sb, "\\\\")) return false; break;
            case '\n': if (!sb_append(sb, "\\n")) return false; break;
            case '\r': if (!sb_append(sb, "\\r")) return false; break;
            case '\t': if (!sb_append(sb, "\\t")) return false; break;
            default:   if (!sb_append_char(sb, *p)) return false; break;
        }
    }

    return sb_append_char(sb, '"');
}

static bool is_valid_identifier(const char *str) {
    if (!str || !*str) return false;
    if (!isalpha((unsigned char)*str) && *str != '_') return false;

    for (const char *p = str + 1; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') return false;
    }
    return true;
}

static bool write_prop_value(StringBuilder *sb, const Agentite_PropValue *value) {
    switch (value->type) {
        case AGENTITE_PROP_NULL:
            return sb_append(sb, "null");

        case AGENTITE_PROP_INT:
            return sb_appendf(sb, "%lld", (long long)value->int_val);

        case AGENTITE_PROP_FLOAT: {
            /* Write with appropriate precision */
            double v = value->float_val;
            if (floor(v) == v && fabs(v) < 1e9) {
                /* Integer-valued float */
                return sb_appendf(sb, "%.1f", v);
            } else {
                return sb_appendf(sb, "%g", v);
            }
        }

        case AGENTITE_PROP_BOOL:
            return sb_append(sb, value->bool_val ? "true" : "false");

        case AGENTITE_PROP_STRING:
            return write_escaped_string(sb, value->string_val ? value->string_val : "");

        case AGENTITE_PROP_IDENTIFIER:
            if (is_valid_identifier(value->string_val)) {
                return sb_append(sb, value->string_val);
            } else {
                return write_escaped_string(sb, value->string_val ? value->string_val : "");
            }

        case AGENTITE_PROP_VEC2:
            return sb_appendf(sb, "(%g, %g)",
                              (double)value->vec2_val[0],
                              (double)value->vec2_val[1]);

        case AGENTITE_PROP_VEC3:
            return sb_appendf(sb, "(%g, %g, %g)",
                              (double)value->vec3_val[0],
                              (double)value->vec3_val[1],
                              (double)value->vec3_val[2]);

        case AGENTITE_PROP_VEC4:
            return sb_appendf(sb, "(%g, %g, %g, %g)",
                              (double)value->vec4_val[0],
                              (double)value->vec4_val[1],
                              (double)value->vec4_val[2],
                              (double)value->vec4_val[3]);

        default:
            return sb_append(sb, "null");
    }
}

/* ============================================================================
 * Component Writing
 * ============================================================================ */

static bool write_component(StringBuilder *sb,
                            const Agentite_ComponentConfig *config,
                            int indent) {
    if (!sb_append_indent(sb, indent)) return false;
    if (!sb_append(sb, config->component_name)) return false;
    if (!sb_append(sb, ": ")) return false;

    if (config->field_count == 0) {
        /* Empty component - write null or default */
        if (!sb_append(sb, "true\n")) return false;
    } else if (config->field_count == 1 &&
               strcmp(config->fields[0].field_name, "value") == 0) {
        /* Single "value" field - use shorthand */
        if (!write_prop_value(sb, &config->fields[0].value)) return false;
        if (!sb_append_char(sb, '\n')) return false;
    } else {
        /* Multiple fields - use block syntax */
        if (!sb_append(sb, "{\n")) return false;

        for (int i = 0; i < config->field_count; i++) {
            const Agentite_FieldAssign *field = &config->fields[i];

            if (!sb_append_indent(sb, indent + 1)) return false;
            if (!sb_append(sb, field->field_name)) return false;
            if (!sb_append(sb, ": ")) return false;
            if (!write_prop_value(sb, &field->value)) return false;
            if (!sb_append_char(sb, '\n')) return false;
        }

        if (!sb_append_indent(sb, indent)) return false;
        if (!sb_append(sb, "}\n")) return false;
    }

    return true;
}

/* ============================================================================
 * Prefab Writing
 * ============================================================================ */

static bool write_prefab_internal(StringBuilder *sb,
                                   const Agentite_Prefab *prefab,
                                   int indent) {
    if (!prefab) return false;

    /* Entity header */
    if (!sb_append_indent(sb, indent)) return false;
    if (!sb_append(sb, "Entity")) return false;

    /* Optional name */
    if (prefab->name && prefab->name[0]) {
        if (!sb_append_char(sb, ' ')) return false;
        if (!sb_append(sb, prefab->name)) return false;
    }

    /* Position if non-zero */
    if (prefab->position[0] != 0.0f || prefab->position[1] != 0.0f) {
        if (!sb_appendf(sb, " @(%g, %g)",
                        (double)prefab->position[0],
                        (double)prefab->position[1])) return false;
    }

    /* Body */
    if (!sb_append(sb, " {\n")) return false;

    /* Base prefab reference */
    if (prefab->base_prefab_name && prefab->base_prefab_name[0]) {
        if (!sb_append_indent(sb, indent + 1)) return false;
        if (!sb_append(sb, "prefab: ")) return false;
        if (!write_escaped_string(sb, prefab->base_prefab_name)) return false;
        if (!sb_append_char(sb, '\n')) return false;
    }

    /* Components */
    for (int i = 0; i < prefab->component_count; i++) {
        if (!write_component(sb, &prefab->components[i], indent + 1)) {
            return false;
        }
    }

    /* Children */
    if (prefab->child_count > 0 && prefab->component_count > 0) {
        if (!sb_append_char(sb, '\n')) return false;  /* Blank line before children */
    }

    for (int i = 0; i < prefab->child_count; i++) {
        if (!write_prefab_internal(sb, prefab->children[i], indent + 1)) {
            return false;
        }
    }

    /* Close brace */
    if (!sb_append_indent(sb, indent)) return false;
    if (!sb_append(sb, "}\n")) return false;

    return true;
}

/* ============================================================================
 * Public Prefab Writing API
 * ============================================================================ */

extern "C" {

char *agentite_prefab_write_string(const Agentite_Prefab *prefab) {
    if (!prefab) {
        agentite_set_error("scene_writer: prefab is NULL");
        return NULL;
    }

    StringBuilder sb;
    sb_init(&sb);

    if (!sb.data) {
        agentite_set_error("scene_writer: Failed to allocate string builder");
        return NULL;
    }

    if (!write_prefab_internal(&sb, prefab, 0)) {
        sb_destroy(&sb);
        agentite_set_error("scene_writer: Failed to write prefab");
        return NULL;
    }

    return sb_detach(&sb);
}

bool agentite_prefab_write_file(const Agentite_Prefab *prefab, const char *path) {
    if (!prefab || !path) {
        agentite_set_error("scene_writer: Invalid parameters");
        return false;
    }

    char *content = agentite_prefab_write_string(prefab);
    if (!content) {
        return false;
    }

    FILE *file = fopen(path, "w");
    if (!file) {
        free(content);
        agentite_set_error("scene_writer: Failed to open '%s' for writing", path);
        return false;
    }

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);
    free(content);

    if (written != len) {
        agentite_set_error("scene_writer: Failed to write all data to '%s'", path);
        return false;
    }

    return true;
}

/* ============================================================================
 * ECS Entity Writing
 * ============================================================================ */

/**
 * Write a component from ECS entity using reflection.
 */
static bool write_ecs_component(StringBuilder *sb,
                                 ecs_world_t *world,
                                 ecs_entity_t entity,
                                 const Agentite_ComponentMeta *meta,
                                 int indent) {
    const void *data = ecs_get_id(world, entity, meta->component_id);
    if (!data) return true;  /* Component not present */

    if (!sb_append_indent(sb, indent)) return false;
    if (!sb_append(sb, meta->name)) return false;
    if (!sb_append(sb, ": ")) return false;

    if (meta->field_count == 0) {
        /* Tag component (no fields) */
        if (!sb_append(sb, "true\n")) return false;
        return true;
    }

    if (meta->field_count == 1) {
        /* Single field - use shorthand */
        const Agentite_FieldDesc *field = &meta->fields[0];
        const uint8_t *ptr = (const uint8_t *)data + field->offset;

        Agentite_PropValue value;
        memset(&value, 0, sizeof(value));

        switch (field->type) {
            case AGENTITE_FIELD_INT:
                value.type = AGENTITE_PROP_INT;
                value.int_val = *(const int *)ptr;
                break;
            case AGENTITE_FIELD_UINT:
                value.type = AGENTITE_PROP_INT;
                value.int_val = *(const unsigned int *)ptr;
                break;
            case AGENTITE_FIELD_FLOAT:
                value.type = AGENTITE_PROP_FLOAT;
                value.float_val = *(const float *)ptr;
                break;
            case AGENTITE_FIELD_DOUBLE:
                value.type = AGENTITE_PROP_FLOAT;
                value.float_val = *(const double *)ptr;
                break;
            case AGENTITE_FIELD_BOOL:
                value.type = AGENTITE_PROP_BOOL;
                value.bool_val = *(const bool *)ptr;
                break;
            case AGENTITE_FIELD_VEC2:
                value.type = AGENTITE_PROP_VEC2;
                memcpy(value.vec2_val, ptr, sizeof(float) * 2);
                break;
            case AGENTITE_FIELD_VEC3:
                value.type = AGENTITE_PROP_VEC3;
                memcpy(value.vec3_val, ptr, sizeof(float) * 3);
                break;
            case AGENTITE_FIELD_VEC4:
                value.type = AGENTITE_PROP_VEC4;
                memcpy(value.vec4_val, ptr, sizeof(float) * 4);
                break;
            case AGENTITE_FIELD_STRING:
                value.type = AGENTITE_PROP_STRING;
                value.string_val = (char *)*(const char **)ptr;
                break;
            default:
                value.type = AGENTITE_PROP_NULL;
                break;
        }

        if (!write_prop_value(sb, &value)) return false;
        if (!sb_append_char(sb, '\n')) return false;
    } else {
        /* Multiple fields - use block syntax */
        if (!sb_append(sb, "{\n")) return false;

        for (int i = 0; i < meta->field_count; i++) {
            const Agentite_FieldDesc *field = &meta->fields[i];
            const uint8_t *ptr = (const uint8_t *)data + field->offset;

            Agentite_PropValue value;
            memset(&value, 0, sizeof(value));

            switch (field->type) {
                case AGENTITE_FIELD_INT:
                    value.type = AGENTITE_PROP_INT;
                    value.int_val = *(const int *)ptr;
                    break;
                case AGENTITE_FIELD_UINT:
                    value.type = AGENTITE_PROP_INT;
                    value.int_val = *(const unsigned int *)ptr;
                    break;
                case AGENTITE_FIELD_FLOAT:
                    value.type = AGENTITE_PROP_FLOAT;
                    value.float_val = *(const float *)ptr;
                    break;
                case AGENTITE_FIELD_DOUBLE:
                    value.type = AGENTITE_PROP_FLOAT;
                    value.float_val = *(const double *)ptr;
                    break;
                case AGENTITE_FIELD_BOOL:
                    value.type = AGENTITE_PROP_BOOL;
                    value.bool_val = *(const bool *)ptr;
                    break;
                case AGENTITE_FIELD_VEC2:
                    value.type = AGENTITE_PROP_VEC2;
                    memcpy(value.vec2_val, ptr, sizeof(float) * 2);
                    break;
                case AGENTITE_FIELD_VEC3:
                    value.type = AGENTITE_PROP_VEC3;
                    memcpy(value.vec3_val, ptr, sizeof(float) * 3);
                    break;
                case AGENTITE_FIELD_VEC4:
                    value.type = AGENTITE_PROP_VEC4;
                    memcpy(value.vec4_val, ptr, sizeof(float) * 4);
                    break;
                case AGENTITE_FIELD_STRING:
                    value.type = AGENTITE_PROP_STRING;
                    value.string_val = (char *)*(const char **)ptr;
                    break;
                case AGENTITE_FIELD_INT8:
                    value.type = AGENTITE_PROP_INT;
                    value.int_val = *(const int8_t *)ptr;
                    break;
                case AGENTITE_FIELD_UINT8:
                    value.type = AGENTITE_PROP_INT;
                    value.int_val = *(const uint8_t *)ptr;
                    break;
                case AGENTITE_FIELD_INT16:
                    value.type = AGENTITE_PROP_INT;
                    value.int_val = *(const int16_t *)ptr;
                    break;
                case AGENTITE_FIELD_UINT16:
                    value.type = AGENTITE_PROP_INT;
                    value.int_val = *(const uint16_t *)ptr;
                    break;
                case AGENTITE_FIELD_INT64:
                    value.type = AGENTITE_PROP_INT;
                    value.int_val = *(const int64_t *)ptr;
                    break;
                case AGENTITE_FIELD_UINT64:
                    value.type = AGENTITE_PROP_INT;
                    value.int_val = (int64_t)*(const uint64_t *)ptr;
                    break;
                default:
                    value.type = AGENTITE_PROP_NULL;
                    break;
            }

            if (!sb_append_indent(sb, indent + 1)) return false;
            if (!sb_append(sb, field->name)) return false;
            if (!sb_append(sb, ": ")) return false;
            if (!write_prop_value(sb, &value)) return false;
            if (!sb_append_char(sb, '\n')) return false;
        }

        if (!sb_append_indent(sb, indent)) return false;
        if (!sb_append(sb, "}\n")) return false;
    }

    return true;
}

/**
 * Write an ECS entity and its children to DSL format.
 */
static bool write_ecs_entity(StringBuilder *sb,
                              ecs_world_t *world,
                              ecs_entity_t entity,
                              const Agentite_ReflectRegistry *reflect,
                              int indent) {
    /* Entity header */
    if (!sb_append_indent(sb, indent)) return false;
    if (!sb_append(sb, "Entity")) return false;

    /* Entity name if it has one */
    const char *name = ecs_get_name(world, entity);
    if (name && name[0]) {
        if (!sb_append_char(sb, ' ')) return false;
        if (is_valid_identifier(name)) {
            if (!sb_append(sb, name)) return false;
        } else {
            /* Quote non-identifier names */
            if (!write_escaped_string(sb, name)) return false;
        }
    }

    /* Check for position component */
    const Agentite_ComponentMeta *pos_meta =
        agentite_reflect_get_by_name(reflect, "C_Position");
    if (pos_meta) {
        const void *pos_data = ecs_get_id(world, entity, pos_meta->component_id);
        if (pos_data && pos_meta->field_count >= 2) {
            const float *pos = (const float *)pos_data;
            if (pos[0] != 0.0f || pos[1] != 0.0f) {
                if (!sb_appendf(sb, " @(%g, %g)",
                                (double)pos[0], (double)pos[1])) return false;
            }
        }
    }

    if (!sb_append(sb, " {\n")) return false;

    /* Write all registered components */
    size_t comp_count = agentite_reflect_count(reflect);
    for (size_t i = 0; i < comp_count; i++) {
        const Agentite_ComponentMeta *meta = agentite_reflect_get_by_index(reflect, i);
        if (!meta) continue;

        /* Skip position component (already in header) */
        if (pos_meta && meta->component_id == pos_meta->component_id) continue;

        /* Skip internal/system components */
        if (strncmp(meta->name, "flecs.", 6) == 0) continue;
        if (strncmp(meta->name, "ecs.", 4) == 0) continue;

        if (!write_ecs_component(sb, world, entity, meta, indent + 1)) {
            return false;
        }
    }

    /* Write children */
    ecs_iter_t it = ecs_children(world, entity);
    bool has_children = ecs_children_next(&it);
    if (has_children && it.count > 0) {
        if (!sb_append_char(sb, '\n')) return false;  /* Blank line before children */
    }

    if (has_children) {
        do {
            for (int i = 0; i < it.count; i++) {
                if (!write_ecs_entity(sb, world, it.entities[i], reflect, indent + 1)) {
                    return false;
                }
            }
        } while (ecs_children_next(&it));
    }

    /* Close brace */
    if (!sb_append_indent(sb, indent)) return false;
    if (!sb_append(sb, "}\n")) return false;

    return true;
}

char *agentite_scene_write_entities(ecs_world_t *world,
                                     const ecs_entity_t *entities,
                                     size_t count,
                                     const Agentite_ReflectRegistry *reflect) {
    if (!world || !entities || count == 0 || !reflect) {
        agentite_set_error("scene_writer: Invalid parameters");
        return NULL;
    }

    StringBuilder sb;
    sb_init(&sb);

    if (!sb.data) {
        agentite_set_error("scene_writer: Failed to allocate string builder");
        return NULL;
    }

    for (size_t i = 0; i < count; i++) {
        if (!ecs_is_alive(world, entities[i])) continue;

        /* Skip if entity is a child (will be written with parent) */
        if (ecs_get_target(world, entities[i], EcsChildOf, 0) != 0) continue;

        if (!write_ecs_entity(&sb, world, entities[i], reflect, 0)) {
            sb_destroy(&sb);
            agentite_set_error("scene_writer: Failed to write entity");
            return NULL;
        }

        /* Blank line between root entities */
        if (i < count - 1) {
            if (!sb_append_char(&sb, '\n')) {
                sb_destroy(&sb);
                return NULL;
            }
        }
    }

    return sb_detach(&sb);
}

} /* extern "C" */
