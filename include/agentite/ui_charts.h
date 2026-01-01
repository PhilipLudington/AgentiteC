/*
 * Agentite UI - Chart Widgets
 *
 * Line, bar, and pie charts for data visualization.
 *
 * Usage:
 *   // Line chart
 *   float data[] = {10, 25, 15, 30, 45, 20};
 *   AUI_ChartSeries series = {
 *       .label = "Sales",
 *       .values = data,
 *       .value_count = 6,
 *       .color = 0xFF00FF00,
 *   };
 *   AUI_ChartConfig cfg = {
 *       .type = AUI_CHART_LINE,
 *       .title = "Monthly Sales",
 *       .series = &series,
 *       .series_count = 1,
 *       .show_grid = true,
 *   };
 *   aui_draw_line_chart(ctx, bounds, &cfg);
 */

#ifndef AGENTITE_UI_CHARTS_H
#define AGENTITE_UI_CHARTS_H

#include "agentite/ui.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct AUI_Node AUI_Node;

/* ============================================================================
 * Chart Types
 * ============================================================================ */

typedef enum AUI_ChartType {
    AUI_CHART_LINE,
    AUI_CHART_BAR,
    AUI_CHART_STACKED_BAR,
    AUI_CHART_PIE,
    AUI_CHART_DONUT,
    AUI_CHART_AREA,
    AUI_CHART_SCATTER,
} AUI_ChartType;

/* ============================================================================
 * Legend Position
 * ============================================================================ */

typedef enum AUI_LegendPosition {
    AUI_LEGEND_NONE,
    AUI_LEGEND_TOP,
    AUI_LEGEND_BOTTOM,
    AUI_LEGEND_LEFT,
    AUI_LEGEND_RIGHT,
} AUI_LegendPosition;

/* ============================================================================
 * Chart Data Series
 * ============================================================================ */

typedef struct AUI_ChartSeries {
    const char *label;
    const float *values;
    int value_count;
    uint32_t color;

    /* Line chart options */
    float line_width;
    bool show_points;
    float point_size;
    bool smooth;               /* Bezier smoothing */
    bool filled;               /* Fill area under line */
    float fill_opacity;

    /* Bar chart options */
    float bar_width_ratio;     /* 0-1, relative to slot */
} AUI_ChartSeries;

/* ============================================================================
 * Pie/Donut Chart Slice
 * ============================================================================ */

typedef struct AUI_PieSlice {
    const char *label;
    float value;
    uint32_t color;
    bool exploded;             /* Offset from center */
    float explode_distance;
} AUI_PieSlice;

/* ============================================================================
 * Chart Configuration
 * ============================================================================ */

typedef struct AUI_ChartConfig {
    AUI_ChartType type;
    const char *title;

    /* Axes */
    const char *x_axis_label;
    const char *y_axis_label;
    const char **x_labels;     /* Category labels for x-axis */
    int x_label_count;

    /* Y-axis range (0,0 = auto) */
    float y_min;
    float y_max;
    int y_divisions;           /* Number of grid lines */
    bool y_log_scale;

    /* Appearance */
    bool show_grid;
    bool show_legend;
    AUI_LegendPosition legend_position;
    bool show_values;          /* Show value labels on data points */
    bool show_tooltips;        /* Show hover tooltips */

    /* Colors */
    uint32_t background_color;
    uint32_t grid_color;
    uint32_t axis_color;
    uint32_t text_color;

    /* Bar chart options */
    float bar_width;           /* 0-1, relative to slot */
    float bar_spacing;         /* Space between bars in group */
    bool horizontal_bars;

    /* Pie/donut options */
    float donut_inner_radius;  /* 0 = pie, >0 = donut */
    float start_angle;         /* Starting angle in degrees */
    bool show_percentages;

    /* Animation */
    bool animated;
    float animation_duration;

    /* Data (for line/bar charts) */
    const AUI_ChartSeries *series;
    int series_count;

    /* Data (for pie charts) */
    const AUI_PieSlice *slices;
    int slice_count;
} AUI_ChartConfig;

