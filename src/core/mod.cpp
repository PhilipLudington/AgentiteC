/**
 * Agentite Engine - Mod System Implementation
 *
 * Provides mod loading, management, and virtual filesystem for asset overrides.
 */

#include "agentite/mod.h"
#include "agentite/event.h"
#include "agentite/error.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

/* TOML parsing */
extern "C" {
#include "toml.h"
}

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_MODS 128
#define MAX_SEARCH_PATHS 16
#define MAX_DEPENDENCIES 32
#define MAX_CONFLICTS 32
#define MAX_LOAD_ORDER_HINTS 16
#define PATH_BUFFER_SIZE 512
#define RESOLVED_PATH_BUFFER_SIZE 1024

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/**
 * Dependency entry.
 */
typedef struct ModDependency {
    char id[64];
    char version_constraint[32];
} ModDependency;

/**
 * Load order hint.
 */
typedef struct LoadOrderHint {
    char id[64];
    bool is_before;  /* true = load before, false = load after */
} LoadOrderHint;

/**
 * Internal mod entry with full data.
 */
typedef struct ModEntry {
    Agentite_ModInfo info;
    bool active;

    /* Dependencies */
    ModDependency dependencies[MAX_DEPENDENCIES];
    size_t dependency_count;

    /* Conflicts */
    char conflicts[MAX_CONFLICTS][64];
    size_t conflict_count;

    /* Load order hints */
    LoadOrderHint load_hints[MAX_LOAD_ORDER_HINTS];
    size_t load_hint_count;

    /* Asset directories */
    char asset_dirs[8][64];
    size_t asset_dir_count;

    /* Enabled state (persisted) */
    bool enabled;
} ModEntry;

/**
 * Mod manager structure.
 */
struct Agentite_ModManager {
    /* Configuration */
    Agentite_ModManagerConfig config;

    /* Search paths */
    char search_paths[MAX_SEARCH_PATHS][PATH_BUFFER_SIZE];
    size_t search_path_count;

    /* Discovered mods */
    ModEntry mods[MAX_MODS];
    size_t mod_count;

    /* Load order (indices into mods array) */
    size_t load_order[MAX_MODS];
    size_t loaded_count;

    /* Callback */
    Agentite_ModCallback callback;
    void *callback_userdata;

    /* Path resolution buffer (reused) */
    char resolved_path[RESOLVED_PATH_BUFFER_SIZE];
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Check if a file exists.
 */
static bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

/**
 * Check if a path is a directory.
 */
static bool is_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

/**
 * Find mod entry by ID.
 */
static ModEntry *find_mod(Agentite_ModManager *manager, const char *mod_id)
{
    for (size_t i = 0; i < manager->mod_count; i++) {
        if (manager->mods[i].active &&
            strcmp(manager->mods[i].info.id, mod_id) == 0) {
            return &manager->mods[i];
        }
    }
    return NULL;
}

/**
 * Find empty mod slot.
 */
static ModEntry *find_empty_mod_slot(Agentite_ModManager *manager)
{
    for (size_t i = 0; i < MAX_MODS; i++) {
        if (!manager->mods[i].active) {
            return &manager->mods[i];
        }
    }
    return NULL;
}

/**
 * Safe string copy with TOML datum.
 */
static void copy_toml_string(toml_table_t *table, const char *key, char *out, size_t out_size)
{
    toml_datum_t datum = toml_string_in(table, key);
    if (datum.ok) {
        strncpy(out, datum.u.s, out_size - 1);
        out[out_size - 1] = '\0';
        free(datum.u.s);
    } else {
        out[0] = '\0';
    }
}

/**
 * Parse mod.toml manifest file.
 */
static bool parse_mod_manifest(const char *manifest_path, ModEntry *mod)
{
    FILE *fp = fopen(manifest_path, "r");
    if (!fp) {
        agentite_set_error("mod: failed to open manifest: %s", manifest_path);
        return false;
    }

    char errbuf[256];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root) {
        agentite_set_error("mod: failed to parse manifest %s: %s", manifest_path, errbuf);
        return false;
    }

