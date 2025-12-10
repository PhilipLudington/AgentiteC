#ifndef AGENTITE_DATA_CONFIG_H
#define AGENTITE_DATA_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

// Forward declarations for tomlc99 types
typedef struct toml_table_t toml_table_t;
typedef struct toml_array_t toml_array_t;

#define AGENTITE_DATA_MAX_ERROR 256
#define AGENTITE_DATA_MAX_ENTRIES 256

// Generic definition with string ID and name
typedef struct Agentite_DataEntry {
    char id[64];
    char name[128];
    void *data;           // Points to type-specific struct
} Agentite_DataEntry;

// Parse callback: game defines how to parse each entry from TOML
// key: the entry's key in the array (may be empty for [[array]] entries)
// table: the TOML table containing this entry's data
// out_entry: pointer to game's struct to fill (already allocated, entry_size bytes)
// userdata: passed through from agentite_data_load
// Return true on success, false to skip this entry
typedef bool (*Agentite_DataParseFunc)(const char *key, toml_table_t *table,
                                      void *out_entry, void *userdata);

// Data loader manages multiple data types
typedef struct Agentite_DataLoader Agentite_DataLoader;

// Create/destroy
Agentite_DataLoader *agentite_data_create(void);
void agentite_data_destroy(Agentite_DataLoader *loader);

// Load data from TOML file with custom parser callback
// path: path to .toml file
// array_key: name of array in TOML file (e.g., "policy", "event") - NULL for root tables
// entry_size: sizeof(YourDataStruct)
// parse_func: callback to parse each entry
// userdata: passed to parse_func
bool agentite_data_load(Agentite_DataLoader *loader, const char *path,
                      const char *array_key, size_t entry_size,
                      Agentite_DataParseFunc parse_func, void *userdata);

// Load from TOML string instead of file
bool agentite_data_load_string(Agentite_DataLoader *loader, const char *toml_string,
                             const char *array_key, size_t entry_size,
                             Agentite_DataParseFunc parse_func, void *userdata);

// Access data
size_t agentite_data_count(const Agentite_DataLoader *loader);
void *agentite_data_get_by_index(const Agentite_DataLoader *loader, size_t index);
void *agentite_data_find(const Agentite_DataLoader *loader, const char *id);  // O(1) hash lookup
const char *agentite_data_get_last_error(const Agentite_DataLoader *loader);

// Clear all loaded data
void agentite_data_clear(Agentite_DataLoader *loader);

// Helper functions for parsing TOML values
// These handle the toml_datum_t pattern and free strings properly

// Get string value (copies into buffer, handles missing keys)
bool agentite_toml_get_string(toml_table_t *table, const char *key,
                            char *out_buf, size_t buf_size);

// Get integer value
bool agentite_toml_get_int(toml_table_t *table, const char *key, int *out_value);
bool agentite_toml_get_int64(toml_table_t *table, const char *key, long long *out_value);

// Get floating point value
bool agentite_toml_get_float(toml_table_t *table, const char *key, float *out_value);
bool agentite_toml_get_double(toml_table_t *table, const char *key, double *out_value);

// Get boolean value
bool agentite_toml_get_bool(toml_table_t *table, const char *key, bool *out_value);

// Get array of strings (allocates array, caller must free with agentite_toml_free_strings)
bool agentite_toml_get_string_array(toml_table_t *table, const char *key,
                                   char ***out_array, int *out_count);
void agentite_toml_free_strings(char **array, int count);

// Get array of integers
bool agentite_toml_get_int_array(toml_table_t *table, const char *key,
                                int **out_array, int *out_count);

// Get array of floats
bool agentite_toml_get_float_array(toml_table_t *table, const char *key,
                                  float **out_array, int *out_count);

// Check if key exists
bool agentite_toml_has_key(toml_table_t *table, const char *key);

// Get nested table
toml_table_t *agentite_toml_get_table(toml_table_t *table, const char *key);

// Get array
toml_array_t *agentite_toml_get_array(toml_table_t *table, const char *key);

#endif // AGENTITE_DATA_CONFIG_H
