/*
 * Carbon Game Engine - Biome System
 *
 * Terrain types affecting resource distribution and visuals.
 */

#include "carbon/carbon.h"
#include "carbon/biome.h"
#include "carbon/error.h"
#include "carbon/validate.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

struct Carbon_BiomeSystem {
    Carbon_BiomeDef *biomes;
    int capacity;
    int count;
};

struct Carbon_BiomeMap {
    Carbon_BiomeSystem *system;
    int8_t *data;               /* Biome ID per cell (-1 = invalid) */
    int width;
    int height;
};

/*============================================================================
 * Simple Noise Function
 *============================================================================*/

/* Simple hash-based noise */
static float hash_noise(int x, int y, uint32_t seed) {
    uint32_t n = (uint32_t)x + (uint32_t)y * 57 + seed * 131;
    n = (n << 13) ^ n;
    n = (n * (n * n * 15731 + 789221) + 1376312589);
    return (float)(n & 0x7fffffff) / (float)0x7fffffff;
}

/* Smoothed noise with interpolation */
static float smooth_noise(float x, float y, uint32_t seed) {
    int xi = (int)floorf(x);
    int yi = (int)floorf(y);
    float xf = x - xi;
    float yf = y - yi;

    float n00 = hash_noise(xi, yi, seed);
    float n10 = hash_noise(xi + 1, yi, seed);
    float n01 = hash_noise(xi, yi + 1, seed);
    float n11 = hash_noise(xi + 1, yi + 1, seed);

    /* Smooth interpolation */
    float sx = xf * xf * (3.0f - 2.0f * xf);
    float sy = yf * yf * (3.0f - 2.0f * yf);

    float n0 = n00 * (1.0f - sx) + n10 * sx;
    float n1 = n01 * (1.0f - sx) + n11 * sx;

    return n0 * (1.0f - sy) + n1 * sy;
}

/* Multi-octave noise */
static float fbm_noise(float x, float y, uint32_t seed, int octaves) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 0.05f;
    float max_value = 0.0f;

    for (int i = 0; i < octaves; i++) {
        value += smooth_noise(x * frequency, y * frequency, seed + i * 1000) * amplitude;
        max_value += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return value / max_value;
}

/*============================================================================
 * System Creation and Destruction
 *============================================================================*/

Carbon_BiomeSystem *carbon_biome_create(void) {
    Carbon_BiomeSystem *system = CARBON_ALLOC(Carbon_BiomeSystem);
    if (!system) {
        carbon_set_error("Failed to allocate biome system");
        return NULL;
    }

    system->capacity = CARBON_BIOME_MAX;
    system->biomes = CARBON_ALLOC_ARRAY(Carbon_BiomeDef, system->capacity);
    if (!system->biomes) {
        carbon_set_error("Failed to allocate biome storage");
        free(system);
        return NULL;
    }

    return system;
}

void carbon_biome_destroy(Carbon_BiomeSystem *system) {
    if (!system) return;

    free(system->biomes);
    free(system);
}

/*============================================================================
 * Biome Registration
 *============================================================================*/

int carbon_biome_register(Carbon_BiomeSystem *system, const Carbon_BiomeDef *def) {
    CARBON_VALIDATE_PTR_RET(system, CARBON_BIOME_INVALID);
    CARBON_VALIDATE_PTR_RET(def, CARBON_BIOME_INVALID);

    if (system->count >= system->capacity) {
        carbon_set_error("Biome system is full");
        return CARBON_BIOME_INVALID;
    }

    /* Check for duplicate ID */
    for (int i = 0; i < system->count; i++) {
        if (strcmp(system->biomes[i].id, def->id) == 0) {
            carbon_set_error("Biome with ID '%s' already exists", def->id);
            return CARBON_BIOME_INVALID;
        }
    }

    /* Copy biome to system */
    int index = system->count;
    memcpy(&system->biomes[index], def, sizeof(Carbon_BiomeDef));
    system->count++;

    return index;
}

