#include "carbon/data_config.h"
#include "toml.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Simple hash function (djb2)
static unsigned long hash_string(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Hash table entry for O(1) lookup
typedef struct HashEntry {
    char id[64];
    size_t data_index;
    struct HashEntry *next;
} HashEntry;

#define HASH_TABLE_SIZE 256

struct Carbon_DataLoader {
    // Data storage
    void *data;             // Contiguous array of entries
    size_t entry_size;      // Size of each entry
    size_t count;           // Number of entries
    size_t capacity;        // Allocated capacity

    // Hash table for O(1) lookup by ID
    HashEntry *hash_table[HASH_TABLE_SIZE];

    // Error message
    char error[CARBON_DATA_MAX_ERROR];
};

Carbon_DataLoader *carbon_data_create(void) {
    Carbon_DataLoader *loader = calloc(1, sizeof(Carbon_DataLoader));
    return loader;
}

static void free_hash_table(Carbon_DataLoader *loader) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry *entry = loader->hash_table[i];
        while (entry) {
            HashEntry *next = entry->next;
            free(entry);
            entry = next;
        }
        loader->hash_table[i] = NULL;
    }
}

void carbon_data_destroy(Carbon_DataLoader *loader) {
    if (!loader) return;

    free_hash_table(loader);
    free(loader->data);
    free(loader);
}

void carbon_data_clear(Carbon_DataLoader *loader) {
    if (!loader) return;

    free_hash_table(loader);
    free(loader->data);
    loader->data = NULL;
    loader->count = 0;
    loader->capacity = 0;
    loader->entry_size = 0;
    loader->error[0] = '\0';
}

static bool add_to_hash_table(Carbon_DataLoader *loader, const char *id, size_t data_index) {
    if (!id || !id[0]) return false;

    unsigned long hash = hash_string(id) % HASH_TABLE_SIZE;

    HashEntry *entry = malloc(sizeof(HashEntry));
    if (!entry) return false;

    strncpy(entry->id, id, sizeof(entry->id) - 1);
    entry->id[sizeof(entry->id) - 1] = '\0';
    entry->data_index = data_index;
    entry->next = loader->hash_table[hash];
    loader->hash_table[hash] = entry;

    return true;
}

static bool ensure_capacity(Carbon_DataLoader *loader, size_t needed) {
    if (loader->capacity >= needed) return true;

    size_t new_capacity = loader->capacity == 0 ? 16 : loader->capacity * 2;
    while (new_capacity < needed) new_capacity *= 2;

    void *new_data = realloc(loader->data, new_capacity * loader->entry_size);
    if (!new_data) return false;

    loader->data = new_data;
    loader->capacity = new_capacity;
    return true;
}

static bool load_from_table(Carbon_DataLoader *loader, toml_table_t *root,
                            const char *array_key, size_t entry_size,
                            Carbon_DataParseFunc parse_func, void *userdata) {
    loader->entry_size = entry_size;

    toml_array_t *array = NULL;

    if (array_key && array_key[0]) {
        // Look for [[array_key]] array of tables
        array = toml_array_in(root, array_key);
        if (!array) {
            snprintf(loader->error, sizeof(loader->error),
                     "Array '%s' not found in TOML", array_key);
            return false;
        }
    }

    if (array) {
        // Parse array of tables
        int count = toml_array_nelem(array);

        for (int i = 0; i < count; i++) {
            toml_table_t *table = toml_table_at(array, i);
            if (!table) continue;

            if (!ensure_capacity(loader, loader->count + 1)) {
                snprintf(loader->error, sizeof(loader->error), "Out of memory");
                return false;
            }

            void *entry = (char *)loader->data + (loader->count * entry_size);
            memset(entry, 0, entry_size);

            if (parse_func("", table, entry, userdata)) {
                // Try to extract ID for hash table (assume first field is id)
                const char *id = (const char *)entry;
                add_to_hash_table(loader, id, loader->count);
                loader->count++;
            }
        }
    } else {
        // Parse root-level [tables]
        int i = 0;
        const char *key;

        while ((key = toml_key_in(root, i++)) != NULL) {
            toml_table_t *table = toml_table_in(root, key);
            if (!table) continue;

            if (!ensure_capacity(loader, loader->count + 1)) {
                snprintf(loader->error, sizeof(loader->error), "Out of memory");
                return false;
            }

            void *entry = (char *)loader->data + (loader->count * entry_size);
            memset(entry, 0, entry_size);

            if (parse_func(key, table, entry, userdata)) {
                // Try to extract ID for hash table
                const char *id = (const char *)entry;
                add_to_hash_table(loader, id, loader->count);
                loader->count++;
            }
        }
    }

    return true;
}

bool carbon_data_load(Carbon_DataLoader *loader, const char *path,
                      const char *array_key, size_t entry_size,
                      Carbon_DataParseFunc parse_func, void *userdata) {
    if (!loader || !path || !parse_func || entry_size == 0) {
        if (loader) {
            snprintf(loader->error, sizeof(loader->error), "Invalid parameters");
        }
        return false;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        snprintf(loader->error, sizeof(loader->error),
                 "Cannot open file: %s", path);
        return false;
    }

    char errbuf[256];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root) {
        snprintf(loader->error, sizeof(loader->error),
                 "TOML parse error: %s", errbuf);
        return false;
    }

    bool success = load_from_table(loader, root, array_key, entry_size,
                                   parse_func, userdata);
    toml_free(root);
    return success;
}

