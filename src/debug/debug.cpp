/**
 * @file debug.cpp
 * @brief Enhanced Debug Tools - Core System Implementation
 */

#include "agentite/debug.h"
#include "agentite/error.h"
#include "agentite/pathfinding.h"
#include "debug_internal.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/** Main debug system */
struct Agentite_DebugSystem {
    Agentite_DebugConfig config;
    uint32_t flags;
    bool enabled;

    /* Bound systems (borrowed references) */
    Agentite_World *ecs_world;
    Agentite_CollisionWorld *collision_world;
    Agentite_Pathfinder *pathfinder;
    Agentite_FogOfWar *fog;
    Agentite_TurnManager *turn_manager;
    Agentite_SpatialIndex *spatial;
    Agentite_Profiler *profiler;

    /* Path visualization */
    DebugPath paths[DEBUG_MAX_PATHS];
    int path_count;
    uint32_t next_path_id;

    /* Console */
    DebugConsole console;
};

/* ============================================================================
 * Forward Declarations - Built-in Commands
 * ============================================================================ */

static void cmd_help(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud);
static void cmd_debug(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud);
static void cmd_clear(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud);
static void cmd_fps(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud);
static void cmd_entities(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud);
static void cmd_memory(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud);
static void cmd_flags(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud);
static void cmd_bind(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud);

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void console_init(DebugConsole *console, int max_history, int max_output)
{
    memset(console, 0, sizeof(*console));

    console->history_capacity = max_history;
    console->history = (char **)calloc((size_t)max_history, sizeof(char *));

    console->output_capacity = max_output;
    console->output = (DebugOutputLine *)calloc((size_t)max_output, sizeof(DebugOutputLine));

    console->history_index = -1;
}

static void console_deinit(DebugConsole *console)
{
    /* Free history */
    if (console->history) {
        for (int i = 0; i < console->history_count; i++) {
            free(console->history[i]);
        }
        free(console->history);
    }

    /* Free output */
    free(console->output);

    memset(console, 0, sizeof(*console));
}

static void console_add_history(DebugConsole *console, const char *command)
{
    if (!console->history || console->history_capacity <= 0) return;
    if (!command || command[0] == '\0') return;

    /* Don't add duplicates of last command */
    if (console->history_count > 0 &&
        strcmp(console->history[console->history_count - 1], command) == 0) {
        return;
    }

    /* Shift old entries if at capacity */
    if (console->history_count >= console->history_capacity) {
        free(console->history[0]);
        memmove(&console->history[0], &console->history[1],
                (size_t)(console->history_capacity - 1) * sizeof(char *));
        console->history_count--;
    }

    /* Add new entry */
    console->history[console->history_count] = strdup(command);
    if (console->history[console->history_count]) {
        console->history_count++;
    }
}

static void console_add_output(DebugConsole *console, const char *text, bool is_error)
{
    if (!console->output || console->output_capacity <= 0) return;

    DebugOutputLine *line = &console->output[console->output_head];
    snprintf(line->text, DEBUG_MAX_OUTPUT_LINE, "%s", text);
    line->is_error = is_error;

    console->output_head = (console->output_head + 1) % console->output_capacity;
    if (console->output_count < console->output_capacity) {
        console->output_count++;
    }
}

static DebugCommand *find_command(DebugConsole *console, const char *name)
{
    for (int i = 0; i < DEBUG_MAX_COMMANDS; i++) {
        if (console->commands[i].active &&
            strcasecmp(console->commands[i].name, name) == 0) {
            return &console->commands[i];
        }
    }
    return NULL;
}