/* ============================================================================
 * Chart State (for animation and interaction)
 * ============================================================================ */

typedef struct AUI_ChartState {
    float anim_progress;       /* 0-1 for entry animation */
    int hovered_series;        /* -1 = none */
    int hovered_index;         /* Data point index */
    float hover_x, hover_y;    /* Mouse position */
    bool tooltip_visible;

    /* Computed bounds */
    AUI_Rect plot_area;        /* Area for the actual chart */
    AUI_Rect legend_area;
    float y_scale;             /* Pixels per unit */
    float y_offset;            /* Pixel offset for y_min */
} AUI_ChartState;

/* ============================================================================
 * Chart Node Data (internal storage for chart nodes)
 * ============================================================================ */

typedef struct AUI_ChartNodeData {
    AUI_ChartConfig config;
    AUI_ChartState state;
    AUI_ChartSeries *series_storage;
    int series_capacity;
    AUI_PieSlice *slice_storage;
    int slice_capacity;
    float *value_storage;
    int value_capacity;
} AUI_ChartNodeData;

/* ============================================================================
 * Immediate Mode Chart Drawing
 * ============================================================================ */

/* Draw a line chart */
void aui_draw_line_chart(AUI_Context *ctx, AUI_Rect bounds,
                          const AUI_ChartConfig *config);

/* Draw a bar chart */
void aui_draw_bar_chart(AUI_Context *ctx, AUI_Rect bounds,
                         const AUI_ChartConfig *config);

/* Draw a pie chart */
void aui_draw_pie_chart(AUI_Context *ctx, AUI_Rect bounds,
                         const AUI_ChartConfig *config);

/* Draw any chart type (dispatches based on config.type) */
void aui_draw_chart(AUI_Context *ctx, AUI_Rect bounds,
                     const AUI_ChartConfig *config);

/* Draw with state for animation/interaction */
void aui_draw_chart_ex(AUI_Context *ctx, AUI_Rect bounds,
                        const AUI_ChartConfig *config,
                        AUI_ChartState *state);

/* ============================================================================
 * Chart Node Widget
 * ============================================================================ */

/* Create a chart node */
AUI_Node *aui_chart_create(AUI_Context *ctx, const char *name,
                            const AUI_ChartConfig *config);

/* Update chart configuration */
void aui_chart_set_config(AUI_Node *chart, const AUI_ChartConfig *config);

/* Update just the data (keeps other settings) */
void aui_chart_set_data(AUI_Node *chart, const AUI_ChartSeries *series,
                         int series_count);

/* Update pie data */
void aui_chart_set_pie_data(AUI_Node *chart, const AUI_PieSlice *slices,
                             int slice_count);

/* Add a series dynamically */
void aui_chart_add_series(AUI_Node *chart, const AUI_ChartSeries *series);

/* Update a single series */
void aui_chart_update_series(AUI_Node *chart, int series_index,
                              const float *values, int count);

/* Clear all data */
void aui_chart_clear(AUI_Node *chart);

/* Animation control */
void aui_chart_set_animated(AUI_Node *chart, bool animated);
void aui_chart_restart_animation(AUI_Node *chart);

/* Get hovered data point */
bool aui_chart_get_hover(AUI_Node *chart, int *series, int *index,
                          float *value);

/* ============================================================================
 * Chart Utilities
 * ============================================================================ */

/* Calculate nice axis tick values */
void aui_chart_nice_axis(float data_min, float data_max,
                          float *axis_min, float *axis_max,
                          float *tick_step, int *tick_count);

/* Format a value for display */
const char *aui_chart_format_value(float value, const char *format);

/* Generate a color for series index */
uint32_t aui_chart_series_color(int index);

/* Default chart colors palette */
extern const uint32_t AUI_CHART_PALETTE[12];

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_UI_CHARTS_H */
