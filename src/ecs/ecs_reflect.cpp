/**
 * Agentite Engine - ECS Component Reflection Implementation
 */

#include "agentite/ecs_reflect.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Registry Structure
 * ============================================================================ */

struct Agentite_ReflectRegistry {
    Agentite_ComponentMeta components[AGENTITE_REFLECT_MAX_COMPONENTS];
    int component_count;

    /* Simple hash table for fast lookup by component_id */
    struct {
        ecs_entity_t key;
        int index;  /* Index into components array, -1 if empty */
    } lookup[512];  /* Power of 2 for fast modulo */
};

/* Hash function for component IDs */
static unsigned int hash_entity_id(ecs_entity_t id)
{
    /* Simple multiplicative hash */
    return (unsigned int)((id * 2654435761ULL) & 0xFFFFFFFF);
}

/* ============================================================================
 * Registry Lifecycle
 * ============================================================================ */

Agentite_ReflectRegistry *agentite_reflect_create(void)
{
    Agentite_ReflectRegistry *registry = (Agentite_ReflectRegistry *)
        calloc(1, sizeof(Agentite_ReflectRegistry));
    if (!registry) return NULL;

    /* Initialize lookup table as empty */
    for (int i = 0; i < 512; i++) {
        registry->lookup[i].key = 0;
        registry->lookup[i].index = -1;
    }

    return registry;
}

void agentite_reflect_destroy(Agentite_ReflectRegistry *registry)
{
    free(registry);
}

/* ============================================================================
 * Component Registration
 * ============================================================================ */

bool agentite_reflect_register(Agentite_ReflectRegistry *registry,
                               ecs_entity_t component_id,
                               const char *name,
                               size_t size,
                               const Agentite_FieldDesc *fields,
                               int field_count)
{
    if (!registry || !name || !fields) return false;
    if (field_count <= 0 || field_count > AGENTITE_REFLECT_MAX_FIELDS) return false;
    if (registry->component_count >= AGENTITE_REFLECT_MAX_COMPONENTS) return false;

    /* Check if already registered */
    if (agentite_reflect_get(registry, component_id) != NULL) {
        return false;  /* Already registered */
    }

    /* Add to components array */
    int idx = registry->component_count++;
    Agentite_ComponentMeta *meta = &registry->components[idx];

    meta->component_id = component_id;
    meta->name = name;
    meta->size = size;
    meta->field_count = field_count;

    /* Copy field descriptors */
    for (int i = 0; i < field_count; i++) {
        meta->fields[i] = fields[i];
    }

    /* Add to hash table */
    unsigned int hash = hash_entity_id(component_id);
    for (int i = 0; i < 512; i++) {
        int slot = (hash + i) & 511;  /* Linear probing */
        if (registry->lookup[slot].index == -1) {
            registry->lookup[slot].key = component_id;
            registry->lookup[slot].index = idx;
            break;
        }
    }

    return true;
}

const Agentite_ComponentMeta *agentite_reflect_get(
    const Agentite_ReflectRegistry *registry,
    ecs_entity_t component_id)
{
    if (!registry || component_id == 0) return NULL;

    unsigned int hash = hash_entity_id(component_id);
    for (int i = 0; i < 512; i++) {
        int slot = (hash + i) & 511;
        if (registry->lookup[slot].index == -1) {
            return NULL;  /* Not found */
        }
        if (registry->lookup[slot].key == component_id) {
            return &registry->components[registry->lookup[slot].index];
        }
    }

    return NULL;
}

const Agentite_ComponentMeta *agentite_reflect_get_by_name(
    const Agentite_ReflectRegistry *registry,
    const char *name)
{
    if (!registry || !name) return NULL;

    /* Linear search by name */
    for (int i = 0; i < registry->component_count; i++) {
        if (strcmp(registry->components[i].name, name) == 0) {
            return &registry->components[i];
        }
    }

    return NULL;
}

int agentite_reflect_get_all(const Agentite_ReflectRegistry *registry,
                              const Agentite_ComponentMeta **out_metas,
                              int max_count)
{
    if (!registry || !out_metas || max_count <= 0) return 0;

    int count = registry->component_count;
    if (count > max_count) count = max_count;

    for (int i = 0; i < count; i++) {
        out_metas[i] = &registry->components[i];
    }

    return count;
}

int agentite_reflect_count(const Agentite_ReflectRegistry *registry)
{
    if (!registry) return 0;
    return registry->component_count;
}

/* ============================================================================
 * Field Value Formatting
 * ============================================================================ */