    /* Parse [mod] section */
    toml_table_t *mod_table = toml_table_in(root, "mod");
    if (!mod_table) {
        agentite_set_error("mod: missing [mod] section in %s", manifest_path);
        toml_free(root);
        return false;
    }

    /* Required fields */
    copy_toml_string(mod_table, "id", mod->info.id, sizeof(mod->info.id));
    copy_toml_string(mod_table, "name", mod->info.name, sizeof(mod->info.name));
    copy_toml_string(mod_table, "version", mod->info.version, sizeof(mod->info.version));

    if (mod->info.id[0] == '\0') {
        agentite_set_error("mod: missing required 'id' in %s", manifest_path);
        toml_free(root);
        return false;
    }

    /* Optional fields */
    copy_toml_string(mod_table, "author", mod->info.author, sizeof(mod->info.author));
    copy_toml_string(mod_table, "description", mod->info.description, sizeof(mod->info.description));
    copy_toml_string(mod_table, "min_engine_version", mod->info.min_engine_version,
                     sizeof(mod->info.min_engine_version));

    /* Parse [dependencies] section */
    toml_table_t *deps = toml_table_in(root, "dependencies");
    if (deps) {
        int key_count = toml_table_nkval(deps);
        for (int i = 0; i < key_count && mod->dependency_count < MAX_DEPENDENCIES; i++) {
            const char *key = toml_key_in(deps, i);
            if (key) {
                toml_datum_t val = toml_string_in(deps, key);
                if (val.ok) {
                    strncpy(mod->dependencies[mod->dependency_count].id, key,
                            sizeof(mod->dependencies[0].id) - 1);
                    strncpy(mod->dependencies[mod->dependency_count].version_constraint, val.u.s,
                            sizeof(mod->dependencies[0].version_constraint) - 1);
                    mod->dependency_count++;
                    free(val.u.s);
                }
            }
        }
        mod->info.dependency_count = mod->dependency_count;
    }

    /* Parse [conflicts] section */
    toml_table_t *conflicts = toml_table_in(root, "conflicts");
    if (conflicts) {
        int key_count = toml_table_nkval(conflicts);
        for (int i = 0; i < key_count && mod->conflict_count < MAX_CONFLICTS; i++) {
            const char *key = toml_key_in(conflicts, i);
            if (key) {
                strncpy(mod->conflicts[mod->conflict_count], key,
                        sizeof(mod->conflicts[0]) - 1);
                mod->conflict_count++;
            }
        }
        mod->info.conflict_count = mod->conflict_count;
    }

    /* Parse [load_order] section */
    toml_table_t *load_order = toml_table_in(root, "load_order");
    if (load_order) {
        /* Parse 'before' array */
        toml_array_t *before = toml_array_in(load_order, "before");
        if (before) {
            int n = toml_array_nelem(before);
            for (int i = 0; i < n && mod->load_hint_count < MAX_LOAD_ORDER_HINTS; i++) {
                toml_datum_t val = toml_string_at(before, i);
                if (val.ok) {
                    strncpy(mod->load_hints[mod->load_hint_count].id, val.u.s,
                            sizeof(mod->load_hints[0].id) - 1);
                    mod->load_hints[mod->load_hint_count].is_before = true;
                    mod->load_hint_count++;
                    free(val.u.s);
                }
            }
        }

        /* Parse 'after' array */
        toml_array_t *after = toml_array_in(load_order, "after");
        if (after) {
            int n = toml_array_nelem(after);
            for (int i = 0; i < n && mod->load_hint_count < MAX_LOAD_ORDER_HINTS; i++) {
                toml_datum_t val = toml_string_at(after, i);
                if (val.ok) {
                    strncpy(mod->load_hints[mod->load_hint_count].id, val.u.s,
                            sizeof(mod->load_hints[0].id) - 1);
                    mod->load_hints[mod->load_hint_count].is_before = false;
                    mod->load_hint_count++;
                    free(val.u.s);
                }
            }
        }
    }

