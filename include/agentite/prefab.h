/**
 * Agentite Engine - Entity Prefab System
 *
 * Prefabs are entity templates loaded from data files. They define component
 * configurations that can be spawned as ECS entities at runtime.
 *
 * DSL Format (AI-friendly, no "Entity" keyword required):
 *   # Comments start with hash or double-slash
 *   EntityName @(x, y) {
 *       ComponentName: value
 *       ComponentName: "string value"
 *       ComponentName: (x, y)
 *
 *       # Nested child entity
 *       ChildName @(local_x, local_y) {
 *           ...
 *       }
 *   }
 *
 * The "Entity" keyword is optional for backward compatibility:
 *   Entity OldStyle @(x, y) { ... }  // Still works
 *   NewStyle @(x, y) { ... }         // Preferred
 *
 * Usage:
 *   Agentite_PrefabRegistry *prefabs = agentite_prefab_registry_create();
 *
 *   // Load prefab from file
 *   Agentite_Prefab *enemy = agentite_prefab_load(prefabs, "prefabs/enemy.prefab",
 *                                                   reflect_registry);
 *
 *   // Spawn entity from prefab
 *   ecs_entity_t e = agentite_prefab_spawn(prefabs, enemy, ecs_world, 100, 200);
 *
 *   agentite_prefab_registry_destroy(prefabs);
 */

#ifndef AGENTITE_PREFAB_H
#define AGENTITE_PREFAB_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct Agentite_Prefab Agentite_Prefab;
typedef struct Agentite_PrefabRegistry Agentite_PrefabRegistry;
typedef struct Agentite_ReflectRegistry Agentite_ReflectRegistry;
typedef struct Agentite_AssetRegistry Agentite_AssetRegistry;
typedef uint64_t ecs_entity_t;
typedef struct ecs_world_t ecs_world_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

#define AGENTITE_PREFAB_MAX_COMPONENTS 32
#define AGENTITE_PREFAB_MAX_CHILDREN 64
#define AGENTITE_PREFAB_MAX_FIELDS 32

/* ============================================================================
 * Property Value Types
 * ============================================================================ */

typedef enum Agentite_PropType {
    AGENTITE_PROP_NULL = 0,
    AGENTITE_PROP_INT,
    AGENTITE_PROP_FLOAT,
    AGENTITE_PROP_BOOL,
    AGENTITE_PROP_STRING,
    AGENTITE_PROP_VEC2,
    AGENTITE_PROP_VEC3,
    AGENTITE_PROP_VEC4,
    AGENTITE_PROP_IDENTIFIER  /* Unquoted string like "aggressive" */
} Agentite_PropType;

/**
 * Property value (component field assignment).
 */
typedef struct Agentite_PropValue {
    Agentite_PropType type;
    union {
        int64_t int_val;
        double float_val;
        bool bool_val;
        char *string_val;      /* Heap-allocated */
        float vec2_val[2];
        float vec3_val[3];
        float vec4_val[4];
    };
} Agentite_PropValue;

/**
 * Component field assignment (name = value).
 */
typedef struct Agentite_FieldAssign {
    char *field_name;          /* Heap-allocated field name */
    Agentite_PropValue value;
} Agentite_FieldAssign;

/**
 * Component configuration (all field assignments for one component).
 */
typedef struct Agentite_ComponentConfig {
    char *component_name;      /* Heap-allocated component type name */
    Agentite_FieldAssign fields[AGENTITE_PREFAB_MAX_FIELDS];
    int field_count;
} Agentite_ComponentConfig;

/* ============================================================================
 * Prefab Structure
 * ============================================================================ */

/**
 * Entity prefab (template for spawning entities).
 * Contains component configurations and optional child prefabs.
 */
struct Agentite_Prefab {
    char *name;                /* Optional entity name */
    char *path;                /* Source file path (for registry lookup) */
    float position[2];         /* Default spawn position offset */

    /* Component configurations */
    Agentite_ComponentConfig components[AGENTITE_PREFAB_MAX_COMPONENTS];
    int component_count;

    /* Child prefabs (for hierarchies) */
    Agentite_Prefab *children[AGENTITE_PREFAB_MAX_CHILDREN];
    int child_count;

    /* Reference to another prefab (for "prefab: name" syntax) */
    char *base_prefab_name;    /* If set, inherit from this prefab */
};

/* ============================================================================
 * Prefab Registry
 * ============================================================================ */