int agentite_reflect_format_field(const Agentite_FieldDesc *field,
                                   const void *data,
                                   char *buffer,
                                   int size)
{
    if (!field || !data || !buffer || size <= 0) return 0;

    const uint8_t *ptr = (const uint8_t *)data;

    switch (field->type) {
        case AGENTITE_FIELD_INT:
            return snprintf(buffer, size, "%d", *(const int *)ptr);

        case AGENTITE_FIELD_UINT:
            return snprintf(buffer, size, "%u", *(const unsigned int *)ptr);

        case AGENTITE_FIELD_FLOAT:
            return snprintf(buffer, size, "%.3f", *(const float *)ptr);

        case AGENTITE_FIELD_DOUBLE:
            return snprintf(buffer, size, "%.6f", *(const double *)ptr);

        case AGENTITE_FIELD_BOOL:
            return snprintf(buffer, size, "%s", *(const bool *)ptr ? "true" : "false");

        case AGENTITE_FIELD_VEC2: {
            const float *v = (const float *)ptr;
            return snprintf(buffer, size, "(%.2f, %.2f)", v[0], v[1]);
        }

        case AGENTITE_FIELD_VEC3: {
            const float *v = (const float *)ptr;
            return snprintf(buffer, size, "(%.2f, %.2f, %.2f)", v[0], v[1], v[2]);
        }

        case AGENTITE_FIELD_VEC4: {
            const float *v = (const float *)ptr;
            return snprintf(buffer, size, "(%.2f, %.2f, %.2f, %.2f)",
                           v[0], v[1], v[2], v[3]);
        }

        case AGENTITE_FIELD_STRING: {
            const char *s = *(const char *const *)ptr;
            if (s) {
                return snprintf(buffer, size, "\"%s\"", s);
            } else {
                return snprintf(buffer, size, "(null)");
            }
        }

        case AGENTITE_FIELD_ENTITY: {
            ecs_entity_t e = *(const ecs_entity_t *)ptr;
            if (e == 0) {
                return snprintf(buffer, size, "(none)");
            } else {
                return snprintf(buffer, size, "%llu", (unsigned long long)e);
            }
        }

        case AGENTITE_FIELD_INT8:
            return snprintf(buffer, size, "%d", *(const int8_t *)ptr);

        case AGENTITE_FIELD_UINT8:
            return snprintf(buffer, size, "%u", *(const uint8_t *)ptr);

        case AGENTITE_FIELD_INT16:
            return snprintf(buffer, size, "%d", *(const int16_t *)ptr);

        case AGENTITE_FIELD_UINT16:
            return snprintf(buffer, size, "%u", *(const uint16_t *)ptr);

        case AGENTITE_FIELD_INT64:
            return snprintf(buffer, size, "%lld", (long long)*(const int64_t *)ptr);

        case AGENTITE_FIELD_UINT64:
            return snprintf(buffer, size, "%llu", (unsigned long long)*(const uint64_t *)ptr);

        case AGENTITE_FIELD_UNKNOWN:
        default: {
            /* Display as hex bytes */
            int written = 0;
            size_t bytes_to_show = field->size < 8 ? field->size : 8;
            for (size_t i = 0; i < bytes_to_show && written + 3 < size; i++) {
                written += snprintf(buffer + written, size - written,
                                   "%02X ", ptr[i]);
            }
            if (field->size > 8 && written + 4 < size) {
                written += snprintf(buffer + written, size - written, "...");
            }
            return written;
        }
    }
}

const char *agentite_reflect_type_name(Agentite_FieldType type)
{
    switch (type) {
        case AGENTITE_FIELD_INT:     return "int";
        case AGENTITE_FIELD_UINT:    return "uint";
        case AGENTITE_FIELD_FLOAT:   return "float";
        case AGENTITE_FIELD_DOUBLE:  return "double";
        case AGENTITE_FIELD_BOOL:    return "bool";
        case AGENTITE_FIELD_VEC2:    return "vec2";
        case AGENTITE_FIELD_VEC3:    return "vec3";
        case AGENTITE_FIELD_VEC4:    return "vec4";
        case AGENTITE_FIELD_STRING:  return "string";
        case AGENTITE_FIELD_ENTITY:  return "entity";
        case AGENTITE_FIELD_INT8:    return "int8";
        case AGENTITE_FIELD_UINT8:   return "uint8";
        case AGENTITE_FIELD_INT16:   return "int16";
        case AGENTITE_FIELD_UINT16:  return "uint16";
        case AGENTITE_FIELD_INT64:   return "int64";
        case AGENTITE_FIELD_UINT64:  return "uint64";
        case AGENTITE_FIELD_UNKNOWN:
        default:                     return "unknown";
    }
}
