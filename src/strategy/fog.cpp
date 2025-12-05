/**
 * @file fog.c
 * @brief Fog of War / Exploration System implementation
 */

#include "carbon/carbon.h"
#include "carbon/fog.h"
#include "carbon/error.h"
#include "carbon/validate.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structures
 * ========================================================================= */

/**
 * @brief Vision source data
 */
typedef struct Carbon_VisionSourceData {
    int x;              /**< X position */
    int y;              /**< Y position */
    int radius;         /**< Vision radius */
    bool active;        /**< Is this slot in use */
} Carbon_VisionSourceData;

/**
 * @brief Fog of war structure
 */
struct Carbon_FogOfWar {
    int width;                          /**< Map width */
    int height;                         /**< Map height */
    uint8_t *exploration;               /**< Exploration state grid (0=unexplored, 1+=explored) */
    uint8_t *visibility;                /**< Current visibility grid (0=not visible, 1+=visible) */

    Carbon_VisionSourceData *sources;   /**< Vision sources array */
    int source_capacity;                /**< Sources array capacity */
    int source_count;                   /**< Active source count */
    uint32_t next_source_id;            /**< Next source ID to assign */

    float shroud_alpha;                 /**< Alpha for explored but not visible cells */
    bool dirty;                         /**< Needs visibility recalculation */

    /* Callbacks */
    Carbon_ExplorationCallback exploration_callback;
    void *exploration_userdata;
    Carbon_VisionBlockerCallback los_callback;
    void *los_userdata;
};

/* ============================================================================
 * Helper Functions
 * ========================================================================= */

static inline int clamp_coord(int v, int min_val, int max_val) {
    if (v < min_val) return min_val;
    if (v > max_val) return max_val;
    return v;
}

static inline int get_index(Carbon_FogOfWar *fog, int x, int y) {
    return y * fog->width + x;
}

static inline bool in_bounds(Carbon_FogOfWar *fog, int x, int y) {
    return x >= 0 && x < fog->width && y >= 0 && y < fog->height;
}

/**
 * @brief Find a vision source by ID
 */
static Carbon_VisionSourceData *find_source(Carbon_FogOfWar *fog, Carbon_VisionSource id) {
    if (id == CARBON_VISION_SOURCE_INVALID || id > (uint32_t)fog->source_capacity) {
        return NULL;
    }
    Carbon_VisionSourceData *source = &fog->sources[id - 1];
    return source->active ? source : NULL;
}

/**
 * @brief Bresenham line check for LOS
 */
static bool check_los_line(Carbon_FogOfWar *fog, int x1, int y1, int x2, int y2) {
    if (!fog->los_callback) {
        return true;  /* No LOS checking, always visible */
    }

    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    int x = x1;
    int y = y1;

    while (x != x2 || y != y2) {
        /* Check intermediate cells (not start or end) */
        if ((x != x1 || y != y1) && (x != x2 || y != y2)) {
            if (fog->los_callback(x, y, fog->los_userdata)) {
                return false;  /* Blocked */
            }
        }

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }

    return true;
}

/**
 * @brief Apply visibility from a single source
 */
static void apply_source_visibility(Carbon_FogOfWar *fog, Carbon_VisionSourceData *source) {
    int cx = source->x;
    int cy = source->y;
    int r = source->radius;
    int r_sq = r * r;

    /* Iterate over square bounding box */
    int min_x = clamp_coord(cx - r, 0, fog->width - 1);
    int max_x = clamp_coord(cx + r, 0, fog->width - 1);
    int min_y = clamp_coord(cy - r, 0, fog->height - 1);
    int max_y = clamp_coord(cy + r, 0, fog->height - 1);

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            int dx = x - cx;
            int dy = y - cy;

            /* Check if within circular radius */
            if (dx * dx + dy * dy <= r_sq) {
                /* Check line of sight if enabled */
                if (fog->los_callback && !check_los_line(fog, cx, cy, x, y)) {
                    continue;
                }

                int idx = get_index(fog, x, y);

                /* Mark as visible */
                fog->visibility[idx] = 1;

                /* Mark as explored (if first time, fire callback) */
                if (fog->exploration[idx] == 0) {
                    fog->exploration[idx] = 1;
                    if (fog->exploration_callback) {
                        fog->exploration_callback(fog, x, y, fog->exploration_userdata);
                    }
                }
            }
        }
    }
}