int carbon_biome_count(const Carbon_BiomeSystem *system) {
    CARBON_VALIDATE_PTR_RET(system, 0);
    return system->count;
}

const Carbon_BiomeDef *carbon_biome_get(const Carbon_BiomeSystem *system, int biome_id) {
    CARBON_VALIDATE_PTR_RET(system, NULL);

    if (biome_id < 0 || biome_id >= system->count) {
        return NULL;
    }

    return &system->biomes[biome_id];
}

const Carbon_BiomeDef *carbon_biome_find(const Carbon_BiomeSystem *system, const char *id) {
    CARBON_VALIDATE_PTR_RET(system, NULL);
    CARBON_VALIDATE_STRING_RET(id, NULL);

    for (int i = 0; i < system->count; i++) {
        if (strcmp(system->biomes[i].id, id) == 0) {
            return &system->biomes[i];
        }
    }

    return NULL;
}

int carbon_biome_find_index(const Carbon_BiomeSystem *system, const char *id) {
    CARBON_VALIDATE_PTR_RET(system, CARBON_BIOME_INVALID);
    CARBON_VALIDATE_STRING_RET(id, CARBON_BIOME_INVALID);

    for (int i = 0; i < system->count; i++) {
        if (strcmp(system->biomes[i].id, id) == 0) {
            return i;
        }
    }

    return CARBON_BIOME_INVALID;
}

/*============================================================================
 * Resource Weights
 *============================================================================*/

bool carbon_biome_set_resource_weight(
    Carbon_BiomeSystem *system,
    int biome_id,
    int resource_type,
    float weight)
{
    CARBON_VALIDATE_PTR_RET(system, false);

    if (biome_id < 0 || biome_id >= system->count) {
        return false;
    }

    if (resource_type < 0 || resource_type >= CARBON_BIOME_MAX_RESOURCES) {
        return false;
    }

    system->biomes[biome_id].resource_weights[resource_type] = weight;
    return true;
}

bool carbon_biome_set_resource_weight_by_id(
    Carbon_BiomeSystem *system,
    const char *id,
    int resource_type,
    float weight)
{
    int biome_id = carbon_biome_find_index(system, id);
    if (biome_id == CARBON_BIOME_INVALID) {
        return false;
    }

    return carbon_biome_set_resource_weight(system, biome_id, resource_type, weight);
}

float carbon_biome_get_resource_weight(
    const Carbon_BiomeSystem *system,
    int biome_id,
    int resource_type)
{
    CARBON_VALIDATE_PTR_RET(system, 0.0f);

    if (biome_id < 0 || biome_id >= system->count) {
        return 0.0f;
    }

    if (resource_type < 0 || resource_type >= CARBON_BIOME_MAX_RESOURCES) {
        return 0.0f;
    }

    return system->biomes[biome_id].resource_weights[resource_type];
}

int carbon_biome_get_best_for_resource(
    const Carbon_BiomeSystem *system,
    int resource_type)
{
    CARBON_VALIDATE_PTR_RET(system, CARBON_BIOME_INVALID);

    if (resource_type < 0 || resource_type >= CARBON_BIOME_MAX_RESOURCES) {
        return CARBON_BIOME_INVALID;
    }

    int best_biome = CARBON_BIOME_INVALID;
    float best_weight = 0.0f;

    for (int i = 0; i < system->count; i++) {
        float weight = system->biomes[i].resource_weights[resource_type];
        if (weight > best_weight) {
            best_weight = weight;
            best_biome = i;
        }
    }

    return best_biome;
}

int carbon_biome_get_all_for_resource(
    const Carbon_BiomeSystem *system,
    int resource_type,
    int *out_biome_ids,
    int max_count)
{
    CARBON_VALIDATE_PTR_RET(system, 0);
    CARBON_VALIDATE_PTR_RET(out_biome_ids, 0);

    if (resource_type < 0 || resource_type >= CARBON_BIOME_MAX_RESOURCES) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < system->count && count < max_count; i++) {
        if (system->biomes[i].resource_weights[resource_type] > 0.0f) {
            out_biome_ids[count++] = i;
        }
    }

    return count;
}

