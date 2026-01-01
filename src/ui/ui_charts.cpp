/*
 * Agentite UI - Chart Widgets Implementation
 */

#include "agentite/ui_charts.h"
#include "agentite/ui.h"
#include "agentite/ui_node.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Default Color Palette
 * ============================================================================ */

const uint32_t AUI_CHART_PALETTE[12] = {
    0xFF4285F4,  /* Blue */
    0xFF34A853,  /* Green */
    0xFFFBBC04,  /* Yellow */
    0xFFEA4335,  /* Red */
    0xFF9334E6,  /* Purple */
    0xFFFF6D01,  /* Orange */
    0xFF46BDC6,  /* Teal */
    0xFFE91E8C,  /* Pink */
    0xFF4E342E,  /* Brown */
    0xFF7B1FA2,  /* Deep Purple */
    0xFF0097A7,  /* Cyan */
    0xFF689F38,  /* Light Green */
};

uint32_t aui_chart_series_color(int index)
{
    return AUI_CHART_PALETTE[index % 12];
}

/* ============================================================================
 * Axis Calculation
 * ============================================================================ */

void aui_chart_nice_axis(float data_min, float data_max,
                          float *axis_min, float *axis_max,
                          float *tick_step, int *tick_count)
{
    float range = data_max - data_min;
    if (range <= 0) range = 1;

    /* Find nice step size */
    float rough_step = range / 5;  /* Aim for ~5 ticks */
    float exponent = floorf(log10f(rough_step));
    float magnitude = powf(10, exponent);

    /* Round to nice values: 1, 2, 2.5, 5, 10 */
    float normalized = rough_step / magnitude;
    float nice;
    if (normalized < 1.5f) nice = 1;
    else if (normalized < 3) nice = 2;
    else if (normalized < 7) nice = 5;
    else nice = 10;

    *tick_step = nice * magnitude;

    /* Round axis bounds to tick values */
    *axis_min = floorf(data_min / *tick_step) * *tick_step;
    *axis_max = ceilf(data_max / *tick_step) * *tick_step;

    /* Calculate tick count */
    *tick_count = (int)((*axis_max - *axis_min) / *tick_step) + 1;
}