/* ============================================================================
 * Creation and Destruction
 * ========================================================================= */

Carbon_FogOfWar *carbon_fog_create(int width, int height) {
    if (width <= 0 || height <= 0) {
        carbon_set_error("Fog: Invalid dimensions %dx%d", width, height);
        return NULL;
    }

    Carbon_FogOfWar *fog = CARBON_ALLOC(Carbon_FogOfWar);
    if (!fog) {
        carbon_set_error("Fog: Failed to allocate fog structure");
        return NULL;
    }

    fog->width = width;
    fog->height = height;

    size_t grid_size = (size_t)width * (size_t)height;
    fog->exploration = (uint8_t*)calloc(grid_size, sizeof(uint8_t));
    fog->visibility = (uint8_t*)calloc(grid_size, sizeof(uint8_t));

    if (!fog->exploration || !fog->visibility) {
        carbon_set_error("Fog: Failed to allocate grids");
        free(fog->exploration);
        free(fog->visibility);
        free(fog);
        return NULL;
    }

    /* Allocate sources array */
    fog->source_capacity = CARBON_FOG_MAX_SOURCES;
    fog->sources = CARBON_ALLOC_ARRAY(Carbon_VisionSourceData, fog->source_capacity);
    if (!fog->sources) {
        carbon_set_error("Fog: Failed to allocate sources");
        free(fog->exploration);
        free(fog->visibility);
        free(fog);
        return NULL;
    }

    fog->source_count = 0;
    fog->next_source_id = 1;
    fog->shroud_alpha = 0.5f;
    fog->dirty = false;

    return fog;
}

void carbon_fog_destroy(Carbon_FogOfWar *fog) {
    if (!fog) return;
    free(fog->exploration);
    free(fog->visibility);
    free(fog->sources);
    free(fog);
}

void carbon_fog_reset(Carbon_FogOfWar *fog) {
    CARBON_VALIDATE_PTR(fog);

    size_t grid_size = (size_t)fog->width * (size_t)fog->height;
    memset(fog->exploration, 0, grid_size);
    memset(fog->visibility, 0, grid_size);

    /* Clear all sources */
    for (int i = 0; i < fog->source_capacity; i++) {
        fog->sources[i].active = false;
    }
    fog->source_count = 0;
    fog->dirty = false;
}

void carbon_fog_reveal_all(Carbon_FogOfWar *fog) {
    CARBON_VALIDATE_PTR(fog);

    size_t grid_size = (size_t)fog->width * (size_t)fog->height;
    memset(fog->exploration, 1, grid_size);
    memset(fog->visibility, 1, grid_size);
}

void carbon_fog_explore_all(Carbon_FogOfWar *fog) {
    CARBON_VALIDATE_PTR(fog);

    size_t grid_size = (size_t)fog->width * (size_t)fog->height;
    memset(fog->exploration, 1, grid_size);
    /* Don't set visibility - that requires active sources */
}

/* ============================================================================
 * Vision Sources
 * ========================================================================= */

