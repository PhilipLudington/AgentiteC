/**
 * @file debug_draw.cpp
 * @brief Enhanced Debug Tools - Visualization Drawing Implementation
 */

#include "agentite/debug.h"
#include "agentite/gizmos.h"
#include "agentite/ecs.h"
#include "agentite/collision.h"
#include "agentite/fog.h"
#include "agentite/turn.h"
#include "agentite/spatial.h"
#include "agentite/profiler.h"
#include "agentite/camera.h"
#include "agentite/ui.h"
#include "debug_internal.h"

#include <SDL3/SDL.h>
#include <cglm/cglm.h>
#include <stdio.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static void unpack_color(uint32_t packed, float *out)
{
    out[0] = ((packed >> 24) & 0xFF) / 255.0f;
    out[1] = ((packed >> 16) & 0xFF) / 255.0f;
    out[2] = ((packed >> 8) & 0xFF) / 255.0f;
    out[3] = (packed & 0xFF) / 255.0f;
}

/* ============================================================================
 * Entity Gizmos
 * ============================================================================ */

static void debug_draw_entity_gizmos(Agentite_DebugSystem *debug, Agentite_Gizmos *gizmos)
{
    Agentite_DebugConfig *config = debug_get_config(debug);
    Agentite_World *world = debug_get_ecs(debug);
    if (!config || !world) return;

    ecs_world_t *ecs = agentite_ecs_get_world(world);
    if (!ecs) return;

    /* Query all entities with C_Position */
    ecs_query_desc_t desc = {0};
    desc.terms[0].id = ecs_id(C_Position);
    desc.terms[1].id = ecs_id(C_Velocity);
    desc.terms[1].oper = EcsOptional;

    ecs_query_t *query = ecs_query_init(ecs, &desc);
    if (!query) return;

    ecs_iter_t it = ecs_query_iter(ecs, query);
    while (ecs_query_next(&it)) {
        C_Position *pos = ecs_field(&it, C_Position, 0);
        C_Velocity *vel = ecs_field(&it, C_Velocity, 1);
        bool has_vel = ecs_field_is_set(&it, 1);

        for (int i = 0; i < it.count; i++) {
            /* Draw position marker */
            agentite_gizmos_circle_2d(gizmos,
                                       pos[i].x, pos[i].y,
                                       config->entity_marker_radius,
                                       config->entity_position_color);

            /* Draw velocity arrow if present */
            if (has_vel && (vel[i].vx != 0 || vel[i].vy != 0)) {
                float scale = config->velocity_scale;
                vec3 from = { pos[i].x, pos[i].y, 0 };
                vec3 to = {
                    pos[i].x + vel[i].vx * scale,
                    pos[i].y + vel[i].vy * scale,
                    0
                };
                agentite_gizmos_arrow(gizmos, from, to, config->entity_velocity_color);
            }
        }
    }

    ecs_query_fini(query);
}

/* ============================================================================
 * Collision Shapes
 * ============================================================================ */

static void debug_draw_collision_shapes(Agentite_DebugSystem *debug, Agentite_Gizmos *gizmos)
{
    Agentite_DebugConfig *config = debug_get_config(debug);
    Agentite_CollisionWorld *collision = debug_get_collision(debug);
    if (!config || !collision) return;

    float color[4];
    unpack_color(config->collision_shape_color, color);
    agentite_collision_debug_draw(collision, gizmos, color);
}

/* ============================================================================
 * AI Paths
 * ============================================================================ */

static void debug_draw_paths(Agentite_DebugSystem *debug, Agentite_Gizmos *gizmos)
{
    Agentite_DebugConfig *config = debug_get_config(debug);
    int max_paths = 0;
    DebugPath *paths = debug_get_paths(debug, &max_paths);
    if (!config || !paths) return;

    for (int p = 0; p < max_paths; p++) {
        DebugPath *path = &paths[p];
        if (!path->active || path->length <= 0) continue;

        /* Draw path line segments */
        for (int i = 0; i < path->length - 1; i++) {
            vec3 from = { path->points_x[i], path->points_y[i], 0 };
            vec3 to = { path->points_x[i + 1], path->points_y[i + 1], 0 };
            agentite_gizmos_line(gizmos, from, to, path->color);
        }

        /* Draw waypoint markers */
        for (int i = 0; i < path->length; i++) {
            uint32_t color;
            if (i == path->current_waypoint) {
                color = config->path_current_color;
            } else if (i < path->current_waypoint) {
                /* Past waypoints: dimmer */
                color = (config->path_waypoint_color & 0xFFFFFF00) | 0x80;
            } else {
                color = config->path_waypoint_color;
            }

            agentite_gizmos_circle_2d(gizmos,
                                       path->points_x[i], path->points_y[i],
                                       config->path_waypoint_radius,
                                       color);
        }
    }
}

