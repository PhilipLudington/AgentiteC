/**
 * Carbon Command Queue System
 *
 * Validated, atomic command execution for player actions. Provides
 * command registration, pre-execution validation, queued execution
 * during turn processing, and command history for undo/replay.
 *
 * Usage:
 *   // Create command system
 *   Carbon_CommandSystem *sys = carbon_command_create();
 *
 *   // Register command types
 *   carbon_command_register(sys, CMD_MOVE_UNIT, validate_move, execute_move);
 *   carbon_command_register(sys, CMD_BUILD, validate_build, execute_build);
 *
 *   // Create and validate command
 *   Carbon_Command *cmd = carbon_command_new(CMD_MOVE_UNIT);
 *   carbon_command_set_entity(cmd, "unit", player_unit);
 *   carbon_command_set_int(cmd, "dest_x", 10);
 *   carbon_command_set_int(cmd, "dest_y", 20);
 *
 *   Carbon_CommandResult result = carbon_command_validate(sys, cmd, game_state);
 *   if (result.success) {
 *       carbon_command_queue(sys, cmd);
 *   }
 *
 *   // Execute all queued commands
 *   Carbon_CommandResult results[32];
 *   int count = carbon_command_execute_all(sys, game_state, results, 32);
 *
 *   // Cleanup
 *   carbon_command_destroy(sys);
 */

#ifndef CARBON_COMMAND_H
#define CARBON_COMMAND_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_COMMAND_MAX_PARAMS      16    /* Maximum parameters per command */
#define CARBON_COMMAND_MAX_PARAM_KEY   32    /* Maximum key length */
#define CARBON_COMMAND_MAX_ERROR       128   /* Maximum error message length */
#define CARBON_COMMAND_MAX_QUEUE       64    /* Maximum queued commands */
#define CARBON_COMMAND_MAX_TYPES       64    /* Maximum registered command types */
#define CARBON_COMMAND_MAX_HISTORY     256   /* Maximum history entries */

/*============================================================================
 * Parameter Types
 *============================================================================*/

/**
 * Command parameter value types.
 */
typedef enum Carbon_CommandParamType {
    CARBON_CMD_PARAM_NONE = 0,
    CARBON_CMD_PARAM_INT,
    CARBON_CMD_PARAM_INT64,
    CARBON_CMD_PARAM_FLOAT,
    CARBON_CMD_PARAM_DOUBLE,
    CARBON_CMD_PARAM_BOOL,
    CARBON_CMD_PARAM_ENTITY,
    CARBON_CMD_PARAM_STRING,
    CARBON_CMD_PARAM_PTR,
} Carbon_CommandParamType;

/**
 * Command parameter value.
 */
typedef struct Carbon_CommandParam {
    char key[CARBON_COMMAND_MAX_PARAM_KEY];
    Carbon_CommandParamType type;
    union {
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        bool b;
        uint32_t entity;
        char str[CARBON_COMMAND_MAX_PARAM_KEY];
        void *ptr;
    };
} Carbon_CommandParam;

/*============================================================================
 * Command Structure
 *============================================================================*/

/**
 * A command with typed parameters.
 */
typedef struct Carbon_Command {
    int type;                                       /* Command type ID */
    Carbon_CommandParam params[CARBON_COMMAND_MAX_PARAMS];
    int param_count;
    uint32_t sequence;                              /* Sequence number for ordering */
    int32_t source_faction;                         /* Faction that issued command (-1 = any) */
    void *userdata;                                 /* User-defined data */
} Carbon_Command;

/**
 * Result of command validation or execution.
 */
typedef struct Carbon_CommandResult {
    bool success;                                   /* Whether command succeeded */
    int command_type;                               /* Type of command */
    uint32_t sequence;                              /* Command sequence number */
    char error[CARBON_COMMAND_MAX_ERROR];           /* Error message if failed */
} Carbon_CommandResult;

/*============================================================================
 * Command System Forward Declaration
 *============================================================================*/

