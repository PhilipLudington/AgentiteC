/**
 * Agentite Engine - Scene/Level System
 *
 * Scenes represent complete game levels loaded from data files. Unlike prefabs
 * (which are templates spawned multiple times), scenes are instantiated once
 * and manage the lifetime of their entities.
 *
 * DSL Format (AI-friendly, no "Entity" keyword required):
 *   # Player entity with child weapon
 *   Player @(400, 300) {
 *       Sprite: "player.png"
 *       Health: 100
 *
 *       Weapon @(20, 0) {
 *           Sprite: "sword.png"
 *       }
 *   }
 *
 *   # Enemy using prefab reference
 *   Enemy @(600, 300) {
 *       prefab: "enemies/goblin"
 *   }
 *
 * Comments: Both # and // are supported.
 * The "Entity" keyword is optional for backward compatibility.
 *
 * Usage:
 *   // Create scene manager
 *   Agentite_SceneManager *scenes = agentite_scene_manager_create();
 *
 *   // Load a scene
 *   Agentite_Scene *level = agentite_scene_load(scenes, "levels/level1.scene",
 *                                                &load_ctx);
 *
 *   // Instantiate into ECS world
 *   agentite_scene_instantiate(level, world, reflect);
 *
 *   // Query entities from the scene
 *   size_t count = agentite_scene_get_entity_count(level);
 *
 *   // Transition to next scene
 *   agentite_scene_transition(scenes, "levels/level2.scene", world, &load_ctx);
 *
 *   // Cleanup
 *   agentite_scene_manager_destroy(scenes);
 */

#ifndef AGENTITE_SCENE_H
#define AGENTITE_SCENE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct Agentite_Scene Agentite_Scene;
typedef struct Agentite_SceneManager Agentite_SceneManager;
typedef struct Agentite_ReflectRegistry Agentite_ReflectRegistry;
typedef struct Agentite_AssetRegistry Agentite_AssetRegistry;
typedef struct Agentite_PrefabRegistry Agentite_PrefabRegistry;
typedef struct Agentite_Prefab Agentite_Prefab;
typedef uint64_t ecs_entity_t;
typedef struct ecs_world_t ecs_world_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

#define AGENTITE_SCENE_MAX_ENTITIES 1024
#define AGENTITE_SCENE_MAX_ASSETS 256

/* ============================================================================
 * Scene Load Context
 * ============================================================================ */

/**
 * Context for scene loading and instantiation.
 */
typedef struct Agentite_SceneLoadContext {
    const Agentite_ReflectRegistry *reflect;  /* Component reflection (required) */
    Agentite_AssetRegistry *assets;           /* Asset registry (optional) */
    Agentite_PrefabRegistry *prefabs;         /* Prefab registry for references (optional) */
    bool preload_assets;                      /* Preload referenced assets before instantiate */
} Agentite_SceneLoadContext;

/* Default context initializer */
#define AGENTITE_SCENE_LOAD_CONTEXT_DEFAULT { NULL, NULL, NULL, false }

/* ============================================================================
 * Scene State
 * ============================================================================ */

typedef enum Agentite_SceneState {
    AGENTITE_SCENE_UNLOADED = 0,   /* Not loaded */
    AGENTITE_SCENE_PARSED,          /* Parsed but not instantiated */
    AGENTITE_SCENE_LOADED,          /* Entities instantiated in world */
    AGENTITE_SCENE_UNLOADING        /* Being unloaded */
} Agentite_SceneState;

/* ============================================================================
 * Asset Reference (for preloading)
 * ============================================================================ */

/* Asset type is defined in asset.h - include it or use forward declaration */
#include "agentite/asset.h"

typedef struct Agentite_AssetRef {
    char *path;
    Agentite_AssetType type;
} Agentite_AssetRef;

/* ============================================================================
 * Scene Manager
 * ============================================================================ */

/**
 * Create a scene manager.
 * Manages scene loading, caching, and transitions.
 *
 * @return New scene manager, or NULL on failure
 */
Agentite_SceneManager *agentite_scene_manager_create(void);

/**
 * Destroy scene manager and all loaded scenes.
 * Safe to pass NULL.
 */
void agentite_scene_manager_destroy(Agentite_SceneManager *manager);

/**
 * Get currently active scene.
 *
 * @param manager Scene manager
 * @return Active scene, or NULL if none
 */