static void register_builtin_commands(Agentite_DebugSystem *debug)
{
    agentite_debug_register_command(debug, "help", "List commands or show help: help [command]", cmd_help, NULL);
    agentite_debug_register_command(debug, "debug", "Toggle debug flag: debug <flag>", cmd_debug, NULL);
    agentite_debug_register_command(debug, "clear", "Clear console output", cmd_clear, NULL);
    agentite_debug_register_command(debug, "fps", "Show current FPS", cmd_fps, NULL);
    agentite_debug_register_command(debug, "entities", "Show entity count", cmd_entities, NULL);
    agentite_debug_register_command(debug, "memory", "Show memory statistics", cmd_memory, NULL);
    agentite_debug_register_command(debug, "flags", "Show current debug flags", cmd_flags, NULL);
    agentite_debug_register_command(debug, "bind", "Show bound systems", cmd_bind, NULL);
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

Agentite_DebugSystem *agentite_debug_create(const Agentite_DebugConfig *config)
{
    Agentite_DebugSystem *debug = (Agentite_DebugSystem *)calloc(1, sizeof(*debug));
    if (!debug) {
        agentite_set_error("Failed to allocate debug system");
        return NULL;
    }

    /* Apply config or defaults */
    if (config) {
        debug->config = *config;
    } else {
        Agentite_DebugConfig defaults = AGENTITE_DEBUG_CONFIG_DEFAULT;
        debug->config = defaults;
    }

    /* Initialize state */
    debug->flags = AGENTITE_DEBUG_NONE;
    debug->enabled = true;
    debug->next_path_id = 1;

    /* Initialize console */
    console_init(&debug->console, debug->config.console_max_history,
                 debug->config.console_max_output);
    if (!debug->console.history || !debug->console.output) {
        agentite_set_error("Failed to allocate console buffers");
        agentite_debug_destroy(debug);
        return NULL;
    }

    /* Register built-in commands */
    register_builtin_commands(debug);

    /* Print welcome message */
    agentite_debug_print(debug, "Agentite Debug Console");
    agentite_debug_print(debug, "Type 'help' for available commands.");

    return debug;
}

void agentite_debug_destroy(Agentite_DebugSystem *debug)
{
    if (!debug) return;

    /* Free paths */
    for (int i = 0; i < DEBUG_MAX_PATHS; i++) {
        if (debug->paths[i].active) {
            free(debug->paths[i].points_x);
            free(debug->paths[i].points_y);
        }
    }

    /* Free console */
    console_deinit(&debug->console);

    free(debug);
}

/* ============================================================================
 * Enable/Disable Controls
 * ============================================================================ */

void agentite_debug_set_flags(Agentite_DebugSystem *debug, uint32_t flags)
{
    if (debug) {
        debug->flags = flags;
    }
}

uint32_t agentite_debug_get_flags(const Agentite_DebugSystem *debug)
{
    return debug ? debug->flags : 0;
}

void agentite_debug_toggle(Agentite_DebugSystem *debug, Agentite_DebugFlags flag)
{
    if (debug) {
        debug->flags ^= flag;
    }
}

bool agentite_debug_is_enabled(const Agentite_DebugSystem *debug, Agentite_DebugFlags flag)
{
    return debug && (debug->flags & flag);
}

void agentite_debug_set_enabled(Agentite_DebugSystem *debug, bool enabled)
{
    if (debug) {
        debug->enabled = enabled;
    }
}

bool agentite_debug_get_enabled(const Agentite_DebugSystem *debug)
{
    return debug && debug->enabled;
}

/* ============================================================================
 * System Bindings
 * ============================================================================ */

void agentite_debug_bind_ecs(Agentite_DebugSystem *debug, Agentite_World *world)
{
    if (debug) {
        debug->ecs_world = world;
    }
}

void agentite_debug_bind_collision(Agentite_DebugSystem *debug, Agentite_CollisionWorld *collision)
{
    if (debug) {
        debug->collision_world = collision;
    }
}

void agentite_debug_bind_pathfinder(Agentite_DebugSystem *debug, Agentite_Pathfinder *pathfinder)
{
    if (debug) {
        debug->pathfinder = pathfinder;
    }
}

void agentite_debug_bind_fog(Agentite_DebugSystem *debug, Agentite_FogOfWar *fog)
{
    if (debug) {
        debug->fog = fog;
    }
}

void agentite_debug_bind_turn(Agentite_DebugSystem *debug, Agentite_TurnManager *turn)
{
    if (debug) {
        debug->turn_manager = turn;
    }
}

void agentite_debug_bind_spatial(Agentite_DebugSystem *debug, Agentite_SpatialIndex *spatial)
{
    if (debug) {
        debug->spatial = spatial;
    }
}

void agentite_debug_bind_profiler(Agentite_DebugSystem *debug, Agentite_Profiler *profiler)
{
    if (debug) {
        debug->profiler = profiler;
    }
}

/* ============================================================================
 * Path Visualization
 * ============================================================================ */

uint32_t agentite_debug_add_path(Agentite_DebugSystem *debug,
                                  const Agentite_Path *path,
                                  uint32_t color)
{
    if (!debug || !path || path->length <= 0) return 0;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < DEBUG_MAX_PATHS; i++) {
        if (!debug->paths[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return 0;

    /* Allocate point arrays */
    DebugPath *dp = &debug->paths[slot];
    dp->points_x = (float *)malloc((size_t)path->length * sizeof(float));
    dp->points_y = (float *)malloc((size_t)path->length * sizeof(float));
    if (!dp->points_x || !dp->points_y) {
        free(dp->points_x);
        free(dp->points_y);
        return 0;
    }

    /* Copy points, converting tile coords to world coords */
    float tile_size = debug->config.path_tile_size;
    if (tile_size <= 0) tile_size = 32.0f;
    for (int i = 0; i < path->length; i++) {
        dp->points_x[i] = (float)path->points[i].x * tile_size + tile_size * 0.5f;
        dp->points_y[i] = (float)path->points[i].y * tile_size + tile_size * 0.5f;
    }

    dp->length = path->length;
    dp->color = color ? color : debug->config.path_line_color;
    dp->entity_id = 0;
    dp->current_waypoint = 0;
    dp->active = true;
    debug->path_count++;

    uint32_t id = debug->next_path_id++;
    return id;
}

uint32_t agentite_debug_add_entity_path(Agentite_DebugSystem *debug,
                                         uint64_t entity_id,
                                         const Agentite_Path *path)
{
    uint32_t id = agentite_debug_add_path(debug, path, 0);
    if (id && debug) {
        /* Find the path we just added (last active slot with matching id concept) */
        for (int i = DEBUG_MAX_PATHS - 1; i >= 0; i--) {
            if (debug->paths[i].active && debug->paths[i].entity_id == 0) {
                debug->paths[i].entity_id = entity_id;
                break;
            }
        }
    }
    return id;
}

void agentite_debug_set_path_waypoint(Agentite_DebugSystem *debug,
                                       uint32_t path_id,
                                       int waypoint_idx)
{
    (void)path_id;  /* TODO: Track path IDs properly */
    if (!debug) return;

    /* For now, set on all active paths (simplified) */
    for (int i = 0; i < DEBUG_MAX_PATHS; i++) {
        if (debug->paths[i].active) {
            if (waypoint_idx >= 0 && waypoint_idx < debug->paths[i].length) {
                debug->paths[i].current_waypoint = waypoint_idx;
            }
        }
    }
}

void agentite_debug_remove_path(Agentite_DebugSystem *debug, uint32_t path_id)
{
    (void)path_id;  /* TODO: Track path IDs properly */
    if (!debug) return;

    /* Remove first active path (simplified) */
    for (int i = 0; i < DEBUG_MAX_PATHS; i++) {
        if (debug->paths[i].active) {
            free(debug->paths[i].points_x);
            free(debug->paths[i].points_y);
            memset(&debug->paths[i], 0, sizeof(debug->paths[i]));
            debug->path_count--;
            break;
        }
    }
}

void agentite_debug_clear_paths(Agentite_DebugSystem *debug)
{
    if (!debug) return;

    for (int i = 0; i < DEBUG_MAX_PATHS; i++) {
        if (debug->paths[i].active) {
            free(debug->paths[i].points_x);
            free(debug->paths[i].points_y);
            memset(&debug->paths[i], 0, sizeof(debug->paths[i]));
        }
    }
    debug->path_count = 0;
}

/* ============================================================================
 * Console Command Registration
 * ============================================================================ */

bool agentite_debug_register_command(Agentite_DebugSystem *debug,
                                      const char *name,
                                      const char *help,
                                      Agentite_DebugCommandFunc func,
                                      void *userdata)
{
    if (!debug || !name || !func) return false;

    DebugConsole *console = &debug->console;

    /* Check for duplicate */
    if (find_command(console, name)) {
        return false;  /* Already registered */
    }

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < DEBUG_MAX_COMMANDS; i++) {
        if (!console->commands[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return false;

    /* Register */
    DebugCommand *cmd = &console->commands[slot];
    snprintf(cmd->name, DEBUG_MAX_CMD_NAME, "%s", name);
    snprintf(cmd->help, DEBUG_MAX_CMD_HELP, "%s", help ? help : "");
    cmd->func = func;
    cmd->userdata = userdata;
    cmd->active = true;
    console->command_count++;

    return true;
}

bool agentite_debug_unregister_command(Agentite_DebugSystem *debug, const char *name)
{
    if (!debug || !name) return false;

    DebugCommand *cmd = find_command(&debug->console, name);
    if (cmd) {
        memset(cmd, 0, sizeof(*cmd));
        debug->console.command_count--;
        return true;
    }
    return false;
}

bool agentite_debug_execute(Agentite_DebugSystem *debug, const char *command)
{
    if (!debug || !command) return false;

    /* Skip leading whitespace */
    while (*command && isspace((unsigned char)*command)) command++;
    if (*command == '\0') return false;

    /* Add to history */
    console_add_history(&debug->console, command);

    /* Tokenize (simple split on whitespace) */
    char buffer[DEBUG_MAX_INPUT];
    snprintf(buffer, sizeof(buffer), "%s", command);

    const char *argv[DEBUG_MAX_ARGS];
    int argc = 0;

    char *token = strtok(buffer, " \t");
    while (token && argc < DEBUG_MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }

    if (argc == 0) return false;

    /* Find and execute command */
    DebugCommand *cmd = find_command(&debug->console, argv[0]);
    if (cmd) {
        cmd->func(debug, argc, argv, cmd->userdata);
        return true;
    }

    agentite_debug_error(debug, "Unknown command: %s", argv[0]);
    return false;
}

/* ============================================================================
 * Console Output
 * ============================================================================ */

void agentite_debug_print(Agentite_DebugSystem *debug, const char *fmt, ...)
{
    if (!debug) return;

    va_list args;
    va_start(args, fmt);
    agentite_debug_vprint(debug, fmt, args);
    va_end(args);
}

void agentite_debug_error(Agentite_DebugSystem *debug, const char *fmt, ...)
{
    if (!debug) return;

    char buffer[DEBUG_MAX_OUTPUT_LINE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    console_add_output(&debug->console, buffer, true);
}

void agentite_debug_vprint(Agentite_DebugSystem *debug, const char *fmt, va_list args)
{
    if (!debug) return;

    char buffer[DEBUG_MAX_OUTPUT_LINE];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    console_add_output(&debug->console, buffer, false);
}

int agentite_debug_get_output(const Agentite_DebugSystem *debug,
                               const char **out_lines, int max_lines)
{
    if (!debug || !out_lines || max_lines <= 0) return 0;

    const DebugConsole *console = &debug->console;
    int count = console->output_count < max_lines ? console->output_count : max_lines;

    /* Return lines in order (oldest first) */
    int start = (console->output_head - console->output_count + console->output_capacity)
                % console->output_capacity;

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % console->output_capacity;
        out_lines[i] = console->output[idx].text;
    }

    return count;
}

void agentite_debug_clear_output(Agentite_DebugSystem *debug)
{
    if (!debug) return;
    debug->console.output_count = 0;
    debug->console.output_head = 0;
}

/* ============================================================================
 * Console Visibility
 * ============================================================================ */

void agentite_debug_toggle_console(Agentite_DebugSystem *debug)
{
    if (debug) {
        debug->console.is_open = !debug->console.is_open;
        debug->console.history_index = -1;

        /* Start/stop SDL text input for text events */
        if (debug->console.is_open) {
            SDL_StartTextInput(SDL_GetKeyboardFocus());
        } else {
            SDL_StopTextInput(SDL_GetKeyboardFocus());
        }
    }
}

void agentite_debug_set_console_open(Agentite_DebugSystem *debug, bool open)
{
    if (debug) {
        debug->console.is_open = open;
        if (open) {
            debug->console.history_index = -1;
            SDL_StartTextInput(SDL_GetKeyboardFocus());
        } else {
            SDL_StopTextInput(SDL_GetKeyboardFocus());
        }
    }
}

bool agentite_debug_console_is_open(const Agentite_DebugSystem *debug)
{
    return debug && debug->console.is_open;
}

/* ============================================================================
 * Built-in Command Implementations
 * ============================================================================ */

static void cmd_help(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud)
{
    (void)ud;

    if (argc > 1) {
        /* Show help for specific command */
        DebugCommand *cmd = find_command(&debug->console, argv[1]);
        if (cmd) {
            agentite_debug_print(debug, "%s: %s", cmd->name, cmd->help);
        } else {
            agentite_debug_error(debug, "Unknown command: %s", argv[1]);
        }
        return;
    }

    /* List all commands */
    agentite_debug_print(debug, "Available commands:");
    for (int i = 0; i < DEBUG_MAX_COMMANDS; i++) {
        if (debug->console.commands[i].active) {
            agentite_debug_print(debug, "  %s - %s",
                                  debug->console.commands[i].name,
                                  debug->console.commands[i].help);
        }
    }
}

static void cmd_debug(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud)
{
    (void)ud;

    if (argc < 2) {
        agentite_debug_print(debug, "Usage: debug <flag>");
        agentite_debug_print(debug, "Flags: entities, collision, paths, spatial, fog, turn, fps, all");
        return;
    }

    const char *flag = argv[1];
    Agentite_DebugFlags f = AGENTITE_DEBUG_NONE;

    if (strcasecmp(flag, "entities") == 0) f = AGENTITE_DEBUG_ENTITY_GIZMOS;
    else if (strcasecmp(flag, "collision") == 0) f = AGENTITE_DEBUG_COLLISION_SHAPES;
    else if (strcasecmp(flag, "paths") == 0) f = AGENTITE_DEBUG_AI_PATHS;
    else if (strcasecmp(flag, "spatial") == 0) f = AGENTITE_DEBUG_SPATIAL_GRID;
    else if (strcasecmp(flag, "fog") == 0) f = AGENTITE_DEBUG_FOG_OF_WAR;
    else if (strcasecmp(flag, "turn") == 0) f = AGENTITE_DEBUG_TURN_STATE;
    else if (strcasecmp(flag, "fps") == 0) f = AGENTITE_DEBUG_PERFORMANCE;
    else if (strcasecmp(flag, "all") == 0) f = AGENTITE_DEBUG_ALL;
    else {
        agentite_debug_error(debug, "Unknown flag: %s", flag);
        return;
    }

    agentite_debug_toggle(debug, f);
    bool enabled = agentite_debug_is_enabled(debug, f);
    agentite_debug_print(debug, "%s: %s", flag, enabled ? "ON" : "OFF");
}

static void cmd_clear(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud)
{
    (void)argc;
    (void)argv;
    (void)ud;
    agentite_debug_clear_output(debug);
}

static void cmd_fps(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud)
{
    (void)argc;
    (void)argv;
    (void)ud;

    if (debug->profiler) {
        agentite_debug_print(debug, "(Profiler bound - FPS available in performance overlay)");
        agentite_debug_print(debug, "Enable with: debug fps");
    } else {
        agentite_debug_print(debug, "Profiler not bound. Use agentite_debug_bind_profiler()");
    }
}

static void cmd_entities(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud)
{
    (void)argc;
    (void)argv;
    (void)ud;

    if (debug->ecs_world) {
        agentite_debug_print(debug, "(ECS bound - entity count available)");
    } else {
        agentite_debug_print(debug, "ECS not bound. Use agentite_debug_bind_ecs()");
    }
}

static void cmd_memory(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud)
{
    (void)argc;
    (void)argv;
    (void)ud;

    if (debug->profiler) {
        agentite_debug_print(debug, "(Profiler bound - memory stats available)");
    } else {
        agentite_debug_print(debug, "Profiler not bound. Use agentite_debug_bind_profiler()");
    }
}

static void cmd_flags(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud)
{
    (void)argc;
    (void)argv;
    (void)ud;

    agentite_debug_print(debug, "Debug flags: 0x%X", debug->flags);
    agentite_debug_print(debug, "  entities:  %s", (debug->flags & AGENTITE_DEBUG_ENTITY_GIZMOS) ? "ON" : "OFF");
    agentite_debug_print(debug, "  collision: %s", (debug->flags & AGENTITE_DEBUG_COLLISION_SHAPES) ? "ON" : "OFF");
    agentite_debug_print(debug, "  paths:     %s", (debug->flags & AGENTITE_DEBUG_AI_PATHS) ? "ON" : "OFF");
    agentite_debug_print(debug, "  spatial:   %s", (debug->flags & AGENTITE_DEBUG_SPATIAL_GRID) ? "ON" : "OFF");
    agentite_debug_print(debug, "  fog:       %s", (debug->flags & AGENTITE_DEBUG_FOG_OF_WAR) ? "ON" : "OFF");
    agentite_debug_print(debug, "  turn:      %s", (debug->flags & AGENTITE_DEBUG_TURN_STATE) ? "ON" : "OFF");
    agentite_debug_print(debug, "  fps:       %s", (debug->flags & AGENTITE_DEBUG_PERFORMANCE) ? "ON" : "OFF");
}

static void cmd_bind(Agentite_DebugSystem *debug, int argc, const char **argv, void *ud)
{
    (void)argc;
    (void)argv;
    (void)ud;

    agentite_debug_print(debug, "Bound systems:");
    agentite_debug_print(debug, "  ECS:       %s", debug->ecs_world ? "yes" : "no");
    agentite_debug_print(debug, "  Collision: %s", debug->collision_world ? "yes" : "no");
    agentite_debug_print(debug, "  Pathfinder:%s", debug->pathfinder ? "yes" : "no");
    agentite_debug_print(debug, "  Fog:       %s", debug->fog ? "yes" : "no");
    agentite_debug_print(debug, "  Turn:      %s", debug->turn_manager ? "yes" : "no");
    agentite_debug_print(debug, "  Spatial:   %s", debug->spatial ? "yes" : "no");
    agentite_debug_print(debug, "  Profiler:  %s", debug->profiler ? "yes" : "no");
}

/* ============================================================================
 * Accessors for debug_draw.cpp
 * ============================================================================ */

/* These functions provide access to internal state for the drawing module */

extern "C" {
    Agentite_DebugConfig *debug_get_config(Agentite_DebugSystem *debug) {
        return debug ? &debug->config : NULL;
    }

    Agentite_World *debug_get_ecs(Agentite_DebugSystem *debug) {
        return debug ? debug->ecs_world : NULL;
    }

    Agentite_CollisionWorld *debug_get_collision(Agentite_DebugSystem *debug) {
        return debug ? debug->collision_world : NULL;
    }

    Agentite_FogOfWar *debug_get_fog(Agentite_DebugSystem *debug) {
        return debug ? debug->fog : NULL;
    }

    Agentite_TurnManager *debug_get_turn(Agentite_DebugSystem *debug) {
        return debug ? debug->turn_manager : NULL;
    }

    Agentite_SpatialIndex *debug_get_spatial(Agentite_DebugSystem *debug) {
        return debug ? debug->spatial : NULL;
    }

    Agentite_Profiler *debug_get_profiler(Agentite_DebugSystem *debug) {
        return debug ? debug->profiler : NULL;
    }

    DebugPath *debug_get_paths(Agentite_DebugSystem *debug, int *count) {
        if (!debug) {
            if (count) *count = 0;
            return NULL;
        }
        if (count) *count = DEBUG_MAX_PATHS;
        return debug->paths;
    }

    DebugConsole *debug_get_console(Agentite_DebugSystem *debug) {
        return debug ? &debug->console : NULL;
    }
}
