/**
 * Agentite Engine - Scene/Level System Implementation
 *
 * Scenes represent complete game levels. Unlike prefabs (templates spawned
 * multiple times), scenes manage the lifetime of their entities.
 */

#include "agentite/scene.h"
#include "agentite/prefab.h"
#include "agentite/ecs_reflect.h"
#include "agentite/asset.h"
#include "agentite/error.h"
#include "scene_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "flecs.h"

/* Forward declarations from scene_parser.cpp */
extern "C" {
    Agentite_Prefab *agentite_prefab_load_string(const char *source,
                                                  size_t length,
                                                  const char *name,
                                                  const Agentite_ReflectRegistry *reflect);
    void agentite_prefab_destroy(Agentite_Prefab *prefab);
    const char *agentite_prefab_get_error(void);
}

/* Forward declarations from scene_writer.cpp */
extern "C" {
    char *agentite_prefab_write_string(const Agentite_Prefab *prefab);
}

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SCENE_MANAGER_CAPACITY 64

/* Thread-local error storage */
static thread_local char s_last_error[512] = {0};

/* ============================================================================
 * Scene Structure
 * ============================================================================ */

struct Agentite_Scene {
    char *path;                 /* Source file path (NULL for string-loaded) */
    char *name;                 /* Scene name (derived from path or set explicitly) */
    Agentite_SceneState state;  /* Current state */

    /* Parsed data */
    Agentite_Prefab **roots;    /* Array of root entity prefabs */
    size_t root_count;          /* Number of root entities */
    size_t root_capacity;       /* Allocated capacity */

    /* Asset references */
    Agentite_AssetRef *asset_refs;
    size_t asset_ref_count;
    size_t asset_ref_capacity;

    /* Spawned entity tracking */
    ecs_entity_t *entities;     /* All spawned entities (including children) */
    size_t entity_count;        /* Number of spawned entities */
    size_t entity_capacity;     /* Allocated capacity */

    ecs_entity_t *root_entities; /* Root entity IDs only */
    size_t root_entity_count;

    /* World reference (valid while instantiated) */
    ecs_world_t *world;
};

/* ============================================================================
 * Scene Manager Structure
 * ============================================================================ */

typedef struct SceneEntry {
    char *path;
    Agentite_Scene *scene;
} SceneEntry;

struct Agentite_SceneManager {
    SceneEntry entries[SCENE_MANAGER_CAPACITY];
    size_t count;
    Agentite_Scene *active_scene;
};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void set_scene_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_last_error, sizeof(s_last_error), fmt, args);
    va_end(args);
    agentite_set_error("%s", s_last_error);
}

static char *derive_scene_name(const char *path) {
    if (!path) return strdup("unnamed");

    /* Find last path separator */
    const char *filename = path;
    const char *sep = strrchr(path, '/');
    if (sep) filename = sep + 1;
    sep = strrchr(filename, '\\');
    if (sep) filename = sep + 1;

    /* Find extension */
    const char *dot = strrchr(filename, '.');
    size_t name_len = dot ? (size_t)(dot - filename) : strlen(filename);

    char *name = (char *)malloc(name_len + 1);
    if (!name) return NULL;

    memcpy(name, filename, name_len);
    name[name_len] = '\0';

    return name;
}

static char *read_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        set_scene_error("scene: Failed to open '%s'", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fclose(file);
        set_scene_error("scene: Failed to get size of '%s'", path);
        return NULL;
    }

    char *buffer = (char *)malloc(size + 1);
    if (!buffer) {
        fclose(file);
        set_scene_error("scene: Failed to allocate buffer for '%s'", path);
        return NULL;
    }

    size_t read = fread(buffer, 1, size, file);
    fclose(file);

    buffer[read] = '\0';
    if (out_size) *out_size = read;

    return buffer;
}

/* ============================================================================
 * Scene Manager Implementation
 * ============================================================================ */

Agentite_SceneManager *agentite_scene_manager_create(void) {
    Agentite_SceneManager *manager = (Agentite_SceneManager *)
        calloc(1, sizeof(Agentite_SceneManager));
    return manager;
}

void agentite_scene_manager_destroy(Agentite_SceneManager *manager) {
    if (!manager) return;

    for (size_t i = 0; i < manager->count; i++) {
        free(manager->entries[i].path);
        agentite_scene_destroy(manager->entries[i].scene);
    }

    free(manager);
}

Agentite_Scene *agentite_scene_manager_get_active(Agentite_SceneManager *manager) {
    return manager ? manager->active_scene : NULL;
}

