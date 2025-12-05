#include "carbon/save.h"
#include "toml.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

struct Carbon_SaveWriter {
    FILE *fp;
    char current_section[64];
    bool in_section;
};

struct Carbon_SaveManager {
    char saves_dir[CARBON_SAVE_MAX_PATH];
    int version;
    int min_compatible;
};

// Create directory if it doesn't exist
static bool ensure_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        return mkdir(path, 0755) == 0;
    }
    return true;
}

// Get current timestamp in ISO 8601 format
static void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%Y-%m-%dT%H:%M:%S", tm_info);
}

// Build save file path
static void build_save_path(const Carbon_SaveManager *sm, const char *save_name,
                            char *out_path, size_t path_size) {
    snprintf(out_path, path_size, "%s/%s.toml", sm->saves_dir, save_name);
}

Carbon_SaveManager *carbon_save_create(const char *saves_dir) {
    Carbon_SaveManager *sm = calloc(1, sizeof(Carbon_SaveManager));
    if (!sm) return NULL;

    if (saves_dir && saves_dir[0]) {
        strncpy(sm->saves_dir, saves_dir, sizeof(sm->saves_dir) - 1);
    } else {
        strncpy(sm->saves_dir, "./saves", sizeof(sm->saves_dir) - 1);
    }

    sm->version = 1;
    sm->min_compatible = 1;

    // Create saves directory
    ensure_directory(sm->saves_dir);

    return sm;
}

void carbon_save_destroy(Carbon_SaveManager *sm) {
    free(sm);
}

void carbon_save_set_version(Carbon_SaveManager *sm, int version, int min_compatible) {
    if (!sm) return;
    sm->version = version;
    sm->min_compatible = min_compatible;
}

Carbon_SaveResult carbon_save_game(Carbon_SaveManager *sm,
                                    const char *save_name,
                                    Carbon_SerializeFunc serialize,
                                    void *game_state) {
    Carbon_SaveResult result = {0};

    if (!sm || !save_name || !serialize) {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Invalid parameters");
        return result;
    }

    build_save_path(sm, save_name, result.filepath, sizeof(result.filepath));

    FILE *fp = fopen(result.filepath, "w");
    if (!fp) {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Cannot create save file: %s", result.filepath);
        return result;
    }

    // Write metadata section
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));

    fprintf(fp, "[metadata]\n");
    fprintf(fp, "version = %d\n", sm->version);
    fprintf(fp, "timestamp = \"%s\"\n", timestamp);
    fprintf(fp, "save_name = \"%s\"\n", save_name);
    fprintf(fp, "\n");

    // Create writer for game state
    Carbon_SaveWriter writer = {0};
    writer.fp = fp;
    writer.in_section = false;

    // Write game_state section header
    fprintf(fp, "[game_state]\n");
    strcpy(writer.current_section, "game_state");
    writer.in_section = true;

    // Let game serialize its state
    bool success = serialize(game_state, &writer);

    fclose(fp);

    if (success) {
        result.success = true;
        result.save_version = sm->version;
    } else {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Serialization failed");
    }

    return result;
}

Carbon_SaveResult carbon_load_game(Carbon_SaveManager *sm,
                                    const char *save_name,
                                    Carbon_DeserializeFunc deserialize,
                                    void *game_state) {
    Carbon_SaveResult result = {0};

    if (!sm || !save_name || !deserialize) {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Invalid parameters");
        return result;
    }

    build_save_path(sm, save_name, result.filepath, sizeof(result.filepath));

    FILE *fp = fopen(result.filepath, "r");
    if (!fp) {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Save file not found: %s", result.filepath);
        return result;
    }

    char errbuf[256];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root) {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Parse error: %s", errbuf);
        return result;
    }

    // Check version in metadata
    toml_table_t *metadata = toml_table_in(root, "metadata");
    if (metadata) {
        toml_datum_t d = toml_int_in(metadata, "version");
        if (d.ok) {
            result.save_version = (int)d.u.i;

            if (result.save_version < sm->min_compatible) {
                snprintf(result.error_message, sizeof(result.error_message),
                         "Save version %d is too old (min: %d)",
                         result.save_version, sm->min_compatible);
                toml_free(root);
                return result;
            }

            if (result.save_version != sm->version) {
                result.was_migrated = true;
            }
        }
    }

    // Create reader
    Carbon_SaveReader reader = {0};
    reader.root = root;
    reader.game_state = toml_table_in(root, "game_state");

    // Let game deserialize its state
    bool success = deserialize(game_state, &reader);

    toml_free(root);

    if (success) {
        result.success = true;
    } else {
        snprintf(result.error_message, sizeof(result.error_message),
                 "Deserialization failed");
    }

    return result;
}

Carbon_SaveResult carbon_save_quick(Carbon_SaveManager *sm,
                                     Carbon_SerializeFunc serialize,
                                     void *game_state) {
    return carbon_save_game(sm, "quicksave", serialize, game_state);
}

Carbon_SaveResult carbon_load_quick(Carbon_SaveManager *sm,
                                     Carbon_DeserializeFunc deserialize,
                                     void *game_state) {
    return carbon_load_game(sm, "quicksave", deserialize, game_state);
}