const char *aui_chart_format_value(float value, const char *format)
{
    static char buffer[32];

    if (format) {
        snprintf(buffer, sizeof(buffer), format, value);
    } else if (fabsf(value) >= 1000000) {
        snprintf(buffer, sizeof(buffer), "%.1fM", value / 1000000);
    } else if (fabsf(value) >= 1000) {
        snprintf(buffer, sizeof(buffer), "%.1fK", value / 1000);
    } else if (fabsf(value) < 1 && value != 0) {
        snprintf(buffer, sizeof(buffer), "%.2f", value);
    } else {
        snprintf(buffer, sizeof(buffer), "%.0f", value);
    }

    return buffer;
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void aui_chart_compute_bounds(AUI_Context *ctx, AUI_Rect bounds,
                                      const AUI_ChartConfig *config,
                                      AUI_ChartState *state)
{
    float padding = 10;
    float title_height = config->title ? 24 : 0;
    float x_label_height = config->x_axis_label ? 20 : 0;
    float y_label_width = config->y_axis_label ? 20 : 0;
    float legend_size = 0;

    if (config->show_legend && config->legend_position != AUI_LEGEND_NONE) {
        legend_size = 80;  /* Approximate */
    }

    /* Calculate plot area */
    state->plot_area.x = bounds.x + padding + y_label_width + 40;  /* Y-axis ticks */
    state->plot_area.y = bounds.y + padding + title_height;
    state->plot_area.w = bounds.w - 2 * padding - y_label_width - 40;
    state->plot_area.h = bounds.h - 2 * padding - title_height - x_label_height - 20;

    if (config->legend_position == AUI_LEGEND_RIGHT) {
        state->plot_area.w -= legend_size;
        state->legend_area.x = state->plot_area.x + state->plot_area.w + 10;
        state->legend_area.y = state->plot_area.y;
        state->legend_area.w = legend_size - 10;
        state->legend_area.h = state->plot_area.h;
    } else if (config->legend_position == AUI_LEGEND_BOTTOM) {
        state->plot_area.h -= legend_size;
        state->legend_area.x = state->plot_area.x;
        state->legend_area.y = state->plot_area.y + state->plot_area.h + 10;
        state->legend_area.w = state->plot_area.w;
        state->legend_area.h = legend_size - 10;
    }
    (void)ctx;
}

static void aui_chart_find_data_range(const AUI_ChartConfig *config,
                                       float *data_min, float *data_max)
{
    *data_min = 0;
    *data_max = 100;

    if (config->y_min != 0 || config->y_max != 0) {
        *data_min = config->y_min;
        *data_max = config->y_max;
        return;
    }

    bool first = true;
    for (int s = 0; s < config->series_count; s++) {
        const AUI_ChartSeries *series = &config->series[s];
        for (int i = 0; i < series->value_count; i++) {
            float v = series->values[i];
            if (first) {
                *data_min = v;
                *data_max = v;
                first = false;
            } else {
                if (v < *data_min) *data_min = v;
                if (v > *data_max) *data_max = v;
            }
        }
    }

    /* Ensure min is at 0 for non-negative data */
    if (*data_min > 0) *data_min = 0;
}

static void aui_chart_draw_grid(AUI_Context *ctx, AUI_ChartState *state,
                                 const AUI_ChartConfig *config,
                                 float axis_min, float axis_max, float tick_step)
{
    uint32_t grid_color = config->grid_color ? config->grid_color : 0x20FFFFFF;
    uint32_t axis_color = config->axis_color ? config->axis_color : 0xFFFFFFFF;
    uint32_t text_color = config->text_color ? config->text_color : 0xFFFFFFFF;

    AUI_Rect plot = state->plot_area;

    /* Draw axes */
    aui_draw_rect(ctx, plot.x, plot.y, 1, plot.h, axis_color);  /* Y axis */
    aui_draw_rect(ctx, plot.x, plot.y + plot.h, plot.w, 1, axis_color);  /* X axis */

    /* Draw Y-axis grid lines and labels */
    float y_range = axis_max - axis_min;
    if (y_range <= 0) y_range = 1;

    for (float v = axis_min; v <= axis_max + tick_step * 0.1f; v += tick_step) {
        float y = plot.y + plot.h - ((v - axis_min) / y_range) * plot.h;

        if (config->show_grid && v > axis_min) {
            aui_draw_rect(ctx, plot.x, y, plot.w, 1, grid_color);
        }

        /* Draw tick */
        aui_draw_rect(ctx, plot.x - 4, y, 4, 1, axis_color);

        /* Draw label */
        const char *label = aui_chart_format_value(v, NULL);
        float tw = aui_text_width(ctx, label);
        aui_draw_text(ctx, label, plot.x - tw - 8, y - 6, text_color);
    }

    /* Draw X-axis labels */
    if (config->x_labels && config->x_label_count > 0) {
        int label_count = config->x_label_count;
        float slot_w = plot.w / label_count;

        for (int i = 0; i < label_count; i++) {
            float x = plot.x + slot_w * i + slot_w / 2;
            float tw = aui_text_width(ctx, config->x_labels[i]);
            aui_draw_text(ctx, config->x_labels[i],
                          x - tw / 2, plot.y + plot.h + 8, text_color);
        }
    }

    /* Draw title */
    if (config->title) {
        float tw = aui_text_width(ctx, config->title);
        aui_draw_text(ctx, config->title,
                      plot.x + (plot.w - tw) / 2, plot.y - 20, text_color);
    }

    /* Store scale for data points */
    state->y_scale = plot.h / y_range;
    state->y_offset = axis_min;
}

static void aui_chart_draw_legend(AUI_Context *ctx, AUI_ChartState *state,
                                   const AUI_ChartConfig *config)
{
    if (!config->show_legend || config->legend_position == AUI_LEGEND_NONE) return;

    AUI_Rect legend = state->legend_area;
    uint32_t text_color = config->text_color ? config->text_color : 0xFFFFFFFF;
    float line_h = 20;
    float y = legend.y;

    for (int s = 0; s < config->series_count; s++) {
        const AUI_ChartSeries *series = &config->series[s];
        uint32_t color = series->color ? series->color : aui_chart_series_color(s);

        /* Color box */
        aui_draw_rect(ctx, legend.x, y + 4, 12, 12, color);

        /* Label */
        if (series->label) {
            aui_draw_text(ctx, series->label, legend.x + 18, y + 2, text_color);
        }

        y += line_h;
    }

    /* Pie chart legend */
    for (int i = 0; i < config->slice_count; i++) {
        const AUI_PieSlice *slice = &config->slices[i];
        uint32_t color = slice->color ? slice->color : aui_chart_series_color(i);

        aui_draw_rect(ctx, legend.x, y + 4, 12, 12, color);

        if (slice->label) {
            aui_draw_text(ctx, slice->label, legend.x + 18, y + 2, text_color);
        }

        y += line_h;
    }
}

/* ============================================================================
 * Line Chart
 * ============================================================================ */

void aui_draw_line_chart(AUI_Context *ctx, AUI_Rect bounds,
                          const AUI_ChartConfig *config)
{
    AUI_ChartState state = {0};
    state.anim_progress = 1.0f;
    aui_draw_chart_ex(ctx, bounds, config, &state);
}

static void aui_chart_draw_line_internal(AUI_Context *ctx, AUI_ChartState *state,
                                          const AUI_ChartConfig *config)
{
    AUI_Rect plot = state->plot_area;

    for (int s = 0; s < config->series_count; s++) {
        const AUI_ChartSeries *series = &config->series[s];
        if (series->value_count < 1) continue;

        uint32_t color = series->color ? series->color : aui_chart_series_color(s);
        float line_w = series->line_width > 0 ? series->line_width : 2;
        float point_r = series->point_size > 0 ? series->point_size : 4;

        int n = series->value_count;
        float slot_w = plot.w / (n > 1 ? n - 1 : 1);

        /* Animation */
        int visible_count = (int)(n * state->anim_progress);
        if (visible_count < 1) visible_count = 1;

        /* Draw filled area */
        if (series->filled) {
            uint32_t fill_color = color & 0x00FFFFFF;
            uint8_t fill_alpha = (uint8_t)(series->fill_opacity * 255);
            fill_color |= (fill_alpha << 24);

            for (int i = 0; i < visible_count - 1; i++) {
                float x1 = plot.x + slot_w * i;
                float x2 = plot.x + slot_w * (i + 1);
                float y1 = plot.y + plot.h - (series->values[i] - state->y_offset) * state->y_scale;
                float y2 = plot.y + plot.h - (series->values[i + 1] - state->y_offset) * state->y_scale;
                float base_y = plot.y + plot.h;

                /* Draw as two triangles (simplified quad) */
                aui_draw_rect(ctx, x1, fminf(y1, y2), x2 - x1, base_y - fminf(y1, y2), fill_color);
            }
        }

        /* Draw lines */
        for (int i = 0; i < visible_count - 1; i++) {
            float x1 = plot.x + slot_w * i;
            float x2 = plot.x + slot_w * (i + 1);
            float y1 = plot.y + plot.h - (series->values[i] - state->y_offset) * state->y_scale;
            float y2 = plot.y + plot.h - (series->values[i + 1] - state->y_offset) * state->y_scale;

            aui_draw_line(ctx, x1, y1, x2, y2, color, line_w);
        }

        /* Draw points */
        if (series->show_points || point_r > 0) {
            for (int i = 0; i < visible_count; i++) {
                float x = plot.x + slot_w * i;
                float y = plot.y + plot.h - (series->values[i] - state->y_offset) * state->y_scale;

                /* Highlight hovered point */
                bool hovered = (state->hovered_series == s && state->hovered_index == i);
                float r = hovered ? point_r * 1.5f : point_r;
                uint32_t c = hovered ? 0xFFFFFFFF : color;

                aui_draw_rect(ctx, x - r / 2, y - r / 2, r, r, c);
            }
        }
    }
}

/* ============================================================================
 * Bar Chart
 * ============================================================================ */

void aui_draw_bar_chart(AUI_Context *ctx, AUI_Rect bounds,
                         const AUI_ChartConfig *config)
{
    AUI_ChartState state = {0};
    state.anim_progress = 1.0f;
    aui_draw_chart_ex(ctx, bounds, config, &state);
}

static void aui_chart_draw_bar_internal(AUI_Context *ctx, AUI_ChartState *state,
                                         const AUI_ChartConfig *config)
{
    AUI_Rect plot = state->plot_area;

    /* Find max value count across series */
    int max_count = 0;
    for (int s = 0; s < config->series_count; s++) {
        if (config->series[s].value_count > max_count) {
            max_count = config->series[s].value_count;
        }
    }
    if (max_count == 0) return;

    float slot_w = plot.w / max_count;
    float bar_w_ratio = config->bar_width > 0 ? config->bar_width : 0.8f;
    float group_w = slot_w * bar_w_ratio;
    float bar_w = group_w / config->series_count;
    float bar_spacing = config->bar_spacing > 0 ? config->bar_spacing : 2;

    for (int i = 0; i < max_count; i++) {
        float group_x = plot.x + slot_w * i + (slot_w - group_w) / 2;

        for (int s = 0; s < config->series_count; s++) {
            const AUI_ChartSeries *series = &config->series[s];
            if (i >= series->value_count) continue;

            uint32_t color = series->color ? series->color : aui_chart_series_color(s);
            float value = series->values[i];

            /* Animation */
            float anim_value = value * state->anim_progress;

            float bar_h = (anim_value - state->y_offset) * state->y_scale;
            if (bar_h < 0) bar_h = 0;

            float x = group_x + (bar_w + bar_spacing) * s;
            float y = plot.y + plot.h - bar_h;
            float w = bar_w - bar_spacing;
            if (w < 1) w = 1;

            /* Highlight hovered bar */
            bool hovered = (state->hovered_series == s && state->hovered_index == i);
            if (hovered) {
                color = aui_color_brighten(color, 0.2f);
            }

            aui_draw_rect(ctx, x, y, w, bar_h, color);

            /* Value label */
            if (config->show_values) {
                const char *label = aui_chart_format_value(value, NULL);
                float tw = aui_text_width(ctx, label);
                uint32_t text_color = config->text_color ? config->text_color : 0xFFFFFFFF;
                aui_draw_text(ctx, label, x + w / 2 - tw / 2, y - 14, text_color);
            }
        }
    }
}

/* ============================================================================
 * Pie Chart
 * ============================================================================ */

void aui_draw_pie_chart(AUI_Context *ctx, AUI_Rect bounds,
                         const AUI_ChartConfig *config)
{
    AUI_ChartState state = {0};
    state.anim_progress = 1.0f;
    aui_draw_chart_ex(ctx, bounds, config, &state);
}

static void aui_chart_draw_pie_internal(AUI_Context *ctx, AUI_ChartState *state,
                                         const AUI_ChartConfig *config)
{
    AUI_Rect plot = state->plot_area;
    float cx = plot.x + plot.w / 2;
    float cy = plot.y + plot.h / 2;
    float radius = fminf(plot.w, plot.h) / 2 - 10;
    float inner_radius = config->donut_inner_radius * radius;

    /* Calculate total */
    float total = 0;
    for (int i = 0; i < config->slice_count; i++) {
        total += config->slices[i].value;
    }
    if (total <= 0) return;

    /* Draw slices */
    float start_angle = config->start_angle * (float)M_PI / 180.0f;
    float angle = start_angle;

    for (int i = 0; i < config->slice_count; i++) {
        const AUI_PieSlice *slice = &config->slices[i];
        float sweep = (slice->value / total) * 2 * (float)M_PI;

        /* Animation */
        sweep *= state->anim_progress;

        uint32_t color = slice->color ? slice->color : aui_chart_series_color(i);

        /* Explode offset */
        float offset = 0;
        if (slice->exploded) {
            offset = slice->explode_distance > 0 ? slice->explode_distance : 15;
        }

        float mid_angle = angle + sweep / 2;
        float offset_x = cosf(mid_angle) * offset;
        float offset_y = sinf(mid_angle) * offset;

        /* Draw arc as segments */
        int segments = (int)(sweep * 20) + 1;
        if (segments < 4) segments = 4;

        for (int j = 0; j < segments; j++) {
            float a1 = angle + sweep * j / segments;
            float a2 = angle + sweep * (j + 1) / segments;

            float x1 = cx + offset_x + cosf(a1) * radius;
            float y1 = cy + offset_y + sinf(a1) * radius;
            float x2 = cx + offset_x + cosf(a2) * radius;
            float y2 = cy + offset_y + sinf(a2) * radius;

            /* Draw triangle from center to arc edge */
            if (inner_radius > 0) {
                /* Donut - draw quad */
                float ix1 = cx + offset_x + cosf(a1) * inner_radius;
                float iy1 = cy + offset_y + sinf(a1) * inner_radius;
                float ix2 = cx + offset_x + cosf(a2) * inner_radius;
                float iy2 = cy + offset_y + sinf(a2) * inner_radius;

                aui_draw_line(ctx, x1, y1, x2, y2, color, 1);
                aui_draw_line(ctx, ix1, iy1, ix2, iy2, color, 1);
            } else {
                /* Pie - fill from center */
                aui_draw_line(ctx, cx + offset_x, cy + offset_y, x1, y1, color, 1);
                aui_draw_line(ctx, x1, y1, x2, y2, color, 1);
            }
        }

        /* Draw percentage label */
        if (config->show_percentages && sweep > 0.1f) {
            float label_r = radius * 0.7f;
            float lx = cx + offset_x + cosf(mid_angle) * label_r;
            float ly = cy + offset_y + sinf(mid_angle) * label_r;

            char percent[16];
            snprintf(percent, sizeof(percent), "%.0f%%", slice->value / total * 100);

            float tw = aui_text_width(ctx, percent);
            uint32_t text_color = config->text_color ? config->text_color : 0xFFFFFFFF;
            aui_draw_text(ctx, percent, lx - tw / 2, ly - 6, text_color);
        }

        angle += sweep / state->anim_progress;  /* Unadjusted for next slice */
    }
}

/* ============================================================================
 * Main Chart Drawing
 * ============================================================================ */

void aui_draw_chart(AUI_Context *ctx, AUI_Rect bounds,
                     const AUI_ChartConfig *config)
{
    AUI_ChartState state = {0};
    state.anim_progress = 1.0f;
    aui_draw_chart_ex(ctx, bounds, config, &state);
}

void aui_draw_chart_ex(AUI_Context *ctx, AUI_Rect bounds,
                        const AUI_ChartConfig *config,
                        AUI_ChartState *state)
{
    if (!ctx || !config) return;

    /* Draw background */
    if (config->background_color) {
        aui_draw_rect(ctx, bounds.x, bounds.y, bounds.w, bounds.h,
                      config->background_color);
    }

    /* Compute layout */
    aui_chart_compute_bounds(ctx, bounds, config, state);

    /* Draw based on type */
    switch (config->type) {
        case AUI_CHART_LINE:
        case AUI_CHART_AREA: {
            float data_min, data_max;
            aui_chart_find_data_range(config, &data_min, &data_max);

            float axis_min, axis_max, tick_step;
            int tick_count;
            aui_chart_nice_axis(data_min, data_max, &axis_min, &axis_max,
                                 &tick_step, &tick_count);

            aui_chart_draw_grid(ctx, state, config, axis_min, axis_max, tick_step);
            aui_chart_draw_line_internal(ctx, state, config);
            break;
        }

        case AUI_CHART_BAR:
        case AUI_CHART_STACKED_BAR: {
            float data_min, data_max;
            aui_chart_find_data_range(config, &data_min, &data_max);

            float axis_min, axis_max, tick_step;
            int tick_count;
            aui_chart_nice_axis(data_min, data_max, &axis_min, &axis_max,
                                 &tick_step, &tick_count);

            aui_chart_draw_grid(ctx, state, config, axis_min, axis_max, tick_step);
            aui_chart_draw_bar_internal(ctx, state, config);
            break;
        }

        case AUI_CHART_PIE:
        case AUI_CHART_DONUT:
            aui_chart_draw_pie_internal(ctx, state, config);
            break;

        default:
            break;
    }

    /* Draw legend */
    aui_chart_draw_legend(ctx, state, config);

    /* Draw tooltip */
    if (state->tooltip_visible && state->hovered_series >= 0) {
        /* TODO: Draw tooltip at hover position */
    }
}

/* ============================================================================
 * Chart Node Widget
 * ============================================================================ */

AUI_Node *aui_chart_create(AUI_Context *ctx, const char *name,
                            const AUI_ChartConfig *config)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_CHART, name);
    if (!node) return NULL;

    AUI_ChartNodeData *data = (AUI_ChartNodeData *)calloc(1, sizeof(AUI_ChartNodeData));
    if (!data) {
        aui_node_destroy(node);
        return NULL;
    }

    node->custom_data = data;

    if (config) {
        data->config = *config;
        data->state.anim_progress = config->animated ? 0 : 1;
    }

    /* Set default size */
    node->custom_min_size_x = 200;
    node->custom_min_size_y = 150;

    return node;
}

