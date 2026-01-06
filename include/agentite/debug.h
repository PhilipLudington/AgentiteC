/**
 * @file debug.h
 * @brief Enhanced Debug Tools for Agentite
 *
 * Provides runtime debugging capabilities including:
 * - Entity gizmo overlays (positions, velocities)
 * - Collision shape visualization
 * - AI path visualization
 * - Spatial hash grid overlay
 * - Fog of war debug view
 * - Turn/phase state inspector
 * - Console command system
 *
 * Usage:
 *   Agentite_DebugSystem *debug = agentite_debug_create(NULL);
 *
 *   // Bind systems for visualization
 *   agentite_debug_bind_ecs(debug, world);
 *   agentite_debug_bind_collision(debug, collision);
 *
 *   // Enable visualizations
 *   agentite_debug_set_flags(debug, AGENTITE_DEBUG_ENTITY_GIZMOS |
 *                                   AGENTITE_DEBUG_COLLISION_SHAPES);
 *
 *   // In render loop (before gizmos end):
 *   agentite_gizmos_begin(gizmos, camera);
 *   agentite_debug_draw(debug, gizmos);
 *   agentite_gizmos_end(gizmos);
 *
 *   // For UI overlays:
 *   agentite_debug_draw_ui(debug, ui);
 *
 *   // Console (toggle with backtick):
 *   if (agentite_debug_console_is_open(debug)) {
 *       agentite_debug_console_panel(debug, ui, x, y, w, h);
 *   }
 *
 *   agentite_debug_destroy(debug);
 */

#ifndef AGENTITE_DEBUG_H
#define AGENTITE_DEBUG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_DebugSystem Agentite_DebugSystem;
typedef struct Agentite_Gizmos Agentite_Gizmos;
typedef struct Agentite_World Agentite_World;
typedef struct Agentite_CollisionWorld Agentite_CollisionWorld;
typedef struct Agentite_Pathfinder Agentite_Pathfinder;
typedef struct Agentite_Path Agentite_Path;
typedef struct Agentite_FogOfWar Agentite_FogOfWar;
typedef struct Agentite_TurnManager Agentite_TurnManager;
typedef struct Agentite_SpatialIndex Agentite_SpatialIndex;
typedef struct Agentite_Camera Agentite_Camera;
typedef struct Agentite_Profiler Agentite_Profiler;
typedef struct AUI_Context AUI_Context;

/* ============================================================================
 * Debug Visualization Flags
 * ============================================================================ */

/**
 * Flags to control which debug visualizations are active.
 * Combine with bitwise OR.
 */
typedef enum Agentite_DebugFlags {
    AGENTITE_DEBUG_NONE              = 0,
    AGENTITE_DEBUG_ENTITY_GIZMOS     = (1 << 0),  /**< Entity position/velocity arrows */
    AGENTITE_DEBUG_COLLISION_SHAPES  = (1 << 1),  /**< Collision shape outlines */
    AGENTITE_DEBUG_AI_PATHS          = (1 << 2),  /**< Pathfinding visualization */
    AGENTITE_DEBUG_SPATIAL_GRID      = (1 << 3),  /**< Spatial hash grid overlay */
    AGENTITE_DEBUG_FOG_OF_WAR        = (1 << 4),  /**< Fog visibility states */
    AGENTITE_DEBUG_TURN_STATE        = (1 << 5),  /**< Turn/phase indicator */
    AGENTITE_DEBUG_PERFORMANCE       = (1 << 6),  /**< FPS/frame time overlay */
    AGENTITE_DEBUG_ALL               = 0x7F
} Agentite_DebugFlags;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * Configuration for the debug system.
 * All colors are in RGBA format (0xRRGGBBAA).
 */