    /* Parse [assets] section */
    toml_table_t *assets = toml_table_in(root, "assets");
    if (assets) {
        int key_count = toml_table_nkval(assets);
        for (int i = 0; i < key_count && mod->asset_dir_count < 8; i++) {
            const char *key = toml_key_in(assets, i);
            if (key) {
                toml_datum_t val = toml_string_in(assets, key);
                if (val.ok) {
                    strncpy(mod->asset_dirs[mod->asset_dir_count], val.u.s,
                            sizeof(mod->asset_dirs[0]) - 1);
                    mod->asset_dir_count++;
                    free(val.u.s);
                }
            }
        }
    }

    toml_free(root);

    mod->info.state = AGENTITE_MOD_DISCOVERED;
    mod->active = true;
    mod->enabled = true;

    return true;
}

/**
 * Emit mod event.
 */
static void emit_mod_event(Agentite_ModManager *manager, const ModEntry *mod)
{
    if (!manager->config.events || !manager->config.emit_events) {
        return;
    }

    Agentite_Event event;
    memset(&event, 0, sizeof(event));

    switch (mod->info.state) {
        case AGENTITE_MOD_LOADED:
            event.type = AGENTITE_EVENT_MOD_LOADED;
            break;
        case AGENTITE_MOD_UNLOADED:
            event.type = AGENTITE_EVENT_MOD_UNLOADED;
            break;
        default:
            return;
    }

    event.mod.mod_id = mod->info.id;
    event.mod.mod_name = mod->info.name;
    event.mod.state = mod->info.state;

    agentite_event_emit(manager->config.events, &event);
}

/**
 * Invoke callback for mod state change.
 */
static void invoke_callback(Agentite_ModManager *manager, const ModEntry *mod)
{
    if (manager->callback) {
        manager->callback(mod->info.id, mod->info.state, manager->callback_userdata);
    }
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

Agentite_ModManager *agentite_mod_manager_create(const Agentite_ModManagerConfig *config)
{
    Agentite_ModManager *manager = (Agentite_ModManager *)calloc(1, sizeof(Agentite_ModManager));
    if (!manager) {
        agentite_set_error("mod: failed to allocate manager");
        return NULL;
    }

    if (config) {
        manager->config = *config;
    } else {
        manager->config = AGENTITE_MOD_MANAGER_CONFIG_DEFAULT;
    }

    return manager;
}

void agentite_mod_manager_destroy(Agentite_ModManager *manager)
{
    if (!manager) return;

    /* Unload all mods */
    agentite_mod_unload_all(manager);

    free(manager);
}

/* ============================================================================
 * Search Paths
 * ============================================================================ */

bool agentite_mod_add_search_path(Agentite_ModManager *manager, const char *path)
{
    if (!manager || !path) return false;

    if (manager->search_path_count >= MAX_SEARCH_PATHS) {
        agentite_set_error("mod: maximum search paths reached");
        return false;
    }

    if (!is_directory(path)) {
        agentite_set_error("mod: search path is not a directory: %s", path);
        return false;
    }

    strncpy(manager->search_paths[manager->search_path_count], path,
            sizeof(manager->search_paths[0]) - 1);
    manager->search_path_count++;

    return true;
}

void agentite_mod_remove_search_path(Agentite_ModManager *manager, const char *path)
{
    if (!manager || !path) return;

    for (size_t i = 0; i < manager->search_path_count; i++) {
        if (strcmp(manager->search_paths[i], path) == 0) {
            /* Shift remaining paths */
            for (size_t j = i; j < manager->search_path_count - 1; j++) {
                strcpy(manager->search_paths[j], manager->search_paths[j + 1]);
            }
            manager->search_path_count--;
            break;
        }
    }
}

/* ============================================================================
 * Discovery
 * ============================================================================ */

size_t agentite_mod_scan(Agentite_ModManager *manager)
{
    if (!manager) return 0;

    size_t found = 0;

    /* Scan each search path */
    for (size_t i = 0; i < manager->search_path_count; i++) {
        DIR *dir = opendir(manager->search_paths[i]);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            /* Skip . and .. */
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            /* Build path to potential mod directory */
            char mod_dir[PATH_BUFFER_SIZE];
            snprintf(mod_dir, sizeof(mod_dir), "%s/%s",
                     manager->search_paths[i], entry->d_name);

            if (!is_directory(mod_dir)) {
                continue;
            }

            /* Check for mod.toml */
            char manifest_path[PATH_BUFFER_SIZE];
            snprintf(manifest_path, sizeof(manifest_path), "%s/mod.toml", mod_dir);

            if (!file_exists(manifest_path)) {
                continue;
            }

            /* Check if already discovered */
            bool already_found = false;
            for (size_t j = 0; j < manager->mod_count; j++) {
                if (manager->mods[j].active &&
                    strcmp(manager->mods[j].info.path, mod_dir) == 0) {
                    already_found = true;
                    break;
                }
            }
            if (already_found) {
                continue;
            }

            /* Find empty slot */
            ModEntry *mod = find_empty_mod_slot(manager);
            if (!mod) {
                SDL_Log("mod: maximum mods reached, skipping: %s", mod_dir);
                continue;
            }

            /* Parse manifest */
            memset(mod, 0, sizeof(ModEntry));
            if (parse_mod_manifest(manifest_path, mod)) {
                strncpy(mod->info.path, mod_dir, sizeof(mod->info.path) - 1);
                mod->active = true;  /* Mark slot as in use */
                manager->mod_count++;
                found++;
                SDL_Log("mod: discovered '%s' v%s at %s",
                        mod->info.name, mod->info.version, mod_dir);
            }
        }

        closedir(dir);
    }

    return found;
}