void agentite_scene_manager_set_active(Agentite_SceneManager *manager,
                                        Agentite_Scene *scene) {
    if (manager) {
        manager->active_scene = scene;
    }
}

/* ============================================================================
 * Scene Creation/Destruction
 * ============================================================================ */

static Agentite_Scene *scene_create(void) {
    Agentite_Scene *scene = (Agentite_Scene *)calloc(1, sizeof(Agentite_Scene));
    if (!scene) return NULL;

    scene->state = AGENTITE_SCENE_UNLOADED;

    /* Initialize arrays with small capacity */
    scene->root_capacity = 16;
    scene->roots = (Agentite_Prefab **)calloc(scene->root_capacity,
                                               sizeof(Agentite_Prefab *));

    scene->entity_capacity = 64;
    scene->entities = (ecs_entity_t *)calloc(scene->entity_capacity,
                                              sizeof(ecs_entity_t));
    scene->root_entities = (ecs_entity_t *)calloc(scene->root_capacity,
                                                   sizeof(ecs_entity_t));

    scene->asset_ref_capacity = 32;
    scene->asset_refs = (Agentite_AssetRef *)calloc(scene->asset_ref_capacity,
                                                     sizeof(Agentite_AssetRef));

    if (!scene->roots || !scene->entities || !scene->root_entities ||
        !scene->asset_refs) {
        agentite_scene_destroy(scene);
        return NULL;
    }

    return scene;
}

void agentite_scene_destroy(Agentite_Scene *scene) {
    if (!scene) return;

    /* Uninstantiate if needed */
    if (scene->state == AGENTITE_SCENE_LOADED && scene->world) {
        agentite_scene_uninstantiate(scene, scene->world);
    }

    /* Free parsed data */
    for (size_t i = 0; i < scene->root_count; i++) {
        agentite_prefab_destroy(scene->roots[i]);
    }
    free(scene->roots);

    /* Free asset refs */
    for (size_t i = 0; i < scene->asset_ref_count; i++) {
        free(scene->asset_refs[i].path);
    }
    free(scene->asset_refs);

    /* Free entity arrays */
    free(scene->entities);
    free(scene->root_entities);

    free(scene->path);
    free(scene->name);
    free(scene);
}

/* ============================================================================
 * Scene Parsing
 * ============================================================================ */

/**
 * Parse scene source containing multiple root entities.
 */
static bool parse_scene_source(Agentite_Scene *scene,
                                const char *source,
                                size_t length,
                                const char *name,
                                const Agentite_ReflectRegistry *reflect) {
    if (length == 0) length = strlen(source);

    Agentite_Lexer lexer;
    agentite_lexer_init(&lexer, source, length, name);

    /* Parse multiple Entity blocks */
    while (true) {
        Agentite_Token tok = agentite_lexer_peek(&lexer);

        if (tok.type == TOK_EOF) break;

        if (tok.type == TOK_ERROR) {
            set_scene_error("scene: Lexer error in '%s': %s", name, lexer.error);
            return false;
        }

        if (tok.type != TOK_IDENTIFIER) {
            set_scene_error("scene: Expected entity name or 'Entity' keyword in '%s' at line %d",
                           name, tok.line);
            return false;
        }

        /* Identifier can be "Entity" keyword (old format) or entity name (new format).
         * The prefab parser handles both cases. */

        /* Parse entity using prefab parser */
        /* Calculate remaining source */
        size_t offset = tok.start - source;
        size_t remaining = length - offset;

        Agentite_Prefab *prefab = agentite_prefab_load_string(
            source + offset, remaining, name, reflect);

        if (!prefab) {
            set_scene_error("scene: Failed to parse entity in '%s': %s",
                           name, agentite_prefab_get_error());
            return false;
        }

        /* Add to roots array */
        if (scene->root_count >= scene->root_capacity) {
            size_t new_capacity = scene->root_capacity * 2;
            Agentite_Prefab **new_roots = (Agentite_Prefab **)realloc(
                scene->roots, new_capacity * sizeof(Agentite_Prefab *));
            if (!new_roots) {
                agentite_prefab_destroy(prefab);
                set_scene_error("scene: Out of memory");
                return false;
            }
            scene->roots = new_roots;
            scene->root_capacity = new_capacity;
        }

        scene->roots[scene->root_count++] = prefab;

        /* Advance lexer past the entity we just parsed */
        /* We need to find the closing brace - count braces */
        int brace_depth = 0;
        bool found_start = false;
        while (true) {
            Agentite_Token t = agentite_lexer_next(&lexer);
            if (t.type == TOK_EOF) break;
            if (t.type == TOK_LBRACE) {
                found_start = true;
                brace_depth++;
            }
            if (t.type == TOK_RBRACE) {
                brace_depth--;
                if (found_start && brace_depth == 0) break;
            }
        }
    }

    if (scene->root_count == 0) {
        set_scene_error("scene: No entities found in '%s'", name);
        return false;
    }

    scene->state = AGENTITE_SCENE_PARSED;
    return true;
}

