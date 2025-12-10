/*
 * Agentite UI - Table Widget
 *
 * Sortable, resizable table with column headers
 */

#include "agentite/ui.h"
#include "agentite/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Forward declarations from other UI modules */
extern void aui_draw_rect(AUI_Context *ctx, float x, float y, float w, float h, uint32_t color);
extern void aui_draw_rect_outline(AUI_Context *ctx, float x, float y, float w, float h, uint32_t color, float thickness);
extern void aui_draw_line(AUI_Context *ctx, float x1, float y1, float x2, float y2, uint32_t color, float thickness);
extern float aui_draw_text(AUI_Context *ctx, const char *text, float x, float y, uint32_t color);
extern void aui_draw_text_clipped(AUI_Context *ctx, const char *text, AUI_Rect bounds, uint32_t color);
extern float aui_text_width(AUI_Context *ctx, const char *text);
extern float aui_text_height(AUI_Context *ctx);
extern void aui_draw_triangle(AUI_Context *ctx, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color);
extern void aui_push_scissor(AUI_Context *ctx, float x, float y, float w, float h);
extern void aui_pop_scissor(AUI_Context *ctx);

/* Maximum columns supported */
#define AUI_TABLE_MAX_COLUMNS 32

/* Persistent table state (stored per table ID) */
typedef struct AUI_TablePersist {
    float column_widths[AUI_TABLE_MAX_COLUMNS];
    float scroll_x, scroll_y;
    AUI_TableSortSpec sort_spec;
    bool initialized;
} AUI_TablePersist;

/* Get or create persistent state for a table */
static AUI_TablePersist *aui_table_get_persist(AUI_Context *ctx, AUI_Id id)
{
    AUI_WidgetState *state = aui_get_state(ctx, id);
    if (!state) return NULL;

    /* We store the persist data in the state's scroll fields plus extended data */
    /* For simplicity, we'll just use a static table for now - proper impl would use hash table */
    static AUI_TablePersist persist_table[64];
    static AUI_Id persist_ids[64];
    static int persist_count = 0;

    /* Find existing */
    for (int i = 0; i < persist_count; i++) {
        if (persist_ids[i] == id) {
            return &persist_table[i];
        }
    }

    /* Create new if space available */
    if (persist_count < 64) {
        persist_ids[persist_count] = id;
        AUI_TablePersist *p = &persist_table[persist_count];
        memset(p, 0, sizeof(*p));
        p->sort_spec.column_index = -1;  /* No sort by default */
        persist_count++;
        return p;
    }

    return NULL;
}

bool aui_begin_table(AUI_Context *ctx, const char *id, int columns,
                     uint32_t flags, float width, float height)
{
    if (!ctx || !id || columns <= 0 || columns > AUI_TABLE_MAX_COLUMNS) return false;

    AUI_Id table_id = aui_id(id);
    AUI_TablePersist *persist = aui_table_get_persist(ctx, table_id);
    if (!persist) return false;

    /* Get layout position */
    if (ctx->layout_depth <= 0) return false;
    AUI_LayoutFrame *layout = &ctx->layout_stack[ctx->layout_depth - 1];

    /* Use available width/height if not specified */
    if (width <= 0) width = layout->bounds.w - layout->cursor_x - layout->padding;
    if (height <= 0) height = layout->bounds.h - layout->cursor_y - layout->padding;

    float x = layout->bounds.x + layout->cursor_x;
    float y = layout->bounds.y + layout->cursor_y;

    /* Initialize table state */
    ctx->table.id = table_id;
    ctx->table.column_count = columns;
    ctx->table.current_column = -1;
    ctx->table.current_row = -1;
    ctx->table.flags = flags;
    ctx->table.bounds = (AUI_Rect){x, y, width, height};
    ctx->table.row_height = ctx->theme.widget_height;
    ctx->table.columns_setup = 0;
    ctx->table.sort_specs_changed = false;

    /* Allocate temporary column data */
    ctx->table.column_widths = (float *)malloc(columns * sizeof(float));
    ctx->table.column_labels = (const char **)malloc(columns * sizeof(const char *));
    ctx->table.column_flags = (uint32_t *)malloc(columns * sizeof(uint32_t));

    if (!ctx->table.column_widths || !ctx->table.column_labels || !ctx->table.column_flags) {
        free(ctx->table.column_widths);
        free(ctx->table.column_labels);
        free(ctx->table.column_flags);
        ctx->table.column_widths = NULL;
        ctx->table.column_labels = NULL;
        ctx->table.column_flags = NULL;
        return false;
    }

    /* Initialize column widths */
    float default_width = (width - ctx->theme.padding * 2) / columns;
    for (int i = 0; i < columns; i++) {
        if (persist->initialized && persist->column_widths[i] > 0) {
            ctx->table.column_widths[i] = persist->column_widths[i];
        } else {
            ctx->table.column_widths[i] = default_width;
        }
        ctx->table.column_labels[i] = NULL;
        ctx->table.column_flags[i] = 0;
    }

    /* Restore scroll position */
    ctx->table.scroll_x = persist->scroll_x;
    ctx->table.scroll_y = persist->scroll_y;

    /* Restore sort spec */
    ctx->table.sort_spec = persist->sort_spec;

    /* Draw table background */
    if (flags & AUI_TABLE_BORDERS) {
        aui_draw_rect(ctx, x, y, width, height, ctx->theme.bg_panel);
        aui_draw_rect_outline(ctx, x, y, width, height, ctx->theme.border, 1.0f);
    }

    /* Set up scissor for table content */
    aui_push_scissor(ctx, x, y, width, height);

    return true;
}

