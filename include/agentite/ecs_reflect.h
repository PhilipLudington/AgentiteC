/**
 * Agentite Engine - ECS Component Reflection System
 *
 * Provides runtime introspection of component fields for tools like
 * Entity Inspector. Components must be registered with their field
 * metadata to be inspectable.
 *
 * Usage:
 *   Agentite_ReflectRegistry *registry = agentite_reflect_create();
 *
 *   AGENTITE_REFLECT_COMPONENT(registry, world, C_Position,
 *       AGENTITE_FIELD(C_Position, x, AGENTITE_FIELD_FLOAT),
 *       AGENTITE_FIELD(C_Position, y, AGENTITE_FIELD_FLOAT)
 *   );
 *
 *   // Later, query field info
 *   const Agentite_ComponentMeta *meta = agentite_reflect_get(registry, comp_id);
 */

#ifndef AGENTITE_ECS_REFLECT_H
#define AGENTITE_ECS_REFLECT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for Flecs entity type */
typedef uint64_t ecs_entity_t;

/* Maximum fields per component and components per registry */
#define AGENTITE_REFLECT_MAX_FIELDS 32
#define AGENTITE_REFLECT_MAX_COMPONENTS 256

/* ============================================================================
 * Field Types
 * ============================================================================ */

typedef enum Agentite_FieldType {
    AGENTITE_FIELD_INT,        /* int */
    AGENTITE_FIELD_UINT,       /* unsigned int */
    AGENTITE_FIELD_FLOAT,      /* float */
    AGENTITE_FIELD_DOUBLE,     /* double */
    AGENTITE_FIELD_BOOL,       /* bool */
    AGENTITE_FIELD_VEC2,       /* float[2] */
    AGENTITE_FIELD_VEC3,       /* float[3] */
    AGENTITE_FIELD_VEC4,       /* float[4] */
    AGENTITE_FIELD_STRING,     /* const char* */
    AGENTITE_FIELD_ENTITY,     /* ecs_entity_t */
    AGENTITE_FIELD_INT8,       /* int8_t */
    AGENTITE_FIELD_UINT8,      /* uint8_t */
    AGENTITE_FIELD_INT16,      /* int16_t */
    AGENTITE_FIELD_UINT16,     /* uint16_t */
    AGENTITE_FIELD_INT64,      /* int64_t */
    AGENTITE_FIELD_UINT64,     /* uint64_t */
    AGENTITE_FIELD_UNKNOWN     /* Unrecognized type (displays as hex bytes) */
} Agentite_FieldType;

/* ============================================================================
 * Field Descriptor
 * ============================================================================ */

typedef struct Agentite_FieldDesc {
    const char *name;          /* Field name for display */
    Agentite_FieldType type;   /* Field type */
    size_t offset;             /* offsetof(Component, field) */
    size_t size;               /* sizeof(field) */
} Agentite_FieldDesc;

/* ============================================================================
 * Component Metadata
 * ============================================================================ */

typedef struct Agentite_ComponentMeta {
    ecs_entity_t component_id; /* Flecs component entity ID */
    const char *name;          /* Component name (e.g., "C_Position") */
    size_t size;               /* Total component size in bytes */
    Agentite_FieldDesc fields[AGENTITE_REFLECT_MAX_FIELDS];
    int field_count;           /* Number of fields */
} Agentite_ComponentMeta;

/* ============================================================================
 * Reflection Registry (opaque)
 * ============================================================================ */

typedef struct Agentite_ReflectRegistry Agentite_ReflectRegistry;

/* ============================================================================
 * Registry Lifecycle
 * ============================================================================ */

/**
 * Create a reflection registry.
 * Returns: New registry, or NULL on failure.
 */
Agentite_ReflectRegistry *agentite_reflect_create(void);

/**
 * Destroy a reflection registry.
 * Safe to pass NULL.
 */
void agentite_reflect_destroy(Agentite_ReflectRegistry *registry);

/* ============================================================================
 * Component Registration
 * ============================================================================ */

/**
 * Register a component with its field metadata.
 *
 * @param registry    The reflection registry
 * @param component_id Flecs component entity ID (from ecs_id(ComponentType))
 * @param name        Component name string (usually #ComponentType)
 * @param size        sizeof(ComponentType)
 * @param fields      Array of field descriptors
 * @param field_count Number of fields
 * @return true on success, false if registry is full
 */
bool agentite_reflect_register(Agentite_ReflectRegistry *registry,
                               ecs_entity_t component_id,
                               const char *name,
                               size_t size,
                               const Agentite_FieldDesc *fields,
                               int field_count);

/**
 * Get metadata for a component by ID.
 * Returns: Component metadata, or NULL if not registered.
 */
const Agentite_ComponentMeta *agentite_reflect_get(
    const Agentite_ReflectRegistry *registry,
    ecs_entity_t component_id);

/**
 * Get metadata for a component by name.
 * Returns: Component metadata, or NULL if not registered.
 */
const Agentite_ComponentMeta *agentite_reflect_get_by_name(
    const Agentite_ReflectRegistry *registry,
    const char *name);

/**
 * Get all registered components.
 *
 * @param registry   The reflection registry
 * @param out_metas  Output array to fill with component metadata pointers
 * @param max_count  Maximum number of entries to return
 * @return Number of components written to out_metas
 */
int agentite_reflect_get_all(const Agentite_ReflectRegistry *registry,
                              const Agentite_ComponentMeta **out_metas,
                              int max_count);

/**
 * Get the number of registered components.
 */
int agentite_reflect_count(const Agentite_ReflectRegistry *registry);

/* ============================================================================
 * Convenience Macros
 * ============================================================================ */

/**
 * Define a field descriptor.
 * Usage: AGENTITE_FIELD(C_Position, x, AGENTITE_FIELD_FLOAT)
 */
#define AGENTITE_FIELD(component, field, type) \
    { #field, type, offsetof(component, field), sizeof(((component*)0)->field) }

/**
 * Register a component with variadic fields.
 * Usage:
 *   AGENTITE_REFLECT_COMPONENT(registry, world, C_Position,
 *       AGENTITE_FIELD(C_Position, x, AGENTITE_FIELD_FLOAT),
 *       AGENTITE_FIELD(C_Position, y, AGENTITE_FIELD_FLOAT)
 *   );
 */
#define AGENTITE_REFLECT_COMPONENT(registry, world, component, ...) \
    do { \
        Agentite_FieldDesc _fields[] = { __VA_ARGS__ }; \
        agentite_reflect_register(registry, ecs_id(component), #component, \
            sizeof(component), _fields, \
            (int)(sizeof(_fields) / sizeof(_fields[0]))); \
    } while(0)

/* ============================================================================
 * Field Value Formatting
 * ============================================================================ */

/**
 * Format a field value as a string.
 *
 * @param field  Field descriptor
 * @param data   Pointer to the field data (component base + field offset)
 * @param buffer Output buffer
 * @param size   Buffer size
 * @return Number of characters written (excluding null terminator)
 */
int agentite_reflect_format_field(const Agentite_FieldDesc *field,
                                   const void *data,
                                   char *buffer,
                                   int size);

/**
 * Get a human-readable name for a field type.
 */
const char *agentite_reflect_type_name(Agentite_FieldType type);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_ECS_REFLECT_H */