Carbon_VisionSource carbon_fog_add_source(Carbon_FogOfWar *fog, int x, int y, int radius) {
    CARBON_VALIDATE_PTR_RET(fog, CARBON_VISION_SOURCE_INVALID);

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < fog->source_capacity; i++) {
        if (!fog->sources[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        carbon_set_error("Fog: Maximum vision sources reached (%d)", CARBON_FOG_MAX_SOURCES);
        return CARBON_VISION_SOURCE_INVALID;
    }

    fog->sources[slot].x = x;
    fog->sources[slot].y = y;
    fog->sources[slot].radius = radius > 0 ? radius : 0;
    fog->sources[slot].active = true;
    fog->source_count++;
    fog->dirty = true;

    return (Carbon_VisionSource)(slot + 1);  /* IDs are 1-based */
}

void carbon_fog_remove_source(Carbon_FogOfWar *fog, Carbon_VisionSource source) {
    CARBON_VALIDATE_PTR(fog);

    Carbon_VisionSourceData *s = find_source(fog, source);
    if (s) {
        s->active = false;
        fog->source_count--;
        fog->dirty = true;
    }
}

void carbon_fog_move_source(Carbon_FogOfWar *fog, Carbon_VisionSource source, int new_x, int new_y) {
    CARBON_VALIDATE_PTR(fog);

    Carbon_VisionSourceData *s = find_source(fog, source);
    if (s) {
        if (s->x != new_x || s->y != new_y) {
            s->x = new_x;
            s->y = new_y;
            fog->dirty = true;
        }
    }
}

void carbon_fog_set_source_radius(Carbon_FogOfWar *fog, Carbon_VisionSource source, int new_radius) {
    CARBON_VALIDATE_PTR(fog);

    Carbon_VisionSourceData *s = find_source(fog, source);
    if (s) {
        new_radius = new_radius > 0 ? new_radius : 0;
        if (s->radius != new_radius) {
            s->radius = new_radius;
            fog->dirty = true;
        }
    }
}

bool carbon_fog_get_source(Carbon_FogOfWar *fog, Carbon_VisionSource source,
                           int *out_x, int *out_y, int *out_radius) {
    CARBON_VALIDATE_PTR_RET(fog, false);

    Carbon_VisionSourceData *s = find_source(fog, source);
    if (!s) return false;

    if (out_x) *out_x = s->x;
    if (out_y) *out_y = s->y;
    if (out_radius) *out_radius = s->radius;
    return true;
}

void carbon_fog_clear_sources(Carbon_FogOfWar *fog) {
    CARBON_VALIDATE_PTR(fog);

    for (int i = 0; i < fog->source_capacity; i++) {
        fog->sources[i].active = false;
    }
    fog->source_count = 0;
    fog->dirty = true;

    /* Clear current visibility (exploration stays) */
    size_t grid_size = (size_t)fog->width * (size_t)fog->height;
    memset(fog->visibility, 0, grid_size);
}

int carbon_fog_source_count(Carbon_FogOfWar *fog) {
    CARBON_VALIDATE_PTR_RET(fog, 0);
    return fog->source_count;
}

/* ============================================================================
 * Visibility Updates
 * ========================================================================= */

void carbon_fog_update(Carbon_FogOfWar *fog) {
    CARBON_VALIDATE_PTR(fog);

    if (!fog->dirty) return;
    carbon_fog_force_update(fog);
}

void carbon_fog_force_update(Carbon_FogOfWar *fog) {
    CARBON_VALIDATE_PTR(fog);

    /* Clear current visibility */
    size_t grid_size = (size_t)fog->width * (size_t)fog->height;
    memset(fog->visibility, 0, grid_size);

    /* Apply visibility from each active source */
    for (int i = 0; i < fog->source_capacity; i++) {
        if (fog->sources[i].active) {
            apply_source_visibility(fog, &fog->sources[i]);
        }
    }

    fog->dirty = false;
}

/* ============================================================================
 * Visibility Queries
 * ========================================================================= */

Carbon_VisibilityState carbon_fog_get_state(Carbon_FogOfWar *fog, int x, int y) {
    CARBON_VALIDATE_PTR_RET(fog, CARBON_VIS_UNEXPLORED);

    if (!in_bounds(fog, x, y)) {
        return CARBON_VIS_UNEXPLORED;
    }

    int idx = get_index(fog, x, y);

    if (fog->visibility[idx] > 0) {
        return CARBON_VIS_VISIBLE;
    } else if (fog->exploration[idx] > 0) {
        return CARBON_VIS_EXPLORED;
    } else {
        return CARBON_VIS_UNEXPLORED;
    }
}

bool carbon_fog_is_visible(Carbon_FogOfWar *fog, int x, int y) {
    CARBON_VALIDATE_PTR_RET(fog, false);

    if (!in_bounds(fog, x, y)) return false;
    return fog->visibility[get_index(fog, x, y)] > 0;
}

bool carbon_fog_is_explored(Carbon_FogOfWar *fog, int x, int y) {
    CARBON_VALIDATE_PTR_RET(fog, false);

    if (!in_bounds(fog, x, y)) return false;
    int idx = get_index(fog, x, y);
    return fog->exploration[idx] > 0 || fog->visibility[idx] > 0;
}

bool carbon_fog_is_unexplored(Carbon_FogOfWar *fog, int x, int y) {
    CARBON_VALIDATE_PTR_RET(fog, true);

    if (!in_bounds(fog, x, y)) return true;
    int idx = get_index(fog, x, y);
    return fog->exploration[idx] == 0 && fog->visibility[idx] == 0;
}

float carbon_fog_get_alpha(Carbon_FogOfWar *fog, int x, int y) {
    CARBON_VALIDATE_PTR_RET(fog, 0.0f);

    Carbon_VisibilityState state = carbon_fog_get_state(fog, x, y);
    switch (state) {
        case CARBON_VIS_VISIBLE:
            return 1.0f;
        case CARBON_VIS_EXPLORED:
            return fog->shroud_alpha;
        case CARBON_VIS_UNEXPLORED:
        default:
            return 0.0f;
    }
}

void carbon_fog_set_shroud_alpha(Carbon_FogOfWar *fog, float alpha) {
    CARBON_VALIDATE_PTR(fog);

    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    fog->shroud_alpha = alpha;
}

float carbon_fog_get_shroud_alpha(Carbon_FogOfWar *fog) {
    CARBON_VALIDATE_PTR_RET(fog, 0.5f);
    return fog->shroud_alpha;
}

/* ============================================================================
 * Region Queries
 * ========================================================================= */

bool carbon_fog_any_visible_in_rect(Carbon_FogOfWar *fog, int x1, int y1, int x2, int y2) {
    CARBON_VALIDATE_PTR_RET(fog, false);

    /* Normalize and clamp coordinates */
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    x1 = clamp_coord(x1, 0, fog->width - 1);
    x2 = clamp_coord(x2, 0, fog->width - 1);
    y1 = clamp_coord(y1, 0, fog->height - 1);
    y2 = clamp_coord(y2, 0, fog->height - 1);

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            if (fog->visibility[get_index(fog, x, y)] > 0) {
                return true;
            }
        }
    }
    return false;
}

