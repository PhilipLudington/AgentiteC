/**
 * Carbon Command Queue System
 *
 * Validated, atomic command execution for player actions. Provides
 * command registration, pre-execution validation, queued execution
 * during turn processing, and command history for undo/replay.
 *
 * Usage:
 *   // Create command system
 *   Agentite_CommandSystem *sys = agentite_command_create();
 *
 *   // Register command types
 *   agentite_command_register(sys, CMD_MOVE_UNIT, validate_move, execute_move);
 *   agentite_command_register(sys, CMD_BUILD, validate_build, execute_build);
 *
 *   // Create and validate command
 *   Agentite_Command *cmd = agentite_command_new(CMD_MOVE_UNIT);
 *   agentite_command_set_entity(cmd, "unit", player_unit);
 *   agentite_command_set_int(cmd, "dest_x", 10);
 *   agentite_command_set_int(cmd, "dest_y", 20);
 *
 *   Agentite_CommandResult result = agentite_command_validate(sys, cmd, game_state);
 *   if (result.success) {
 *       agentite_command_queue(sys, cmd);
 *   }
 *
 *   // Execute all queued commands
 *   Agentite_CommandResult results[32];
 *   int count = agentite_command_execute_all(sys, game_state, results, 32);
 *
 *   // Cleanup
 *   agentite_command_destroy(sys);
 */

#ifndef AGENTITE_COMMAND_H
#define AGENTITE_COMMAND_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_COMMAND_MAX_PARAMS      16    /* Maximum parameters per command */
#define AGENTITE_COMMAND_MAX_PARAM_KEY   32    /* Maximum key length */
#define AGENTITE_COMMAND_MAX_ERROR       128   /* Maximum error message length */
#define AGENTITE_COMMAND_MAX_QUEUE       64    /* Maximum queued commands */
#define AGENTITE_COMMAND_MAX_TYPES       64    /* Maximum registered command types */
#define AGENTITE_COMMAND_MAX_HISTORY     256   /* Maximum history entries */

/*============================================================================
 * Parameter Types
 *============================================================================*/

/**
 * Command parameter value types.
 */
typedef enum Agentite_CommandParamType {
    AGENTITE_CMD_PARAM_NONE = 0,
    AGENTITE_CMD_PARAM_INT,
    AGENTITE_CMD_PARAM_INT64,
    AGENTITE_CMD_PARAM_FLOAT,
    AGENTITE_CMD_PARAM_DOUBLE,
    AGENTITE_CMD_PARAM_BOOL,
    AGENTITE_CMD_PARAM_ENTITY,
    AGENTITE_CMD_PARAM_STRING,
    AGENTITE_CMD_PARAM_PTR,
} Agentite_CommandParamType;

/**
 * Command parameter value.
 */
typedef struct Agentite_CommandParam {
    char key[AGENTITE_COMMAND_MAX_PARAM_KEY];
    Agentite_CommandParamType type;
    union {
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        bool b;
        uint32_t entity;
        char str[AGENTITE_COMMAND_MAX_PARAM_KEY];
        void *ptr;
    };
} Agentite_CommandParam;

/*============================================================================
 * Command Structure
 *============================================================================*/

/**
 * A command with typed parameters.
 */
typedef struct Agentite_Command {
    int type;                                       /* Command type ID */
    Agentite_CommandParam params[AGENTITE_COMMAND_MAX_PARAMS];
    int param_count;
    uint32_t sequence;                              /* Sequence number for ordering */
    int32_t source_faction;                         /* Faction that issued command (-1 = any) */
    void *userdata;                                 /* User-defined data */
} Agentite_Command;

/**
 * Result of command validation or execution.
 */
typedef struct Agentite_CommandResult {
    bool success;                                   /* Whether command succeeded */
    int command_type;                               /* Type of command */
    uint32_t sequence;                              /* Command sequence number */
    char error[AGENTITE_COMMAND_MAX_ERROR];           /* Error message if failed */
} Agentite_CommandResult;

/*============================================================================
 * Command System Forward Declaration
 *============================================================================*/