bool carbon_data_load_string(Carbon_DataLoader *loader, const char *toml_string,
                             const char *array_key, size_t entry_size,
                             Carbon_DataParseFunc parse_func, void *userdata) {
    if (!loader || !toml_string || !parse_func || entry_size == 0) {
        if (loader) {
            snprintf(loader->error, sizeof(loader->error), "Invalid parameters");
        }
        return false;
    }

    // toml_parse needs mutable string, so make a copy
    char *copy = strdup(toml_string);
    if (!copy) {
        snprintf(loader->error, sizeof(loader->error), "Out of memory");
        return false;
    }

    char errbuf[256];
    toml_table_t *root = toml_parse(copy, errbuf, sizeof(errbuf));
    free(copy);

    if (!root) {
        snprintf(loader->error, sizeof(loader->error),
                 "TOML parse error: %s", errbuf);
        return false;
    }

    bool success = load_from_table(loader, root, array_key, entry_size,
                                   parse_func, userdata);
    toml_free(root);
    return success;
}

size_t carbon_data_count(const Carbon_DataLoader *loader) {
    if (!loader) return 0;
    return loader->count;
}

void *carbon_data_get_by_index(const Carbon_DataLoader *loader, size_t index) {
    if (!loader || index >= loader->count) return NULL;
    return (char *)loader->data + (index * loader->entry_size);
}

void *carbon_data_find(const Carbon_DataLoader *loader, const char *id) {
    if (!loader || !id) return NULL;

    unsigned long hash = hash_string(id) % HASH_TABLE_SIZE;
    HashEntry *entry = loader->hash_table[hash];

    while (entry) {
        if (strcmp(entry->id, id) == 0) {
            return carbon_data_get_by_index(loader, entry->data_index);
        }
        entry = entry->next;
    }

    return NULL;
}

const char *carbon_data_get_last_error(const Carbon_DataLoader *loader) {
    if (!loader) return "Invalid loader";
    return loader->error;
}

// Helper functions for TOML parsing

bool carbon_toml_get_string(toml_table_t *table, const char *key,
                            char *out_buf, size_t buf_size) {
    if (!table || !key || !out_buf || buf_size == 0) return false;

    toml_datum_t d = toml_string_in(table, key);
    if (!d.ok) return false;

    strncpy(out_buf, d.u.s, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    free(d.u.s);
    return true;
}

bool carbon_toml_get_int(toml_table_t *table, const char *key, int *out_value) {
    if (!table || !key || !out_value) return false;

    toml_datum_t d = toml_int_in(table, key);
    if (!d.ok) return false;

    *out_value = (int)d.u.i;
    return true;
}

bool carbon_toml_get_int64(toml_table_t *table, const char *key, long long *out_value) {
    if (!table || !key || !out_value) return false;

    toml_datum_t d = toml_int_in(table, key);
    if (!d.ok) return false;

    *out_value = d.u.i;
    return true;
}

bool carbon_toml_get_float(toml_table_t *table, const char *key, float *out_value) {
    if (!table || !key || !out_value) return false;

    toml_datum_t d = toml_double_in(table, key);
    if (!d.ok) return false;

    *out_value = (float)d.u.d;
    return true;
}

bool carbon_toml_get_double(toml_table_t *table, const char *key, double *out_value) {
    if (!table || !key || !out_value) return false;

    toml_datum_t d = toml_double_in(table, key);
    if (!d.ok) return false;

    *out_value = d.u.d;
    return true;
}

bool carbon_toml_get_bool(toml_table_t *table, const char *key, bool *out_value) {
    if (!table || !key || !out_value) return false;

    toml_datum_t d = toml_bool_in(table, key);
    if (!d.ok) return false;

    *out_value = d.u.b != 0;
    return true;
}

bool carbon_toml_get_string_array(toml_table_t *table, const char *key,
                                   char ***out_array, int *out_count) {
    if (!table || !key || !out_array || !out_count) return false;

    toml_array_t *arr = toml_array_in(table, key);
    if (!arr) return false;

    int count = toml_array_nelem(arr);
    if (count == 0) {
        *out_array = NULL;
        *out_count = 0;
        return true;
    }

    char **result = malloc(sizeof(char *) * count);
    if (!result) return false;

    for (int i = 0; i < count; i++) {
        toml_datum_t d = toml_string_at(arr, i);
        if (d.ok) {
            result[i] = d.u.s;  // Transfer ownership
        } else {
            result[i] = strdup("");
        }
    }

    *out_array = result;
    *out_count = count;
    return true;
}

void carbon_toml_free_strings(char **array, int count) {
    if (!array) return;
    for (int i = 0; i < count; i++) {
        free(array[i]);
    }
    free(array);
}

bool carbon_toml_get_int_array(toml_table_t *table, const char *key,
                                int **out_array, int *out_count) {
    if (!table || !key || !out_array || !out_count) return false;

    toml_array_t *arr = toml_array_in(table, key);
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

bool carbon_toml_get_float_array(toml_table_t *table, const char *key,
                                  float **out_array, int *out_count) {
    if (!table || !key || !out_array || !out_count) return false;

    toml_array_t *arr = toml_array_in(table, key);
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

bool carbon_toml_has_key(toml_table_t *table, const char *key) {
    if (!table || !key) return false;
    return toml_key_exists(table, key) != 0;
}

toml_table_t *carbon_toml_get_table(toml_table_t *table, const char *key) {
    if (!table || !key) return NULL;
    return toml_table_in(table, key);
}

toml_array_t *carbon_toml_get_array(toml_table_t *table, const char *key) {
    if (!table || !key) return NULL;
    return toml_array_in(table, key);
}
