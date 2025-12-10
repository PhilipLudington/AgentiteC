#ifndef GAME_DATA_LOADER_H
#define GAME_DATA_LOADER_H

#include "agentite/game_context.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * Game Data Loader
 *
 * Provides simple JSON parsing and data loading utilities for game content.
 * Uses a minimal JSON parser suitable for game data files.
 *
 * Supported data types:
 * - Entity definitions (spawn templates)
 * - Level data (tile layouts, spawn points)
 * - Animation definitions
 */

/*============================================================================
 * JSON Value Types
 *============================================================================*/

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonValueType;

typedef struct JsonValue JsonValue;
typedef struct JsonObject JsonObject;
typedef struct JsonArray JsonArray;

struct JsonValue {
    JsonValueType type;
    union {
        bool bool_val;
        double number_val;
        char *string_val;
        JsonArray *array_val;
        JsonObject *object_val;
    };
};

struct JsonArray {
    JsonValue *items;
    int count;
    int capacity;
};

struct JsonObject {
    char **keys;
    JsonValue *values;
    int count;
    int capacity;
};

/*============================================================================
 * JSON Parsing
 *============================================================================*/

/**
 * Parse JSON from a string.
 *
 * @param json JSON string to parse
 * @param out_value Output parsed value
 * @return true on success, false on parse error
 */
bool json_parse(const char *json, JsonValue *out_value);

/**
 * Parse JSON from a file.
 *
 * @param path File path
 * @param out_value Output parsed value
 * @return true on success, false on error
 */
bool json_parse_file(const char *path, JsonValue *out_value);

/**
 * Free a JSON value and all nested values.
 *
 * @param value Value to free
 */
void json_free(JsonValue *value);

/*============================================================================
 * JSON Value Access
 *============================================================================*/

/**
 * Get object field by key.
 *
 * @param obj Object value
 * @param key Field name
 * @return Field value, or NULL if not found
 */
JsonValue *json_object_get(JsonValue *obj, const char *key);

/**
 * Get array item by index.
 *
 * @param arr Array value
 * @param index Array index
 * @return Item value, or NULL if out of bounds
 */
JsonValue *json_array_get(JsonValue *arr, int index);

/**
 * Get array length.
 *
 * @param arr Array value
 * @return Number of items, or 0 if not an array
 */
int json_array_length(JsonValue *arr);

/**
 * Get string value with default.
 *
 * @param val JSON value
 * @param default_val Default if not a string
 * @return String value or default
 */
const char *json_get_string(JsonValue *val, const char *default_val);

/**
 * Get number value with default.
 *
 * @param val JSON value
 * @param default_val Default if not a number
 * @return Number value or default
 */
double json_get_number(JsonValue *val, double default_val);

/**
 * Get integer value with default.
 *
 * @param val JSON value
 * @param default_val Default if not a number
 * @return Integer value or default
 */
int json_get_int(JsonValue *val, int default_val);

/**
 * Get boolean value with default.
 *
 * @param val JSON value
 * @param default_val Default if not a boolean
 * @return Boolean value or default
 */
bool json_get_bool(JsonValue *val, bool default_val);

/*============================================================================
 * Game Data Loading
 *============================================================================*/

/**
 * Entity spawn data loaded from JSON.
 */
typedef struct {
    const char *type;           /* Entity type name */
    float x, y;                 /* Spawn position */
    /* Add more fields as needed */
} EntitySpawnData;

/**
 * Level data loaded from JSON.
 */
typedef struct {
    int width, height;          /* Tilemap dimensions */
    int *tiles;                 /* Tile IDs (width * height) */
    EntitySpawnData *spawns;    /* Entity spawn points */
    int spawn_count;
} LevelData;

/**
 * Load level data from a JSON file.
 *
 * @param path Path to level JSON file
 * @param out_level Output level data
 * @return true on success
 */
bool game_load_level(const char *path, LevelData *out_level);

/**
 * Free level data.
 *
 * @param level Level data to free
 */
void game_free_level(LevelData *level);

#endif /* GAME_DATA_LOADER_H */
