/*
 * Carbon Pathfinding System Implementation
 *
 * A* algorithm with binary heap priority queue for efficient pathfinding.
 */

#include "carbon/carbon.h"
#include "carbon/pathfinding.h"
#include "carbon/tilemap.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/* Node state for A* */
typedef struct PathNode {
    float g_cost;           /* Cost from start to this node */
    float f_cost;           /* g_cost + heuristic (total estimated cost) */
    int parent_x;           /* Parent node coordinates (-1 if none) */
    int parent_y;
    uint8_t flags;          /* OPEN, CLOSED, etc. */
} PathNode;

#define NODE_FLAG_NONE   0
#define NODE_FLAG_OPEN   1
#define NODE_FLAG_CLOSED 2

/* Grid cell data */
typedef struct GridCell {
    float cost;             /* Movement cost (1.0 = normal) */
    bool walkable;          /* Can units pass through? */
} GridCell;

/* Binary heap entry for open list */
typedef struct HeapEntry {
    int x, y;
    float f_cost;
} HeapEntry;

/* Binary min-heap for open list */
typedef struct BinaryHeap {
    HeapEntry *entries;
    int count;
    int capacity;
} BinaryHeap;

/* Pathfinder state */
struct Carbon_Pathfinder {
    GridCell *grid;         /* Grid of walkability and costs */
    PathNode *nodes;        /* A* node state (reused between searches) */
    BinaryHeap open_list;   /* Priority queue for A* */
    int width;
    int height;
};

