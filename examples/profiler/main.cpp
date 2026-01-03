/**
 * Agentite Engine - Profiler Demo
 *
 * Demonstrates the performance profiling system:
 * - Frame timing and FPS tracking
 * - Phase-based profiling (update/render/present)
 * - Scope-based profiling with AGENTITE_PROFILE_SCOPE
 * - Render statistics tracking
 * - Memory allocation tracking
 * - Real-time statistics display
 * - CSV/JSON export
 *
 * Controls:
 *   Space  - Toggle profiler enabled/disabled
 *   E      - Export stats to JSON/CSV files
 *   R      - Reset profiler statistics
 *   +/-    - Adjust simulated workload
 *   ESC    - Quit
 */

#include "agentite/agentite.h"
#include "agentite/profiler.h"
#include "agentite/text.h"
#include "agentite/input.h"
#include "agentite/gizmos.h"
#include "agentite/camera.h"
#include <stdio.h>
#include <math.h>

/* Simulated workload level (0-5) */
static int g_workload_level = 2;

/* Simulate CPU-intensive work */
static void simulate_work(int iterations) {
    volatile float sum = 0.0f;
    for (int i = 0; i < iterations * 10000; i++) {
        sum += sinf((float)i * 0.001f);
    }
    (void)sum;
}

/* Format a size in bytes to a human-readable string */
static void format_bytes(size_t bytes, char *buffer, size_t buffer_size) {
    if (bytes < 1024) {
        snprintf(buffer, buffer_size, "%zu B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f KB", bytes / 1024.0);
    } else {
        snprintf(buffer, buffer_size, "%.1f MB", bytes / (1024.0 * 1024.0));
    }
}