/* ============================================================================
 * Asset Reference Extraction
 * ============================================================================ */

static void add_asset_ref(Agentite_Scene *scene,
                           const char *path,
                           Agentite_AssetType type) {
    if (!path || !path[0]) return;

    /* Check for duplicates */
    for (size_t i = 0; i < scene->asset_ref_count; i++) {
        if (strcmp(scene->asset_refs[i].path, path) == 0) {
            return;
        }
    }

    /* Grow array if needed */
    if (scene->asset_ref_count >= scene->asset_ref_capacity) {
        size_t new_capacity = scene->asset_ref_capacity * 2;
        Agentite_AssetRef *new_refs = (Agentite_AssetRef *)realloc(
            scene->asset_refs, new_capacity * sizeof(Agentite_AssetRef));
        if (!new_refs) return;
        scene->asset_refs = new_refs;
        scene->asset_ref_capacity = new_capacity;
    }

    Agentite_AssetRef *ref = &scene->asset_refs[scene->asset_ref_count++];
    ref->path = strdup(path);
    ref->type = type;
}

static Agentite_AssetType guess_asset_type(const char *path) {
    if (!path) return AGENTITE_ASSET_UNKNOWN;

    const char *ext = strrchr(path, '.');
    if (!ext) return AGENTITE_ASSET_UNKNOWN;

    ext++;  /* Skip the dot */

    /* Texture extensions */
    if (strcasecmp(ext, "png") == 0 || strcasecmp(ext, "jpg") == 0 ||
        strcasecmp(ext, "jpeg") == 0 || strcasecmp(ext, "bmp") == 0 ||
        strcasecmp(ext, "tga") == 0 || strcasecmp(ext, "gif") == 0) {
        return AGENTITE_ASSET_TEXTURE;
    }

    /* Sound extensions */
    if (strcasecmp(ext, "wav") == 0 || strcasecmp(ext, "ogg") == 0 ||
        strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "flac") == 0) {
        return AGENTITE_ASSET_SOUND;
    }

    /* Prefab extension */
    if (strcasecmp(ext, "prefab") == 0) {
        return AGENTITE_ASSET_PREFAB;
    }

    return AGENTITE_ASSET_UNKNOWN;
}

static void extract_asset_refs_from_value(Agentite_Scene *scene,
                                           const Agentite_PropValue *value) {
    if (value->type == AGENTITE_PROP_STRING && value->string_val) {
        add_asset_ref(scene, value->string_val, guess_asset_type(value->string_val));
    }
}

static void extract_asset_refs_from_prefab(Agentite_Scene *scene,
                                            const Agentite_Prefab *prefab) {
    if (!prefab) return;

    /* Base prefab reference */
    if (prefab->base_prefab_name) {
        add_asset_ref(scene, prefab->base_prefab_name, AGENTITE_ASSET_PREFAB);
    }

    /* Component values */
    for (int i = 0; i < prefab->component_count; i++) {
        const Agentite_ComponentConfig *config = &prefab->components[i];
        for (int j = 0; j < config->field_count; j++) {
            extract_asset_refs_from_value(scene, &config->fields[j].value);
        }
    }

    /* Recurse into children */
    for (int i = 0; i < prefab->child_count; i++) {
        extract_asset_refs_from_prefab(scene, prefab->children[i]);
    }
}

static void extract_all_asset_refs(Agentite_Scene *scene) {
    for (size_t i = 0; i < scene->root_count; i++) {
        extract_asset_refs_from_prefab(scene, scene->roots[i]);
    }
}

/* ============================================================================
 * Scene Loading
 * ============================================================================ */