/* Direction offsets for neighbors (8-directional) */
static const int DIR_X[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const int DIR_Y[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
static const bool DIR_DIAG[8] = { false, true, false, true, false, true, false, true };

/* ============================================================================
 * Binary Heap Implementation
 * ============================================================================ */

static bool heap_init(BinaryHeap *heap, int initial_capacity)
{
    heap->entries = (HeapEntry*)malloc(initial_capacity * sizeof(HeapEntry));
    if (!heap->entries) return false;
    heap->count = 0;
    heap->capacity = initial_capacity;
    return true;
}

static void heap_destroy(BinaryHeap *heap)
{
    free(heap->entries);
    heap->entries = NULL;
    heap->count = 0;
    heap->capacity = 0;
}

static void heap_clear(BinaryHeap *heap)
{
    heap->count = 0;
}

static bool heap_push(BinaryHeap *heap, int x, int y, float f_cost)
{
    /* Grow if needed */
    if (heap->count >= heap->capacity) {
        int new_cap = heap->capacity * 2;
        HeapEntry *new_entries = (HeapEntry*)realloc(heap->entries, new_cap * sizeof(HeapEntry));
        if (!new_entries) return false;
        heap->entries = new_entries;
        heap->capacity = new_cap;
    }

    /* Add at end */
    int idx = heap->count++;
    heap->entries[idx].x = x;
    heap->entries[idx].y = y;
    heap->entries[idx].f_cost = f_cost;

    /* Bubble up */
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (heap->entries[parent].f_cost <= heap->entries[idx].f_cost) break;

        HeapEntry tmp = heap->entries[parent];
        heap->entries[parent] = heap->entries[idx];
        heap->entries[idx] = tmp;
        idx = parent;
    }

    return true;
}

static bool heap_pop(BinaryHeap *heap, int *out_x, int *out_y)
{
    if (heap->count == 0) return false;

    *out_x = heap->entries[0].x;
    *out_y = heap->entries[0].y;

    /* Move last to root */
    heap->entries[0] = heap->entries[--heap->count];

    /* Bubble down */
    int idx = 0;
    while (true) {
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;
        int smallest = idx;

        if (left < heap->count &&
            heap->entries[left].f_cost < heap->entries[smallest].f_cost) {
            smallest = left;
        }
        if (right < heap->count &&
            heap->entries[right].f_cost < heap->entries[smallest].f_cost) {
            smallest = right;
        }

        if (smallest == idx) break;

        HeapEntry tmp = heap->entries[idx];
        heap->entries[idx] = heap->entries[smallest];
        heap->entries[smallest] = tmp;
        idx = smallest;
    }

    return true;
}

static bool heap_is_empty(BinaryHeap *heap)
{
    return heap->count == 0;
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static inline int grid_index(Carbon_Pathfinder *pf, int x, int y)
{
    return y * pf->width + x;
}

static inline bool in_bounds(Carbon_Pathfinder *pf, int x, int y)
{
    return x >= 0 && x < pf->width && y >= 0 && y < pf->height;
}

static float heuristic(int x1, int y1, int x2, int y2, bool allow_diagonal)
{
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);

    if (allow_diagonal) {
        /* Octile distance - more accurate for 8-directional movement */
        int min_d = dx < dy ? dx : dy;
        int max_d = dx > dy ? dx : dy;
        return (float)max_d + 0.41421356f * (float)min_d;
    } else {
        /* Manhattan distance for 4-directional */
        return (float)(dx + dy);
    }
}

static Carbon_Path *reconstruct_path(Carbon_Pathfinder *pf,
                                      int start_x, int start_y,
                                      int end_x, int end_y)
{
    /* Count path length */
    int length = 0;
    int x = end_x, y = end_y;

    while (x != start_x || y != start_y) {
        length++;
        PathNode *node = &pf->nodes[grid_index(pf, x, y)];
        int px = node->parent_x;
        int py = node->parent_y;
        if (px < 0 || py < 0) return NULL;  /* Should not happen */
        x = px;
        y = py;
    }
    length++;  /* Include start */

    /* Allocate path */
    Carbon_Path *path = (Carbon_Path*)malloc(sizeof(Carbon_Path));
    if (!path) return NULL;

    path->points = (Carbon_PathPoint*)malloc(length * sizeof(Carbon_PathPoint));
    if (!path->points) {
        free(path);
        return NULL;
    }

    path->length = length;
    path->total_cost = pf->nodes[grid_index(pf, end_x, end_y)].g_cost;

    /* Fill path in reverse */
    x = end_x;
    y = end_y;
    for (int i = length - 1; i >= 0; i--) {
        path->points[i].x = x;
        path->points[i].y = y;

        if (i > 0) {
            PathNode *node = &pf->nodes[grid_index(pf, x, y)];
            x = node->parent_x;
            y = node->parent_y;
        }
    }

    return path;
}

/* ============================================================================
 * Pathfinder Lifecycle
 * ============================================================================ */

Carbon_Pathfinder *carbon_pathfinder_create(int width, int height)
{
    if (width <= 0 || height <= 0) return NULL;

    Carbon_Pathfinder *pf = CARBON_ALLOC(Carbon_Pathfinder);
    if (!pf) return NULL;

    pf->width = width;
    pf->height = height;

    int total = width * height;

    /* Allocate grid */
    pf->grid = (GridCell*)malloc(total * sizeof(GridCell));
    if (!pf->grid) {
        free(pf);
        return NULL;
    }

    /* Allocate node state */
    pf->nodes = (PathNode*)malloc(total * sizeof(PathNode));
    if (!pf->nodes) {
        free(pf->grid);
        free(pf);
        return NULL;
    }

    /* Initialize heap */
    if (!heap_init(&pf->open_list, 256)) {
        free(pf->nodes);
        free(pf->grid);
        free(pf);
        return NULL;
    }

    /* Initialize grid to all walkable with cost 1.0 */
    for (int i = 0; i < total; i++) {
        pf->grid[i].walkable = true;
        pf->grid[i].cost = 1.0f;
    }

    return pf;
}

void carbon_pathfinder_destroy(Carbon_Pathfinder *pf)
{
    if (!pf) return;
    heap_destroy(&pf->open_list);
    free(pf->nodes);
    free(pf->grid);
    free(pf);
}

void carbon_pathfinder_get_size(Carbon_Pathfinder *pf, int *width, int *height)
{
    if (!pf) return;
    if (width) *width = pf->width;
    if (height) *height = pf->height;
}

/* ============================================================================
 * Grid Configuration
 * ============================================================================ */

void carbon_pathfinder_set_walkable(Carbon_Pathfinder *pf, int x, int y, bool walkable)
{
    if (!pf || !in_bounds(pf, x, y)) return;
    pf->grid[grid_index(pf, x, y)].walkable = walkable;
}

bool carbon_pathfinder_is_walkable(Carbon_Pathfinder *pf, int x, int y)
{
    if (!pf || !in_bounds(pf, x, y)) return false;
    return pf->grid[grid_index(pf, x, y)].walkable;
}

void carbon_pathfinder_set_cost(Carbon_Pathfinder *pf, int x, int y, float cost)
{
    if (!pf || !in_bounds(pf, x, y)) return;
    if (cost < 0.0f) cost = 0.0f;
    pf->grid[grid_index(pf, x, y)].cost = cost;
}

float carbon_pathfinder_get_cost(Carbon_Pathfinder *pf, int x, int y)
{
    if (!pf || !in_bounds(pf, x, y)) return FLT_MAX;
    return pf->grid[grid_index(pf, x, y)].cost;
}

void carbon_pathfinder_fill_walkable(Carbon_Pathfinder *pf,
                                      int x, int y, int width, int height,
                                      bool walkable)
{
    if (!pf) return;

    /* Clamp to bounds */
    int x2 = x + width;
    int y2 = y + height;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > pf->width) x2 = pf->width;
    if (y2 > pf->height) y2 = pf->height;

    for (int ty = y; ty < y2; ty++) {
        for (int tx = x; tx < x2; tx++) {
            pf->grid[grid_index(pf, tx, ty)].walkable = walkable;
        }
    }
}