void aui_table_setup_column(AUI_Context *ctx, const char *label,
                            uint32_t flags, float init_width)
{
    if (!ctx || ctx->table.id == AUI_ID_NONE) return;
    if (ctx->table.columns_setup >= ctx->table.column_count) return;

    int col = ctx->table.columns_setup;
    ctx->table.column_labels[col] = label;
    ctx->table.column_flags[col] = flags;

    if (init_width > 0) {
        /* Only use init_width if not already persisted */
        AUI_TablePersist *persist = aui_table_get_persist(ctx, ctx->table.id);
        if (persist && !persist->initialized) {
            ctx->table.column_widths[col] = init_width;
        }
    }

    /* Check for default sort */
    if ((flags & AUI_TABLE_COLUMN_DEFAULT_SORT) && ctx->table.sort_spec.column_index < 0) {
        ctx->table.sort_spec.column_index = col;
        ctx->table.sort_spec.descending = false;
    }

    ctx->table.columns_setup++;
}

void aui_table_headers_row(AUI_Context *ctx)
{
    if (!ctx || ctx->table.id == AUI_ID_NONE) return;

    float x = ctx->table.bounds.x + ctx->theme.padding - ctx->table.scroll_x;
    float y = ctx->table.bounds.y;
    float header_height = ctx->table.row_height;

    /* Draw header background */
    aui_draw_rect(ctx, ctx->table.bounds.x, y,
                  ctx->table.bounds.w, header_height,
                  ctx->theme.bg_widget);

    /* Draw each column header */
    for (int i = 0; i < ctx->table.column_count; i++) {
        float col_width = ctx->table.column_widths[i];
        AUI_Rect header_rect = {x, y, col_width, header_height};

        /* Check for hover/click on header */
        bool hovered = aui_rect_contains(header_rect, ctx->input.mouse_x, ctx->input.mouse_y);
        bool sortable = (ctx->table.flags & AUI_TABLE_SORTABLE) &&
                       !(ctx->table.column_flags[i] & AUI_TABLE_COLUMN_NO_SORT);

        if (hovered) {
            ctx->hot = ctx->table.id + i + 1;  /* Unique ID per column */
            aui_draw_rect(ctx, x, y, col_width, header_height, ctx->theme.bg_widget_hover);

            if (sortable && ctx->input.mouse_pressed[0]) {
                if (ctx->table.sort_spec.column_index == i) {
                    /* Toggle sort direction */
                    ctx->table.sort_spec.descending = !ctx->table.sort_spec.descending;
                } else {
                    /* Sort by this column */
                    ctx->table.sort_spec.column_index = i;
                    ctx->table.sort_spec.descending = false;
                }
                ctx->table.sort_specs_changed = true;
            }
        }

        /* Draw column label */
        if (ctx->table.column_labels[i]) {
            float text_x = x + ctx->theme.padding;
            float text_y = y + (header_height - aui_text_height(ctx)) * 0.5f;
            AUI_Rect text_bounds = {text_x, text_y, col_width - ctx->theme.padding * 2, header_height};
            aui_draw_text_clipped(ctx, ctx->table.column_labels[i], text_bounds, ctx->theme.text);
        }

        /* Draw sort indicator */
        if (sortable && ctx->table.sort_spec.column_index == i) {
            float arrow_x = x + col_width - ctx->theme.padding - 8;
            float arrow_y = y + header_height * 0.5f;
            float arrow_size = 5.0f;

            if (ctx->table.sort_spec.descending) {
                /* Down arrow */
                aui_draw_triangle(ctx,
                    arrow_x, arrow_y - arrow_size,
                    arrow_x + arrow_size, arrow_y + arrow_size,
                    arrow_x - arrow_size, arrow_y + arrow_size,
                    ctx->theme.accent);
            } else {
                /* Up arrow */
                aui_draw_triangle(ctx,
                    arrow_x - arrow_size, arrow_y + arrow_size,
                    arrow_x + arrow_size, arrow_y + arrow_size,
                    arrow_x, arrow_y - arrow_size,
                    ctx->theme.accent);
            }
        }

        /* Draw resize handle */
        if (ctx->table.flags & AUI_TABLE_RESIZABLE &&
            !(ctx->table.column_flags[i] & AUI_TABLE_COLUMN_NO_RESIZE)) {
            float handle_x = x + col_width - 2;
            AUI_Rect handle_rect = {handle_x - 2, y, 4, header_height};
            bool handle_hovered = aui_rect_contains(handle_rect, ctx->input.mouse_x, ctx->input.mouse_y);

            if (handle_hovered) {
                aui_draw_line(ctx, handle_x, y + 4, handle_x, y + header_height - 4,
                              ctx->theme.accent, 2.0f);
            }

            /* Handle resize dragging */
            AUI_Id resize_id = ctx->table.id + 100 + i;
            if (handle_hovered && ctx->input.mouse_pressed[0]) {
                ctx->active = resize_id;
            }

            if (ctx->active == resize_id) {
                if (ctx->input.mouse_down[0]) {
                    float delta = ctx->input.mouse_x - ctx->input.mouse_prev_x;
                    float new_width = ctx->table.column_widths[i] + delta;
                    if (new_width >= 30.0f) {
                        ctx->table.column_widths[i] = new_width;
                    }
                } else {
                    ctx->active = AUI_ID_NONE;
                }
            }
        }

        /* Draw column separator */
        if (ctx->table.flags & AUI_TABLE_BORDERS) {
            aui_draw_line(ctx, x + col_width, y, x + col_width, y + header_height,
                          ctx->theme.border, 1.0f);
        }

        x += col_width;
    }

    /* Draw bottom border of header */
    if (ctx->table.flags & AUI_TABLE_BORDERS) {
        aui_draw_line(ctx, ctx->table.bounds.x, y + header_height,
                      ctx->table.bounds.x + ctx->table.bounds.w, y + header_height,
                      ctx->theme.border, 1.0f);
    }

    ctx->table.current_row = -1;  /* Reset for data rows */
}