/* ============================================================================
 * Spatial Grid
 * ============================================================================ */

static void debug_draw_spatial_grid(Agentite_DebugSystem *debug, Agentite_Gizmos *gizmos,
                                     float view_x, float view_y, float view_w, float view_h)
{
    Agentite_DebugConfig *config = debug_get_config(debug);
    Agentite_SpatialIndex *spatial = debug_get_spatial(debug);
    if (!config || !spatial) return;

    float cell_size = config->spatial_cell_size;
    if (cell_size <= 0) cell_size = 32.0f;

    /* Calculate grid bounds */
    int start_x = (int)(view_x / cell_size) - 1;
    int start_y = (int)(view_y / cell_size) - 1;
    int end_x = (int)((view_x + view_w) / cell_size) + 2;
    int end_y = (int)((view_y + view_h) / cell_size) + 2;

    /* Draw vertical grid lines */
    for (int x = start_x; x <= end_x; x++) {
        float px = (float)x * cell_size;
        agentite_gizmos_line_2d(gizmos,
                                 px, view_y - cell_size,
                                 px, view_y + view_h + cell_size,
                                 config->spatial_grid_color);
    }

    /* Draw horizontal grid lines */
    for (int y = start_y; y <= end_y; y++) {
        float py = (float)y * cell_size;
        agentite_gizmos_line_2d(gizmos,
                                 view_x - cell_size, py,
                                 view_x + view_w + cell_size, py,
                                 config->spatial_grid_color);
    }

    /* Highlight occupied cells */
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            if (agentite_spatial_has(spatial, x, y)) {
                agentite_gizmos_rect_filled_2d(gizmos,
                                                (float)x * cell_size,
                                                (float)y * cell_size,
                                                cell_size, cell_size,
                                                config->spatial_occupied_color);
            }
        }
    }
}

/* ============================================================================
 * Fog of War Debug
 * ============================================================================ */

static void debug_draw_fog(Agentite_DebugSystem *debug, Agentite_Gizmos *gizmos,
                           float view_x, float view_y, float view_w, float view_h)
{
    Agentite_DebugConfig *config = debug_get_config(debug);
    Agentite_FogOfWar *fog = debug_get_fog(debug);
    if (!config || !fog) return;

    float tile_w = config->fog_tile_width;
    float tile_h = config->fog_tile_height;
    if (tile_w <= 0) tile_w = 32.0f;
    if (tile_h <= 0) tile_h = 32.0f;

    /* Get fog dimensions */
    int fog_w = 0, fog_h = 0;
    agentite_fog_get_size(fog, &fog_w, &fog_h);

    /* Calculate visible tile range */
    int start_x = (int)(view_x / tile_w);
    int start_y = (int)(view_y / tile_h);
    int end_x = (int)((view_x + view_w) / tile_w) + 1;
    int end_y = (int)((view_y + view_h) / tile_h) + 1;

    /* Clamp to fog bounds */
    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;
    if (end_x > fog_w) end_x = fog_w;
    if (end_y > fog_h) end_y = fog_h;

    /* Draw fog cells */
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            Agentite_VisibilityState state = agentite_fog_get_state(fog, x, y);
            uint32_t color;

            switch (state) {
                case AGENTITE_VIS_UNEXPLORED:
                    color = config->fog_unexplored_color;
                    break;
                case AGENTITE_VIS_EXPLORED:
                    color = config->fog_explored_color;
                    break;
                case AGENTITE_VIS_VISIBLE:
                    color = config->fog_visible_color;
                    break;
                default:
                    continue;
            }

            /* Only draw if alpha > 0 */
            if ((color & 0xFF) > 0) {
                agentite_gizmos_rect_filled_2d(gizmos,
                                                (float)x * tile_w,
                                                (float)y * tile_h,
                                                tile_w, tile_h,
                                                color);
            }
        }
    }
}

/* ============================================================================
 * Main Draw Functions
 * ============================================================================ */

void agentite_debug_draw(Agentite_DebugSystem *debug, Agentite_Gizmos *gizmos)
{
    agentite_debug_draw_ex(debug, gizmos, NULL);
}

