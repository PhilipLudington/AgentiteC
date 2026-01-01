/**
 * Agentite Engine - Charts Example
 *
 * Demonstrates data visualization widgets:
 * - Line charts with animation and hover
 * - Bar charts (vertical and horizontal)
 * - Pie and donut charts
 * - Multiple data series
 * - Dynamic data updates
 */

#include "agentite/agentite.h"
#include "agentite/ui.h"
#include "agentite/ui_charts.h"
#include "agentite/input.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* Sample data for charts */
static float monthly_sales[] = {120, 150, 180, 220, 195, 280, 310, 290, 340, 380, 420, 450};
static float monthly_costs[] = {80, 95, 110, 140, 130, 180, 200, 185, 210, 240, 260, 280};
static float realtime_data[20] = {0};
static int realtime_index = 0;

/* Update realtime data with simulated values */
static void update_realtime_data(float dt) {
    static float time = 0.0f;
    time += dt;

    /* Generate a value based on sine wave with noise */
    float value = 50.0f + 30.0f * sinf(time * 0.5f) + (rand() % 20 - 10);
    if (value < 0) value = 0;
    if (value > 100) value = 100;

    /* Shift data left and add new value */
    for (int i = 0; i < 19; i++) {
        realtime_data[i] = realtime_data[i + 1];
    }
    realtime_data[19] = value;
    realtime_index++;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Agentite_Config config = {
        .window_title = "Agentite - Charts Example",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    /* Initialize UI system */
    AUI_Context *ui = aui_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine),
        config.window_width,
        config.window_height,
        "assets/fonts/Roboto-Regular.ttf",
        16.0f
    );

    if (!ui) {
        fprintf(stderr, "Failed to initialize UI (make sure font exists)\n");
        agentite_shutdown(engine);
        return 1;
    }

    float dpi_scale = agentite_get_dpi_scale(engine);
    aui_set_dpi_scale(ui, dpi_scale);

    Agentite_Input *input = agentite_input_init();

    /* Initialize realtime data */
    for (int i = 0; i < 20; i++) {
        realtime_data[i] = 50.0f;
    }

    /* Chart states for animation */
    AUI_ChartState line_state = {0};
    AUI_ChartState bar_state = {0};
    AUI_ChartState pie_state = {0};
    AUI_ChartState realtime_state = {0};
    realtime_state.anim_progress = 1.0f;  /* Real-time doesn't animate entry */

    /* Selected chart type for switching */
    int selected_chart = 0;
    int prev_selected = -1;  /* Track chart changes to restart animation */
    const char *chart_names[] = {"Line Chart", "Bar Chart", "Pie Chart", "Real-time"};

    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);

        agentite_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (aui_process_event(ui, &event)) {
                continue;
            }
            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }
        agentite_input_update(input);

        /* Switch charts with number keys */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_1)) selected_chart = 0;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_2)) selected_chart = 1;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_3)) selected_chart = 2;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_4)) selected_chart = 3;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            agentite_quit(engine);

        /* Reset animation when switching charts */
        if (selected_chart != prev_selected) {
            prev_selected = selected_chart;
            line_state.anim_progress = 0.0f;
            bar_state.anim_progress = 0.0f;
            pie_state.anim_progress = 0.0f;
        }

        /* Update animation progress (1 second animation) */
        float anim_speed = 1.0f;
        if (line_state.anim_progress < 1.0f)
            line_state.anim_progress += dt * anim_speed;
        if (bar_state.anim_progress < 1.0f)
            bar_state.anim_progress += dt * anim_speed;
        if (pie_state.anim_progress < 1.0f)
            pie_state.anim_progress += dt * anim_speed;

        /* Update realtime data */
        update_realtime_data(dt);

        /* Begin UI frame */
        aui_begin_frame(ui, dt);

        /* Title and instructions */
        if (aui_begin_panel(ui, "Charts Demo", 50, 30, 400, 60,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {
            aui_label(ui, "Press 1-4 to switch chart types. ESC to quit.");
            aui_end_panel(ui);
        }

        /* Chart type selector */
        if (aui_begin_panel(ui, "Chart Type", 50, 110, 200, 180,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {
            for (int i = 0; i < 4; i++) {
                char label[32];
                snprintf(label, sizeof(label), "%d. %s", i + 1, chart_names[i]);
                if (selected_chart == i) {
                    aui_label(ui, label);
                } else {
                    if (aui_button(ui, label)) {
                        selected_chart = i;
                    }
                }
            }
            aui_end_panel(ui);
        }

        /* Main chart display area */
        AUI_Rect chart_area = {280, 110, 700, 450};

        switch (selected_chart) {
        case 0: {
            /* Line Chart - Monthly sales and costs */
            const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

            AUI_ChartSeries series[2] = {
                {
                    .label = "Sales",
                    .values = monthly_sales,
                    .value_count = 12,
                    .color = 0x4488FFFF,
                    .line_width = 2.0f,
                    .show_points = true,
                    .point_size = 5.0f,
                    .smooth = true,
                    .filled = true,
                    .fill_opacity = 0.2f,
                },
                {
                    .label = "Costs",
                    .values = monthly_costs,
                    .value_count = 12,
                    .color = 0xFF6B6BFF,
                    .line_width = 2.0f,
                    .show_points = true,
                    .point_size = 4.0f,
                    .smooth = true,
                    .filled = false,
                }
            };

            AUI_ChartConfig config = {
                .type = AUI_CHART_LINE,
                .title = "Monthly Sales vs Costs",
                .x_axis_label = "Month",
                .y_axis_label = "Amount ($)",
                .x_labels = months,
                .x_label_count = 12,
                .y_min = 0,
                .y_max = 500,
                .y_divisions = 5,
                .show_grid = true,
                .show_legend = true,
                .legend_position = AUI_LEGEND_BOTTOM,
                .show_tooltips = true,
                .animated = true,
                .animation_duration = 1.0f,
                .series = series,
                .series_count = 2,
                .background_color = 0x1A1A2AFF,
                .grid_color = 0x333355FF,
                .axis_color = 0x666688FF,
                .text_color = 0xCCCCCCFF,
            };

            aui_draw_chart_ex(ui, chart_area, &config, &line_state);
            break;
        }

        case 1: {
            /* Bar Chart - Quarterly comparison */
            static float q1[] = {100, 150, 120, 180};
            static float q2[] = {120, 180, 140, 200};
            static float q3[] = {140, 200, 160, 220};

            const char *categories[] = {"Product A", "Product B", "Product C", "Product D"};

            AUI_ChartSeries series[3] = {
                {.label = "Q1", .values = q1, .value_count = 4, .color = 0x4ECDC4FF},
                {.label = "Q2", .values = q2, .value_count = 4, .color = 0x45B7D1FF},
                {.label = "Q3", .values = q3, .value_count = 4, .color = 0x96CEB4FF},
            };

            AUI_ChartConfig config = {
                .type = AUI_CHART_BAR,
                .title = "Quarterly Product Sales",
                .x_labels = categories,
                .x_label_count = 4,
                .y_min = 0,
                .y_max = 250,
                .show_grid = true,
                .show_legend = true,
                .legend_position = AUI_LEGEND_RIGHT,
                .show_values = true,
                .show_tooltips = true,
                .bar_width = 0.25f,
                .bar_spacing = 0.05f,
                .animated = true,
                .animation_duration = 0.8f,
                .series = series,
                .series_count = 3,
                .background_color = 0x1A1A2AFF,
                .grid_color = 0x333355FF,
                .axis_color = 0x666688FF,
                .text_color = 0xCCCCCCFF,
            };

            aui_draw_chart_ex(ui, chart_area, &config, &bar_state);
            break;
        }

        case 2: {
            /* Pie Chart - Market share */
            AUI_PieSlice slices[] = {
                {.label = "Chrome", .value = 65.0f, .color = 0x4285F4FF},
                {.label = "Safari", .value = 18.0f, .color = 0x34A853FF},
                {.label = "Firefox", .value = 8.0f, .color = 0xFF5722FF},
                {.label = "Edge", .value = 5.0f, .color = 0x0078D4FF},
                {.label = "Other", .value = 4.0f, .color = 0x9E9E9EFF},
            };

            AUI_ChartConfig config = {
                .type = AUI_CHART_DONUT,
                .title = "Browser Market Share",
                .show_legend = true,
                .legend_position = AUI_LEGEND_RIGHT,
                .show_percentages = true,
                .show_tooltips = true,
                .donut_inner_radius = 0.5f,
                .animated = true,
                .animation_duration = 1.0f,
                .slices = slices,
                .slice_count = 5,
                .background_color = 0x1A1A2AFF,
                .text_color = 0xCCCCCCFF,
            };

            aui_draw_chart_ex(ui, chart_area, &config, &pie_state);
            break;
        }

        case 3: {
            /* Real-time line chart */
            AUI_ChartSeries series = {
                .label = "CPU Usage",
                .values = realtime_data,
                .value_count = 20,
                .color = 0x88FF88FF,
                .line_width = 2.0f,
                .show_points = false,
                .smooth = true,
                .filled = true,
                .fill_opacity = 0.4f,
            };

            AUI_ChartConfig config = {
                .type = AUI_CHART_AREA,
                .title = "Real-time CPU Usage (%)",
                .y_min = 0,
                .y_max = 100,
                .y_divisions = 4,
                .show_grid = true,
                .show_legend = false,
                .show_tooltips = true,
                .animated = false,  /* No animation for real-time */
                .series = &series,
                .series_count = 1,
                .background_color = 0x1A1A2AFF,
                .grid_color = 0x333355FF,
                .axis_color = 0x666688FF,
                .text_color = 0xCCCCCCFF,
            };

            aui_draw_chart_ex(ui, chart_area, &config, &realtime_state);
            break;
        }
        }

        /* Info panel */
        if (aui_begin_panel(ui, "Info", 50, 580, 200, 120, AUI_PANEL_BORDER)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "FPS: %.0f", 1.0f / dt);
            aui_label(ui, buf);
            aui_separator(ui);
            aui_label(ui, "Features:");
            aui_label(ui, "- Animated entry");
            aui_label(ui, "- Hover tooltips");
            aui_label(ui, "- Multi-series");
            aui_end_panel(ui);
        }

        aui_end_frame(ui);

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            aui_upload(ui, cmd);

            if (agentite_begin_render_pass(engine, 0.08f, 0.08f, 0.12f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
                aui_render(ui, cmd, pass);
                agentite_end_render_pass(engine);
            }
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_input_shutdown(input);
    aui_shutdown(ui);
    agentite_shutdown(engine);

    return 0;
}
