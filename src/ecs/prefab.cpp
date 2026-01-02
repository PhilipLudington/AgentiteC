/**
 * Agentite Engine - Prefab System Implementation
 *
 * Prefab registry, loading, and spawning.
 */

#include "agentite/prefab.h"
#include "agentite/ecs_reflect.h"
#include "agentite/ecs.h"
#include "agentite/error.h"
#include "agentite/asset.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "flecs.h"

/* Forward declarations for parser functions */
extern "C" {
    Agentite_Prefab *agentite_prefab_load_string(const char *source,
                                                  size_t length,
                                                  const char *name,
                                                  const Agentite_ReflectRegistry *reflect);
    void agentite_prefab_destroy(Agentite_Prefab *prefab);
}

/* ============================================================================
 * Registry Structure
 * ============================================================================ */

#define PREFAB_REGISTRY_CAPACITY 256

typedef struct PrefabEntry {
    char *path;
    Agentite_Prefab *prefab;
} PrefabEntry;

struct Agentite_PrefabRegistry {
    PrefabEntry entries[PREFAB_REGISTRY_CAPACITY];
    size_t count;
};

/* ============================================================================
 * Registry Implementation
 * ============================================================================ */

Agentite_PrefabRegistry *agentite_prefab_registry_create(void) {
    Agentite_PrefabRegistry *registry = (Agentite_PrefabRegistry *)
        calloc(1, sizeof(Agentite_PrefabRegistry));
    return registry;
}

void agentite_prefab_registry_destroy(Agentite_PrefabRegistry *registry) {
    if (!registry) return;

    agentite_prefab_registry_clear(registry);
    free(registry);
}

void agentite_prefab_registry_clear(Agentite_PrefabRegistry *registry) {
    if (!registry) return;

    for (size_t i = 0; i < registry->count; i++) {
        free(registry->entries[i].path);
        agentite_prefab_destroy(registry->entries[i].prefab);
    }
    registry->count = 0;
}

size_t agentite_prefab_registry_count(const Agentite_PrefabRegistry *registry) {
    return registry ? registry->count : 0;
}

/* ============================================================================
 * Prefab Loading
 * ============================================================================ */

static char *read_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        agentite_set_error("prefab: Failed to open file '%s'", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fclose(file);
        agentite_set_error("prefab: Failed to get file size '%s'", path);
        return NULL;
    }

    char *buffer = (char *)malloc(size + 1);
    if (!buffer) {
        fclose(file);
        agentite_set_error("prefab: Failed to allocate buffer for '%s'", path);
        return NULL;
    }

    size_t read = fread(buffer, 1, size, file);
    fclose(file);

    buffer[read] = '\0';
    if (out_size) *out_size = read;

    return buffer;
}

Agentite_Prefab *agentite_prefab_lookup(Agentite_PrefabRegistry *registry,
                                         const char *path) {
    if (!registry || !path) return NULL;

    for (size_t i = 0; i < registry->count; i++) {
        if (strcmp(registry->entries[i].path, path) == 0) {
            return registry->entries[i].prefab;
        }
    }

    return NULL;
}

Agentite_Prefab *agentite_prefab_load(Agentite_PrefabRegistry *registry,
                                       const char *path,
                                       const Agentite_ReflectRegistry *reflect) {
    if (!registry || !path) {
        agentite_set_error("prefab: Invalid parameters");
        return NULL;
    }

    /* Check cache */
    Agentite_Prefab *existing = agentite_prefab_lookup(registry, path);
    if (existing) {
        return existing;
    }

    /* Check capacity */
    if (registry->count >= PREFAB_REGISTRY_CAPACITY) {
        agentite_set_error("prefab: Registry is full");
        return NULL;
    }

    /* Read file */
    size_t size;
    char *source = read_file(path, &size);
    if (!source) {
        return NULL;
    }

    /* Parse */
    Agentite_Prefab *prefab = agentite_prefab_load_string(source, size, path, reflect);
    free(source);

    if (!prefab) {
        agentite_set_error("prefab: Failed to parse '%s': %s",
                           path, agentite_prefab_get_error());
        return NULL;
    }

    /* Store path in prefab */
    prefab->path = strdup(path);

    /* Add to registry */
    PrefabEntry *entry = &registry->entries[registry->count++];
    entry->path = strdup(path);
    entry->prefab = prefab;

    return prefab;
}

/* ============================================================================
 * Field Value Application
 * ============================================================================ */