typedef struct Agentite_DebugConfig {
    /* Entity gizmo colors */
    uint32_t entity_position_color;      /**< Position marker (default: green) */
    uint32_t entity_velocity_color;      /**< Velocity arrow (default: yellow) */
    float entity_marker_radius;          /**< Position marker radius (default: 4.0) */

    /* Collision colors */
    uint32_t collision_shape_color;      /**< Shape outline (default: cyan) */
    uint32_t collision_trigger_color;    /**< Trigger shapes (default: magenta) */

    /* Path colors */
    uint32_t path_line_color;            /**< Path line (default: orange) */
    uint32_t path_waypoint_color;        /**< Waypoint markers (default: white) */
    uint32_t path_current_color;         /**< Current waypoint (default: green) */
    float path_waypoint_radius;          /**< Waypoint marker radius (default: 4.0) */
    float path_tile_size;                /**< Tile size for path coords (default: 32.0) */

    /* Spatial grid colors */
    uint32_t spatial_grid_color;         /**< Grid lines (default: dark gray) */
    uint32_t spatial_occupied_color;     /**< Occupied cells (default: green alpha) */

    /* Fog of war colors */
    uint32_t fog_unexplored_color;       /**< Unexplored cells (default: black) */
    uint32_t fog_explored_color;         /**< Explored cells (default: gray alpha) */
    uint32_t fog_visible_color;          /**< Visible cells (default: green alpha) */
    float fog_tile_width;                /**< Tile width for fog display (default: 32.0) */
    float fog_tile_height;               /**< Tile height for fog display (default: 32.0) */

    /* Turn state display */
    uint32_t turn_text_color;            /**< Turn text color (default: white) */
    uint32_t turn_phase_active_color;    /**< Active phase (default: green) */
    uint32_t turn_phase_inactive_color;  /**< Inactive phase (default: gray) */

    /* Visualization scales */
    float velocity_scale;                /**< Scale factor for velocity arrows (default: 0.1) */
    float spatial_cell_size;             /**< Spatial grid cell size (default: 32.0) */

    /* Console settings */
    int console_max_history;             /**< Command history size (default: 64) */
    int console_max_output;              /**< Output buffer lines (default: 256) */
    uint32_t console_bg_color;           /**< Console background (default: dark) */
    uint32_t console_text_color;         /**< Console text (default: white) */
    uint32_t console_error_color;        /**< Error text (default: red) */
    uint32_t console_input_color;        /**< Input text (default: green) */
} Agentite_DebugConfig;

/** Default configuration values */
#define AGENTITE_DEBUG_CONFIG_DEFAULT {                                     \
    .entity_position_color = 0x00FF00FF,     /* Green */                    \
    .entity_velocity_color = 0xFFFF00FF,     /* Yellow */                   \
    .entity_marker_radius = 4.0f,                                           \
    .collision_shape_color = 0x00FFFFFF,     /* Cyan */                     \
    .collision_trigger_color = 0xFF00FFFF,   /* Magenta */                  \
    .path_line_color = 0xFF8000FF,           /* Orange */                   \
    .path_waypoint_color = 0xFFFFFFFF,       /* White */                    \
    .path_current_color = 0x00FF00FF,        /* Green */                    \
    .path_waypoint_radius = 4.0f,                                           \
    .path_tile_size = 32.0f,                                                \
    .spatial_grid_color = 0x404040FF,        /* Dark gray */                \
    .spatial_occupied_color = 0x00FF0040,    /* Green 25% alpha */          \
    .fog_unexplored_color = 0x000000C0,      /* Black 75% alpha */          \
    .fog_explored_color = 0x40404080,        /* Gray 50% alpha */           \
    .fog_visible_color = 0x00FF0020,         /* Green 12% alpha */          \
    .fog_tile_width = 32.0f,                                                \
    .fog_tile_height = 32.0f,                                               \
    .turn_text_color = 0xFFFFFFFF,           /* White */                    \
    .turn_phase_active_color = 0x00FF00FF,   /* Green */                    \
    .turn_phase_inactive_color = 0x808080FF, /* Gray */                     \
    .velocity_scale = 0.1f,                                                 \
    .spatial_cell_size = 32.0f,                                             \
    .console_max_history = 64,                                              \
    .console_max_output = 256,                                              \
    .console_bg_color = 0x1A1A1AE0,          /* Dark 88% alpha */           \
    .console_text_color = 0xFFFFFFFF,        /* White */                    \
    .console_error_color = 0xFF4444FF,       /* Red */                      \
    .console_input_color = 0x44FF44FF        /* Green */                    \
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * Create debug system.
 * Caller OWNS the returned pointer and MUST call agentite_debug_destroy().
 *
 * @param config    Configuration (NULL for defaults)
 * @return          Debug system or NULL on failure
 */