void aui_table_next_row(AUI_Context *ctx)
{
    if (!ctx || ctx->table.id == AUI_ID_NONE) return;

    ctx->table.current_row++;
    ctx->table.current_column = -1;

    /* Calculate row Y position (account for header) */
    float row_y = ctx->table.bounds.y + ctx->table.row_height +
                  ctx->table.current_row * ctx->table.row_height - ctx->table.scroll_y;

    /* Skip if row is outside visible area */
    if (row_y + ctx->table.row_height < ctx->table.bounds.y ||
        row_y > ctx->table.bounds.y + ctx->table.bounds.h) {
        return;
    }

    /* Draw row highlight on hover */
    if (ctx->table.flags & AUI_TABLE_ROW_HIGHLIGHT) {
        AUI_Rect row_rect = {
            ctx->table.bounds.x,
            row_y,
            ctx->table.bounds.w,
            ctx->table.row_height
        };

        if (aui_rect_contains(row_rect, ctx->input.mouse_x, ctx->input.mouse_y)) {
            aui_draw_rect(ctx, row_rect.x, row_rect.y, row_rect.w, row_rect.h,
                          ctx->theme.bg_widget_hover);
        }
    }

    /* Draw alternating row background */
    if (ctx->table.current_row % 2 == 1) {
        aui_draw_rect(ctx, ctx->table.bounds.x, row_y,
                      ctx->table.bounds.w, ctx->table.row_height,
                      aui_color_alpha(ctx->theme.bg_widget, 0.3f));
    }

    /* Draw row bottom border */
    if (ctx->table.flags & AUI_TABLE_BORDERS) {
        aui_draw_line(ctx, ctx->table.bounds.x, row_y + ctx->table.row_height,
                      ctx->table.bounds.x + ctx->table.bounds.w, row_y + ctx->table.row_height,
                      aui_color_alpha(ctx->theme.border, 0.5f), 1.0f);
    }
}

