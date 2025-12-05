#include "loader.h"
#include "carbon/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*============================================================================
 * Simple JSON Parser
 *============================================================================*/

typedef struct {
    const char *str;
    int pos;
    int len;
} JsonParser;

static void skip_whitespace(JsonParser *p) {
    while (p->pos < p->len && isspace((unsigned char)p->str[p->pos])) {
        p->pos++;
    }
}

static char peek(JsonParser *p) {
    skip_whitespace(p);
    return p->pos < p->len ? p->str[p->pos] : '\0';
}

static char next(JsonParser *p) {
    skip_whitespace(p);
    return p->pos < p->len ? p->str[p->pos++] : '\0';
}

static bool parse_value(JsonParser *p, JsonValue *out);

static bool parse_string(JsonParser *p, char **out) {
    if (next(p) != '"') return false;

    int start = p->pos;
    while (p->pos < p->len && p->str[p->pos] != '"') {
        if (p->str[p->pos] == '\\') p->pos++;  /* Skip escaped char */
        p->pos++;
    }

    int len = p->pos - start;
    *out = malloc(len + 1);
    if (!*out) return false;

    /* Copy with basic escape handling */
    int j = 0;
    for (int i = start; i < p->pos; i++) {
        if (p->str[i] == '\\' && i + 1 < p->pos) {
            i++;
            switch (p->str[i]) {
                case 'n': (*out)[j++] = '\n'; break;
                case 't': (*out)[j++] = '\t'; break;
                case 'r': (*out)[j++] = '\r'; break;
                default: (*out)[j++] = p->str[i]; break;
            }
        } else {
            (*out)[j++] = p->str[i];
        }
    }
    (*out)[j] = '\0';

    p->pos++;  /* Skip closing quote */
    return true;
}

static bool parse_number(JsonParser *p, double *out) {
    skip_whitespace(p);
    int start = p->pos;

    if (p->str[p->pos] == '-') p->pos++;

    while (p->pos < p->len && (isdigit((unsigned char)p->str[p->pos]) ||
                               p->str[p->pos] == '.' ||
                               p->str[p->pos] == 'e' ||
                               p->str[p->pos] == 'E' ||
                               p->str[p->pos] == '+' ||
                               p->str[p->pos] == '-')) {
        p->pos++;
    }

    char buf[64];
    int len = p->pos - start;
    if (len >= 64) len = 63;
    memcpy(buf, &p->str[start], len);
    buf[len] = '\0';

    *out = atof(buf);
    return true;
}

static bool parse_array(JsonParser *p, JsonArray **out) {
    if (next(p) != '[') return false;

    *out = calloc(1, sizeof(JsonArray));
    if (!*out) return false;

    (*out)->capacity = 8;
    (*out)->items = malloc(sizeof(JsonValue) * (*out)->capacity);
    (*out)->count = 0;

    if (peek(p) == ']') {
        p->pos++;
        return true;
    }

    while (1) {
        if ((*out)->count >= (*out)->capacity) {
            (*out)->capacity *= 2;
            (*out)->items = realloc((*out)->items, sizeof(JsonValue) * (*out)->capacity);
        }

        if (!parse_value(p, &(*out)->items[(*out)->count])) return false;
        (*out)->count++;

        char c = peek(p);
        if (c == ']') {
            p->pos++;
            break;
        }
        if (c != ',') return false;
        p->pos++;
    }

    return true;
}

static bool parse_object(JsonParser *p, JsonObject **out) {
    if (next(p) != '{') return false;

    *out = calloc(1, sizeof(JsonObject));
    if (!*out) return false;

    (*out)->capacity = 8;
    (*out)->keys = malloc(sizeof(char*) * (*out)->capacity);
    (*out)->values = malloc(sizeof(JsonValue) * (*out)->capacity);
    (*out)->count = 0;

    if (peek(p) == '}') {
        p->pos++;
        return true;
    }

    while (1) {
        if ((*out)->count >= (*out)->capacity) {
            (*out)->capacity *= 2;
            (*out)->keys = realloc((*out)->keys, sizeof(char*) * (*out)->capacity);
            (*out)->values = realloc((*out)->values, sizeof(JsonValue) * (*out)->capacity);
        }

        if (!parse_string(p, &(*out)->keys[(*out)->count])) return false;
        if (next(p) != ':') return false;
        if (!parse_value(p, &(*out)->values[(*out)->count])) return false;
        (*out)->count++;

        char c = peek(p);
        if (c == '}') {
            p->pos++;
            break;
        }
        if (c != ',') return false;
        p->pos++;
    }

    return true;
}

static bool parse_value(JsonParser *p, JsonValue *out) {
    memset(out, 0, sizeof(JsonValue));

    char c = peek(p);

    if (c == '"') {
        out->type = JSON_STRING;
        return parse_string(p, &out->string_val);
    }
    if (c == '[') {
        out->type = JSON_ARRAY;
        return parse_array(p, &out->array_val);
    }
    if (c == '{') {
        out->type = JSON_OBJECT;
        return parse_object(p, &out->object_val);
    }
    if (c == 't' && strncmp(&p->str[p->pos], "true", 4) == 0) {
        out->type = JSON_BOOL;
        out->bool_val = true;
        p->pos += 4;
        return true;
    }
    if (c == 'f' && strncmp(&p->str[p->pos], "false", 5) == 0) {
        out->type = JSON_BOOL;
        out->bool_val = false;
        p->pos += 5;
        return true;
    }
    if (c == 'n' && strncmp(&p->str[p->pos], "null", 4) == 0) {
        out->type = JSON_NULL;
        p->pos += 4;
        return true;
    }
    if (c == '-' || isdigit((unsigned char)c)) {
        out->type = JSON_NUMBER;
        return parse_number(p, &out->number_val);
    }

    return false;
}