void carbon_pathfinder_fill_cost(Carbon_Pathfinder *pf,
                                  int x, int y, int width, int height,
                                  float cost)
{
    if (!pf) return;
    if (cost < 0.0f) cost = 0.0f;

    /* Clamp to bounds */
    int x2 = x + width;
    int y2 = y + height;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > pf->width) x2 = pf->width;
    if (y2 > pf->height) y2 = pf->height;

    for (int ty = y; ty < y2; ty++) {
        for (int tx = x; tx < x2; tx++) {
            pf->grid[grid_index(pf, tx, ty)].cost = cost;
        }
    }
}

void carbon_pathfinder_clear(Carbon_Pathfinder *pf)
{
    if (!pf) return;

    int total = pf->width * pf->height;
    for (int i = 0; i < total; i++) {
        pf->grid[i].walkable = true;
        pf->grid[i].cost = 1.0f;
    }
}

/* ============================================================================
 * Tilemap Integration
 * ============================================================================ */

void carbon_pathfinder_sync_tilemap(Carbon_Pathfinder *pf,
                                     Carbon_Tilemap *tilemap,
                                     int layer,
                                     const uint16_t *blocked_tiles,
                                     int count)
{
    if (!pf || !tilemap) return;

    int map_w, map_h;
    carbon_tilemap_get_size(tilemap, &map_w, &map_h);

    /* Process each tile in the overlapping region */
    int max_x = pf->width < map_w ? pf->width : map_w;
    int max_y = pf->height < map_h ? pf->height : map_h;

    for (int y = 0; y < max_y; y++) {
        for (int x = 0; x < max_x; x++) {
            uint16_t tile_id = carbon_tilemap_get_tile(tilemap, layer, x, y);

            /* Check if this tile ID is in the blocked list */
            bool blocked = false;
            for (int i = 0; i < count; i++) {
                if (tile_id == blocked_tiles[i]) {
                    blocked = true;
                    break;
                }
            }

            pf->grid[grid_index(pf, x, y)].walkable = !blocked;
        }
    }
}

void carbon_pathfinder_sync_tilemap_ex(Carbon_Pathfinder *pf,
                                        Carbon_Tilemap *tilemap,
                                        int layer,
                                        Carbon_TileCostFunc cost_func,
                                        void *userdata)
{
    if (!pf || !tilemap || !cost_func) return;

    int map_w, map_h;
    carbon_tilemap_get_size(tilemap, &map_w, &map_h);

    int max_x = pf->width < map_w ? pf->width : map_w;
    int max_y = pf->height < map_h ? pf->height : map_h;

    for (int y = 0; y < max_y; y++) {
        for (int x = 0; x < max_x; x++) {
            uint16_t tile_id = carbon_tilemap_get_tile(tilemap, layer, x, y);
            float cost = cost_func(tile_id, userdata);

            int idx = grid_index(pf, x, y);
            if (cost <= 0.0f) {
                pf->grid[idx].walkable = false;
                pf->grid[idx].cost = 1.0f;
            } else {
                pf->grid[idx].walkable = true;
                pf->grid[idx].cost = cost;
            }
        }
    }
}

/* ============================================================================
 * Pathfinding
 * ============================================================================ */