bool carbon_fog_all_visible_in_rect(Carbon_FogOfWar *fog, int x1, int y1, int x2, int y2) {
    CARBON_VALIDATE_PTR_RET(fog, false);

    /* Normalize and clamp coordinates */
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    x1 = clamp_coord(x1, 0, fog->width - 1);
    x2 = clamp_coord(x2, 0, fog->width - 1);
    y1 = clamp_coord(y1, 0, fog->height - 1);
    y2 = clamp_coord(y2, 0, fog->height - 1);

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            if (fog->visibility[get_index(fog, x, y)] == 0) {
                return false;
            }
        }
    }
    return true;
}

int carbon_fog_count_visible_in_rect(Carbon_FogOfWar *fog, int x1, int y1, int x2, int y2) {
    CARBON_VALIDATE_PTR_RET(fog, 0);

    /* Normalize and clamp coordinates */
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    x1 = clamp_coord(x1, 0, fog->width - 1);
    x2 = clamp_coord(x2, 0, fog->width - 1);
    y1 = clamp_coord(y1, 0, fog->height - 1);
    y2 = clamp_coord(y2, 0, fog->height - 1);

    int count = 0;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            if (fog->visibility[get_index(fog, x, y)] > 0) {
                count++;
            }
        }
    }
    return count;
}

/* ============================================================================
 * Manual Exploration
 * ========================================================================= */

void carbon_fog_explore_cell(Carbon_FogOfWar *fog, int x, int y) {
    CARBON_VALIDATE_PTR(fog);

    if (!in_bounds(fog, x, y)) return;

    int idx = get_index(fog, x, y);
    if (fog->exploration[idx] == 0) {
        fog->exploration[idx] = 1;
        if (fog->exploration_callback) {
            fog->exploration_callback(fog, x, y, fog->exploration_userdata);
        }
    }
}