void agentite_mod_refresh(Agentite_ModManager *manager)
{
    /* Simple implementation: just scan again */
    agentite_mod_scan(manager);
}

/* ============================================================================
 * Query
 * ============================================================================ */

size_t agentite_mod_count(const Agentite_ModManager *manager)
{
    if (!manager) return 0;

    size_t count = 0;
    for (size_t i = 0; i < MAX_MODS; i++) {
        if (manager->mods[i].active) {
            count++;
        }
    }
    return count;
}

const Agentite_ModInfo *agentite_mod_get_info(const Agentite_ModManager *manager, size_t index)
{
    if (!manager) return NULL;

    size_t current = 0;
    for (size_t i = 0; i < MAX_MODS; i++) {
        if (manager->mods[i].active) {
            if (current == index) {
                return &manager->mods[i].info;
            }
            current++;
        }
    }
    return NULL;
}

const Agentite_ModInfo *agentite_mod_find(const Agentite_ModManager *manager, const char *mod_id)
{
    if (!manager || !mod_id) return NULL;

    ModEntry *mod = find_mod((Agentite_ModManager *)manager, mod_id);
    return mod ? &mod->info : NULL;
}

Agentite_ModState agentite_mod_get_state(const Agentite_ModManager *manager, const char *mod_id)
{
    const Agentite_ModInfo *info = agentite_mod_find(manager, mod_id);
    return info ? info->state : AGENTITE_MOD_UNLOADED;
}

size_t agentite_mod_get_dependencies(const Agentite_ModManager *manager,
                                      const char *mod_id,
                                      const char **out_deps,
                                      size_t max_deps)
{
    if (!manager || !mod_id || !out_deps) return 0;

    ModEntry *mod = find_mod((Agentite_ModManager *)manager, mod_id);
    if (!mod) return 0;

    size_t count = mod->dependency_count < max_deps ? mod->dependency_count : max_deps;
    for (size_t i = 0; i < count; i++) {
        out_deps[i] = mod->dependencies[i].id;
    }
    return mod->dependency_count;
}

size_t agentite_mod_get_conflicts(const Agentite_ModManager *manager,
                                   const char *mod_id,
                                   const char **out_conflicts,
                                   size_t max_conflicts)
{
    if (!manager || !mod_id || !out_conflicts) return 0;

    ModEntry *mod = find_mod((Agentite_ModManager *)manager, mod_id);
    if (!mod) return 0;

    size_t count = mod->conflict_count < max_conflicts ? mod->conflict_count : max_conflicts;
    for (size_t i = 0; i < count; i++) {
        out_conflicts[i] = mod->conflicts[i];
    }
    return mod->conflict_count;
}

/* ============================================================================
 * Load Order Resolution
 * ============================================================================ */