void agentite_debug_draw_ex(Agentite_DebugSystem *debug, Agentite_Gizmos *gizmos,
                             Agentite_Camera *camera)
{
    if (!debug || !gizmos) return;
    if (!agentite_debug_get_enabled(debug)) return;

    uint32_t flags = agentite_debug_get_flags(debug);
    if (flags == AGENTITE_DEBUG_NONE) return;

    /* Get view bounds from camera or use defaults */
    float view_x = 0, view_y = 0, view_w = 1920, view_h = 1080;
    if (camera) {
        /* Get camera position and view size */
        float cx, cy;
        agentite_camera_get_position(camera, &cx, &cy);
        float zoom = agentite_camera_get_zoom(camera);
        float vp_w, vp_h;
        agentite_camera_get_viewport(camera, &vp_w, &vp_h);

        view_w = vp_w / zoom;
        view_h = vp_h / zoom;
        view_x = cx - view_w / 2;
        view_y = cy - view_h / 2;
    }

    /* Draw visualizations in order (background to foreground) */

    if (flags & AGENTITE_DEBUG_FOG_OF_WAR) {
        debug_draw_fog(debug, gizmos, view_x, view_y, view_w, view_h);
    }

    if (flags & AGENTITE_DEBUG_SPATIAL_GRID) {
        debug_draw_spatial_grid(debug, gizmos, view_x, view_y, view_w, view_h);
    }

    if (flags & AGENTITE_DEBUG_COLLISION_SHAPES) {
        debug_draw_collision_shapes(debug, gizmos);
    }

    if (flags & AGENTITE_DEBUG_AI_PATHS) {
        debug_draw_paths(debug, gizmos);
    }

    if (flags & AGENTITE_DEBUG_ENTITY_GIZMOS) {
        debug_draw_entity_gizmos(debug, gizmos);
    }
}

/* ============================================================================
 * UI Overlays
 * ============================================================================ */

/* Note: UI drawing requires the AUI system which is optional.
 * This function provides a stub that can be enabled when AUI is available.
 */

/* Track frame time for performance display */
static float s_frame_times[60] = {0};
static int s_frame_time_index = 0;
static float s_last_time = 0;

void agentite_debug_draw_ui(Agentite_DebugSystem *debug, AUI_Context *ui)
{
    if (!debug || !ui) return;
    if (!agentite_debug_get_enabled(debug)) return;

    Agentite_DebugConfig *config = debug_get_config(debug);
    if (!config) return;

    uint32_t flags = agentite_debug_get_flags(debug);

    /* Turn/Phase indicator - bottom left */
    if (flags & AGENTITE_DEBUG_TURN_STATE) {
        Agentite_TurnManager *turn = debug_get_turn(debug);
        if (turn) {
            int turn_num = agentite_turn_number(turn);
            Agentite_TurnPhase phase = agentite_turn_current_phase(turn);
            const char *phase_name = agentite_turn_phase_name(phase);

            /* Draw background panel - bottom left, above any bottom UI */
            float panel_x = 10.0f;
            float panel_y = 650.0f;
            float panel_w = 200.0f;
            float panel_h = 30.0f;
            aui_draw_rect(ui, panel_x, panel_y, panel_w, panel_h, 0x1A1A1AE0);

            /* Draw turn info */
            char buf[64];
            snprintf(buf, sizeof(buf), "Turn %d - %s", turn_num, phase_name);
            aui_draw_text(ui, buf, panel_x + 8, panel_y + 7, config->turn_text_color);
        }
    }

    /* Performance overlay - top right */
    if (flags & AGENTITE_DEBUG_PERFORMANCE) {
        /* Update frame time tracking */
        float current_time = (float)SDL_GetTicks() / 1000.0f;
        float dt = current_time - s_last_time;
        if (s_last_time > 0 && dt > 0 && dt < 1.0f) {
            s_frame_times[s_frame_time_index] = dt;
            s_frame_time_index = (s_frame_time_index + 1) % 60;
        }
        s_last_time = current_time;

        /* Calculate average frame time and FPS */
        float total = 0;
        int count = 0;
        for (int i = 0; i < 60; i++) {
            if (s_frame_times[i] > 0) {
                total += s_frame_times[i];
                count++;
            }
        }
        float avg_dt = count > 0 ? total / (float)count : 0.016f;
        float fps = avg_dt > 0 ? 1.0f / avg_dt : 0;

        /* Draw background panel */
        float panel_x = 1080.0f;
        float panel_y = 10.0f;
        float panel_w = 190.0f;
        float panel_h = 50.0f;
        aui_draw_rect(ui, panel_x, panel_y, panel_w, panel_h, 0x1A1A1AE0);

        /* Draw FPS */
        char buf[64];
        snprintf(buf, sizeof(buf), "FPS: %.1f", fps);
        uint32_t fps_color = fps >= 55 ? 0x00FF00FF : (fps >= 30 ? 0xFFFF00FF : 0xFF0000FF);
        aui_draw_text(ui, buf, panel_x + 8, panel_y + 8, fps_color);

        /* Draw frame time */
        snprintf(buf, sizeof(buf), "Frame: %.2f ms", avg_dt * 1000.0f);
        aui_draw_text(ui, buf, panel_x + 8, panel_y + 28, 0xFFFFFFFF);
    }
}