/*============================================================================
 * Biome Properties
 *============================================================================*/

const char *carbon_biome_get_name(const Carbon_BiomeSystem *system, int biome_id) {
    const Carbon_BiomeDef *def = carbon_biome_get(system, biome_id);
    return def ? def->name : NULL;
}

uint32_t carbon_biome_get_color(const Carbon_BiomeSystem *system, int biome_id) {
    const Carbon_BiomeDef *def = carbon_biome_get(system, biome_id);
    return def ? def->color : 0;
}

float carbon_biome_get_movement_cost(const Carbon_BiomeSystem *system, int biome_id) {
    const Carbon_BiomeDef *def = carbon_biome_get(system, biome_id);
    return def ? def->movement_cost : 1.0f;
}

float carbon_biome_get_resource_multiplier(const Carbon_BiomeSystem *system, int biome_id) {
    const Carbon_BiomeDef *def = carbon_biome_get(system, biome_id);
    return def ? def->resource_multiplier : 1.0f;
}

float carbon_biome_get_visibility_modifier(const Carbon_BiomeSystem *system, int biome_id) {
    const Carbon_BiomeDef *def = carbon_biome_get(system, biome_id);
    return def ? def->visibility_modifier : 1.0f;
}

float carbon_biome_get_defense_bonus(const Carbon_BiomeSystem *system, int biome_id) {
    const Carbon_BiomeDef *def = carbon_biome_get(system, biome_id);
    return def ? def->defense_bonus : 0.0f;
}

/*============================================================================
 * Biome Flags
 *============================================================================*/

bool carbon_biome_has_flag(const Carbon_BiomeSystem *system, int biome_id, Carbon_BiomeFlags flag) {
    const Carbon_BiomeDef *def = carbon_biome_get(system, biome_id);
    return def ? (def->flags & flag) != 0 : false;
}

bool carbon_biome_is_passable(const Carbon_BiomeSystem *system, int biome_id) {
    return carbon_biome_has_flag(system, biome_id, CARBON_BIOME_FLAG_PASSABLE);
}

bool carbon_biome_is_buildable(const Carbon_BiomeSystem *system, int biome_id) {
    return carbon_biome_has_flag(system, biome_id, CARBON_BIOME_FLAG_BUILDABLE);
}

bool carbon_biome_is_water(const Carbon_BiomeSystem *system, int biome_id) {
    return carbon_biome_has_flag(system, biome_id, CARBON_BIOME_FLAG_WATER);
}

bool carbon_biome_is_hazardous(const Carbon_BiomeSystem *system, int biome_id) {
    return carbon_biome_has_flag(system, biome_id, CARBON_BIOME_FLAG_HAZARDOUS);
}

/*============================================================================
 * Biome Map
 *============================================================================*/

Carbon_BiomeMap *carbon_biome_map_create(
    Carbon_BiomeSystem *system,
    int width,
    int height)
{
    CARBON_VALIDATE_PTR_RET(system, NULL);

    if (width <= 0 || height <= 0) {
        carbon_set_error("Invalid biome map dimensions");
        return NULL;
    }

    Carbon_BiomeMap *map = CARBON_ALLOC(Carbon_BiomeMap);
    if (!map) {
        carbon_set_error("Failed to allocate biome map");
        return NULL;
    }

    map->data = (int8_t*)malloc(width * height);
    if (!map->data) {
        carbon_set_error("Failed to allocate biome map data");
        free(map);
        return NULL;
    }

    /* Initialize all cells to -1 (invalid/unset) */
    memset(map->data, -1, width * height);

    map->system = system;
    map->width = width;
    map->height = height;

    return map;
}