typedef struct Carbon_CommandSystem Carbon_CommandSystem;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Command validator callback.
 * Called before execution to check if command is valid.
 *
 * @param cmd        Command to validate
 * @param game_state Game state pointer
 * @param error_buf  Buffer for error message
 * @param error_size Size of error buffer
 * @return true if command is valid
 */
typedef bool (*Carbon_CommandValidator)(const Carbon_Command *cmd,
                                         void *game_state,
                                         char *error_buf,
                                         size_t error_size);

/**
 * Command executor callback.
 * Called to execute a validated command.
 *
 * @param cmd        Command to execute
 * @param game_state Game state pointer
 * @return true if execution succeeded
 */
typedef bool (*Carbon_CommandExecutor)(const Carbon_Command *cmd,
                                         void *game_state);

/**
 * Command execution callback (optional).
 * Called after each command execution.
 *
 * @param sys      Command system
 * @param cmd      Executed command
 * @param result   Execution result
 * @param userdata User context
 */
typedef void (*Carbon_CommandCallback)(Carbon_CommandSystem *sys,
                                         const Carbon_Command *cmd,
                                         const Carbon_CommandResult *result,
                                         void *userdata);

/*============================================================================
 * Lifecycle
 *============================================================================*/

/**
 * Create a new command system.
 *
 * @return New command system or NULL on failure
 */
Carbon_CommandSystem *carbon_command_create(void);

/**
 * Destroy a command system and free resources.
 *
 * @param sys Command system to destroy
 */
void carbon_command_destroy(Carbon_CommandSystem *sys);

/*============================================================================
 * Command Type Registration
 *============================================================================*/

/**
 * Register a command type with validator and executor.
 *
 * @param sys       Command system
 * @param type      Unique command type ID
 * @param validator Validation callback
 * @param executor  Execution callback
 * @return true if registered successfully
 */
bool carbon_command_register(Carbon_CommandSystem *sys,
                              int type,
                              Carbon_CommandValidator validator,
                              Carbon_CommandExecutor executor);

/**
 * Register a command type with name.
 *
 * @param sys       Command system
 * @param type      Unique command type ID
 * @param name      Human-readable name
 * @param validator Validation callback
 * @param executor  Execution callback
 * @return true if registered successfully
 */
bool carbon_command_register_named(Carbon_CommandSystem *sys,
                                    int type,
                                    const char *name,
                                    Carbon_CommandValidator validator,
                                    Carbon_CommandExecutor executor);

/**
 * Check if a command type is registered.
 *
 * @param sys  Command system
 * @param type Command type ID
 * @return true if registered
 */
bool carbon_command_is_registered(const Carbon_CommandSystem *sys, int type);

/**
 * Get command type name.
 *
 * @param sys  Command system
 * @param type Command type ID
 * @return Name or NULL if not found
 */
const char *carbon_command_get_type_name(const Carbon_CommandSystem *sys, int type);

/*============================================================================
 * Command Creation
 *============================================================================*/

/**
 * Create a new command.
 *
 * @param type Command type ID
 * @return New command or NULL on failure
 */
Carbon_Command *carbon_command_new(int type);

/**
 * Create a command with source faction.
 *
 * @param type    Command type ID
 * @param faction Source faction ID
 * @return New command or NULL on failure
 */
Carbon_Command *carbon_command_new_ex(int type, int32_t faction);

/**
 * Clone a command.
 *
 * @param cmd Command to clone
 * @return New command copy or NULL on failure
 */
Carbon_Command *carbon_command_clone(const Carbon_Command *cmd);

/**
 * Free a command.
 *
 * @param cmd Command to free
 */
void carbon_command_free(Carbon_Command *cmd);

/*============================================================================
 * Command Parameters
 *============================================================================*/

/**
 * Set integer parameter.
 */
void carbon_command_set_int(Carbon_Command *cmd, const char *key, int32_t value);

/**
 * Set 64-bit integer parameter.
 */