void aui_chart_set_config(AUI_Node *chart, const AUI_ChartConfig *config)
{
    if (!chart || chart->type != AUI_NODE_CHART || !config) return;
    AUI_ChartNodeData *data = (AUI_ChartNodeData *)chart->custom_data;
    if (!data) return;

    data->config = *config;
    if (config->animated) {
        data->state.anim_progress = 0;
    }
}

void aui_chart_set_data(AUI_Node *chart, const AUI_ChartSeries *series,
                         int series_count)
{
    if (!chart || chart->type != AUI_NODE_CHART) return;
    AUI_ChartNodeData *data = (AUI_ChartNodeData *)chart->custom_data;
    if (!data) return;

    data->config.series = series;
    data->config.series_count = series_count;

    if (data->config.animated) {
        data->state.anim_progress = 0;
    }
}

void aui_chart_set_pie_data(AUI_Node *chart, const AUI_PieSlice *slices,
                             int slice_count)
{
    if (!chart || chart->type != AUI_NODE_CHART) return;
    AUI_ChartNodeData *data = (AUI_ChartNodeData *)chart->custom_data;
    if (!data) return;

    data->config.slices = slices;
    data->config.slice_count = slice_count;

    if (data->config.animated) {
        data->state.anim_progress = 0;
    }
}