bool aui_table_next_column(AUI_Context *ctx)
{
    if (!ctx || ctx->table.id == AUI_ID_NONE) return false;

    ctx->table.current_column++;
    if (ctx->table.current_column >= ctx->table.column_count) {
        return false;
    }

    return true;
}

bool aui_table_set_column(AUI_Context *ctx, int column)
{
    if (!ctx || ctx->table.id == AUI_ID_NONE) return false;
    if (column < 0 || column >= ctx->table.column_count) return false;

    ctx->table.current_column = column;
    return true;
}

AUI_TableSortSpec *aui_table_get_sort_specs(AUI_Context *ctx, int *count)
{
    if (!ctx || ctx->table.id == AUI_ID_NONE) {
        if (count) *count = 0;
        return NULL;
    }

    if (ctx->table.sort_spec.column_index < 0) {
        if (count) *count = 0;
        return NULL;
    }

    if (count) *count = 1;
    return &ctx->table.sort_spec;
}

bool aui_table_sort_specs_changed(AUI_Context *ctx)
{
    if (!ctx || ctx->table.id == AUI_ID_NONE) return false;
    return ctx->table.sort_specs_changed;
}

void aui_end_table(AUI_Context *ctx)
{
    if (!ctx || ctx->table.id == AUI_ID_NONE) return;

    /* Save persistent state */
    AUI_TablePersist *persist = aui_table_get_persist(ctx, ctx->table.id);
    if (persist) {
        for (int i = 0; i < ctx->table.column_count && i < AUI_TABLE_MAX_COLUMNS; i++) {
            persist->column_widths[i] = ctx->table.column_widths[i];
        }
        persist->scroll_x = ctx->table.scroll_x;
        persist->scroll_y = ctx->table.scroll_y;
        persist->sort_spec = ctx->table.sort_spec;
        persist->initialized = true;
    }

    /* Pop scissor */
    aui_pop_scissor(ctx);

    /* Free temporary data */
    free(ctx->table.column_widths);
    free(ctx->table.column_labels);
    free(ctx->table.column_flags);

    /* Update layout cursor */
    if (ctx->layout_depth > 0) {
        AUI_LayoutFrame *layout = &ctx->layout_stack[ctx->layout_depth - 1];
        if (layout->horizontal) {
            layout->cursor_x += ctx->table.bounds.w + layout->spacing;
        } else {
            layout->cursor_y += ctx->table.bounds.h + layout->spacing;
        }
    }

    /* Reset table state */
    ctx->table.id = AUI_ID_NONE;
    ctx->table.column_widths = NULL;
    ctx->table.column_labels = NULL;
    ctx->table.column_flags = NULL;
}

/* Helper to get current cell rect */
AUI_Rect aui_table_get_cell_rect(AUI_Context *ctx)
{
    if (!ctx || ctx->table.id == AUI_ID_NONE) {
        return (AUI_Rect){0, 0, 0, 0};
    }

    /* Calculate X position */
    float x = ctx->table.bounds.x + ctx->theme.padding - ctx->table.scroll_x;
    for (int i = 0; i < ctx->table.current_column; i++) {
        x += ctx->table.column_widths[i];
    }

    /* Calculate Y position */
    float y = ctx->table.bounds.y + ctx->table.row_height +
              ctx->table.current_row * ctx->table.row_height - ctx->table.scroll_y;

    return (AUI_Rect){
        x + ctx->theme.padding,
        y,
        ctx->table.column_widths[ctx->table.current_column] - ctx->theme.padding * 2,
        ctx->table.row_height
    };
}