void carbon_command_set_int64(Carbon_Command *cmd, const char *key, int64_t value);

/**
 * Set float parameter.
 */
void carbon_command_set_float(Carbon_Command *cmd, const char *key, float value);

/**
 * Set double parameter.
 */
void carbon_command_set_double(Carbon_Command *cmd, const char *key, double value);

/**
 * Set boolean parameter.
 */
void carbon_command_set_bool(Carbon_Command *cmd, const char *key, bool value);

/**
 * Set entity parameter.
 */
void carbon_command_set_entity(Carbon_Command *cmd, const char *key, uint32_t entity);

/**
 * Set string parameter.
 */
void carbon_command_set_string(Carbon_Command *cmd, const char *key, const char *value);

/**
 * Set pointer parameter (not owned).
 */
void carbon_command_set_ptr(Carbon_Command *cmd, const char *key, void *ptr);

/*============================================================================
 * Parameter Retrieval
 *============================================================================*/

/**
 * Check if parameter exists.
 */
bool carbon_command_has_param(const Carbon_Command *cmd, const char *key);

/**
 * Get parameter type.
 */
Carbon_CommandParamType carbon_command_get_param_type(const Carbon_Command *cmd, const char *key);

/**
 * Get integer parameter.
 */
int32_t carbon_command_get_int(const Carbon_Command *cmd, const char *key);

/**
 * Get integer parameter with default.
 */
int32_t carbon_command_get_int_or(const Carbon_Command *cmd, const char *key, int32_t def);

/**
 * Get 64-bit integer parameter.
 */
int64_t carbon_command_get_int64(const Carbon_Command *cmd, const char *key);

/**
 * Get float parameter.
 */
float carbon_command_get_float(const Carbon_Command *cmd, const char *key);

/**
 * Get float parameter with default.
 */
float carbon_command_get_float_or(const Carbon_Command *cmd, const char *key, float def);

/**
 * Get double parameter.
 */
double carbon_command_get_double(const Carbon_Command *cmd, const char *key);

/**
 * Get boolean parameter.
 */
bool carbon_command_get_bool(const Carbon_Command *cmd, const char *key);

/**
 * Get entity parameter.
 */
uint32_t carbon_command_get_entity(const Carbon_Command *cmd, const char *key);

/**
 * Get string parameter.
 */
const char *carbon_command_get_string(const Carbon_Command *cmd, const char *key);

/**
 * Get pointer parameter.
 */
void *carbon_command_get_ptr(const Carbon_Command *cmd, const char *key);

/*============================================================================
 * Validation
 *============================================================================*/

/**
 * Validate a command before execution.
 *
 * @param sys        Command system
 * @param cmd        Command to validate
 * @param game_state Game state pointer
 * @return Validation result
 */
Carbon_CommandResult carbon_command_validate(Carbon_CommandSystem *sys,
                                               const Carbon_Command *cmd,
                                               void *game_state);

/*============================================================================
 * Queue Operations
 *============================================================================*/

/**
 * Add a command to the queue.
 * Command is cloned; original can be freed.
 *
 * @param sys Command system
 * @param cmd Command to queue
 * @return true if queued successfully
 */
bool carbon_command_queue(Carbon_CommandSystem *sys, Carbon_Command *cmd);

/**
 * Add a command to the queue with validation.
 * Only queues if validation passes.
 *
 * @param sys        Command system
 * @param cmd        Command to queue
 * @param game_state Game state for validation
 * @return Validation result (success = queued)
 */
Carbon_CommandResult carbon_command_queue_validated(Carbon_CommandSystem *sys,
                                                      Carbon_Command *cmd,
                                                      void *game_state);

/**
 * Get number of queued commands.
 */
int carbon_command_queue_count(const Carbon_CommandSystem *sys);

/**
 * Clear command queue.
 */
void carbon_command_queue_clear(Carbon_CommandSystem *sys);

/**
 * Get queued command by index.
 */
