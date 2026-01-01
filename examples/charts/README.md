# Charts Example

Demonstrates the AUI chart widgets for data visualization.

## Features

- **Line Charts**: Multi-series with smooth curves, fill areas, and data points
- **Bar Charts**: Grouped bars with multiple series comparison
- **Pie/Donut Charts**: Percentage-based data with labels
- **Real-time Charts**: Live-updating data visualization
- **Animations**: Entry animations for chart elements
- **Interactivity**: Hover tooltips showing data values

## Controls

| Key | Action |
|-----|--------|
| 1 | Show line chart |
| 2 | Show bar chart |
| 3 | Show pie/donut chart |
| 4 | Show real-time chart |
| ESC | Quit |

## Usage

```c
// Create chart configuration
AUI_ChartSeries series = {
    .label = "Sales",
    .values = data,
    .value_count = 12,
    .color = 0x4488FFFF,
    .line_width = 2.0f,
    .show_points = true,
    .smooth = true,
};

AUI_ChartConfig config = {
    .type = AUI_CHART_LINE,
    .title = "Monthly Sales",
    .series = &series,
    .series_count = 1,
    .show_grid = true,
    .animated = true,
};

// Draw chart (immediate mode)
aui_draw_chart(ctx, bounds, &config);

// Or with state for animation/hover
AUI_ChartState state = {0};
aui_draw_chart_ex(ctx, bounds, &config, &state);
```

## Chart Types

- `AUI_CHART_LINE` - Line chart with optional fill
- `AUI_CHART_BAR` - Vertical bar chart
- `AUI_CHART_STACKED_BAR` - Stacked bar chart
- `AUI_CHART_PIE` - Pie chart
- `AUI_CHART_DONUT` - Donut chart (pie with hole)
- `AUI_CHART_AREA` - Filled area chart
- `AUI_CHART_SCATTER` - Scatter plot