bool json_parse(const char *json, JsonValue *out_value) {
    if (!json || !out_value) return false;

    JsonParser p = { .str = json, .pos = 0, .len = (int)strlen(json) };
    return parse_value(&p, out_value);
}

bool json_parse_file(const char *path, JsonValue *out_value) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        carbon_set_error("Failed to open file: %s", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        carbon_set_error("Failed to allocate buffer for file: %s", path);
        return false;
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    bool result = json_parse(buffer, out_value);
    free(buffer);

    if (!result) {
        carbon_set_error("Failed to parse JSON file: %s", path);
    }

    return result;
}

void json_free(JsonValue *value) {
    if (!value) return;

    switch (value->type) {
        case JSON_STRING:
            free(value->string_val);
            break;
        case JSON_ARRAY:
            if (value->array_val) {
                for (int i = 0; i < value->array_val->count; i++) {
                    json_free(&value->array_val->items[i]);
                }
                free(value->array_val->items);
                free(value->array_val);
            }
            break;
        case JSON_OBJECT:
            if (value->object_val) {
                for (int i = 0; i < value->object_val->count; i++) {
                    free(value->object_val->keys[i]);
                    json_free(&value->object_val->values[i]);
                }
                free(value->object_val->keys);
                free(value->object_val->values);
                free(value->object_val);
            }
            break;
        default:
            break;
    }

    memset(value, 0, sizeof(JsonValue));
}

JsonValue *json_object_get(JsonValue *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT || !obj->object_val || !key) return NULL;

    for (int i = 0; i < obj->object_val->count; i++) {
        if (strcmp(obj->object_val->keys[i], key) == 0) {
            return &obj->object_val->values[i];
        }
    }
    return NULL;
}

JsonValue *json_array_get(JsonValue *arr, int index) {
    if (!arr || arr->type != JSON_ARRAY || !arr->array_val) return NULL;
    if (index < 0 || index >= arr->array_val->count) return NULL;
    return &arr->array_val->items[index];
}

int json_array_length(JsonValue *arr) {
    if (!arr || arr->type != JSON_ARRAY || !arr->array_val) return 0;
    return arr->array_val->count;
}

const char *json_get_string(JsonValue *val, const char *default_val) {
    if (!val || val->type != JSON_STRING) return default_val;
    return val->string_val;
}

double json_get_number(JsonValue *val, double default_val) {
    if (!val || val->type != JSON_NUMBER) return default_val;
    return val->number_val;
}

int json_get_int(JsonValue *val, int default_val) {
    if (!val || val->type != JSON_NUMBER) return default_val;
    return (int)val->number_val;
}

bool json_get_bool(JsonValue *val, bool default_val) {
    if (!val || val->type != JSON_BOOL) return default_val;
    return val->bool_val;
}

/*============================================================================
 * Game Data Loading
 *============================================================================*/

bool game_load_level(const char *path, LevelData *out_level) {
    if (!path || !out_level) return false;

    memset(out_level, 0, sizeof(LevelData));

    JsonValue root;
    if (!json_parse_file(path, &root)) {
        return false;
    }

    /* Parse level dimensions */
    JsonValue *width = json_object_get(&root, "width");
    JsonValue *height = json_object_get(&root, "height");
    out_level->width = json_get_int(width, 0);
    out_level->height = json_get_int(height, 0);

    /* Parse tiles array */
    JsonValue *tiles = json_object_get(&root, "tiles");
    if (tiles && tiles->type == JSON_ARRAY) {
        int count = json_array_length(tiles);
        out_level->tiles = malloc(sizeof(int) * count);
        for (int i = 0; i < count; i++) {
            out_level->tiles[i] = json_get_int(json_array_get(tiles, i), 0);
        }
    }

    /* Parse spawn points */
    JsonValue *spawns = json_object_get(&root, "spawns");
    if (spawns && spawns->type == JSON_ARRAY) {
        out_level->spawn_count = json_array_length(spawns);
        out_level->spawns = malloc(sizeof(EntitySpawnData) * out_level->spawn_count);

        for (int i = 0; i < out_level->spawn_count; i++) {
            JsonValue *spawn = json_array_get(spawns, i);
            out_level->spawns[i].type = json_get_string(json_object_get(spawn, "type"), "unknown");
            out_level->spawns[i].x = (float)json_get_number(json_object_get(spawn, "x"), 0);
            out_level->spawns[i].y = (float)json_get_number(json_object_get(spawn, "y"), 0);
        }
    }

    json_free(&root);
    return true;
}

void game_free_level(LevelData *level) {
    if (!level) return;
    free(level->tiles);
    free(level->spawns);
    memset(level, 0, sizeof(LevelData));
}