void carbon_biome_map_destroy(Carbon_BiomeMap *map) {
    if (!map) return;

    free(map->data);
    free(map);
}

bool carbon_biome_map_set(Carbon_BiomeMap *map, int x, int y, int biome_id) {
    CARBON_VALIDATE_PTR_RET(map, false);

    if (x < 0 || x >= map->width || y < 0 || y >= map->height) {
        return false;
    }

    if (biome_id < -1 || biome_id >= map->system->count) {
        return false;
    }

    map->data[y * map->width + x] = (int8_t)biome_id;
    return true;
}

int carbon_biome_map_get(const Carbon_BiomeMap *map, int x, int y) {
    CARBON_VALIDATE_PTR_RET(map, CARBON_BIOME_INVALID);

    if (x < 0 || x >= map->width || y < 0 || y >= map->height) {
        return CARBON_BIOME_INVALID;
    }

    return map->data[y * map->width + x];
}

void carbon_biome_map_fill_rect(
    Carbon_BiomeMap *map,
    int x, int y,
    int width, int height,
    int biome_id)
{
    CARBON_VALIDATE_PTR(map);

    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            carbon_biome_map_set(map, x + dx, y + dy, biome_id);
        }
    }
}

void carbon_biome_map_fill_circle(
    Carbon_BiomeMap *map,
    int center_x, int center_y,
    int radius,
    int biome_id)
{
    CARBON_VALIDATE_PTR(map);

    int r2 = radius * radius;

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= r2) {
                carbon_biome_map_set(map, center_x + dx, center_y + dy, biome_id);
            }
        }
    }
}

const Carbon_BiomeDef *carbon_biome_map_get_def(const Carbon_BiomeMap *map, int x, int y) {
    CARBON_VALIDATE_PTR_RET(map, NULL);

    int biome_id = carbon_biome_map_get(map, x, y);
    if (biome_id == CARBON_BIOME_INVALID) {
        return NULL;
    }

    return carbon_biome_get(map->system, biome_id);
}

float carbon_biome_map_get_movement_cost(const Carbon_BiomeMap *map, int x, int y) {
    const Carbon_BiomeDef *def = carbon_biome_map_get_def(map, x, y);
    return def ? def->movement_cost : 1.0f;
}

float carbon_biome_map_get_resource_weight(
    const Carbon_BiomeMap *map,
    int x, int y,
    int resource_type)
{
    CARBON_VALIDATE_PTR_RET(map, 0.0f);

    int biome_id = carbon_biome_map_get(map, x, y);
    if (biome_id == CARBON_BIOME_INVALID) {
        return 0.0f;
    }

    return carbon_biome_get_resource_weight(map->system, biome_id, resource_type);
}

bool carbon_biome_map_is_passable(const Carbon_BiomeMap *map, int x, int y) {
    CARBON_VALIDATE_PTR_RET(map, false);

    int biome_id = carbon_biome_map_get(map, x, y);
    if (biome_id == CARBON_BIOME_INVALID) {
        return false;
    }

    return carbon_biome_is_passable(map->system, biome_id);
}

bool carbon_biome_map_is_buildable(const Carbon_BiomeMap *map, int x, int y) {
    CARBON_VALIDATE_PTR_RET(map, false);

    int biome_id = carbon_biome_map_get(map, x, y);
    if (biome_id == CARBON_BIOME_INVALID) {
        return false;
    }

    return carbon_biome_is_buildable(map->system, biome_id);
}

void carbon_biome_map_get_size(const Carbon_BiomeMap *map, int *out_width, int *out_height) {
    if (!map) {
        if (out_width) *out_width = 0;
        if (out_height) *out_height = 0;
        return;
    }

    if (out_width) *out_width = map->width;
    if (out_height) *out_height = map->height;
}

