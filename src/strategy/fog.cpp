/**
 * @file fog.c
 * @brief Fog of War / Exploration System implementation
 */

#include "agentite/agentite.h"
#include "agentite/fog.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structures
 * ========================================================================= */

/**
 * @brief Vision source data
 */
typedef struct Agentite_VisionSourceData {
    int x;              /**< X position */
    int y;              /**< Y position */
    int radius;         /**< Vision radius */
    bool active;        /**< Is this slot in use */
} Agentite_VisionSourceData;

/**
 * @brief Fog of war structure
 */
struct Agentite_FogOfWar {
    int width;                          /**< Map width */
    int height;                         /**< Map height */
    uint8_t *exploration;               /**< Exploration state grid (0=unexplored, 1+=explored) */
    uint8_t *visibility;                /**< Current visibility grid (0=not visible, 1+=visible) */

    Agentite_VisionSourceData *sources;   /**< Vision sources array */
    int source_capacity;                /**< Sources array capacity */
    int source_count;                   /**< Active source count */
    uint32_t next_source_id;            /**< Next source ID to assign */

    float shroud_alpha;                 /**< Alpha for explored but not visible cells */
    bool dirty;                         /**< Needs visibility recalculation */

    /* Callbacks */
    Agentite_ExplorationCallback exploration_callback;
    void *exploration_userdata;
    Agentite_VisionBlockerCallback los_callback;
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

static inline int get_index(Agentite_FogOfWar *fog, int x, int y) {
    return y * fog->width + x;
}

static inline bool in_bounds(Agentite_FogOfWar *fog, int x, int y) {
    return x >= 0 && x < fog->width && y >= 0 && y < fog->height;
}

/**
 * @brief Find a vision source by ID
 */
static Agentite_VisionSourceData *find_source(Agentite_FogOfWar *fog, Agentite_VisionSource id) {
    if (id == AGENTITE_VISION_SOURCE_INVALID || id > (uint32_t)fog->source_capacity) {
        return NULL;
    }
    Agentite_VisionSourceData *source = &fog->sources[id - 1];
    return source->active ? source : NULL;
}

/**
 * @brief Bresenham line check for LOS
 */
static bool check_los_line(Agentite_FogOfWar *fog, int x1, int y1, int x2, int y2) {
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
static void apply_source_visibility(Agentite_FogOfWar *fog, Agentite_VisionSourceData *source) {
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

Agentite_FogOfWar *agentite_fog_create(int width, int height) {
    if (width <= 0 || height <= 0) {
        agentite_set_error("Fog: Invalid dimensions %dx%d", width, height);
        return NULL;
    }

    Agentite_FogOfWar *fog = AGENTITE_ALLOC(Agentite_FogOfWar);
    if (!fog) {
        agentite_set_error("Fog: Failed to allocate fog structure");
        return NULL;
    }

    fog->width = width;
    fog->height = height;

    size_t grid_size = (size_t)width * (size_t)height;
    fog->exploration = (uint8_t*)calloc(grid_size, sizeof(uint8_t));
    fog->visibility = (uint8_t*)calloc(grid_size, sizeof(uint8_t));

    if (!fog->exploration || !fog->visibility) {
        agentite_set_error("Fog: Failed to allocate grids");
        free(fog->exploration);
        free(fog->visibility);
        free(fog);
        return NULL;
    }

    /* Allocate sources array */
    fog->source_capacity = AGENTITE_FOG_MAX_SOURCES;
    fog->sources = AGENTITE_ALLOC_ARRAY(Agentite_VisionSourceData, fog->source_capacity);
    if (!fog->sources) {
        agentite_set_error("Fog: Failed to allocate sources");
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

void agentite_fog_destroy(Agentite_FogOfWar *fog) {
    if (!fog) return;
    free(fog->exploration);
    free(fog->visibility);
    free(fog->sources);
    free(fog);
}

void agentite_fog_reset(Agentite_FogOfWar *fog) {
    AGENTITE_VALIDATE_PTR(fog);

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

void agentite_fog_reveal_all(Agentite_FogOfWar *fog) {
    AGENTITE_VALIDATE_PTR(fog);

    size_t grid_size = (size_t)fog->width * (size_t)fog->height;
    memset(fog->exploration, 1, grid_size);
    memset(fog->visibility, 1, grid_size);
}

void agentite_fog_explore_all(Agentite_FogOfWar *fog) {
    AGENTITE_VALIDATE_PTR(fog);

    size_t grid_size = (size_t)fog->width * (size_t)fog->height;
    memset(fog->exploration, 1, grid_size);
    /* Don't set visibility - that requires active sources */
}

/* ============================================================================
 * Vision Sources
 * ========================================================================= */

Agentite_VisionSource agentite_fog_add_source(Agentite_FogOfWar *fog, int x, int y, int radius) {
    AGENTITE_VALIDATE_PTR_RET(fog, AGENTITE_VISION_SOURCE_INVALID);

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < fog->source_capacity; i++) {
        if (!fog->sources[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        agentite_set_error("Fog: Maximum vision sources reached (%d)", AGENTITE_FOG_MAX_SOURCES);
        return AGENTITE_VISION_SOURCE_INVALID;
    }

    fog->sources[slot].x = x;
    fog->sources[slot].y = y;
    fog->sources[slot].radius = radius > 0 ? radius : 0;
    fog->sources[slot].active = true;
    fog->source_count++;
    fog->dirty = true;

    return (Agentite_VisionSource)(slot + 1);  /* IDs are 1-based */
}

void agentite_fog_remove_source(Agentite_FogOfWar *fog, Agentite_VisionSource source) {
    AGENTITE_VALIDATE_PTR(fog);

    Agentite_VisionSourceData *s = find_source(fog, source);
    if (s) {
        s->active = false;
        fog->source_count--;
        fog->dirty = true;
    }
}

void agentite_fog_move_source(Agentite_FogOfWar *fog, Agentite_VisionSource source, int new_x, int new_y) {
    AGENTITE_VALIDATE_PTR(fog);

    Agentite_VisionSourceData *s = find_source(fog, source);
    if (s) {
        if (s->x != new_x || s->y != new_y) {
            s->x = new_x;
            s->y = new_y;
            fog->dirty = true;
        }
    }
}

void agentite_fog_set_source_radius(Agentite_FogOfWar *fog, Agentite_VisionSource source, int new_radius) {
    AGENTITE_VALIDATE_PTR(fog);

    Agentite_VisionSourceData *s = find_source(fog, source);
    if (s) {
        new_radius = new_radius > 0 ? new_radius : 0;
        if (s->radius != new_radius) {
            s->radius = new_radius;
            fog->dirty = true;
        }
    }
}

bool agentite_fog_get_source(Agentite_FogOfWar *fog, Agentite_VisionSource source,
                           int *out_x, int *out_y, int *out_radius) {
    AGENTITE_VALIDATE_PTR_RET(fog, false);

    Agentite_VisionSourceData *s = find_source(fog, source);
    if (!s) return false;

    if (out_x) *out_x = s->x;
    if (out_y) *out_y = s->y;
    if (out_radius) *out_radius = s->radius;
    return true;
}

void agentite_fog_clear_sources(Agentite_FogOfWar *fog) {
    AGENTITE_VALIDATE_PTR(fog);

    for (int i = 0; i < fog->source_capacity; i++) {
        fog->sources[i].active = false;
    }
    fog->source_count = 0;
    fog->dirty = true;

    /* Clear current visibility (exploration stays) */
    size_t grid_size = (size_t)fog->width * (size_t)fog->height;
    memset(fog->visibility, 0, grid_size);
}

int agentite_fog_source_count(Agentite_FogOfWar *fog) {
    AGENTITE_VALIDATE_PTR_RET(fog, 0);
    return fog->source_count;
}

/* ============================================================================
 * Visibility Updates
 * ========================================================================= */

void agentite_fog_update(Agentite_FogOfWar *fog) {
    AGENTITE_VALIDATE_PTR(fog);

    if (!fog->dirty) return;
    agentite_fog_force_update(fog);
}

void agentite_fog_force_update(Agentite_FogOfWar *fog) {
    AGENTITE_VALIDATE_PTR(fog);

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

Agentite_VisibilityState agentite_fog_get_state(Agentite_FogOfWar *fog, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(fog, AGENTITE_VIS_UNEXPLORED);

    if (!in_bounds(fog, x, y)) {
        return AGENTITE_VIS_UNEXPLORED;
    }

    int idx = get_index(fog, x, y);

    if (fog->visibility[idx] > 0) {
        return AGENTITE_VIS_VISIBLE;
    } else if (fog->exploration[idx] > 0) {
        return AGENTITE_VIS_EXPLORED;
    } else {
        return AGENTITE_VIS_UNEXPLORED;
    }
}

bool agentite_fog_is_visible(Agentite_FogOfWar *fog, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(fog, false);

    if (!in_bounds(fog, x, y)) return false;
    return fog->visibility[get_index(fog, x, y)] > 0;
}

bool agentite_fog_is_explored(Agentite_FogOfWar *fog, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(fog, false);

    if (!in_bounds(fog, x, y)) return false;
    int idx = get_index(fog, x, y);
    return fog->exploration[idx] > 0 || fog->visibility[idx] > 0;
}

bool agentite_fog_is_unexplored(Agentite_FogOfWar *fog, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(fog, true);

    if (!in_bounds(fog, x, y)) return true;
    int idx = get_index(fog, x, y);
    return fog->exploration[idx] == 0 && fog->visibility[idx] == 0;
}

float agentite_fog_get_alpha(Agentite_FogOfWar *fog, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(fog, 0.0f);

    Agentite_VisibilityState state = agentite_fog_get_state(fog, x, y);
    switch (state) {
        case AGENTITE_VIS_VISIBLE:
            return 1.0f;
        case AGENTITE_VIS_EXPLORED:
            return fog->shroud_alpha;
        case AGENTITE_VIS_UNEXPLORED:
        default:
            return 0.0f;
    }
}

void agentite_fog_set_shroud_alpha(Agentite_FogOfWar *fog, float alpha) {
    AGENTITE_VALIDATE_PTR(fog);

    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    fog->shroud_alpha = alpha;
}

float agentite_fog_get_shroud_alpha(Agentite_FogOfWar *fog) {
    AGENTITE_VALIDATE_PTR_RET(fog, 0.5f);
    return fog->shroud_alpha;
}

/* ============================================================================
 * Region Queries
 * ========================================================================= */

bool agentite_fog_any_visible_in_rect(Agentite_FogOfWar *fog, int x1, int y1, int x2, int y2) {
    AGENTITE_VALIDATE_PTR_RET(fog, false);

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

bool agentite_fog_all_visible_in_rect(Agentite_FogOfWar *fog, int x1, int y1, int x2, int y2) {
    AGENTITE_VALIDATE_PTR_RET(fog, false);

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

int agentite_fog_count_visible_in_rect(Agentite_FogOfWar *fog, int x1, int y1, int x2, int y2) {
    AGENTITE_VALIDATE_PTR_RET(fog, 0);

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

void agentite_fog_explore_cell(Agentite_FogOfWar *fog, int x, int y) {
    AGENTITE_VALIDATE_PTR(fog);

    if (!in_bounds(fog, x, y)) return;

    int idx = get_index(fog, x, y);
    if (fog->exploration[idx] == 0) {
        fog->exploration[idx] = 1;
        if (fog->exploration_callback) {
            fog->exploration_callback(fog, x, y, fog->exploration_userdata);
        }
    }
}

void agentite_fog_explore_rect(Agentite_FogOfWar *fog, int x1, int y1, int x2, int y2) {
    AGENTITE_VALIDATE_PTR(fog);

    /* Normalize and clamp coordinates */
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    x1 = clamp_coord(x1, 0, fog->width - 1);
    x2 = clamp_coord(x2, 0, fog->width - 1);
    y1 = clamp_coord(y1, 0, fog->height - 1);
    y2 = clamp_coord(y2, 0, fog->height - 1);

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            agentite_fog_explore_cell(fog, x, y);
        }
    }
}

void agentite_fog_explore_circle(Agentite_FogOfWar *fog, int center_x, int center_y, int radius) {
    AGENTITE_VALIDATE_PTR(fog);

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
                agentite_fog_explore_cell(fog, x, y);
            }
        }
    }
}

/* ============================================================================
 * Callbacks
 * ========================================================================= */

void agentite_fog_set_exploration_callback(Agentite_FogOfWar *fog,
                                          Agentite_ExplorationCallback callback,
                                          void *userdata) {
    AGENTITE_VALIDATE_PTR(fog);

    fog->exploration_callback = callback;
    fog->exploration_userdata = userdata;
}

/* ============================================================================
 * Statistics
 * ========================================================================= */

void agentite_fog_get_size(Agentite_FogOfWar *fog, int *out_width, int *out_height) {
    if (!fog) {
        if (out_width) *out_width = 0;
        if (out_height) *out_height = 0;
        return;
    }

    if (out_width) *out_width = fog->width;
    if (out_height) *out_height = fog->height;
}

void agentite_fog_get_stats(Agentite_FogOfWar *fog,
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

float agentite_fog_get_exploration_percent(Agentite_FogOfWar *fog) {
    AGENTITE_VALIDATE_PTR_RET(fog, 0.0f);

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

void agentite_fog_set_los_callback(Agentite_FogOfWar *fog,
                                  Agentite_VisionBlockerCallback callback,
                                  void *userdata) {
    AGENTITE_VALIDATE_PTR(fog);

    fog->los_callback = callback;
    fog->los_userdata = userdata;

    /* LOS rules changed, need to recalculate */
    fog->dirty = true;
}

bool agentite_fog_has_los(Agentite_FogOfWar *fog, int x1, int y1, int x2, int y2) {
    AGENTITE_VALIDATE_PTR_RET(fog, false);

    return check_los_line(fog, x1, y1, x2, y2);
}