Agentite_Scene *agentite_scene_load_string(const char *source,
                                            size_t length,
                                            const char *name,
                                            const Agentite_SceneLoadContext *ctx) {
    if (!source) {
        set_scene_error("scene: Source is NULL");
        return NULL;
    }

    Agentite_Scene *scene = scene_create();
    if (!scene) {
        set_scene_error("scene: Failed to allocate scene");
        return NULL;
    }

    scene->name = strdup(name ? name : "unnamed");

    const Agentite_ReflectRegistry *reflect = ctx ? ctx->reflect : NULL;

    if (!parse_scene_source(scene, source, length, name ? name : "<string>", reflect)) {
        agentite_scene_destroy(scene);
        return NULL;
    }

    /* Extract asset references */
    extract_all_asset_refs(scene);

    return scene;
}

Agentite_Scene *agentite_scene_lookup(Agentite_SceneManager *manager,
                                       const char *path) {
    if (!manager || !path) return NULL;

    for (size_t i = 0; i < manager->count; i++) {
        if (strcmp(manager->entries[i].path, path) == 0) {
            return manager->entries[i].scene;
        }
    }

    return NULL;
}

Agentite_Scene *agentite_scene_load(Agentite_SceneManager *manager,
                                     const char *path,
                                     const Agentite_SceneLoadContext *ctx) {
    if (!manager || !path) {
        set_scene_error("scene: Invalid parameters");
        return NULL;
    }

    /* Check cache */
    Agentite_Scene *existing = agentite_scene_lookup(manager, path);
    if (existing) {
        return existing;
    }

    /* Check capacity */
    if (manager->count >= SCENE_MANAGER_CAPACITY) {
        set_scene_error("scene: Manager is full");
        return NULL;
    }

    /* Read file */
    size_t size;
    char *source = read_file(path, &size);
    if (!source) {
        return NULL;
    }

    /* Parse */
    Agentite_Scene *scene = agentite_scene_load_string(source, size, path, ctx);
    free(source);

    if (!scene) {
        return NULL;
    }

    /* Store path */
    scene->path = strdup(path);
    free(scene->name);
    scene->name = derive_scene_name(path);

    /* Add to manager */
    SceneEntry *entry = &manager->entries[manager->count++];
    entry->path = strdup(path);
    entry->scene = scene;

    return scene;
}

/* ============================================================================
 * Scene Instantiation
 * ============================================================================ */

static void track_entity(Agentite_Scene *scene, ecs_entity_t entity) {
    if (scene->entity_count >= scene->entity_capacity) {
        size_t new_capacity = scene->entity_capacity * 2;
        ecs_entity_t *new_entities = (ecs_entity_t *)realloc(
            scene->entities, new_capacity * sizeof(ecs_entity_t));
        if (!new_entities) return;
        scene->entities = new_entities;
        scene->entity_capacity = new_capacity;
    }
    scene->entities[scene->entity_count++] = entity;
}

static void track_spawned_entities(Agentite_Scene *scene,
                                    ecs_world_t *world,
                                    ecs_entity_t root) {
    track_entity(scene, root);

    /* Track children recursively */
    ecs_iter_t it = ecs_children(world, root);
    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            track_spawned_entities(scene, world, it.entities[i]);
        }
    }
}

bool agentite_scene_instantiate(Agentite_Scene *scene,
                                 ecs_world_t *world,
                                 const Agentite_SceneLoadContext *ctx) {
    if (!scene || !world) {
        set_scene_error("scene: Invalid parameters");
        return false;
    }

    if (scene->state == AGENTITE_SCENE_LOADED) {
        set_scene_error("scene: Already instantiated");
        return false;
    }

    if (scene->state != AGENTITE_SCENE_PARSED) {
        set_scene_error("scene: Scene not parsed");
        return false;
    }

    /* Reset entity tracking */
    scene->entity_count = 0;
    scene->root_entity_count = 0;

    /* Preload assets if requested */
    if (ctx && ctx->preload_assets) {
        agentite_scene_preload_assets(scene, ctx);
    }

    /* Spawn each root entity */
    for (size_t i = 0; i < scene->root_count; i++) {
        Agentite_Prefab *prefab = scene->roots[i];

        Agentite_SpawnContext spawn_ctx = {};
        spawn_ctx.world = world;
        spawn_ctx.reflect = ctx ? ctx->reflect : NULL;
        spawn_ctx.assets = ctx ? ctx->assets : NULL;
        spawn_ctx.prefabs = ctx ? ctx->prefabs : NULL;
        spawn_ctx.offset_x = prefab->position[0];
        spawn_ctx.offset_y = prefab->position[1];

        ecs_entity_t entity = agentite_prefab_spawn(prefab, &spawn_ctx);

        if (entity) {
            /* Track root entity */
            if (scene->root_entity_count < scene->root_capacity) {
                scene->root_entities[scene->root_entity_count++] = entity;
            }

            /* Track all entities including children */
            track_spawned_entities(scene, world, entity);
        }
    }

    scene->world = world;
    scene->state = AGENTITE_SCENE_LOADED;

    return true;
}