Carbon_Path *carbon_pathfinder_find_ex(Carbon_Pathfinder *pf,
                                        int start_x, int start_y,
                                        int end_x, int end_y,
                                        const Carbon_PathOptions *options)
{
    if (!pf) return NULL;

    /* Bounds check */
    if (!in_bounds(pf, start_x, start_y) || !in_bounds(pf, end_x, end_y)) {
        return NULL;
    }

    /* Check start and end are walkable */
    if (!pf->grid[grid_index(pf, start_x, start_y)].walkable ||
        !pf->grid[grid_index(pf, end_x, end_y)].walkable) {
        return NULL;
    }

    /* Same tile - trivial path */
    if (start_x == end_x && start_y == end_y) {
        Carbon_Path *path = (Carbon_Path*)malloc(sizeof(Carbon_Path));
        if (!path) return NULL;
        path->points = (Carbon_PathPoint*)malloc(sizeof(Carbon_PathPoint));
        if (!path->points) {
            free(path);
            return NULL;
        }
        path->points[0].x = start_x;
        path->points[0].y = start_y;
        path->length = 1;
        path->total_cost = 0.0f;
        return path;
    }

    /* Use default options if none provided */
    Carbon_PathOptions opts = CARBON_PATH_OPTIONS_DEFAULT;
    if (options) opts = *options;

    /* Reset node state */
    int total = pf->width * pf->height;
    memset(pf->nodes, 0, total * sizeof(PathNode));
    for (int i = 0; i < total; i++) {
        pf->nodes[i].parent_x = -1;
        pf->nodes[i].parent_y = -1;
        pf->nodes[i].g_cost = FLT_MAX;
    }

    /* Clear and initialize open list */
    heap_clear(&pf->open_list);

    /* Add start node */
    int start_idx = grid_index(pf, start_x, start_y);
    pf->nodes[start_idx].g_cost = 0.0f;
    pf->nodes[start_idx].f_cost = heuristic(start_x, start_y, end_x, end_y,
                                             opts.allow_diagonal);
    pf->nodes[start_idx].flags = NODE_FLAG_OPEN;
    heap_push(&pf->open_list, start_x, start_y, pf->nodes[start_idx].f_cost);

    int iterations = 0;
    int max_iter = opts.max_iterations > 0 ? opts.max_iterations : total;

    while (!heap_is_empty(&pf->open_list) && iterations < max_iter) {
        iterations++;

        /* Get node with lowest f_cost */
        int curr_x, curr_y;
        heap_pop(&pf->open_list, &curr_x, &curr_y);

        int curr_idx = grid_index(pf, curr_x, curr_y);

        /* Skip if already closed (can happen with duplicate heap entries) */
        if (pf->nodes[curr_idx].flags == NODE_FLAG_CLOSED) {
            continue;
        }

        pf->nodes[curr_idx].flags = NODE_FLAG_CLOSED;

        /* Found goal? */
        if (curr_x == end_x && curr_y == end_y) {
            return reconstruct_path(pf, start_x, start_y, end_x, end_y);
        }

        /* Check neighbors */
        int num_dirs = opts.allow_diagonal ? 8 : 4;
        for (int d = 0; d < num_dirs; d++) {
            /* Skip diagonals if disabled */
            if (!opts.allow_diagonal && DIR_DIAG[d]) continue;

            int nx = curr_x + DIR_X[d];
            int ny = curr_y + DIR_Y[d];

            /* Bounds check */
            if (!in_bounds(pf, nx, ny)) continue;

            int neighbor_idx = grid_index(pf, nx, ny);

            /* Skip if not walkable or already closed */
            if (!pf->grid[neighbor_idx].walkable) continue;
            if (pf->nodes[neighbor_idx].flags == NODE_FLAG_CLOSED) continue;

            /* Check corner cutting */
            if (DIR_DIAG[d] && !opts.cut_corners) {
                /* For diagonal movement, check if adjacent cardinal tiles are walkable */
                int adj1_x = curr_x + DIR_X[d];
                int adj1_y = curr_y;
                int adj2_x = curr_x;
                int adj2_y = curr_y + DIR_Y[d];

                if (!in_bounds(pf, adj1_x, adj1_y) ||
                    !pf->grid[grid_index(pf, adj1_x, adj1_y)].walkable ||
                    !in_bounds(pf, adj2_x, adj2_y) ||
                    !pf->grid[grid_index(pf, adj2_x, adj2_y)].walkable) {
                    continue;
                }
            }

            /* Calculate tentative g_cost */
            float move_cost = pf->grid[neighbor_idx].cost;
            if (DIR_DIAG[d]) {
                move_cost *= opts.diagonal_cost;
            }
            float tentative_g = pf->nodes[curr_idx].g_cost + move_cost;

            /* If this is a better path */
            if (tentative_g < pf->nodes[neighbor_idx].g_cost) {
                pf->nodes[neighbor_idx].parent_x = curr_x;
                pf->nodes[neighbor_idx].parent_y = curr_y;
                pf->nodes[neighbor_idx].g_cost = tentative_g;
                pf->nodes[neighbor_idx].f_cost = tentative_g +
                    heuristic(nx, ny, end_x, end_y, opts.allow_diagonal);

                /* Add to open list (may create duplicates, handled above) */
                if (pf->nodes[neighbor_idx].flags != NODE_FLAG_OPEN) {
                    pf->nodes[neighbor_idx].flags = NODE_FLAG_OPEN;
                }
                heap_push(&pf->open_list, nx, ny, pf->nodes[neighbor_idx].f_cost);
            }
        }
    }

    /* No path found */
    return NULL;
}