typedef struct Agentite_CommandSystem Agentite_CommandSystem;

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
typedef bool (*Agentite_CommandValidator)(const Agentite_Command *cmd,
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
typedef bool (*Agentite_CommandExecutor)(const Agentite_Command *cmd,
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
typedef void (*Agentite_CommandCallback)(Agentite_CommandSystem *sys,
                                         const Agentite_Command *cmd,
                                         const Agentite_CommandResult *result,
                                         void *userdata);

/*============================================================================
 * Lifecycle
 *============================================================================*/

/**
 * Create a new command system.
 *
 * @return New command system or NULL on failure
 */
Agentite_CommandSystem *agentite_command_create(void);

/**
 * Destroy a command system and free resources.
 *
 * @param sys Command system to destroy
 */
void agentite_command_destroy(Agentite_CommandSystem *sys);

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
bool agentite_command_register(Agentite_CommandSystem *sys,
                              int type,
                              Agentite_CommandValidator validator,
                              Agentite_CommandExecutor executor);

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
bool agentite_command_register_named(Agentite_CommandSystem *sys,
                                    int type,
                                    const char *name,
                                    Agentite_CommandValidator validator,
                                    Agentite_CommandExecutor executor);

/**
 * Check if a command type is registered.
 *
 * @param sys  Command system
 * @param type Command type ID
 * @return true if registered
 */
bool agentite_command_is_registered(const Agentite_CommandSystem *sys, int type);

/**
 * Get command type name.
 *
 * @param sys  Command system
 * @param type Command type ID
 * @return Name or NULL if not found
 */
const char *agentite_command_get_type_name(const Agentite_CommandSystem *sys, int type);

/*============================================================================
 * Command Creation
 *============================================================================*/

/**
 * Create a new command.
 *
 * @param type Command type ID
 * @return New command or NULL on failure
 */
Agentite_Command *agentite_command_new(int type);

/**
 * Create a command with source faction.
 *
 * @param type    Command type ID
 * @param faction Source faction ID
 * @return New command or NULL on failure
 */
Agentite_Command *agentite_command_new_ex(int type, int32_t faction);

/**
 * Clone a command.
 *
 * @param cmd Command to clone
 * @return New command copy or NULL on failure
 */
Agentite_Command *agentite_command_clone(const Agentite_Command *cmd);

/**
 * Free a command.
 *
 * @param cmd Command to free
 */
void agentite_command_free(Agentite_Command *cmd);

/*============================================================================
 * Command Parameters
 *============================================================================*/

/**
 * Set integer parameter.
 */
void agentite_command_set_int(Agentite_Command *cmd, const char *key, int32_t value);

/**
 * Set 64-bit integer parameter.
 */
void agentite_command_set_int64(Agentite_Command *cmd, const char *key, int64_t value);

/**
 * Set float parameter.
 */
void agentite_command_set_float(Agentite_Command *cmd, const char *key, float value);

/**
 * Set double parameter.
 */
void agentite_command_set_double(Agentite_Command *cmd, const char *key, double value);

/**
 * Set boolean parameter.
 */
void agentite_command_set_bool(Agentite_Command *cmd, const char *key, bool value);

/**
 * Set entity parameter.
 */
void agentite_command_set_entity(Agentite_Command *cmd, const char *key, uint32_t entity);

/**
 * Set string parameter.
 */
void agentite_command_set_string(Agentite_Command *cmd, const char *key, const char *value);

/**
 * Set pointer parameter (not owned).
 */
void agentite_command_set_ptr(Agentite_Command *cmd, const char *key, void *ptr);

/*============================================================================
 * Parameter Retrieval
 *============================================================================*/

/**
 * Check if parameter exists.
 */
bool agentite_command_has_param(const Agentite_Command *cmd, const char *key);

/**
 * Get parameter type.
 */
Agentite_CommandParamType agentite_command_get_param_type(const Agentite_Command *cmd, const char *key);

/**
 * Get integer parameter.
 */
int32_t agentite_command_get_int(const Agentite_Command *cmd, const char *key);

/**
 * Get integer parameter with default.
 */
int32_t agentite_command_get_int_or(const Agentite_Command *cmd, const char *key, int32_t def);

/**
 * Get 64-bit integer parameter.
 */
int64_t agentite_command_get_int64(const Agentite_Command *cmd, const char *key);

/**
 * Get float parameter.
 */
float agentite_command_get_float(const Agentite_Command *cmd, const char *key);

/**
 * Get float parameter with default.
 */
float agentite_command_get_float_or(const Agentite_Command *cmd, const char *key, float def);

/**
 * Get double parameter.
 */
double agentite_command_get_double(const Agentite_Command *cmd, const char *key);

/**
 * Get boolean parameter.
 */
bool agentite_command_get_bool(const Agentite_Command *cmd, const char *key);

/**
 * Get entity parameter.
 */
uint32_t agentite_command_get_entity(const Agentite_Command *cmd, const char *key);

/**
 * Get string parameter.
 */
const char *agentite_command_get_string(const Agentite_Command *cmd, const char *key);

/**
 * Get pointer parameter.
 */
void *agentite_command_get_ptr(const Agentite_Command *cmd, const char *key);

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
Agentite_CommandResult agentite_command_validate(Agentite_CommandSystem *sys,
                                               const Agentite_Command *cmd,
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
bool agentite_command_queue(Agentite_CommandSystem *sys, Agentite_Command *cmd);

/**
 * Add a command to the queue with validation.
 * Only queues if validation passes.
 *
 * @param sys        Command system
 * @param cmd        Command to queue
 * @param game_state Game state for validation
 * @return Validation result (success = queued)
 */
Agentite_CommandResult agentite_command_queue_validated(Agentite_CommandSystem *sys,
                                                      Agentite_Command *cmd,
                                                      void *game_state);

/**
 * Get number of queued commands.
 */
int agentite_command_queue_count(const Agentite_CommandSystem *sys);

/**
 * Clear command queue.
 */
void agentite_command_queue_clear(Agentite_CommandSystem *sys);

/**
 * Get queued command by index.
 */
const Agentite_Command *agentite_command_queue_get(const Agentite_CommandSystem *sys, int index);

/**
 * Remove queued command by index.
 */
bool agentite_command_queue_remove(Agentite_CommandSystem *sys, int index);

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
int agentite_command_execute_all(Agentite_CommandSystem *sys,
                                void *game_state,
                                Agentite_CommandResult *results,
                                int max);

/**
 * Execute a single command immediately (not from queue).
 *
 * @param sys        Command system
 * @param cmd        Command to execute
 * @param game_state Game state pointer
 * @return Execution result
 */
Agentite_CommandResult agentite_command_execute(Agentite_CommandSystem *sys,
                                              const Agentite_Command *cmd,
                                              void *game_state);

/**
 * Execute next queued command.
 *
 * @param sys        Command system
 * @param game_state Game state pointer
 * @return Execution result (success=false if queue empty)
 */
Agentite_CommandResult agentite_command_execute_next(Agentite_CommandSystem *sys,
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
void agentite_command_set_callback(Agentite_CommandSystem *sys,
                                  Agentite_CommandCallback callback,
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
void agentite_command_enable_history(Agentite_CommandSystem *sys, int max_commands);

/**
 * Get command history.
 *
 * @param sys Command system
 * @param out Output array of command pointers
 * @param max Maximum commands to return
 * @return Number of commands returned (newest first)
 */
int agentite_command_get_history(const Agentite_CommandSystem *sys,
                                const Agentite_Command **out,
                                int max);

/**
 * Get history count.
 */
int agentite_command_get_history_count(const Agentite_CommandSystem *sys);

/**
 * Clear command history.
 */
void agentite_command_clear_history(Agentite_CommandSystem *sys);

/**
 * Replay command from history.
 *
 * @param sys        Command system
 * @param index      History index (0 = most recent)
 * @param game_state Game state pointer
 * @return Execution result
 */
Agentite_CommandResult agentite_command_replay(Agentite_CommandSystem *sys,
                                             int index,
                                             void *game_state);

/*============================================================================
 * Statistics
 *============================================================================*/

/**
 * Command system statistics.
 */
typedef struct Agentite_CommandStats {
    int total_executed;          /* Total commands executed */
    int total_succeeded;         /* Commands that succeeded */
    int total_failed;            /* Commands that failed */
    int total_invalid;           /* Commands that failed validation */
    int commands_by_type[AGENTITE_COMMAND_MAX_TYPES]; /* Per-type counts */
} Agentite_CommandStats;

/**
 * Get command system statistics.
 */
void agentite_command_get_stats(const Agentite_CommandSystem *sys, Agentite_CommandStats *out);

/**
 * Reset statistics.
 */
void agentite_command_reset_stats(Agentite_CommandSystem *sys);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Check if result indicates success.
 */
static inline bool agentite_command_result_ok(const Agentite_CommandResult *result) {
    return result && result->success;
}

/**
 * Create a success result.
 */
static inline Agentite_CommandResult agentite_command_result_success(int type) {
    Agentite_CommandResult r = {};
    r.success = true;
    r.command_type = type;
    return r;
}

/**
 * Create a failure result.
 */
Agentite_CommandResult agentite_command_result_failure(int type, const char *error);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_COMMAND_H */