void agentite_scene_uninstantiate(Agentite_Scene *scene, ecs_world_t *world) {
    if (!scene || !world) return;

    if (scene->state != AGENTITE_SCENE_LOADED) return;

    scene->state = AGENTITE_SCENE_UNLOADING;

    /* Delete all tracked entities in reverse order */
    for (size_t i = scene->entity_count; i > 0; i--) {
        ecs_entity_t entity = scene->entities[i - 1];
        if (ecs_is_alive(world, entity)) {
            ecs_delete(world, entity);
        }
    }

    scene->entity_count = 0;
    scene->root_entity_count = 0;
    scene->world = NULL;
    scene->state = AGENTITE_SCENE_PARSED;
}

bool agentite_scene_is_instantiated(const Agentite_Scene *scene) {
    return scene && scene->state == AGENTITE_SCENE_LOADED;
}

/* ============================================================================
 * Scene Transitions
 * ============================================================================ */

Agentite_Scene *agentite_scene_transition(Agentite_SceneManager *manager,
                                           const char *path,
                                           ecs_world_t *world,
                                           const Agentite_SceneLoadContext *ctx) {
    if (!manager || !path || !world) {
        set_scene_error("scene: Invalid parameters");
        return NULL;
    }

    /* Load new scene first (before unloading old) */
    Agentite_Scene *new_scene = agentite_scene_load(manager, path, ctx);
    if (!new_scene) {
        return NULL;  /* Keep old scene */
    }

    /* Unload current active scene */
    if (manager->active_scene) {
        agentite_scene_uninstantiate(manager->active_scene, world);
    }

    /* Instantiate new scene */
    if (!agentite_scene_instantiate(new_scene, world, ctx)) {
        /* Failed to instantiate - try to restore old scene */
        if (manager->active_scene && manager->active_scene->state == AGENTITE_SCENE_PARSED) {
            agentite_scene_instantiate(manager->active_scene, world, ctx);
        }
        return NULL;
    }

    manager->active_scene = new_scene;
    return new_scene;
}

/* ============================================================================
 * Entity Access
 * ============================================================================ */

size_t agentite_scene_get_root_count(const Agentite_Scene *scene) {
    return scene ? scene->root_count : 0;
}

size_t agentite_scene_get_entity_count(const Agentite_Scene *scene) {
    return scene ? scene->entity_count : 0;
}

size_t agentite_scene_get_entities(const Agentite_Scene *scene,
                                    ecs_entity_t *out_entities,
                                    size_t max_count) {
    if (!scene || !out_entities || max_count == 0) return 0;

    size_t count = scene->entity_count < max_count ? scene->entity_count : max_count;
    memcpy(out_entities, scene->entities, count * sizeof(ecs_entity_t));
    return count;
}

size_t agentite_scene_get_root_entities(const Agentite_Scene *scene,
                                         ecs_entity_t *out_entities,
                                         size_t max_count) {
    if (!scene || !out_entities || max_count == 0) return 0;

    size_t count = scene->root_entity_count < max_count ?
                   scene->root_entity_count : max_count;
    memcpy(out_entities, scene->root_entities, count * sizeof(ecs_entity_t));
    return count;
}

ecs_entity_t agentite_scene_find_entity(const Agentite_Scene *scene,
                                         const char *name) {
    if (!scene || !name || !scene->world) return 0;

    for (size_t i = 0; i < scene->entity_count; i++) {
        ecs_entity_t entity = scene->entities[i];
        if (!ecs_is_alive(scene->world, entity)) continue;

        const char *entity_name = ecs_get_name(scene->world, entity);
        if (entity_name && strcmp(entity_name, name) == 0) {
            return entity;
        }
    }

    return 0;
}

/* ============================================================================
 * Asset Management
 * ============================================================================ */

size_t agentite_scene_get_asset_refs(const Agentite_Scene *scene,
                                      Agentite_AssetRef *out_refs,
                                      size_t max_count) {
    if (!scene || !out_refs || max_count == 0) return 0;

    size_t count = scene->asset_ref_count < max_count ?
                   scene->asset_ref_count : max_count;

    for (size_t i = 0; i < count; i++) {
        out_refs[i].path = scene->asset_refs[i].path;  /* Borrowed pointer */
        out_refs[i].type = scene->asset_refs[i].type;
    }

    return count;
}