bool agentite_mod_resolve_load_order(Agentite_ModManager *manager,
                                      const char **enabled_mods,
                                      size_t enabled_count,
                                      char ***out_ordered,
                                      size_t *out_count)
{
    if (!manager || !enabled_mods || !out_ordered || !out_count) return false;

    /* Simple implementation: just copy in order for now
     * Full implementation would do topological sort */

    *out_ordered = (char **)calloc(enabled_count, sizeof(char *));
    if (!*out_ordered) {
        agentite_set_error("mod: failed to allocate load order array");
        return false;
    }

    for (size_t i = 0; i < enabled_count; i++) {
        (*out_ordered)[i] = strdup(enabled_mods[i]);
    }
    *out_count = enabled_count;

    return true;
}

void agentite_mod_free_load_order(char **ordered, size_t count)
{
    if (!ordered) return;
    for (size_t i = 0; i < count; i++) {
        free(ordered[i]);
    }
    free(ordered);
}

/* ============================================================================
 * Validation
 * ============================================================================ */

bool agentite_mod_validate(const Agentite_ModManager *manager,
                            const char *mod_id,
                            char **out_error)
{
    if (!manager || !mod_id) return false;

    const Agentite_ModInfo *info = agentite_mod_find(manager, mod_id);
    if (!info) {
        if (out_error) {
            *out_error = strdup("mod not found");
        }
        return false;
    }

    /* Check required fields */
    if (info->id[0] == '\0') {
        if (out_error) *out_error = strdup("missing mod ID");
        return false;
    }

    return true;
}

bool agentite_mod_check_conflicts(const Agentite_ModManager *manager,
                                   const char **enabled_mods,
                                   size_t enabled_count,
                                   char ***out_conflicts,
                                   size_t *out_count)
{
    if (!manager || !enabled_mods) return true;

    /* Check each mod pair for conflicts */
    for (size_t i = 0; i < enabled_count; i++) {
        ModEntry *mod_a = find_mod((Agentite_ModManager *)manager, enabled_mods[i]);
        if (!mod_a) continue;

        for (size_t j = 0; j < mod_a->conflict_count; j++) {
            for (size_t k = 0; k < enabled_count; k++) {
                if (strcmp(enabled_mods[k], mod_a->conflicts[j]) == 0) {
                    /* Found conflict */
                    if (out_conflicts && out_count) {
                        /* Simple: just return first conflict */
                        *out_conflicts = (char **)calloc(2, sizeof(char *));
                        (*out_conflicts)[0] = strdup(enabled_mods[i]);
                        (*out_conflicts)[1] = strdup(enabled_mods[k]);
                        *out_count = 2;
                    }
                    return false;
                }
            }
        }
    }

    if (out_count) *out_count = 0;
    return true;
}

void agentite_mod_free_conflicts(char **conflicts, size_t count)
{
    if (!conflicts) return;
    for (size_t i = 0; i < count; i++) {
        free(conflicts[i]);
    }
    free(conflicts);
}

/* ============================================================================
 * Loading
 * ============================================================================ */

bool agentite_mod_load(Agentite_ModManager *manager, const char *mod_id)
{
    if (!manager || !mod_id) return false;

    ModEntry *mod = find_mod(manager, mod_id);
    if (!mod) {
        agentite_set_error("mod: mod not found: %s", mod_id);
        return false;
    }

    if (mod->info.state == AGENTITE_MOD_LOADED) {
        return true;  /* Already loaded */
    }

    if (!mod->enabled) {
        agentite_set_error("mod: mod is disabled: %s", mod_id);
        return false;
    }

    mod->info.state = AGENTITE_MOD_LOADING;

    /* Load dependencies first */
    for (size_t i = 0; i < mod->dependency_count; i++) {
        const char *dep_id = mod->dependencies[i].id;
        ModEntry *dep = find_mod(manager, dep_id);
        if (dep && dep->info.state != AGENTITE_MOD_LOADED) {
            if (!agentite_mod_load(manager, dep_id)) {
                mod->info.state = AGENTITE_MOD_FAILED;
                agentite_set_error("mod: failed to load dependency '%s' for '%s'",
                                   dep_id, mod_id);
                return false;
            }
        }
    }

    /* Mark as loaded */
    mod->info.state = AGENTITE_MOD_LOADED;
    manager->load_order[manager->loaded_count++] = mod - manager->mods;

    SDL_Log("mod: loaded '%s' v%s", mod->info.name, mod->info.version);

    emit_mod_event(manager, mod);
    invoke_callback(manager, mod);

    return true;
}