int carbon_biome_map_count_biome(const Carbon_BiomeMap *map, int biome_id) {
    CARBON_VALIDATE_PTR_RET(map, 0);

    int count = 0;
    int size = map->width * map->height;

    for (int i = 0; i < size; i++) {
        if (map->data[i] == biome_id) {
            count++;
        }
    }

    return count;
}

void carbon_biome_map_get_stats(const Carbon_BiomeMap *map, int *out_counts) {
    CARBON_VALIDATE_PTR(map);
    CARBON_VALIDATE_PTR(out_counts);

    /* Initialize counts to 0 */
    memset(out_counts, 0, CARBON_BIOME_MAX * sizeof(int));

    int size = map->width * map->height;
    for (int i = 0; i < size; i++) {
        int8_t biome_id = map->data[i];
        if (biome_id >= 0 && biome_id < CARBON_BIOME_MAX) {
            out_counts[biome_id]++;
        }
    }
}

/*============================================================================
 * Generation Helpers
 *============================================================================*/

void carbon_biome_map_generate_noise(
    Carbon_BiomeMap *map,
    const int *biome_ids,
    const float *thresholds,
    int count,
    uint32_t seed)
{
    CARBON_VALIDATE_PTR(map);
    CARBON_VALIDATE_PTR(biome_ids);
    CARBON_VALIDATE_PTR(thresholds);

    if (count <= 0) return;

    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            float noise = fbm_noise((float)x, (float)y, seed, 4);

            /* Find biome based on threshold */
            int biome_id = biome_ids[0];
            for (int i = 0; i < count; i++) {
                if (noise >= thresholds[i]) {
                    biome_id = biome_ids[i];
                }
            }

            carbon_biome_map_set(map, x, y, biome_id);
        }
    }
}

void carbon_biome_map_smooth(Carbon_BiomeMap *map, int passes) {
    CARBON_VALIDATE_PTR(map);

    if (passes <= 0) return;

    int8_t *temp = (int8_t*)malloc(map->width * map->height);
    if (!temp) return;

    for (int pass = 0; pass < passes; pass++) {
        for (int y = 0; y < map->height; y++) {
            for (int x = 0; x < map->width; x++) {
                int8_t current = map->data[y * map->width + x];

                /* Count neighbors of same type */
                int same_count = 0;
                int different_biome = -1;
                int different_count = 0;

                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;

                        int nx = x + dx;
                        int ny = y + dy;

                        if (nx >= 0 && nx < map->width && ny >= 0 && ny < map->height) {
                            int8_t neighbor = map->data[ny * map->width + nx];
                            if (neighbor == current) {
                                same_count++;
                            } else if (neighbor >= 0) {
                                if (different_biome < 0 || neighbor == different_biome) {
                                    different_biome = neighbor;
                                    different_count++;
                                }
                            }
                        }
                    }
                }

                /* If more neighbors are different, change to that biome */
                if (different_count > same_count && different_biome >= 0) {
                    temp[y * map->width + x] = (int8_t)different_biome;
                } else {
                    temp[y * map->width + x] = current;
                }
            }
        }

        /* Copy back */
        memcpy(map->data, temp, map->width * map->height);
    }

    free(temp);
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

Carbon_BiomeDef carbon_biome_default_def(void) {
    Carbon_BiomeDef def = {};

    strncpy(def.id, "default", sizeof(def.id) - 1);
    def.id[sizeof(def.id) - 1] = '\0';
    strncpy(def.name, "Default", sizeof(def.name) - 1);
    def.name[sizeof(def.name) - 1] = '\0';
    def.color = 0xFF808080;  /* Gray */
    def.movement_cost = 1.0f;
    def.resource_multiplier = 1.0f;
    def.visibility_modifier = 1.0f;
    def.defense_bonus = 0.0f;
    def.flags = CARBON_BIOME_FLAG_PASSABLE | CARBON_BIOME_FLAG_BUILDABLE;

    return def;
}

uint32_t carbon_biome_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000 | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
}

uint32_t carbon_biome_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
}