bool agentite_scene_preload_assets(Agentite_Scene *scene,
                                    const Agentite_SceneLoadContext *ctx) {
    if (!scene || !ctx) return false;

    bool all_loaded = true;

    for (size_t i = 0; i < scene->asset_ref_count; i++) {
        const Agentite_AssetRef *ref = &scene->asset_refs[i];

        switch (ref->type) {
            case AGENTITE_ASSET_PREFAB:
                if (ctx->prefabs) {
                    Agentite_Prefab *prefab = agentite_prefab_load(
                        ctx->prefabs, ref->path, ctx->reflect);
                    if (!prefab) all_loaded = false;
                }
                break;

            case AGENTITE_ASSET_TEXTURE:
            case AGENTITE_ASSET_SOUND:
            case AGENTITE_ASSET_MUSIC:
                /* Asset loading would go through asset registry */
                /* For now, just track that we tried */
                break;

            default:
                break;
        }
    }

    return all_loaded;
}

/* ============================================================================
 * Scene Properties
 * ============================================================================ */

const char *agentite_scene_get_path(const Agentite_Scene *scene) {
    return scene ? scene->path : NULL;
}

const char *agentite_scene_get_name(const Agentite_Scene *scene) {
    return scene ? scene->name : NULL;
}

Agentite_SceneState agentite_scene_get_state(const Agentite_Scene *scene) {
    return scene ? scene->state : AGENTITE_SCENE_UNLOADED;
}

/* ============================================================================
 * Scene Writing
 * ============================================================================ */

char *agentite_scene_write_string(const Agentite_Scene *scene) {
    if (!scene) {
        set_scene_error("scene: Scene is NULL");
        return NULL;
    }

    if (scene->root_count == 0) {
        set_scene_error("scene: No entities to write");
        return NULL;
    }

    /* Build output by writing each root prefab */
    size_t total_len = 0;
    char **parts = (char **)calloc(scene->root_count, sizeof(char *));
    if (!parts) {
        set_scene_error("scene: Out of memory");
        return NULL;
    }

    for (size_t i = 0; i < scene->root_count; i++) {
        parts[i] = agentite_prefab_write_string(scene->roots[i]);
        if (!parts[i]) {
            for (size_t j = 0; j < i; j++) free(parts[j]);
            free(parts);
            return NULL;
        }
        total_len += strlen(parts[i]);
        if (i < scene->root_count - 1) total_len += 1;  /* Newline between entities */
    }

    /* Concatenate */
    char *result = (char *)malloc(total_len + 1);
    if (!result) {
        for (size_t i = 0; i < scene->root_count; i++) free(parts[i]);
        free(parts);
        set_scene_error("scene: Out of memory");
        return NULL;
    }

    char *p = result;
    for (size_t i = 0; i < scene->root_count; i++) {
        size_t len = strlen(parts[i]);
        memcpy(p, parts[i], len);
        p += len;
        if (i < scene->root_count - 1) {
            *p++ = '\n';
        }
        free(parts[i]);
    }
    *p = '\0';

    free(parts);
    return result;
}

bool agentite_scene_write_file(const Agentite_Scene *scene, const char *path) {
    if (!scene || !path) {
        set_scene_error("scene: Invalid parameters");
        return false;
    }

    char *content = agentite_scene_write_string(scene);
    if (!content) {
        return false;
    }

    FILE *file = fopen(path, "w");
    if (!file) {
        free(content);
        set_scene_error("scene: Failed to open '%s' for writing", path);
        return false;
    }

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);
    free(content);

    if (written != len) {
        set_scene_error("scene: Failed to write all data to '%s'", path);
        return false;
    }

    return true;
}

Agentite_Scene *agentite_scene_from_world(ecs_world_t *world,
                                           const Agentite_ReflectRegistry *reflect,
                                           const char *name) {
    (void)world; (void)reflect; (void)name;
    /* This is a more complex operation - for now return NULL */
    /* A full implementation would query all entities with a C_SceneEntity tag */
    set_scene_error("scene: agentite_scene_from_world not yet implemented");
    return NULL;
}

/* ============================================================================
 * Error Handling
 * ============================================================================ */

const char *agentite_scene_get_error(void) {
    return s_last_error;
}