bool agentite_mod_load_all(Agentite_ModManager *manager,
                            const char **enabled_mods,
                            size_t enabled_count)
{
    if (!manager || !enabled_mods) return false;

    bool all_success = true;
    for (size_t i = 0; i < enabled_count; i++) {
        if (!agentite_mod_load(manager, enabled_mods[i])) {
            all_success = false;
        }
    }

    return all_success;
}

void agentite_mod_unload(Agentite_ModManager *manager, const char *mod_id)
{
    if (!manager || !mod_id) return;

    ModEntry *mod = find_mod(manager, mod_id);
    if (!mod || mod->info.state != AGENTITE_MOD_LOADED) {
        return;
    }

    mod->info.state = AGENTITE_MOD_UNLOADED;

    /* Remove from load order */
    size_t mod_idx = mod - manager->mods;
    for (size_t i = 0; i < manager->loaded_count; i++) {
        if (manager->load_order[i] == mod_idx) {
            for (size_t j = i; j < manager->loaded_count - 1; j++) {
                manager->load_order[j] = manager->load_order[j + 1];
            }
            manager->loaded_count--;
            break;
        }
    }

    SDL_Log("mod: unloaded '%s'", mod->info.name);

    emit_mod_event(manager, mod);
    invoke_callback(manager, mod);
}

void agentite_mod_unload_all(Agentite_ModManager *manager)
{
    if (!manager) return;

    /* Unload in reverse order */
    while (manager->loaded_count > 0) {
        size_t idx = manager->load_order[manager->loaded_count - 1];
        agentite_mod_unload(manager, manager->mods[idx].info.id);
    }
}

/* ============================================================================
 * Virtual Filesystem
 * ============================================================================ */

const char *agentite_mod_resolve_path(const Agentite_ModManager *manager,
                                       const char *virtual_path)
{
    if (!manager || !virtual_path) return NULL;

    Agentite_ModManager *m = (Agentite_ModManager *)manager;

    /* Check loaded mods in reverse order (last loaded has highest priority) */
    for (size_t i = manager->loaded_count; i > 0; i--) {
        size_t idx = manager->load_order[i - 1];
        const ModEntry *mod = &manager->mods[idx];

        /* Build potential mod path */
        char mod_path[PATH_BUFFER_SIZE];
        snprintf(mod_path, sizeof(mod_path), "%s/%s", mod->info.path, virtual_path);

        if (file_exists(mod_path)) {
            strncpy(m->resolved_path, mod_path, sizeof(m->resolved_path) - 1);
            return m->resolved_path;
        }
    }

    /* No override found, return original path */
    strncpy(m->resolved_path, virtual_path, sizeof(m->resolved_path) - 1);
    return m->resolved_path;
}

bool agentite_mod_has_override(const Agentite_ModManager *manager,
                                const char *virtual_path)
{
    if (!manager || !virtual_path) return false;

    for (size_t i = manager->loaded_count; i > 0; i--) {
        size_t idx = manager->load_order[i - 1];
        const ModEntry *mod = &manager->mods[idx];

        char mod_path[PATH_BUFFER_SIZE];
        snprintf(mod_path, sizeof(mod_path), "%s/%s", mod->info.path, virtual_path);

        if (file_exists(mod_path)) {
            return true;
        }
    }

    return false;
}

const char *agentite_mod_get_override_source(const Agentite_ModManager *manager,
                                              const char *virtual_path)
{
    if (!manager || !virtual_path) return NULL;

    for (size_t i = manager->loaded_count; i > 0; i--) {
        size_t idx = manager->load_order[i - 1];
        const ModEntry *mod = &manager->mods[idx];

        char mod_path[PATH_BUFFER_SIZE];
        snprintf(mod_path, sizeof(mod_path), "%s/%s", mod->info.path, virtual_path);

        if (file_exists(mod_path)) {
            return mod->info.id;
        }
    }

    return NULL;
}

/* ============================================================================
 * Enable/Disable
 * ============================================================================ */