/**
 * Create a prefab registry.
 * The registry caches loaded prefabs for O(1) lookup by path.
 *
 * @return New registry, or NULL on failure
 */
Agentite_PrefabRegistry *agentite_prefab_registry_create(void);

/**
 * Destroy prefab registry and all cached prefabs.
 * Safe to pass NULL.
 */
void agentite_prefab_registry_destroy(Agentite_PrefabRegistry *registry);

/* ============================================================================
 * Prefab Loading
 * ============================================================================ */

/**
 * Load prefab from file.
 * If already loaded, returns cached version.
 *
 * @param registry  Prefab registry for caching
 * @param path      File path to load
 * @param reflect   Reflection registry for component validation (optional)
 * @return Loaded prefab, or NULL on error
 */
Agentite_Prefab *agentite_prefab_load(Agentite_PrefabRegistry *registry,
                                       const char *path,
                                       const Agentite_ReflectRegistry *reflect);

/**
 * Load prefab from memory.
 * The prefab is NOT cached in the registry.
 *
 * @param source    Source string (DSL text)
 * @param length    Source length (0 to use strlen)
 * @param name      Name for error messages
 * @param reflect   Reflection registry for component validation (optional)
 * @return Loaded prefab, or NULL on error. Caller must call agentite_prefab_destroy().
 */
Agentite_Prefab *agentite_prefab_load_string(const char *source,
                                              size_t length,
                                              const char *name,
                                              const Agentite_ReflectRegistry *reflect);

/**
 * Look up cached prefab by path.
 *
 * @param registry Prefab registry
 * @param path     File path to look up
 * @return Cached prefab, or NULL if not loaded
 */
Agentite_Prefab *agentite_prefab_lookup(Agentite_PrefabRegistry *registry,
                                         const char *path);

/**
 * Destroy a prefab loaded with agentite_prefab_load_string().
 * Do NOT call this on prefabs from agentite_prefab_load() (registry owns those).
 */
void agentite_prefab_destroy(Agentite_Prefab *prefab);

/* ============================================================================
 * Prefab Spawning
 * ============================================================================ */

/**
 * Spawn context for customizing entity creation.
 */
typedef struct Agentite_SpawnContext {
    ecs_world_t *world;              /* ECS world to spawn into (required) */
    const Agentite_ReflectRegistry *reflect;  /* For component creation */
    Agentite_AssetRegistry *assets;  /* For resolving asset paths (optional) */
    Agentite_PrefabRegistry *prefabs; /* For resolving base prefabs (optional) */
    float offset_x, offset_y;        /* Position offset */
    ecs_entity_t parent;             /* Parent entity for hierarchy (0 = none) */
} Agentite_SpawnContext;

/**
 * Spawn an entity from a prefab.
 *
 * @param prefab   Prefab to spawn
 * @param ctx      Spawn context with world and options
 * @return Created entity, or 0 on failure
 */
ecs_entity_t agentite_prefab_spawn(const Agentite_Prefab *prefab,
                                    const Agentite_SpawnContext *ctx);

/**
 * Simple spawn helper (no hierarchy, no assets).
 *
 * @param prefab   Prefab to spawn
 * @param world    ECS world
 * @param reflect  Reflection registry
 * @param x, y     Spawn position
 * @return Created entity, or 0 on failure
 */
ecs_entity_t agentite_prefab_spawn_at(const Agentite_Prefab *prefab,
                                       ecs_world_t *world,
                                       const Agentite_ReflectRegistry *reflect,
                                       float x, float y);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Get number of cached prefabs in registry.
 */
size_t agentite_prefab_registry_count(const Agentite_PrefabRegistry *registry);

/**
 * Clear all cached prefabs from registry.
 */
void agentite_prefab_registry_clear(Agentite_PrefabRegistry *registry);

/**
 * Get last parse error message.
 * Thread-local, valid until next parse call.
 */
const char *agentite_prefab_get_error(void);

/* ============================================================================
 * Prefab Serialization
 * ============================================================================ */

/**
 * Write a prefab to DSL format string.
 *
 * @param prefab Prefab to serialize
 * @return Heap-allocated DSL string, or NULL on error. Caller must free().
 */
char *agentite_prefab_write_string(const Agentite_Prefab *prefab);

/**
 * Write a prefab to file.
 *
 * @param prefab Prefab to write
 * @param path   Output file path
 * @return true on success, false on error
 */
bool agentite_prefab_write_file(const Agentite_Prefab *prefab, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_PREFAB_H */
