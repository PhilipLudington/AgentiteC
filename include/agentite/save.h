#ifndef AGENTITE_SAVE_H
#define AGENTITE_SAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define AGENTITE_SAVE_MAX_PATH 512
#define AGENTITE_SAVE_MAX_NAME 128

// Forward declaration
typedef struct toml_table_t toml_table_t;

// Save file info (for save list UI)
typedef struct Agentite_SaveInfo {
    char filename[256];
    char display_name[128];
    char timestamp[32];     // ISO 8601 format
    int version;
    bool is_compatible;

    // Game can add preview data via metadata
    int preview_turn;
    float preview_values[4]; // Game-defined preview metrics
} Agentite_SaveInfo;

// Result of save/load operation
typedef struct Agentite_SaveResult {
    bool success;
    char filepath[AGENTITE_SAVE_MAX_PATH];
    char error_message[256];
    int save_version;
    bool was_migrated;
} Agentite_SaveResult;

// Writer for saving game state
// Provides methods to write key-value pairs in TOML format
typedef struct Agentite_SaveWriter Agentite_SaveWriter;

// Reader for loading game state
// Wraps a toml_table_t for convenient access
typedef struct Agentite_SaveReader {
    toml_table_t *root;
    toml_table_t *game_state;
} Agentite_SaveReader;

// Callbacks for game-specific serialization
// Writer provides methods to write data
typedef bool (*Agentite_SerializeFunc)(void *game_state, Agentite_SaveWriter *writer);
// Reader wraps TOML table for reading
typedef bool (*Agentite_DeserializeFunc)(void *game_state, Agentite_SaveReader *reader);

// Save manager
typedef struct Agentite_SaveManager Agentite_SaveManager;

// Create save manager with saves directory path
// If saves_dir is NULL, uses "./saves"
Agentite_SaveManager *agentite_save_create(const char *saves_dir);
void agentite_save_destroy(Agentite_SaveManager *sm);

// Set game version for compatibility checking
void agentite_save_set_version(Agentite_SaveManager *sm, int version, int min_compatible);

// Save game with custom name
Agentite_SaveResult agentite_save_game(Agentite_SaveManager *sm,
                                    const char *save_name,
                                    Agentite_SerializeFunc serialize,
                                    void *game_state);

// Load game by name
Agentite_SaveResult agentite_load_game(Agentite_SaveManager *sm,
                                    const char *save_name,
                                    Agentite_DeserializeFunc deserialize,
                                    void *game_state);

// Quick save/load (uses "quicksave" as name)
Agentite_SaveResult agentite_save_quick(Agentite_SaveManager *sm,
                                     Agentite_SerializeFunc serialize,
                                     void *game_state);
Agentite_SaveResult agentite_load_quick(Agentite_SaveManager *sm,
                                     Agentite_DeserializeFunc deserialize,
                                     void *game_state);

// Autosave (uses "autosave" as name)
Agentite_SaveResult agentite_save_auto(Agentite_SaveManager *sm,
                                    Agentite_SerializeFunc serialize,
                                    void *game_state);

// List all saves for load screen
// Returns array of save info, caller must free with agentite_save_list_free
Agentite_SaveInfo *agentite_save_list(const Agentite_SaveManager *sm, int *out_count);
void agentite_save_list_free(Agentite_SaveInfo *list);

// Delete a save
bool agentite_save_delete(Agentite_SaveManager *sm, const char *save_name);

// Check if save exists
bool agentite_save_exists(const Agentite_SaveManager *sm, const char *save_name);

// Writer API for serializing game state
void agentite_save_write_section(Agentite_SaveWriter *w, const char *section_name);
void agentite_save_write_int(Agentite_SaveWriter *w, const char *key, int value);
void agentite_save_write_int64(Agentite_SaveWriter *w, const char *key, long long value);
void agentite_save_write_float(Agentite_SaveWriter *w, const char *key, float value);
void agentite_save_write_double(Agentite_SaveWriter *w, const char *key, double value);
void agentite_save_write_bool(Agentite_SaveWriter *w, const char *key, bool value);
void agentite_save_write_string(Agentite_SaveWriter *w, const char *key, const char *value);
void agentite_save_write_int_array(Agentite_SaveWriter *w, const char *key,
                                  const int *values, int count);
void agentite_save_write_float_array(Agentite_SaveWriter *w, const char *key,
                                    const float *values, int count);

// Reader API for loading game state
// These use the game_state section by default
bool agentite_save_read_int(Agentite_SaveReader *r, const char *key, int *out_value);
bool agentite_save_read_int64(Agentite_SaveReader *r, const char *key, long long *out_value);
bool agentite_save_read_float(Agentite_SaveReader *r, const char *key, float *out_value);
bool agentite_save_read_double(Agentite_SaveReader *r, const char *key, double *out_value);
bool agentite_save_read_bool(Agentite_SaveReader *r, const char *key, bool *out_value);
bool agentite_save_read_string(Agentite_SaveReader *r, const char *key,
                              char *out_buf, size_t buf_size);
bool agentite_save_read_int_array(Agentite_SaveReader *r, const char *key,
                                 int **out_array, int *out_count);
bool agentite_save_read_float_array(Agentite_SaveReader *r, const char *key,
                                   float **out_array, int *out_count);

// Access specific section in reader
toml_table_t *agentite_save_read_section(Agentite_SaveReader *r, const char *section_name);

#endif // AGENTITE_SAVE_H