Agentite_DebugSystem *agentite_debug_create(const Agentite_DebugConfig *config);

/**
 * Destroy debug system and free all resources.
 * Safe to call with NULL.
 *
 * @param debug     Debug system to destroy
 */
void agentite_debug_destroy(Agentite_DebugSystem *debug);

/* ============================================================================
 * Enable/Disable Controls
 * ============================================================================ */

/**
 * Set which debug visualizations are enabled.
 *
 * @param debug     Debug system
 * @param flags     Combination of Agentite_DebugFlags
 */
void agentite_debug_set_flags(Agentite_DebugSystem *debug, uint32_t flags);

/**
 * Get current debug visualization flags.
 *
 * @param debug     Debug system
 * @return          Current flags
 */
uint32_t agentite_debug_get_flags(const Agentite_DebugSystem *debug);

/**
 * Toggle a specific debug visualization.
 *
 * @param debug     Debug system
 * @param flag      Single flag to toggle
 */
void agentite_debug_toggle(Agentite_DebugSystem *debug, Agentite_DebugFlags flag);

/**
 * Check if a specific visualization is enabled.
 *
 * @param debug     Debug system
 * @param flag      Flag to check
 * @return          true if enabled
 */
bool agentite_debug_is_enabled(const Agentite_DebugSystem *debug, Agentite_DebugFlags flag);

/**
 * Master enable/disable for all debug visualization.
 *
 * @param debug     Debug system
 * @param enabled   true to enable, false to disable
 */
void agentite_debug_set_enabled(Agentite_DebugSystem *debug, bool enabled);

/**
 * Check if debug system is globally enabled.
 *
 * @param debug     Debug system
 * @return          true if enabled
 */
bool agentite_debug_get_enabled(const Agentite_DebugSystem *debug);

/* ============================================================================
 * System Bindings
 * ============================================================================ */

/**
 * Bind ECS world for entity gizmo visualization.
 * Pass NULL to unbind.
 *
 * @param debug     Debug system
 * @param world     ECS world (borrowed reference)
 */
void agentite_debug_bind_ecs(Agentite_DebugSystem *debug, Agentite_World *world);

/**
 * Bind collision world for shape visualization.
 * Pass NULL to unbind.
 *
 * @param debug     Debug system
 * @param collision Collision world (borrowed reference)
 */
void agentite_debug_bind_collision(Agentite_DebugSystem *debug,
                                    Agentite_CollisionWorld *collision);

/**
 * Bind pathfinder for path visualization.
 * Pass NULL to unbind.
 *
 * @param debug     Debug system
 * @param pathfinder Pathfinder (borrowed reference)
 */
void agentite_debug_bind_pathfinder(Agentite_DebugSystem *debug,
                                     Agentite_Pathfinder *pathfinder);

/**
 * Bind fog of war for visibility visualization.
 * Pass NULL to unbind.
 *
 * @param debug     Debug system
 * @param fog       Fog of war system (borrowed reference)
 */
void agentite_debug_bind_fog(Agentite_DebugSystem *debug, Agentite_FogOfWar *fog);

/**
 * Bind turn manager for turn/phase visualization.
 * Pass NULL to unbind.
 *
 * @param debug     Debug system
 * @param turn      Turn manager (borrowed reference)
 */
void agentite_debug_bind_turn(Agentite_DebugSystem *debug, Agentite_TurnManager *turn);