void aui_chart_add_series(AUI_Node *chart, const AUI_ChartSeries *series)
{
    if (!chart || chart->type != AUI_NODE_CHART || !series) return;
    AUI_ChartNodeData *data = (AUI_ChartNodeData *)chart->custom_data;
    if (!data) return;

    /* Grow storage if needed */
    if (data->config.series_count >= data->series_capacity) {
        int new_cap = data->series_capacity == 0 ? 4 : data->series_capacity * 2;
        AUI_ChartSeries *new_storage = (AUI_ChartSeries *)realloc(
            data->series_storage, new_cap * sizeof(AUI_ChartSeries));
        if (!new_storage) return;
        data->series_storage = new_storage;
        data->series_capacity = new_cap;
    }

    data->series_storage[data->config.series_count++] = *series;
    data->config.series = data->series_storage;
}

void aui_chart_update_series(AUI_Node *chart, int series_index,
                              const float *values, int count)
{
    if (!chart || chart->type != AUI_NODE_CHART) return;
    AUI_ChartNodeData *data = (AUI_ChartNodeData *)chart->custom_data;
    if (!data || series_index < 0 || series_index >= data->config.series_count) return;

    /* This requires mutable series data - only works with storage */
    if (!data->series_storage) return;

    data->series_storage[series_index].values = values;
    data->series_storage[series_index].value_count = count;
}