static bool apply_field_value(void *component_data,
                               const Agentite_FieldDesc *field,
                               const Agentite_PropValue *value) {
    uint8_t *ptr = (uint8_t *)component_data + field->offset;

    switch (field->type) {
        case AGENTITE_FIELD_INT:
            if (value->type == AGENTITE_PROP_INT) {
                *(int *)ptr = (int)value->int_val;
                return true;
            } else if (value->type == AGENTITE_PROP_FLOAT) {
                *(int *)ptr = (int)value->float_val;
                return true;
            }
            break;

        case AGENTITE_FIELD_UINT:
            if (value->type == AGENTITE_PROP_INT) {
                *(unsigned int *)ptr = (unsigned int)value->int_val;
                return true;
            }
            break;

        case AGENTITE_FIELD_FLOAT:
            if (value->type == AGENTITE_PROP_FLOAT) {
                *(float *)ptr = (float)value->float_val;
                return true;
            } else if (value->type == AGENTITE_PROP_INT) {
                *(float *)ptr = (float)value->int_val;
                return true;
            }
            break;

        case AGENTITE_FIELD_DOUBLE:
            if (value->type == AGENTITE_PROP_FLOAT) {
                *(double *)ptr = value->float_val;
                return true;
            } else if (value->type == AGENTITE_PROP_INT) {
                *(double *)ptr = (double)value->int_val;
                return true;
            }
            break;

        case AGENTITE_FIELD_BOOL:
            if (value->type == AGENTITE_PROP_BOOL) {
                *(bool *)ptr = value->bool_val;
                return true;
            }
            break;

        case AGENTITE_FIELD_VEC2:
            if (value->type == AGENTITE_PROP_VEC2) {
                float *v = (float *)ptr;
                v[0] = value->vec2_val[0];
                v[1] = value->vec2_val[1];
                return true;
            }
            break;

        case AGENTITE_FIELD_VEC3:
            if (value->type == AGENTITE_PROP_VEC3) {
                float *v = (float *)ptr;
                v[0] = value->vec3_val[0];
                v[1] = value->vec3_val[1];
                v[2] = value->vec3_val[2];
                return true;
            }
            break;

        case AGENTITE_FIELD_VEC4:
            if (value->type == AGENTITE_PROP_VEC4) {
                float *v = (float *)ptr;
                v[0] = value->vec4_val[0];
                v[1] = value->vec4_val[1];
                v[2] = value->vec4_val[2];
                v[3] = value->vec4_val[3];
                return true;
            }
            break;

        case AGENTITE_FIELD_STRING:
            if (value->type == AGENTITE_PROP_STRING ||
                value->type == AGENTITE_PROP_IDENTIFIER) {
                /* Note: This stores a borrowed pointer - the string must remain valid */
                *(const char **)ptr = value->string_val;
                return true;
            }
            break;

        case AGENTITE_FIELD_INT8:
            if (value->type == AGENTITE_PROP_INT) {
                *(int8_t *)ptr = (int8_t)value->int_val;
                return true;
            }
            break;

        case AGENTITE_FIELD_UINT8:
            if (value->type == AGENTITE_PROP_INT) {
                *(uint8_t *)ptr = (uint8_t)value->int_val;
                return true;
            }
            break;

        case AGENTITE_FIELD_INT16:
            if (value->type == AGENTITE_PROP_INT) {
                *(int16_t *)ptr = (int16_t)value->int_val;
                return true;
            }
            break;

        case AGENTITE_FIELD_UINT16:
            if (value->type == AGENTITE_PROP_INT) {
                *(uint16_t *)ptr = (uint16_t)value->int_val;
                return true;
            }
            break;

        case AGENTITE_FIELD_INT64:
            if (value->type == AGENTITE_PROP_INT) {
                *(int64_t *)ptr = value->int_val;
                return true;
            }
            break;

        case AGENTITE_FIELD_UINT64:
            if (value->type == AGENTITE_PROP_INT) {
                *(uint64_t *)ptr = (uint64_t)value->int_val;
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

/* ============================================================================
 * Prefab Spawning
 * ============================================================================ */

static ecs_entity_t spawn_prefab_internal(const Agentite_Prefab *prefab,
                                           const Agentite_SpawnContext *ctx,
                                           float pos_x, float pos_y) {
    if (!prefab || !ctx || !ctx->world) return 0;

    ecs_world_t *world = ctx->world;

    /* Create entity */
    ecs_entity_t entity;
    if (prefab->name && prefab->name[0]) {
        ecs_entity_desc_t desc = {};
        desc.name = prefab->name;
        entity = ecs_entity_init(world, &desc);
    } else {
        entity = ecs_new(world);
    }

    if (!entity) return 0;

    /* Set parent if specified */
    if (ctx->parent) {
        ecs_add_pair(world, entity, EcsChildOf, ctx->parent);
    }

    /* Apply base prefab if specified */
    if (prefab->base_prefab_name && ctx->prefabs) {
        Agentite_Prefab *base = agentite_prefab_lookup(ctx->prefabs,
                                                        prefab->base_prefab_name);
        if (base) {
            /* Recursively apply base prefab's components */
            Agentite_SpawnContext base_ctx = *ctx;
            base_ctx.parent = 0;  /* Don't re-parent */

            /* Apply base components to this entity */
            for (int i = 0; i < base->component_count; i++) {
                const Agentite_ComponentConfig *config = &base->components[i];

                if (!ctx->reflect) continue;

                const Agentite_ComponentMeta *meta =
                    agentite_reflect_get_by_name(ctx->reflect, config->component_name);
                if (!meta) continue;

                /* Allocate temp buffer for component data */
                void *data = calloc(1, meta->size);
                if (!data) continue;

                /* Apply field values */
                for (int j = 0; j < config->field_count; j++) {
                    const Agentite_FieldAssign *assign = &config->fields[j];

                    /* Find matching field */
                    for (int k = 0; k < meta->field_count; k++) {
                        if (strcmp(meta->fields[k].name, assign->field_name) == 0) {
                            apply_field_value(data, &meta->fields[k], &assign->value);
                            break;
                        }
                    }
                }

                /* Set component on entity */
                ecs_set_id(world, entity, meta->component_id, meta->size, data);
                free(data);
            }
        }
    }

    /* Apply this prefab's components */
    for (int i = 0; i < prefab->component_count; i++) {
        const Agentite_ComponentConfig *config = &prefab->components[i];

        if (!ctx->reflect) continue;

        const Agentite_ComponentMeta *meta =
            agentite_reflect_get_by_name(ctx->reflect, config->component_name);
        if (!meta) {
            /* Component not found in reflection registry - skip */
            continue;
        }

        /* Allocate temp buffer for component data */
        void *data = calloc(1, meta->size);
        if (!data) continue;

        /* Apply field values */
        for (int j = 0; j < config->field_count; j++) {
            const Agentite_FieldAssign *assign = &config->fields[j];

            /* Special case: single "value" field maps to first component field */
            if (strcmp(assign->field_name, "value") == 0 && meta->field_count > 0) {
                apply_field_value(data, &meta->fields[0], &assign->value);
                continue;
            }

            /* Find matching field by name */
            for (int k = 0; k < meta->field_count; k++) {
                if (strcmp(meta->fields[k].name, assign->field_name) == 0) {
                    apply_field_value(data, &meta->fields[k], &assign->value);
                    break;
                }
            }
        }

        /* Set component on entity */
        ecs_set_id(world, entity, meta->component_id, meta->size, data);
        free(data);
    }

    /* Set position component if we have reflection for C_Position */
    if (ctx->reflect) {
        const Agentite_ComponentMeta *pos_meta =
            agentite_reflect_get_by_name(ctx->reflect, "C_Position");
        if (pos_meta && pos_meta->size >= sizeof(float) * 2) {
            /* Allocate and set position component */
            float pos_data[2] = {
                pos_x + prefab->position[0],
                pos_y + prefab->position[1]
            };
            ecs_set_id(world, entity, pos_meta->component_id, pos_meta->size, pos_data);
        }
    }

    /* Spawn child entities */
    for (int i = 0; i < prefab->child_count; i++) {
        Agentite_SpawnContext child_ctx = *ctx;
        child_ctx.parent = entity;
        child_ctx.offset_x = 0;
        child_ctx.offset_y = 0;

        spawn_prefab_internal(prefab->children[i], &child_ctx,
                              prefab->children[i]->position[0],
                              prefab->children[i]->position[1]);
    }

    return entity;
}

ecs_entity_t agentite_prefab_spawn(const Agentite_Prefab *prefab,
                                    const Agentite_SpawnContext *ctx) {
    if (!prefab || !ctx) return 0;

    float pos_x = ctx->offset_x;
    float pos_y = ctx->offset_y;

    return spawn_prefab_internal(prefab, ctx, pos_x, pos_y);
}

ecs_entity_t agentite_prefab_spawn_at(const Agentite_Prefab *prefab,
                                       ecs_world_t *world,
                                       const Agentite_ReflectRegistry *reflect,
                                       float x, float y) {
    if (!prefab || !world) return 0;

    Agentite_SpawnContext ctx = {};
    ctx.world = world;
    ctx.reflect = reflect;
    ctx.offset_x = x;
    ctx.offset_y = y;

    return agentite_prefab_spawn(prefab, &ctx);
}