/**
 * Bind spatial index for grid overlay.
 * Pass NULL to unbind.
 *
 * @param debug     Debug system
 * @param spatial   Spatial index (borrowed reference)
 */
void agentite_debug_bind_spatial(Agentite_DebugSystem *debug,
                                  Agentite_SpatialIndex *spatial);

/**
 * Bind profiler for performance overlay.
 * Pass NULL to unbind.
 *
 * @param debug     Debug system
 * @param profiler  Profiler (borrowed reference)
 */
void agentite_debug_bind_profiler(Agentite_DebugSystem *debug,
                                   Agentite_Profiler *profiler);

/* ============================================================================
 * Rendering
 * ============================================================================ */

/**
 * Draw debug visualizations using gizmos.
 * Call after agentite_gizmos_begin() and before agentite_gizmos_end().
 *
 * @param debug     Debug system
 * @param gizmos    Gizmo renderer
 */
void agentite_debug_draw(Agentite_DebugSystem *debug, Agentite_Gizmos *gizmos);

/**
 * Draw debug visualizations with camera bounds for culling.
 * Call after agentite_gizmos_begin() and before agentite_gizmos_end().
 *
 * @param debug     Debug system
 * @param gizmos    Gizmo renderer
 * @param camera    Camera for view bounds (NULL to skip culling)
 */
void agentite_debug_draw_ex(Agentite_DebugSystem *debug, Agentite_Gizmos *gizmos,
                             Agentite_Camera *camera);

/**
 * Draw debug UI overlays (turn state, performance).
 * Call during UI frame (after aui_begin_frame).
 *
 * @param debug     Debug system
 * @param ui        UI context
 */
void agentite_debug_draw_ui(Agentite_DebugSystem *debug, AUI_Context *ui);

/* ============================================================================
 * AI Path Visualization
 * ============================================================================ */

/**
 * Add a path to visualize.
 * The path data is copied; caller retains ownership of original.
 *
 * @param debug     Debug system
 * @param path      Path to visualize
 * @param color     Line color (0 for default)
 * @return          Path ID for later reference, or 0 on failure
 */
uint32_t agentite_debug_add_path(Agentite_DebugSystem *debug,
                                  const Agentite_Path *path,
                                  uint32_t color);

/**
 * Add a path associated with a specific entity.
 * Shows current waypoint based on entity's progress.
 *
 * @param debug     Debug system
 * @param entity_id Entity ID to associate with path
 * @param path      Path to visualize
 * @return          Path ID for later reference, or 0 on failure
 */
uint32_t agentite_debug_add_entity_path(Agentite_DebugSystem *debug,
                                         uint64_t entity_id,
                                         const Agentite_Path *path);

/**
 * Update current waypoint for an entity path.
 *
 * @param debug         Debug system
 * @param path_id       Path ID from add_path/add_entity_path
 * @param waypoint_idx  Current waypoint index
 */
void agentite_debug_set_path_waypoint(Agentite_DebugSystem *debug,
                                       uint32_t path_id,
                                       int waypoint_idx);

/**
 * Remove a specific path from visualization.
 *
 * @param debug     Debug system
 * @param path_id   Path ID to remove
 */
void agentite_debug_remove_path(Agentite_DebugSystem *debug, uint32_t path_id);

/**
 * Clear all visualized paths.
 *
 * @param debug     Debug system
 */
void agentite_debug_clear_paths(Agentite_DebugSystem *debug);

/* ============================================================================
 * Console Command System
 * ============================================================================ */

/**
 * Console command callback function type.
 *
 * @param debug     Debug system
 * @param argc      Number of arguments (including command name)
 * @param argv      Argument strings
 * @param userdata  User data from registration
 */
typedef void (*Agentite_DebugCommandFunc)(Agentite_DebugSystem *debug,
                                          int argc, const char **argv,
                                          void *userdata);