void carbon_fog_explore_rect(Carbon_FogOfWar *fog, int x1, int y1, int x2, int y2) {
    CARBON_VALIDATE_PTR(fog);

    /* Normalize and clamp coordinates */
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    x1 = clamp_coord(x1, 0, fog->width - 1);
    x2 = clamp_coord(x2, 0, fog->width - 1);
    y1 = clamp_coord(y1, 0, fog->height - 1);
    y2 = clamp_coord(y2, 0, fog->height - 1);

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            carbon_fog_explore_cell(fog, x, y);
        }
    }
}

void carbon_fog_explore_circle(Carbon_FogOfWar *fog, int center_x, int center_y, int radius) {
    CARBON_VALIDATE_PTR(fog);

    if (radius < 0) radius = 0;
    int r_sq = radius * radius;

    int min_x = clamp_coord(center_x - radius, 0, fog->width - 1);
    int max_x = clamp_coord(center_x + radius, 0, fog->width - 1);
    int min_y = clamp_coord(center_y - radius, 0, fog->height - 1);
    int max_y = clamp_coord(center_y + radius, 0, fog->height - 1);

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            int dx = x - center_x;
            int dy = y - center_y;
            if (dx * dx + dy * dy <= r_sq) {
                carbon_fog_explore_cell(fog, x, y);
            }
        }
    }
}

/* ============================================================================
 * Callbacks
 * ========================================================================= */

void carbon_fog_set_exploration_callback(Carbon_FogOfWar *fog,
                                          Carbon_ExplorationCallback callback,
                                          void *userdata) {
    CARBON_VALIDATE_PTR(fog);

    fog->exploration_callback = callback;
    fog->exploration_userdata = userdata;
}

/* ============================================================================
 * Statistics
 * ========================================================================= */

void carbon_fog_get_size(Carbon_FogOfWar *fog, int *out_width, int *out_height) {
    if (!fog) {
        if (out_width) *out_width = 0;
        if (out_height) *out_height = 0;
        return;
    }

    if (out_width) *out_width = fog->width;
    if (out_height) *out_height = fog->height;
}

void carbon_fog_get_stats(Carbon_FogOfWar *fog,
                          int *out_unexplored, int *out_explored, int *out_visible) {
    if (!fog) {
        if (out_unexplored) *out_unexplored = 0;
        if (out_explored) *out_explored = 0;
        if (out_visible) *out_visible = 0;
        return;
    }

    int unexplored = 0;
    int explored = 0;
    int visible = 0;

    size_t grid_size = (size_t)fog->width * (size_t)fog->height;
    for (size_t i = 0; i < grid_size; i++) {
        if (fog->visibility[i] > 0) {
            visible++;
        } else if (fog->exploration[i] > 0) {
            explored++;
        } else {
            unexplored++;
        }
    }

    if (out_unexplored) *out_unexplored = unexplored;
    if (out_explored) *out_explored = explored;
    if (out_visible) *out_visible = visible;
}

float carbon_fog_get_exploration_percent(Carbon_FogOfWar *fog) {
    CARBON_VALIDATE_PTR_RET(fog, 0.0f);

    int total = fog->width * fog->height;
    if (total == 0) return 0.0f;

    int explored_count = 0;
    size_t grid_size = (size_t)fog->width * (size_t)fog->height;
    for (size_t i = 0; i < grid_size; i++) {
        if (fog->exploration[i] > 0 || fog->visibility[i] > 0) {
            explored_count++;
        }
    }

    return (float)explored_count / (float)total;
}

/* ============================================================================
 * Line of Sight
 * ========================================================================= */

void carbon_fog_set_los_callback(Carbon_FogOfWar *fog,
                                  Carbon_VisionBlockerCallback callback,
                                  void *userdata) {
    CARBON_VALIDATE_PTR(fog);

    fog->los_callback = callback;
    fog->los_userdata = userdata;

    /* LOS rules changed, need to recalculate */
    fog->dirty = true;
}

bool carbon_fog_has_los(Carbon_FogOfWar *fog, int x1, int y1, int x2, int y2) {
    CARBON_VALIDATE_PTR_RET(fog, false);

    return check_los_line(fog, x1, y1, x2, y2);
}