bool agentite_mod_set_enabled(Agentite_ModManager *manager, const char *mod_id, bool enabled)
{
    if (!manager || !mod_id) return false;

    ModEntry *mod = find_mod(manager, mod_id);
    if (!mod) return false;

    if (mod->enabled == enabled) return false;

    mod->enabled = enabled;
    if (!enabled) {
        mod->info.state = AGENTITE_MOD_DISABLED;
        /* Unload if currently loaded */
        agentite_mod_unload(manager, mod_id);
    } else if (mod->info.state == AGENTITE_MOD_DISABLED) {
        mod->info.state = AGENTITE_MOD_DISCOVERED;
    }

    return true;
}

bool agentite_mod_is_enabled(const Agentite_ModManager *manager, const char *mod_id)
{
    if (!manager || !mod_id) return false;

    ModEntry *mod = find_mod((Agentite_ModManager *)manager, mod_id);
    return mod ? mod->enabled : false;
}

/* ============================================================================
 * Persistence
 * ============================================================================ */

bool agentite_mod_save_enabled(const Agentite_ModManager *manager, const char *path)
{
    if (!manager || !path) return false;

    FILE *fp = fopen(path, "w");
    if (!fp) {
        agentite_set_error("mod: failed to open file for writing: %s", path);
        return false;
    }

    fprintf(fp, "# Enabled mods configuration\n\n");
    fprintf(fp, "enabled = [\n");

    bool first = true;
    for (size_t i = 0; i < MAX_MODS; i++) {
        if (manager->mods[i].active && manager->mods[i].enabled) {
            if (!first) fprintf(fp, ",\n");
            fprintf(fp, "    \"%s\"", manager->mods[i].info.id);
            first = false;
        }
    }

    fprintf(fp, "\n]\n");
    fclose(fp);

    return true;
}

bool agentite_mod_load_enabled(Agentite_ModManager *manager, const char *path)
{
    if (!manager || !path) return false;

    FILE *fp = fopen(path, "r");
    if (!fp) {
        /* Not an error - file might not exist yet */
        return true;
    }

    char errbuf[256];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root) {
        agentite_set_error("mod: failed to parse enabled mods file: %s", errbuf);
        return false;
    }

    /* Disable all mods first */
    for (size_t i = 0; i < MAX_MODS; i++) {
        if (manager->mods[i].active) {
            manager->mods[i].enabled = false;
            manager->mods[i].info.state = AGENTITE_MOD_DISABLED;
        }
    }

    /* Enable mods from file */
    toml_array_t *enabled = toml_array_in(root, "enabled");
    if (enabled) {
        int n = toml_array_nelem(enabled);
        for (int i = 0; i < n; i++) {
            toml_datum_t val = toml_string_at(enabled, i);
            if (val.ok) {
                ModEntry *mod = find_mod(manager, val.u.s);
                if (mod) {
                    mod->enabled = true;
                    mod->info.state = AGENTITE_MOD_DISCOVERED;
                }
                free(val.u.s);
            }
        }
    }

    toml_free(root);
    return true;
}

/* ============================================================================
 * Callbacks
 * ============================================================================ */

void agentite_mod_set_callback(Agentite_ModManager *manager,
                                Agentite_ModCallback callback,
                                void *userdata)
{
    if (!manager) return;
    manager->callback = callback;
    manager->callback_userdata = userdata;
}

/* ============================================================================
 * Utility
 * ============================================================================ */

const char *agentite_mod_state_name(Agentite_ModState state)
{
    switch (state) {
        case AGENTITE_MOD_UNLOADED:   return "UNLOADED";
        case AGENTITE_MOD_DISCOVERED: return "DISCOVERED";
        case AGENTITE_MOD_LOADING:    return "LOADING";
        case AGENTITE_MOD_LOADED:     return "LOADED";
        case AGENTITE_MOD_FAILED:     return "FAILED";
        case AGENTITE_MOD_DISABLED:   return "DISABLED";
        default:                       return "UNKNOWN";
    }
}

size_t agentite_mod_loaded_count(const Agentite_ModManager *manager)
{
    if (!manager) return 0;
    return manager->loaded_count;
}