/**
 * Register a console command.
 *
 * @param debug     Debug system
 * @param name      Command name (case-insensitive, max 31 chars)
 * @param help      Help text for the command (max 127 chars)
 * @param func      Command callback function
 * @param userdata  User data passed to callback
 * @return          true if registered successfully
 */
bool agentite_debug_register_command(Agentite_DebugSystem *debug,
                                      const char *name,
                                      const char *help,
                                      Agentite_DebugCommandFunc func,
                                      void *userdata);

/**
 * Unregister a console command.
 *
 * @param debug     Debug system
 * @param name      Command name to unregister
 * @return          true if command was found and removed
 */
bool agentite_debug_unregister_command(Agentite_DebugSystem *debug,
                                        const char *name);

/**
 * Execute a console command string.
 *
 * @param debug     Debug system
 * @param command   Command string (e.g., "spawn enemy 10 20")
 * @return          true if command was found (not necessarily successful)
 */
bool agentite_debug_execute(Agentite_DebugSystem *debug, const char *command);

/**
 * Print message to console output buffer.
 *
 * @param debug     Debug system
 * @param fmt       Printf-style format string
 * @param ...       Format arguments
 */
void agentite_debug_print(Agentite_DebugSystem *debug, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * Print error message to console output buffer.
 * Displayed in error color.
 *
 * @param debug     Debug system
 * @param fmt       Printf-style format string
 * @param ...       Format arguments
 */
void agentite_debug_error(Agentite_DebugSystem *debug, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * Print message to console using va_list.
 *
 * @param debug     Debug system
 * @param fmt       Printf-style format string
 * @param args      Format arguments
 */
void agentite_debug_vprint(Agentite_DebugSystem *debug, const char *fmt, va_list args);

/**
 * Get console output lines for custom rendering.
 *
 * @param debug     Debug system
 * @param out_lines Output array of line pointers (borrowed)
 * @param max_lines Maximum lines to return
 * @return          Number of lines returned
 */
int agentite_debug_get_output(const Agentite_DebugSystem *debug,
                               const char **out_lines, int max_lines);

/**
 * Clear console output buffer.
 *
 * @param debug     Debug system
 */
void agentite_debug_clear_output(Agentite_DebugSystem *debug);

/**
 * Toggle console visibility.
 *
 * @param debug     Debug system
 */
void agentite_debug_toggle_console(Agentite_DebugSystem *debug);

/**
 * Set console visibility.
 *
 * @param debug     Debug system
 * @param open      true to open, false to close
 */
void agentite_debug_set_console_open(Agentite_DebugSystem *debug, bool open);

/**
 * Check if console is open.
 *
 * @param debug     Debug system
 * @return          true if console is visible
 */
bool agentite_debug_console_is_open(const Agentite_DebugSystem *debug);

/**
 * Draw console panel (input field + output).
 * Handles text input internally.
 *
 * @param debug     Debug system
 * @param ui        UI context
 * @param x, y      Panel position
 * @param w, h      Panel size
 * @return          true if console consumed input this frame
 */
bool agentite_debug_console_panel(Agentite_DebugSystem *debug,
                                   AUI_Context *ui,
                                   float x, float y, float w, float h);

/**
 * Process SDL event for console input.
 * Call this in your event loop when console is open.
 *
 * @param debug     Debug system
 * @param event     SDL event
 * @return          true if event was consumed
 */
bool agentite_debug_console_event(Agentite_DebugSystem *debug, const void *event);

/* ============================================================================
 * Built-in Commands
 * ============================================================================
 *
 * The following commands are registered automatically:
 *
 * help [command]     - List all commands or show help for a specific command
 * debug <flag>       - Toggle debug visualization flag
 *                      Flags: entities, collision, paths, spatial, fog, turn, fps
 * clear              - Clear console output
 * fps                - Show current FPS and frame time
 * entities           - Show entity count
 * memory             - Show memory statistics
 * flags              - Show current debug flags
 * bind               - Show bound systems
 */

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_DEBUG_H */