Carbon_Path *carbon_pathfinder_find(Carbon_Pathfinder *pf,
                                     int start_x, int start_y,
                                     int end_x, int end_y)
{
    return carbon_pathfinder_find_ex(pf, start_x, start_y, end_x, end_y, NULL);
}

bool carbon_pathfinder_has_path(Carbon_Pathfinder *pf,
                                 int start_x, int start_y,
                                 int end_x, int end_y)
{
    Carbon_Path *path = carbon_pathfinder_find(pf, start_x, start_y, end_x, end_y);
    if (path) {
        carbon_path_destroy(path);
        return true;
    }
    return false;
}

bool carbon_pathfinder_line_clear(Carbon_Pathfinder *pf,
                                   int x1, int y1,
                                   int x2, int y2)
{
    if (!pf) return false;
    if (!in_bounds(pf, x1, y1) || !in_bounds(pf, x2, y2)) return false;

    /* Bresenham's line algorithm */
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    int x = x1, y = y1;

    while (true) {
        if (!pf->grid[grid_index(pf, x, y)].walkable) {
            return false;
        }

        if (x == x2 && y == y2) break;

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

/* ============================================================================
 * Path Operations
 * ============================================================================ */

void carbon_path_destroy(Carbon_Path *path)
{
    if (!path) return;
    free(path->points);
    free(path);
}

const Carbon_PathPoint *carbon_path_get_point(Carbon_Path *path, int index)
{
    if (!path || index < 0 || index >= path->length) return NULL;
    return &path->points[index];
}

Carbon_Path *carbon_path_simplify(Carbon_Path *path)
{
    if (!path || path->length <= 2) return path;

    /* Count simplified points */
    int count = 1;  /* Always include start */
    int prev_dx = 0, prev_dy = 0;

    for (int i = 1; i < path->length; i++) {
        int dx = path->points[i].x - path->points[i-1].x;
        int dy = path->points[i].y - path->points[i-1].y;

        /* Direction changed? Keep this point */
        if (dx != prev_dx || dy != prev_dy) {
            count++;
            prev_dx = dx;
            prev_dy = dy;
        }
    }

    /* Always include end if not already */
    if (count == 1 ||
        path->points[path->length-1].x != path->points[count-1].x ||
        path->points[path->length-1].y != path->points[count-1].y) {
        /* End point will be handled below */
    }

    /* Allocate simplified path */
    Carbon_Path *simplified = (Carbon_Path*)malloc(sizeof(Carbon_Path));
    if (!simplified) return path;

    simplified->points = (Carbon_PathPoint*)malloc(count * sizeof(Carbon_PathPoint));
    if (!simplified->points) {
        free(simplified);
        return path;
    }

    /* Build simplified path */
    int out_idx = 0;
    simplified->points[out_idx++] = path->points[0];
    prev_dx = 0;
    prev_dy = 0;

    for (int i = 1; i < path->length; i++) {
        int dx = path->points[i].x - path->points[i-1].x;
        int dy = path->points[i].y - path->points[i-1].y;

        if (dx != prev_dx || dy != prev_dy) {
            if (i < path->length - 1) {
                simplified->points[out_idx++] = path->points[i];
            }
            prev_dx = dx;
            prev_dy = dy;
        }
    }

    /* Always include end */
    simplified->points[out_idx++] = path->points[path->length - 1];

    simplified->length = out_idx;
    simplified->total_cost = path->total_cost;

    /* Free original and return simplified */
    carbon_path_destroy(path);
    return simplified;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

int carbon_pathfinder_distance_manhattan(int x1, int y1, int x2, int y2)
{
    return abs(x2 - x1) + abs(y2 - y1);
}

float carbon_pathfinder_distance_euclidean(int x1, int y1, int x2, int y2)
{
    float dx = (float)(x2 - x1);
    float dy = (float)(y2 - y1);
    return sqrtf(dx * dx + dy * dy);
}

int carbon_pathfinder_distance_chebyshev(int x1, int y1, int x2, int y2)
{
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    return dx > dy ? dx : dy;
}
