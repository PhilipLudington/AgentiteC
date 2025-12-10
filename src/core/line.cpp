#include "agentite/line.h"
#include <stdlib.h>

/* Absolute value helper */
static inline int32_t iabs(int32_t x) {
    return x < 0 ? -x : x;
}

/* Sign helper: returns -1, 0, or 1 */
static inline int32_t sign(int32_t x) {
    return (x > 0) - (x < 0);
}

bool agentite_iterate_line_cells(int32_t from_x, int32_t from_y,
                                int32_t to_x, int32_t to_y,
                                Agentite_LineCellCallback callback,
                                void *userdata) {
    return agentite_iterate_line_cells_ex(from_x, from_y, to_x, to_y,
                                        callback, userdata, false, false);
}

bool agentite_iterate_line_cells_ex(int32_t from_x, int32_t from_y,
                                   int32_t to_x, int32_t to_y,
                                   Agentite_LineCellCallback callback,
                                   void *userdata,
                                   bool skip_start,
                                   bool skip_end) {
    if (!callback) return true;

    int32_t dx = iabs(to_x - from_x);
    int32_t dy = -iabs(to_y - from_y);
    int32_t sx = sign(to_x - from_x);
    int32_t sy = sign(to_y - from_y);
    int32_t err = dx + dy;

    int32_t x = from_x;
    int32_t y = from_y;

    while (true) {
        /* Check if we should process this cell */
        bool is_start = (x == from_x && y == from_y);
        bool is_end = (x == to_x && y == to_y);

        if (!(is_start && skip_start) && !(is_end && skip_end)) {
            if (!callback(x, y, userdata)) {
                return false;  /* Callback stopped iteration */
            }
        }

        /* Check if we've reached the end */
        if (x == to_x && y == to_y) {
            break;
        }

        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }

    return true;
}

/* Counter callback for counting cells */
static bool count_callback(int32_t x, int32_t y, void *userdata) {
    (void)x;
    (void)y;
    int *count = (int*)userdata;
    (*count)++;
    return true;
}

int agentite_count_line_cells(int32_t from_x, int32_t from_y,
                            int32_t to_x, int32_t to_y) {
    int count = 0;
    agentite_iterate_line_cells(from_x, from_y, to_x, to_y, count_callback, &count);
    return count;
}

int agentite_count_line_cells_between(int32_t from_x, int32_t from_y,
                                     int32_t to_x, int32_t to_y) {
    int count = 0;
    agentite_iterate_line_cells_ex(from_x, from_y, to_x, to_y,
                                 count_callback, &count, true, true);
    return count;
}

/* Buffer fill context */
typedef struct {
    int32_t *out_x;
    int32_t *out_y;
    int count;
    int max_cells;
} BufferContext;

static bool buffer_callback(int32_t x, int32_t y, void *userdata) {
    BufferContext *ctx = (BufferContext*)userdata;
    if (ctx->count >= ctx->max_cells) {
        return false;  /* Buffer full, stop */
    }
    ctx->out_x[ctx->count] = x;
    ctx->out_y[ctx->count] = y;
    ctx->count++;
    return true;
}

int agentite_get_line_cells(int32_t from_x, int32_t from_y,
                          int32_t to_x, int32_t to_y,
                          int32_t *out_x, int32_t *out_y,
                          int max_cells) {
    if (!out_x || !out_y || max_cells <= 0) return 0;

    BufferContext ctx = {
        .out_x = out_x,
        .out_y = out_y,
        .count = 0,
        .max_cells = max_cells
    };

    agentite_iterate_line_cells(from_x, from_y, to_x, to_y, buffer_callback, &ctx);
    return ctx.count;
}

/* Check if cell is in line context */
typedef struct {
    int32_t target_x;
    int32_t target_y;
    bool found;
} FindContext;

static bool find_callback(int32_t x, int32_t y, void *userdata) {
    FindContext *ctx = (FindContext*)userdata;
    if (x == ctx->target_x && y == ctx->target_y) {
        ctx->found = true;
        return false;  /* Stop iteration, found it */
    }
    return true;
}

bool agentite_line_passes_through(int32_t from_x, int32_t from_y,
                                 int32_t to_x, int32_t to_y,
                                 int32_t cell_x, int32_t cell_y) {
    FindContext ctx = {
        .target_x = cell_x,
        .target_y = cell_y,
        .found = false
    };

    agentite_iterate_line_cells(from_x, from_y, to_x, to_y, find_callback, &ctx);
    return ctx.found;
}