/* Draw text helper */
static void draw_text(Agentite_TextRenderer *tr, Agentite_Font *font,
                      const char *str, float x, float y,
                      float r, float g, float b) {
    if (font) {
        agentite_text_draw_colored(tr, font, str, x, y, r, g, b, 1.0f);
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Initialize engine */
    Agentite_Config config = {
        .window_title = "Agentite - Profiler Demo",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to init engine: %s\n", agentite_get_last_error());
        return 1;
    }

    /* Initialize profiler */
    Agentite_ProfilerConfig profiler_config = AGENTITE_PROFILER_DEFAULT;
    profiler_config.history_size = 256;
    profiler_config.track_scopes = true;
    profiler_config.track_memory = true;

    Agentite_Profiler *profiler = agentite_profiler_create(&profiler_config);
    if (!profiler) {
        fprintf(stderr, "Failed to create profiler: %s\n", agentite_get_last_error());
        agentite_shutdown(engine);
        return 1;
    }

    /* Initialize gizmos for visual demo */
    Agentite_GizmoConfig gizmo_config = AGENTITE_GIZMO_CONFIG_DEFAULT;
    Agentite_Gizmos *gizmos = agentite_gizmos_create(
        agentite_get_gpu_device(engine), &gizmo_config);

    /* Initialize camera */
    Agentite_Camera *camera = agentite_camera_create(1280.0f, 720.0f);
    agentite_camera_set_position(camera, 640.0f, 360.0f);

    /* Initialize text renderer */
    Agentite_TextRenderer *text = agentite_text_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine)
    );

    Agentite_Font *font = agentite_font_load(text, "assets/fonts/Roboto-Regular.ttf", 18);
    if (!font) {
        font = agentite_font_load(text, "/System/Library/Fonts/Helvetica.ttc", 18);
    }
    if (!font) {
        fprintf(stderr, "Warning: Could not load font\n");
    }

    /* Initialize input */
    Agentite_Input *input = agentite_input_init();

    /* Track some fake memory allocations */
    agentite_profiler_report_alloc(profiler, 1024 * 1024);  /* 1 MB */
    agentite_profiler_report_alloc(profiler, 512 * 1024);   /* 512 KB */

    float time = 0.0f;
    int export_count = 0;

    /* Animated objects */
    const int NUM_OBJECTS = 50;
    float obj_x[NUM_OBJECTS], obj_y[NUM_OBJECTS], obj_speed[NUM_OBJECTS];
    for (int i = 0; i < NUM_OBJECTS; i++) {
        obj_x[i] = 400.0f + (float)(i % 10) * 50.0f;
        obj_y[i] = 200.0f + (float)(i / 10) * 50.0f;
        obj_speed[i] = 50.0f + (float)(i * 7 % 30);
    }

    printf("\n=== Profiler Demo ===\n");
    printf("Controls:\n");
    printf("  Space  - Toggle profiler enabled/disabled\n");
    printf("  E      - Export stats to JSON/CSV files\n");
    printf("  R      - Reset profiler statistics\n");
    printf("  +/-    - Adjust simulated workload (affects frame time)\n");
    printf("  ESC    - Quit\n\n");

    while (agentite_is_running(engine)) {
        /* ==== FRAME BEGIN ==== */
        agentite_profiler_begin_frame(profiler);
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);
        time += dt;

        /* ==== INPUT ==== */
        agentite_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }
        agentite_input_update(input);

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE)) {
            agentite_quit(engine);
        }

        /* Toggle profiler */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_SPACE)) {
            bool enabled = agentite_profiler_is_enabled(profiler);
            agentite_profiler_set_enabled(profiler, !enabled);
            printf("Profiler %s\n", !enabled ? "ENABLED" : "DISABLED");
        }

        /* Export stats */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_E)) {
            char json_path[64], csv_path[64], history_path[64];
            snprintf(json_path, sizeof(json_path), "profile_%d.json", export_count);
            snprintf(csv_path, sizeof(csv_path), "profile_%d.csv", export_count);
            snprintf(history_path, sizeof(history_path), "frame_history_%d.csv", export_count);

            agentite_profiler_export_json(profiler, json_path);
            agentite_profiler_export_csv(profiler, csv_path);
            agentite_profiler_export_frame_history_csv(profiler, history_path);

            printf("Exported: %s, %s, %s\n", json_path, csv_path, history_path);
            export_count++;
        }

        /* Reset profiler */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_R)) {
            agentite_profiler_reset(profiler);
            printf("Profiler reset\n");
        }

        /* Adjust workload */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_EQUALS) ||
            agentite_input_key_just_pressed(input, SDL_SCANCODE_KP_PLUS)) {
            if (g_workload_level < 5) {
                g_workload_level++;
                printf("Workload level: %d\n", g_workload_level);
            }
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_MINUS) ||
            agentite_input_key_just_pressed(input, SDL_SCANCODE_KP_MINUS)) {
            if (g_workload_level > 0) {
                g_workload_level--;
                printf("Workload level: %d\n", g_workload_level);
            }
        }

        agentite_camera_update(camera);

        /* ==== UPDATE PHASE ==== */
        agentite_profiler_begin_update(profiler);
        {
            /* Simulate physics work */
            AGENTITE_PROFILE_SCOPE(profiler, "physics");
            simulate_work(g_workload_level);

            /* Update object positions */
            for (int i = 0; i < NUM_OBJECTS; i++) {
                obj_y[i] += sinf(time * obj_speed[i] * 0.02f) * dt * 30.0f;
            }
        }

        {
            /* Simulate AI work */
            AGENTITE_PROFILE_SCOPE(profiler, "ai");
            simulate_work(g_workload_level / 2);
        }

        /* Report entity count */
        agentite_profiler_report_entity_count(profiler, NUM_OBJECTS);

        agentite_profiler_end_update(profiler);

        /* ==== RENDER PHASE ==== */
        agentite_profiler_begin_render(profiler);

        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            /* Begin gizmo frame for shapes */
            agentite_gizmos_begin(gizmos, camera);
            agentite_gizmos_set_screen_size(gizmos, 1280, 720);

            {
                AGENTITE_PROFILE_SCOPE(profiler, "gizmo_draw");

                /* Draw animated objects */
                for (int i = 0; i < NUM_OBJECTS; i++) {
                    float hue = (float)i / NUM_OBJECTS;
                    uint8_t r = (uint8_t)(sinf(hue * 6.28f) * 127 + 128);
                    uint8_t g = (uint8_t)(sinf(hue * 6.28f + 2.09f) * 127 + 128);
                    uint8_t b = (uint8_t)(sinf(hue * 6.28f + 4.18f) * 127 + 128);
                    uint32_t color = (r << 24) | (g << 16) | (b << 8) | 0xFF;

                    vec3 pos = {obj_x[i], obj_y[i], 0.0f};
                    agentite_gizmos_sphere(gizmos, pos, 15.0f, color);
                }

                /* Draw a pulsing circle */
                float pulse = sinf(time * 3.0f) * 0.3f + 0.7f;
                vec3 center = {900.0f, 400.0f, 0.0f};
                vec3 normal = {0.0f, 0.0f, 1.0f};
                agentite_gizmos_circle(gizmos, center, normal, 50.0f * pulse, 0x00FF00FF);

                /* Report render stats */
                agentite_profiler_report_batch(profiler, NUM_OBJECTS * 32, NUM_OBJECTS * 48);
                agentite_profiler_report_draw_call(profiler);
            }

            agentite_gizmos_end(gizmos);

            /* Prepare text */
            agentite_text_begin(text);

            /* Get profiler stats */
            const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);

            /* Format memory stats */
            char mem_current[32], mem_peak[32];
            format_bytes(stats->memory.current_bytes, mem_current, sizeof(mem_current));
            format_bytes(stats->memory.peak_bytes, mem_peak, sizeof(mem_peak));

            /* Draw stats overlay */
            float y = 20.0f;
            const float lh = 22.0f;  /* line height */
            char line[256];

            /* Title */
            draw_text(text, font, "=== PROFILER STATS ===", 20.0f, y, 1.0f, 1.0f, 1.0f);
            y += lh * 1.5f;

            /* Status */
            snprintf(line, sizeof(line), "Status: %s",
                     agentite_profiler_is_enabled(profiler) ? "ENABLED" : "DISABLED");
            draw_text(text, font, line, 20.0f, y, 0.0f, 1.0f, 0.0f);
            y += lh * 1.5f;

            /* Frame timing */
            draw_text(text, font, "Frame Timing:", 20.0f, y, 1.0f, 1.0f, 0.0f);
            y += lh;

            snprintf(line, sizeof(line), "  FPS: %.1f", stats->fps);
            draw_text(text, font, line, 20.0f, y, 1.0f, 1.0f, 1.0f);
            y += lh;

            snprintf(line, sizeof(line), "  Frame: %.2f ms (avg: %.2f)",
                     stats->frame_time_ms, stats->avg_frame_time_ms);
            draw_text(text, font, line, 20.0f, y, 1.0f, 1.0f, 1.0f);
            y += lh;

            snprintf(line, sizeof(line), "  Min/Max: %.2f / %.2f ms",
                     stats->min_frame_time_ms, stats->max_frame_time_ms);
            draw_text(text, font, line, 20.0f, y, 1.0f, 1.0f, 1.0f);
            y += lh * 1.5f;

            /* Phase breakdown */
            draw_text(text, font, "Phase Breakdown:", 20.0f, y, 1.0f, 1.0f, 0.0f);
            y += lh;

            snprintf(line, sizeof(line), "  Update:  %.2f ms", stats->update_time_ms);
            draw_text(text, font, line, 20.0f, y, 0.5f, 1.0f, 0.5f);
            y += lh;

            snprintf(line, sizeof(line), "  Render:  %.2f ms", stats->render_time_ms);
            draw_text(text, font, line, 20.0f, y, 0.5f, 0.5f, 1.0f);
            y += lh;

            snprintf(line, sizeof(line), "  Present: %.2f ms", stats->present_time_ms);
            draw_text(text, font, line, 20.0f, y, 1.0f, 0.5f, 0.5f);
            y += lh * 1.5f;

            /* Scopes */
            draw_text(text, font, "Scopes:", 20.0f, y, 1.0f, 1.0f, 0.0f);
            y += lh;

            for (uint32_t i = 0; i < stats->scope_count && i < 6; i++) {
                snprintf(line, sizeof(line), "  %-12s %.2f ms (%u calls)",
                         stats->scopes[i].name,
                         stats->scopes[i].total_time_ms,
                         stats->scopes[i].call_count);
                draw_text(text, font, line, 20.0f, y, 0.75f, 0.75f, 0.75f);
                y += lh;
            }
            y += lh * 0.5f;

            /* Render stats */
            draw_text(text, font, "Render Stats:", 20.0f, y, 1.0f, 1.0f, 0.0f);
            y += lh;

            snprintf(line, sizeof(line), "  Draw calls: %u", stats->render.draw_calls);
            draw_text(text, font, line, 20.0f, y, 1.0f, 1.0f, 1.0f);
            y += lh;

            snprintf(line, sizeof(line), "  Batches: %u", stats->render.batch_count);
            draw_text(text, font, line, 20.0f, y, 1.0f, 1.0f, 1.0f);
            y += lh;

            snprintf(line, sizeof(line), "  Vertices: %u", stats->render.vertex_count);
            draw_text(text, font, line, 20.0f, y, 1.0f, 1.0f, 1.0f);
            y += lh * 1.5f;

            /* Memory stats */
            draw_text(text, font, "Memory:", 20.0f, y, 1.0f, 1.0f, 0.0f);
            y += lh;

            snprintf(line, sizeof(line), "  Current: %s (peak: %s)", mem_current, mem_peak);
            draw_text(text, font, line, 20.0f, y, 1.0f, 1.0f, 1.0f);
            y += lh;

            snprintf(line, sizeof(line), "  Allocations: %zu live", stats->memory.allocation_count);
            draw_text(text, font, line, 20.0f, y, 1.0f, 1.0f, 1.0f);
            y += lh * 1.5f;

            /* Entity count */
            snprintf(line, sizeof(line), "Entities: %u", stats->entity_count);
            draw_text(text, font, line, 20.0f, y, 0.0f, 1.0f, 1.0f);
            y += lh;

            snprintf(line, sizeof(line), "Frame count: %llu", (unsigned long long)stats->frame_count);
            draw_text(text, font, line, 20.0f, y, 0.5f, 0.5f, 0.5f);
            y += lh * 2.0f;

            /* Workload indicator */
            snprintf(line, sizeof(line), "Workload: %d (+/- to adjust)", g_workload_level);
            draw_text(text, font, line, 20.0f, y, 1.0f, 0.5f, 0.0f);

            /* Controls at bottom of screen */
            draw_text(text, font, "Controls: Space=Toggle  E=Export  R=Reset  +/-=Workload  ESC=Quit",
                      20.0f, 695.0f, 0.5f, 0.5f, 0.5f);

            /* End text batch */
            agentite_text_end(text);

            /* Upload gizmos and text */
            agentite_gizmos_upload(gizmos, cmd);
            agentite_text_upload(text, cmd);

            /* Begin render pass */
            if (agentite_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);

                /* Render gizmos */
                agentite_gizmos_render(gizmos, cmd, pass);
                agentite_profiler_report_draw_call(profiler);

                /* Render text */
                agentite_text_render(text, cmd, pass);
                agentite_profiler_report_draw_call(profiler);

                agentite_end_render_pass(engine);
            }
        }

        agentite_profiler_end_render(profiler);

        /* ==== PRESENT PHASE ==== */
        agentite_profiler_begin_present(profiler);
        agentite_end_frame(engine);
        agentite_profiler_end_present(profiler);

        /* ==== FRAME END ==== */
        agentite_profiler_end_frame(profiler);
    }

    /* Final export */
    printf("\nExporting final stats...\n");
    agentite_profiler_export_json(profiler, "profile_final.json");
    agentite_profiler_export_csv(profiler, "profile_final.csv");
    printf("Exported: profile_final.json, profile_final.csv\n");

    /* Cleanup */
    agentite_input_shutdown(input);
    if (font) agentite_font_destroy(text, font);
    agentite_text_shutdown(text);
    agentite_camera_destroy(camera);
    agentite_gizmos_destroy(gizmos);
    agentite_profiler_destroy(profiler);
    agentite_shutdown(engine);

    return 0;
}