Carbon_SaveResult carbon_save_auto(Carbon_SaveManager *sm,
                                    Carbon_SerializeFunc serialize,
                                    void *game_state) {
    return carbon_save_game(sm, "autosave", serialize, game_state);
}

Carbon_SaveInfo *carbon_save_list(const Carbon_SaveManager *sm, int *out_count) {
    if (!sm || !out_count) return NULL;

    *out_count = 0;

    DIR *dir = opendir(sm->saves_dir);
    if (!dir) return NULL;

    // First pass: count .toml files
    int capacity = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 5 && strcmp(entry->d_name + len - 5, ".toml") == 0) {
            capacity++;
        }
    }

    if (capacity == 0) {
        closedir(dir);
        return NULL;
    }

    Carbon_SaveInfo *list = calloc(capacity, sizeof(Carbon_SaveInfo));
    if (!list) {
        closedir(dir);
        return NULL;
    }

    // Second pass: read save info
    rewinddir(dir);
    int count = 0;

    while ((entry = readdir(dir)) != NULL && count < capacity) {
        size_t len = strlen(entry->d_name);
        if (len <= 5 || strcmp(entry->d_name + len - 5, ".toml") != 0) {
            continue;
        }

        Carbon_SaveInfo *info = &list[count];
        strncpy(info->filename, entry->d_name, sizeof(info->filename) - 1);

        // Remove .toml extension for display name
        strncpy(info->display_name, entry->d_name, sizeof(info->display_name) - 1);
        size_t name_len = strlen(info->display_name);
        if (name_len > 5) {
            info->display_name[name_len - 5] = '\0';
        }

        // Try to read metadata from file
        char filepath[CARBON_SAVE_MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s/%s", sm->saves_dir, entry->d_name);

        FILE *fp = fopen(filepath, "r");
        if (fp) {
            char errbuf[256];
            toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
            fclose(fp);

            if (root) {
                toml_table_t *metadata = toml_table_in(root, "metadata");
                if (metadata) {
                    toml_datum_t d = toml_int_in(metadata, "version");
                    if (d.ok) {
                        info->version = (int)d.u.i;
                        info->is_compatible = info->version >= sm->min_compatible;
                    }

                    d = toml_string_in(metadata, "timestamp");
                    if (d.ok) {
                        strncpy(info->timestamp, d.u.s, sizeof(info->timestamp) - 1);
                        free(d.u.s);
                    }
                }

                // Try to read preview data from game_state
                toml_table_t *gs = toml_table_in(root, "game_state");
                if (gs) {
                    toml_datum_t d = toml_int_in(gs, "turn");
                    if (d.ok) info->preview_turn = (int)d.u.i;
                }

                toml_free(root);
            }
        }

        count++;
    }

    closedir(dir);
    *out_count = count;
    return list;
}

void carbon_save_list_free(Carbon_SaveInfo *list) {
    free(list);
}

bool carbon_save_delete(Carbon_SaveManager *sm, const char *save_name) {
    if (!sm || !save_name) return false;

    char filepath[CARBON_SAVE_MAX_PATH];
    build_save_path(sm, save_name, filepath, sizeof(filepath));

    return remove(filepath) == 0;
}

bool carbon_save_exists(const Carbon_SaveManager *sm, const char *save_name) {
    if (!sm || !save_name) return false;

    char filepath[CARBON_SAVE_MAX_PATH];
    build_save_path(sm, save_name, filepath, sizeof(filepath));

    struct stat st;
    return stat(filepath, &st) == 0;
}

// Writer API implementation

void carbon_save_write_section(Carbon_SaveWriter *w, const char *section_name) {
    if (!w || !w->fp || !section_name) return;

    fprintf(w->fp, "\n[%s]\n", section_name);
    strncpy(w->current_section, section_name, sizeof(w->current_section) - 1);
    w->in_section = true;
}

void carbon_save_write_int(Carbon_SaveWriter *w, const char *key, int value) {
    if (!w || !w->fp || !key) return;
    fprintf(w->fp, "%s = %d\n", key, value);
}

void carbon_save_write_int64(Carbon_SaveWriter *w, const char *key, long long value) {
    if (!w || !w->fp || !key) return;
    fprintf(w->fp, "%s = %lld\n", key, value);
}

void carbon_save_write_float(Carbon_SaveWriter *w, const char *key, float value) {
    if (!w || !w->fp || !key) return;
    fprintf(w->fp, "%s = %f\n", key, value);
}

void carbon_save_write_double(Carbon_SaveWriter *w, const char *key, double value) {
    if (!w || !w->fp || !key) return;
    fprintf(w->fp, "%s = %f\n", key, value);
}

void carbon_save_write_bool(Carbon_SaveWriter *w, const char *key, bool value) {
    if (!w || !w->fp || !key) return;
    fprintf(w->fp, "%s = %s\n", key, value ? "true" : "false");
}

