#ifndef CARBON_SAVE_H
#define CARBON_SAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define CARBON_SAVE_MAX_PATH 512
#define CARBON_SAVE_MAX_NAME 128

// Forward declaration
typedef struct toml_table_t toml_table_t;

// Save file info (for save list UI)
typedef struct Carbon_SaveInfo {
    char filename[256];
    char display_name[128];
    char timestamp[32];     // ISO 8601 format
    int version;
    bool is_compatible;

    // Game can add preview data via metadata
    int preview_turn;
    float preview_values[4]; // Game-defined preview metrics
} Carbon_SaveInfo;

// Result of save/load operation
typedef struct Carbon_SaveResult {
    bool success;
    char filepath[CARBON_SAVE_MAX_PATH];
    char error_message[256];
    int save_version;
    bool was_migrated;
} Carbon_SaveResult;

// Writer for saving game state
// Provides methods to write key-value pairs in TOML format
typedef struct Carbon_SaveWriter Carbon_SaveWriter;

// Reader for loading game state
// Wraps a toml_table_t for convenient access
typedef struct Carbon_SaveReader {
    toml_table_t *root;
    toml_table_t *game_state;
} Carbon_SaveReader;

// Callbacks for game-specific serialization
// Writer provides methods to write data
typedef bool (*Carbon_SerializeFunc)(void *game_state, Carbon_SaveWriter *writer);
// Reader wraps TOML table for reading
typedef bool (*Carbon_DeserializeFunc)(void *game_state, Carbon_SaveReader *reader);

// Save manager
typedef struct Carbon_SaveManager Carbon_SaveManager;

// Create save manager with saves directory path
// If saves_dir is NULL, uses "./saves"
Carbon_SaveManager *carbon_save_create(const char *saves_dir);
void carbon_save_destroy(Carbon_SaveManager *sm);

// Set game version for compatibility checking
void carbon_save_set_version(Carbon_SaveManager *sm, int version, int min_compatible);

// Save game with custom name
Carbon_SaveResult carbon_save_game(Carbon_SaveManager *sm,
                                    const char *save_name,
                                    Carbon_SerializeFunc serialize,
                                    void *game_state);

// Load game by name
Carbon_SaveResult carbon_load_game(Carbon_SaveManager *sm,
                                    const char *save_name,
                                    Carbon_DeserializeFunc deserialize,
                                    void *game_state);

// Quick save/load (uses "quicksave" as name)
Carbon_SaveResult carbon_save_quick(Carbon_SaveManager *sm,
                                     Carbon_SerializeFunc serialize,
                                     void *game_state);
Carbon_SaveResult carbon_load_quick(Carbon_SaveManager *sm,
                                     Carbon_DeserializeFunc deserialize,
                                     void *game_state);

// Autosave (uses "autosave" as name)
Carbon_SaveResult carbon_save_auto(Carbon_SaveManager *sm,
                                    Carbon_SerializeFunc serialize,
                                    void *game_state);

// List all saves for load screen
// Returns array of save info, caller must free with carbon_save_list_free
Carbon_SaveInfo *carbon_save_list(const Carbon_SaveManager *sm, int *out_count);
void carbon_save_list_free(Carbon_SaveInfo *list);

// Delete a save
bool carbon_save_delete(Carbon_SaveManager *sm, const char *save_name);

// Check if save exists
bool carbon_save_exists(const Carbon_SaveManager *sm, const char *save_name);

// Writer API for serializing game state
void carbon_save_write_section(Carbon_SaveWriter *w, const char *section_name);
void carbon_save_write_int(Carbon_SaveWriter *w, const char *key, int value);
void carbon_save_write_int64(Carbon_SaveWriter *w, const char *key, long long value);
void carbon_save_write_float(Carbon_SaveWriter *w, const char *key, float value);
void carbon_save_write_double(Carbon_SaveWriter *w, const char *key, double value);
void carbon_save_write_bool(Carbon_SaveWriter *w, const char *key, bool value);
void carbon_save_write_string(Carbon_SaveWriter *w, const char *key, const char *value);
void carbon_save_write_int_array(Carbon_SaveWriter *w, const char *key,
                                  const int *values, int count);
void carbon_save_write_float_array(Carbon_SaveWriter *w, const char *key,
                                    const float *values, int count);

// Reader API for loading game state
// These use the game_state section by default
bool carbon_save_read_int(Carbon_SaveReader *r, const char *key, int *out_value);
bool carbon_save_read_int64(Carbon_SaveReader *r, const char *key, long long *out_value);
bool carbon_save_read_float(Carbon_SaveReader *r, const char *key, float *out_value);
bool carbon_save_read_double(Carbon_SaveReader *r, const char *key, double *out_value);
bool carbon_save_read_bool(Carbon_SaveReader *r, const char *key, bool *out_value);
bool carbon_save_read_string(Carbon_SaveReader *r, const char *key,
                              char *out_buf, size_t buf_size);
bool carbon_save_read_int_array(Carbon_SaveReader *r, const char *key,
                                 int **out_array, int *out_count);
bool carbon_save_read_float_array(Carbon_SaveReader *r, const char *key,
                                   float **out_array, int *out_count);

// Access specific section in reader
toml_table_t *carbon_save_read_section(Carbon_SaveReader *r, const char *section_name);

#endif // CARBON_SAVE_H