void aui_chart_clear(AUI_Node *chart)
{
    if (!chart || chart->type != AUI_NODE_CHART) return;
    AUI_ChartNodeData *data = (AUI_ChartNodeData *)chart->custom_data;
    if (!data) return;

    data->config.series = NULL;
    data->config.series_count = 0;
    data->config.slices = NULL;
    data->config.slice_count = 0;
}

void aui_chart_set_animated(AUI_Node *chart, bool animated)
{
    if (!chart || chart->type != AUI_NODE_CHART) return;
    AUI_ChartNodeData *data = (AUI_ChartNodeData *)chart->custom_data;
    if (!data) return;

    data->config.animated = animated;
    if (animated) {
        data->state.anim_progress = 0;
    } else {
        data->state.anim_progress = 1;
    }
}

void aui_chart_restart_animation(AUI_Node *chart)
{
    if (!chart || chart->type != AUI_NODE_CHART) return;
    AUI_ChartNodeData *data = (AUI_ChartNodeData *)chart->custom_data;
    if (!data) return;

    data->state.anim_progress = 0;
}

bool aui_chart_get_hover(AUI_Node *chart, int *series, int *index, float *value)
{
    if (!chart || chart->type != AUI_NODE_CHART) return false;
    AUI_ChartNodeData *data = (AUI_ChartNodeData *)chart->custom_data;
    if (!data) return false;

    if (data->state.hovered_series < 0) return false;

    if (series) *series = data->state.hovered_series;
    if (index) *index = data->state.hovered_index;
    if (value && data->state.hovered_series < data->config.series_count) {
        const AUI_ChartSeries *s = &data->config.series[data->state.hovered_series];
        if (data->state.hovered_index < s->value_count) {
            *value = s->values[data->state.hovered_index];
        }
    }

    return true;
}
