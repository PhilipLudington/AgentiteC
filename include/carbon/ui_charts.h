/*
 * Carbon UI - Chart Widgets
 *
 * Line, bar, and pie charts for data visualization.
 *
 * Usage:
 *   // Line chart
 *   float data[] = {10, 25, 15, 30, 45, 20};
 *   CUI_ChartSeries series = {
 *       .label = "Sales",
 *       .values = data,
 *       .value_count = 6,
 *       .color = 0xFF00FF00,
 *   };
 *   CUI_ChartConfig cfg = {
 *       .type = CUI_CHART_LINE,
 *       .title = "Monthly Sales",
 *       .series = &series,
 *       .series_count = 1,
 *       .show_grid = true,
 *   };
 *   cui_draw_line_chart(ctx, bounds, &cfg);
 */

#ifndef CARBON_UI_CHARTS_H
#define CARBON_UI_CHARTS_H

#include "carbon/ui.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct CUI_Node CUI_Node;

/* ============================================================================
 * Chart Types
 * ============================================================================ */

typedef enum CUI_ChartType {
    CUI_CHART_LINE,
    CUI_CHART_BAR,
    CUI_CHART_STACKED_BAR,
    CUI_CHART_PIE,
    CUI_CHART_DONUT,
    CUI_CHART_AREA,
    CUI_CHART_SCATTER,
} CUI_ChartType;

/* ============================================================================
 * Legend Position
 * ============================================================================ */

typedef enum CUI_LegendPosition {
    CUI_LEGEND_NONE,
    CUI_LEGEND_TOP,
    CUI_LEGEND_BOTTOM,
    CUI_LEGEND_LEFT,
    CUI_LEGEND_RIGHT,
} CUI_LegendPosition;

/* ============================================================================
 * Chart Data Series
 * ============================================================================ */

typedef struct CUI_ChartSeries {
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
} CUI_ChartSeries;

/* ============================================================================
 * Pie/Donut Chart Slice
 * ============================================================================ */

typedef struct CUI_PieSlice {
    const char *label;
    float value;
    uint32_t color;
    bool exploded;             /* Offset from center */
    float explode_distance;
} CUI_PieSlice;

/* ============================================================================
 * Chart Configuration
 * ============================================================================ */

typedef struct CUI_ChartConfig {
    CUI_ChartType type;
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
    CUI_LegendPosition legend_position;
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
    const CUI_ChartSeries *series;
    int series_count;

    /* Data (for pie charts) */
    const CUI_PieSlice *slices;
    int slice_count;
} CUI_ChartConfig;

/* ============================================================================
 * Chart State (for animation and interaction)
 * ============================================================================ */

typedef struct CUI_ChartState {
    float anim_progress;       /* 0-1 for entry animation */
    int hovered_series;        /* -1 = none */
    int hovered_index;         /* Data point index */
    float hover_x, hover_y;    /* Mouse position */
    bool tooltip_visible;

    /* Computed bounds */
    CUI_Rect plot_area;        /* Area for the actual chart */
    CUI_Rect legend_area;
    float y_scale;             /* Pixels per unit */
    float y_offset;            /* Pixel offset for y_min */
} CUI_ChartState;

/* ============================================================================
 * Immediate Mode Chart Drawing
 * ============================================================================ */

/* Draw a line chart */
void cui_draw_line_chart(CUI_Context *ctx, CUI_Rect bounds,
                          const CUI_ChartConfig *config);

/* Draw a bar chart */
void cui_draw_bar_chart(CUI_Context *ctx, CUI_Rect bounds,
                         const CUI_ChartConfig *config);

/* Draw a pie chart */
void cui_draw_pie_chart(CUI_Context *ctx, CUI_Rect bounds,
                         const CUI_ChartConfig *config);

/* Draw any chart type (dispatches based on config.type) */
void cui_draw_chart(CUI_Context *ctx, CUI_Rect bounds,
                     const CUI_ChartConfig *config);

/* Draw with state for animation/interaction */
void cui_draw_chart_ex(CUI_Context *ctx, CUI_Rect bounds,
                        const CUI_ChartConfig *config,
                        CUI_ChartState *state);

/* ============================================================================
 * Chart Node Widget
 * ============================================================================ */

/* Create a chart node */
CUI_Node *cui_chart_create(CUI_Context *ctx, const char *name,
                            const CUI_ChartConfig *config);

/* Update chart configuration */
void cui_chart_set_config(CUI_Node *chart, const CUI_ChartConfig *config);

/* Update just the data (keeps other settings) */
void cui_chart_set_data(CUI_Node *chart, const CUI_ChartSeries *series,
                         int series_count);

/* Update pie data */
void cui_chart_set_pie_data(CUI_Node *chart, const CUI_PieSlice *slices,
                             int slice_count);

/* Add a series dynamically */
void cui_chart_add_series(CUI_Node *chart, const CUI_ChartSeries *series);

/* Update a single series */
void cui_chart_update_series(CUI_Node *chart, int series_index,
                              const float *values, int count);

/* Clear all data */
void cui_chart_clear(CUI_Node *chart);

/* Animation control */
void cui_chart_set_animated(CUI_Node *chart, bool animated);
void cui_chart_restart_animation(CUI_Node *chart);

/* Get hovered data point */
bool cui_chart_get_hover(CUI_Node *chart, int *series, int *index,
                          float *value);

/* ============================================================================
 * Chart Utilities
 * ============================================================================ */

/* Calculate nice axis tick values */
void cui_chart_nice_axis(float data_min, float data_max,
                          float *axis_min, float *axis_max,
                          float *tick_step, int *tick_count);

/* Format a value for display */
const char *cui_chart_format_value(float value, const char *format);

/* Generate a color for series index */
uint32_t cui_chart_series_color(int index);

/* Default chart colors palette */
extern const uint32_t CUI_CHART_PALETTE[12];

#ifdef __cplusplus
}
#endif

#endif /* CARBON_UI_CHARTS_H */