void carbon_save_write_string(Carbon_SaveWriter *w, const char *key, const char *value) {
    if (!w || !w->fp || !key) return;

    // Escape special characters in string
    fprintf(w->fp, "%s = \"", key);
    if (value) {
        for (const char *p = value; *p; p++) {
            switch (*p) {
                case '"':  fprintf(w->fp, "\\\""); break;
                case '\\': fprintf(w->fp, "\\\\"); break;
                case '\n': fprintf(w->fp, "\\n"); break;
                case '\r': fprintf(w->fp, "\\r"); break;
                case '\t': fprintf(w->fp, "\\t"); break;
                default:   fputc(*p, w->fp); break;
            }
        }
    }
    fprintf(w->fp, "\"\n");
}

void carbon_save_write_int_array(Carbon_SaveWriter *w, const char *key,
                                  const int *values, int count) {
    if (!w || !w->fp || !key || !values || count <= 0) return;

    fprintf(w->fp, "%s = [", key);
    for (int i = 0; i < count; i++) {
        if (i > 0) fprintf(w->fp, ", ");
        fprintf(w->fp, "%d", values[i]);
    }
    fprintf(w->fp, "]\n");
}

void carbon_save_write_float_array(Carbon_SaveWriter *w, const char *key,
                                    const float *values, int count) {
    if (!w || !w->fp || !key || !values || count <= 0) return;

    fprintf(w->fp, "%s = [", key);
    for (int i = 0; i < count; i++) {
        if (i > 0) fprintf(w->fp, ", ");
        fprintf(w->fp, "%f", values[i]);
    }
    fprintf(w->fp, "]\n");
}

// Reader API implementation

bool carbon_save_read_int(Carbon_SaveReader *r, const char *key, int *out_value) {
    if (!r || !r->game_state || !key || !out_value) return false;

    toml_datum_t d = toml_int_in(r->game_state, key);
    if (!d.ok) return false;

    *out_value = (int)d.u.i;
    return true;
}

bool carbon_save_read_int64(Carbon_SaveReader *r, const char *key, long long *out_value) {
    if (!r || !r->game_state || !key || !out_value) return false;

    toml_datum_t d = toml_int_in(r->game_state, key);
    if (!d.ok) return false;

    *out_value = d.u.i;
    return true;
}

bool carbon_save_read_float(Carbon_SaveReader *r, const char *key, float *out_value) {
    if (!r || !r->game_state || !key || !out_value) return false;

    toml_datum_t d = toml_double_in(r->game_state, key);
    if (!d.ok) return false;

    *out_value = (float)d.u.d;
    return true;
}

bool carbon_save_read_double(Carbon_SaveReader *r, const char *key, double *out_value) {
    if (!r || !r->game_state || !key || !out_value) return false;

    toml_datum_t d = toml_double_in(r->game_state, key);
    if (!d.ok) return false;

    *out_value = d.u.d;
    return true;
}

bool carbon_save_read_bool(Carbon_SaveReader *r, const char *key, bool *out_value) {
    if (!r || !r->game_state || !key || !out_value) return false;

    toml_datum_t d = toml_bool_in(r->game_state, key);
    if (!d.ok) return false;

    *out_value = d.u.b != 0;
    return true;
}

bool carbon_save_read_string(Carbon_SaveReader *r, const char *key,
                              char *out_buf, size_t buf_size) {
    if (!r || !r->game_state || !key || !out_buf || buf_size == 0) return false;

    toml_datum_t d = toml_string_in(r->game_state, key);
    if (!d.ok) return false;

    strncpy(out_buf, d.u.s, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    free(d.u.s);
    return true;
}

bool carbon_save_read_int_array(Carbon_SaveReader *r, const char *key,
                                 int **out_array, int *out_count) {
    if (!r || !r->game_state || !key || !out_array || !out_count) return false;

    toml_array_t *arr = toml_array_in(r->game_state, key);
    if (!arr) return false;

    int count = toml_array_nelem(arr);
    if (count == 0) {
        *out_array = NULL;
        *out_count = 0;
        return true;
    }

    int *result = malloc(sizeof(int) * count);
    if (!result) return false;

    for (int i = 0; i < count; i++) {
        toml_datum_t d = toml_int_at(arr, i);
        result[i] = d.ok ? (int)d.u.i : 0;
    }

    *out_array = result;
    *out_count = count;
    return true;
}

bool carbon_save_read_float_array(Carbon_SaveReader *r, const char *key,
                                   float **out_array, int *out_count) {
    if (!r || !r->game_state || !key || !out_array || !out_count) return false;

    toml_array_t *arr = toml_array_in(r->game_state, key);
    if (!arr) return false;

    int count = toml_array_nelem(arr);
    if (count == 0) {
        *out_array = NULL;
        *out_count = 0;
        return true;
    }

    float *result = malloc(sizeof(float) * count);
    if (!result) return false;

    for (int i = 0; i < count; i++) {
        toml_datum_t d = toml_double_at(arr, i);
        result[i] = d.ok ? (float)d.u.d : 0.0f;
    }

    *out_array = result;
    *out_count = count;
    return true;
}

toml_table_t *carbon_save_read_section(Carbon_SaveReader *r, const char *section_name) {
    if (!r || !r->root || !section_name) return NULL;
    return toml_table_in(r->root, section_name);
}
