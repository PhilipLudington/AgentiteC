/**
 * @file debug_internal.h
 * @brief Internal types shared between debug system source files
 */

#ifndef AGENTITE_DEBUG_INTERNAL_H
#define AGENTITE_DEBUG_INTERNAL_H

#include "agentite/debug.h"
#include <stdbool.h>

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define DEBUG_MAX_COMMANDS 64
#define DEBUG_MAX_CMD_NAME 32
#define DEBUG_MAX_CMD_HELP 128
#define DEBUG_MAX_INPUT 256
#define DEBUG_MAX_OUTPUT_LINE 512
#define DEBUG_MAX_PATHS 64
#define DEBUG_MAX_ARGS 16

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/** Console command entry */
typedef struct DebugCommand {
    char name[DEBUG_MAX_CMD_NAME];
    char help[DEBUG_MAX_CMD_HELP];
    Agentite_DebugCommandFunc func;
    void *userdata;
    bool active;
} DebugCommand;

/** Output line with type */
typedef struct DebugOutputLine {
    char text[DEBUG_MAX_OUTPUT_LINE];
    bool is_error;
} DebugOutputLine;

/** Console state */
typedef struct DebugConsole {
    /* Commands */
    DebugCommand commands[DEBUG_MAX_COMMANDS];
    int command_count;

    /* Input */
    char input_buffer[DEBUG_MAX_INPUT];
    int input_len;
    int cursor_pos;

    /* History */
    char **history;
    int history_capacity;
    int history_count;
    int history_index;  /* -1 = current input, 0+ = history item */

    /* Output (ring buffer) */
    DebugOutputLine *output;
    int output_capacity;
    int output_head;    /* Next write position */
    int output_count;   /* Number of lines in buffer */

    /* State */
    bool is_open;
    float scroll_y;
} DebugConsole;

/** Debug path visualization data */
typedef struct DebugPath {
    float *points_x;         /* X coordinates (owned) */
    float *points_y;         /* Y coordinates (owned) */
    int length;              /* Number of points */
    uint32_t color;          /* Line color */
    uint64_t entity_id;      /* Associated entity (0 if none) */
    int current_waypoint;    /* Current waypoint index */
    bool active;             /* Slot is in use */
} DebugPath;

/* ============================================================================
 * Internal Accessor Functions (defined in debug.cpp)
 * ============================================================================ */

#ifdef __cplusplus
extern "C" {
#endif

Agentite_DebugConfig *debug_get_config(Agentite_DebugSystem *debug);
Agentite_World *debug_get_ecs(Agentite_DebugSystem *debug);
Agentite_CollisionWorld *debug_get_collision(Agentite_DebugSystem *debug);
Agentite_FogOfWar *debug_get_fog(Agentite_DebugSystem *debug);
Agentite_TurnManager *debug_get_turn(Agentite_DebugSystem *debug);
Agentite_SpatialIndex *debug_get_spatial(Agentite_DebugSystem *debug);
Agentite_Profiler *debug_get_profiler(Agentite_DebugSystem *debug);
DebugPath *debug_get_paths(Agentite_DebugSystem *debug, int *count);
DebugConsole *debug_get_console(Agentite_DebugSystem *debug);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_DEBUG_INTERNAL_H */