Agentite_Scene *agentite_scene_manager_get_active(Agentite_SceneManager *manager);

/**
 * Set the active scene (does not instantiate - use scene_transition for that).
 *
 * @param manager Scene manager
 * @param scene   Scene to make active
 */
void agentite_scene_manager_set_active(Agentite_SceneManager *manager,
                                        Agentite_Scene *scene);

/* ============================================================================
 * Scene Loading
 * ============================================================================ */

/**
 * Load a scene from file (parse only, does not instantiate).
 * If already loaded, returns cached version.
 *
 * @param manager Scene manager for caching
 * @param path    File path to load
 * @param ctx     Load context with registries
 * @return Loaded scene, or NULL on error
 */
Agentite_Scene *agentite_scene_load(Agentite_SceneManager *manager,
                                     const char *path,
                                     const Agentite_SceneLoadContext *ctx);

/**
 * Load scene from memory (parse only).
 * The scene is NOT cached in the manager.
 *
 * @param source  Source string (DSL text)
 * @param length  Source length (0 to use strlen)
 * @param name    Name for error messages
 * @param ctx     Load context
 * @return Loaded scene, or NULL on error. Caller must call agentite_scene_destroy().
 */
Agentite_Scene *agentite_scene_load_string(const char *source,
                                            size_t length,
                                            const char *name,
                                            const Agentite_SceneLoadContext *ctx);

/**
 * Look up cached scene by path.
 *
 * @param manager Scene manager
 * @param path    File path to look up
 * @return Cached scene, or NULL if not loaded
 */
Agentite_Scene *agentite_scene_lookup(Agentite_SceneManager *manager,
                                       const char *path);

/**
 * Destroy a scene loaded with agentite_scene_load_string().
 * Do NOT call this on scenes from agentite_scene_load() (manager owns those).
 */
void agentite_scene_destroy(Agentite_Scene *scene);

/* ============================================================================
 * Scene Instantiation
 * ============================================================================ */

/**
 * Instantiate all entities from a scene into the ECS world.
 * After this call, the scene tracks all spawned entities.
 *
 * @param scene   Scene to instantiate
 * @param world   ECS world to spawn into
 * @param ctx     Load context with reflection registry
 * @return true on success, false on error
 */
bool agentite_scene_instantiate(Agentite_Scene *scene,
                                 ecs_world_t *world,
                                 const Agentite_SceneLoadContext *ctx);

/**
 * Unload scene entities from the ECS world.
 * Destroys all entities that were spawned by this scene.
 *
 * @param scene Scene to unload
 * @param world ECS world containing the entities
 */
void agentite_scene_uninstantiate(Agentite_Scene *scene, ecs_world_t *world);

/**
 * Check if scene is currently instantiated.
 *
 * @param scene Scene to check
 * @return true if entities are spawned in a world
 */
bool agentite_scene_is_instantiated(const Agentite_Scene *scene);

/* ============================================================================
 * Scene Transitions
 * ============================================================================ */

/**
 * Transition from current scene to a new scene.
 * This is a convenience function that:
 * 1. Unloads the current active scene (if any)
 * 2. Loads the new scene
 * 3. Instantiates the new scene
 * 4. Sets the new scene as active
 *
 * @param manager   Scene manager
 * @param path      Path to new scene
 * @param world     ECS world
 * @param ctx       Load context
 * @return New scene, or NULL on error (old scene remains if load fails)
 */
Agentite_Scene *agentite_scene_transition(Agentite_SceneManager *manager,
                                           const char *path,
                                           ecs_world_t *world,
                                           const Agentite_SceneLoadContext *ctx);

/* ============================================================================
 * Scene Entity Access
 * ============================================================================ */

/**
 * Get number of root entities in the scene definition.
 *
 * @param scene Scene to query
 * @return Number of root entities
 */
size_t agentite_scene_get_root_count(const Agentite_Scene *scene);

/**
 * Get number of spawned entities (including children).
 * Only valid after agentite_scene_instantiate().
 *
 * @param scene Scene to query
 * @return Number of spawned entities, or 0 if not instantiated
 */
size_t agentite_scene_get_entity_count(const Agentite_Scene *scene);