const Carbon_Command *carbon_command_queue_get(const Carbon_CommandSystem *sys, int index);

/**
 * Remove queued command by index.
 */
bool carbon_command_queue_remove(Carbon_CommandSystem *sys, int index);

/*============================================================================
 * Execution
 *============================================================================*/

/**
 * Execute all queued commands.
 * Clears queue after execution.
 *
 * @param sys        Command system
 * @param game_state Game state pointer
 * @param results    Output array for results (NULL to skip)
 * @param max        Maximum results to return
 * @return Number of commands executed
 */
int carbon_command_execute_all(Carbon_CommandSystem *sys,
                                void *game_state,
                                Carbon_CommandResult *results,
                                int max);

/**
 * Execute a single command immediately (not from queue).
 *
 * @param sys        Command system
 * @param cmd        Command to execute
 * @param game_state Game state pointer
 * @return Execution result
 */
Carbon_CommandResult carbon_command_execute(Carbon_CommandSystem *sys,
                                              const Carbon_Command *cmd,
                                              void *game_state);

/**
 * Execute next queued command.
 *
 * @param sys        Command system
 * @param game_state Game state pointer
 * @return Execution result (success=false if queue empty)
 */
Carbon_CommandResult carbon_command_execute_next(Carbon_CommandSystem *sys,
                                                   void *game_state);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * Set execution callback.
 * Called after each command execution.
 *
 * @param sys      Command system
 * @param callback Callback function (NULL to clear)
 * @param userdata User context
 */
void carbon_command_set_callback(Carbon_CommandSystem *sys,
                                  Carbon_CommandCallback callback,
                                  void *userdata);

/*============================================================================
 * History
 *============================================================================*/

/**
 * Enable command history.
 *
 * @param sys          Command system
 * @param max_commands Maximum commands to keep (0 to disable)
 */
void carbon_command_enable_history(Carbon_CommandSystem *sys, int max_commands);

/**
 * Get command history.
 *
 * @param sys Command system
 * @param out Output array of command pointers
 * @param max Maximum commands to return
 * @return Number of commands returned (newest first)
 */
int carbon_command_get_history(const Carbon_CommandSystem *sys,
                                const Carbon_Command **out,
                                int max);

/**
 * Get history count.
 */
int carbon_command_get_history_count(const Carbon_CommandSystem *sys);

/**
 * Clear command history.
 */
void carbon_command_clear_history(Carbon_CommandSystem *sys);

/**
 * Replay command from history.
 *
 * @param sys        Command system
 * @param index      History index (0 = most recent)
 * @param game_state Game state pointer
 * @return Execution result
 */
Carbon_CommandResult carbon_command_replay(Carbon_CommandSystem *sys,
                                             int index,
                                             void *game_state);

/*============================================================================
 * Statistics
 *============================================================================*/

/**
 * Command system statistics.
 */
typedef struct Carbon_CommandStats {
    int total_executed;          /* Total commands executed */
    int total_succeeded;         /* Commands that succeeded */
    int total_failed;            /* Commands that failed */
    int total_invalid;           /* Commands that failed validation */
    int commands_by_type[CARBON_COMMAND_MAX_TYPES]; /* Per-type counts */
} Carbon_CommandStats;

/**
 * Get command system statistics.
 */
void carbon_command_get_stats(const Carbon_CommandSystem *sys, Carbon_CommandStats *out);

/**
 * Reset statistics.
 */
void carbon_command_reset_stats(Carbon_CommandSystem *sys);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Check if result indicates success.
 */
static inline bool carbon_command_result_ok(const Carbon_CommandResult *result) {
    return result && result->success;
}

/**
 * Create a success result.
 */
static inline Carbon_CommandResult carbon_command_result_success(int type) {
    Carbon_CommandResult r = {0};
    r.success = true;
    r.command_type = type;
    return r;
}

/**
 * Create a failure result.
 */
Carbon_CommandResult carbon_command_result_failure(int type, const char *error);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_COMMAND_H */