/**
 * Get all spawned entities.
 *
 * @param scene       Scene to query
 * @param out_entities Output array to fill with entity IDs
 * @param max_count   Maximum number of entities to return
 * @return Number of entities copied (may be less than total if max_count exceeded)
 */
size_t agentite_scene_get_entities(const Agentite_Scene *scene,
                                    ecs_entity_t *out_entities,
                                    size_t max_count);

/**
 * Get root entities (top-level, no parent in scene).
 *
 * @param scene       Scene to query
 * @param out_entities Output array to fill with entity IDs
 * @param max_count   Maximum number of entities to return
 * @return Number of entities copied
 */
size_t agentite_scene_get_root_entities(const Agentite_Scene *scene,
                                         ecs_entity_t *out_entities,
                                         size_t max_count);

/**
 * Find a spawned entity by name.
 *
 * @param scene Scene to search
 * @param name  Entity name to find
 * @return Entity ID, or 0 if not found
 */
ecs_entity_t agentite_scene_find_entity(const Agentite_Scene *scene,
                                         const char *name);

/* ============================================================================
 * Asset Management
 * ============================================================================ */

/**
 * Get all asset references used by the scene.
 * Useful for preloading before instantiation.
 *
 * @param scene      Scene to query
 * @param out_refs   Output array to fill with asset references
 * @param max_count  Maximum number of refs to return
 * @return Number of asset refs copied
 */
size_t agentite_scene_get_asset_refs(const Agentite_Scene *scene,
                                      Agentite_AssetRef *out_refs,
                                      size_t max_count);

/**
 * Preload all assets referenced by the scene.
 * Loads textures, sounds, music, and prefabs into their registries.
 *
 * @param scene  Scene to preload assets for
 * @param ctx    Load context with asset registries
 * @return true if all assets loaded, false if any failed
 */
bool agentite_scene_preload_assets(Agentite_Scene *scene,
                                    const Agentite_SceneLoadContext *ctx);

/* ============================================================================
 * Scene Properties
 * ============================================================================ */

/**
 * Get scene file path.
 *
 * @param scene Scene to query
 * @return File path, or NULL for string-loaded scenes
 */
const char *agentite_scene_get_path(const Agentite_Scene *scene);

/**
 * Get scene name (filename without extension).
 *
 * @param scene Scene to query
 * @return Scene name
 */
const char *agentite_scene_get_name(const Agentite_Scene *scene);

/**
 * Get scene state.
 *
 * @param scene Scene to query
 * @return Current scene state
 */
Agentite_SceneState agentite_scene_get_state(const Agentite_Scene *scene);

/* ============================================================================
 * Scene Writing (Serialization)
 * ============================================================================ */

/**
 * Write a scene to DSL format string.
 * Serializes all root entities and their children.
 *
 * @param scene  Scene to write
 * @return Heap-allocated DSL string, or NULL on error. Caller must free().
 */
char *agentite_scene_write_string(const Agentite_Scene *scene);

/**
 * Write a scene to file.
 *
 * @param scene  Scene to write
 * @param path   Output file path
 * @return true on success, false on error
 */
bool agentite_scene_write_file(const Agentite_Scene *scene, const char *path);

/**
 * Create a scene from ECS world entities.
 * Captures all entities with C_SceneEntity tag component into a scene.
 *
 * @param world    ECS world to capture from
 * @param reflect  Reflection registry for component serialization
 * @param name     Scene name
 * @return New scene representing world state, or NULL on error
 */
Agentite_Scene *agentite_scene_from_world(ecs_world_t *world,
                                           const Agentite_ReflectRegistry *reflect,
                                           const char *name);

/**
 * Write entities from ECS world directly to DSL string.
 * Captures specified entities and serializes them.
 *
 * @param world     ECS world
 * @param entities  Array of entities to serialize
 * @param count     Number of entities
 * @param reflect   Reflection registry for component serialization
 * @return Heap-allocated DSL string, or NULL on error. Caller must free().
 */
char *agentite_scene_write_entities(ecs_world_t *world,
                                     const ecs_entity_t *entities,
                                     size_t count,
                                     const Agentite_ReflectRegistry *reflect);

/* ============================================================================
 * Error Handling
 * ============================================================================ */

/**
 * Get last scene error message.
 * Thread-local, valid until next scene call.
 */
const char *agentite_scene_get_error(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_SCENE_H */
