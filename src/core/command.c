/**
 * Carbon Command Queue Implementation
 *
 * Validated, atomic command execution for player actions.
 */

#include "carbon/command.h"
#include "carbon/error.h"
#include "carbon/validate.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

/**
 * Registered command type.
 */
typedef struct CommandType {
    int type;
    char name[CARBON_COMMAND_MAX_PARAM_KEY];
    Carbon_CommandValidator validator;
    Carbon_CommandExecutor executor;
    bool registered;
} CommandType;

/**
 * Command system.
 */
struct Carbon_CommandSystem {
    /* Type registry */
    CommandType types[CARBON_COMMAND_MAX_TYPES];
    int type_count;

    /* Command queue */
    Carbon_Command *queue[CARBON_COMMAND_MAX_QUEUE];
    int queue_count;
    uint32_t next_sequence;

    /* History */
    Carbon_Command *history[CARBON_COMMAND_MAX_HISTORY];
    int history_count;
    int history_head;  /* Circular buffer head */
    int history_max;   /* 0 = disabled */

    /* Callback */
    Carbon_CommandCallback callback;
    void *callback_userdata;

    /* Statistics */
    Carbon_CommandStats stats;
};

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static CommandType *find_type(Carbon_CommandSystem *sys, int type) {
    for (int i = 0; i < sys->type_count; i++) {
        if (sys->types[i].type == type && sys->types[i].registered) {
            return &sys->types[i];
        }
    }
    return NULL;
}

static Carbon_CommandParam *find_param(Carbon_Command *cmd, const char *key) {
    for (int i = 0; i < cmd->param_count; i++) {
        if (strcmp(cmd->params[i].key, key) == 0) {
            return &cmd->params[i];
        }
    }
    return NULL;
}

static const Carbon_CommandParam *find_param_const(const Carbon_Command *cmd, const char *key) {
    for (int i = 0; i < cmd->param_count; i++) {
        if (strcmp(cmd->params[i].key, key) == 0) {
            return &cmd->params[i];
        }
    }
    return NULL;
}

static Carbon_CommandParam *get_or_create_param(Carbon_Command *cmd, const char *key) {
    Carbon_CommandParam *param = find_param(cmd, key);
    if (param) return param;

    if (cmd->param_count >= CARBON_COMMAND_MAX_PARAMS) {
        return NULL;
    }

    param = &cmd->params[cmd->param_count];
    memset(param, 0, sizeof(*param));
    strncpy(param->key, key, CARBON_COMMAND_MAX_PARAM_KEY - 1);
    cmd->param_count++;
    return param;
}

static void add_to_history(Carbon_CommandSystem *sys, const Carbon_Command *cmd) {
    if (sys->history_max <= 0) return;

    /* Clone command */
    Carbon_Command *clone = carbon_command_clone(cmd);
    if (!clone) return;

    /* Circular buffer insertion */
    if (sys->history_count < sys->history_max) {
        /* Not full yet, append */
        sys->history[sys->history_count] = clone;
        sys->history_count++;
    } else {
        /* Full, overwrite oldest */
        int oldest = (sys->history_head + sys->history_count) % sys->history_max;
        if (sys->history[oldest]) {
            carbon_command_free(sys->history[oldest]);
        }
        sys->history[oldest] = clone;
        sys->history_head = (sys->history_head + 1) % sys->history_max;
    }
}

static void notify_callback(Carbon_CommandSystem *sys, const Carbon_Command *cmd,
                             const Carbon_CommandResult *result) {
    if (sys->callback) {
        sys->callback(sys, cmd, result, sys->callback_userdata);
    }
}

/*============================================================================
 * Lifecycle
 *============================================================================*/

Carbon_CommandSystem *carbon_command_create(void) {
    Carbon_CommandSystem *sys = calloc(1, sizeof(Carbon_CommandSystem));
    if (!sys) {
        carbon_set_error("carbon_command_create: allocation failed");
        return NULL;
    }

    sys->type_count = 0;
    sys->queue_count = 0;
    sys->next_sequence = 1;
    sys->history_count = 0;
    sys->history_head = 0;
    sys->history_max = 0;
    sys->callback = NULL;
    sys->callback_userdata = NULL;
    memset(&sys->stats, 0, sizeof(sys->stats));

    return sys;
}

void carbon_command_destroy(Carbon_CommandSystem *sys) {
    if (!sys) return;

    /* Free queued commands */
    for (int i = 0; i < sys->queue_count; i++) {
        carbon_command_free(sys->queue[i]);
    }

    /* Free history */
    for (int i = 0; i < sys->history_count; i++) {
        int idx = (sys->history_head + i) % sys->history_max;
        if (sys->history[idx]) {
            carbon_command_free(sys->history[idx]);
        }
    }

    free(sys);
}

/*============================================================================
 * Command Type Registration
 *============================================================================*/

bool carbon_command_register(Carbon_CommandSystem *sys,
                              int type,
                              Carbon_CommandValidator validator,
                              Carbon_CommandExecutor executor) {
    return carbon_command_register_named(sys, type, NULL, validator, executor);
}

bool carbon_command_register_named(Carbon_CommandSystem *sys,
                                    int type,
                                    const char *name,
                                    Carbon_CommandValidator validator,
                                    Carbon_CommandExecutor executor) {
    CARBON_VALIDATE_PTR_RET(sys, false);
    CARBON_VALIDATE_PTR_RET(executor, false);

    /* Check if already registered */
    if (find_type(sys, type)) {
        carbon_set_error("carbon_command_register: type %d already registered", type);
        return false;
    }

    /* Check capacity */
    if (sys->type_count >= CARBON_COMMAND_MAX_TYPES) {
        carbon_set_error("carbon_command_register: max types reached");
        return false;
    }

    CommandType *ct = &sys->types[sys->type_count];
    ct->type = type;
    ct->validator = validator;
    ct->executor = executor;
    ct->registered = true;

    if (name) {
        strncpy(ct->name, name, CARBON_COMMAND_MAX_PARAM_KEY - 1);
    } else {
        snprintf(ct->name, CARBON_COMMAND_MAX_PARAM_KEY, "Command_%d", type);
    }

    sys->type_count++;
    return true;
}

bool carbon_command_is_registered(const Carbon_CommandSystem *sys, int type) {
    CARBON_VALIDATE_PTR_RET(sys, false);
    return find_type((Carbon_CommandSystem*)sys, type) != NULL;
}

const char *carbon_command_get_type_name(const Carbon_CommandSystem *sys, int type) {
    CARBON_VALIDATE_PTR_RET(sys, NULL);
    CommandType *ct = find_type((Carbon_CommandSystem*)sys, type);
    return ct ? ct->name : NULL;
}

/*============================================================================
 * Command Creation
 *============================================================================*/

Carbon_Command *carbon_command_new(int type) {
    return carbon_command_new_ex(type, -1);
}

Carbon_Command *carbon_command_new_ex(int type, int32_t faction) {
    Carbon_Command *cmd = calloc(1, sizeof(Carbon_Command));
    if (!cmd) {
        carbon_set_error("carbon_command_new: allocation failed");
        return NULL;
    }

    cmd->type = type;
    cmd->param_count = 0;
    cmd->sequence = 0;  /* Set when queued */
    cmd->source_faction = faction;
    cmd->userdata = NULL;

    return cmd;
}

Carbon_Command *carbon_command_clone(const Carbon_Command *cmd) {
    CARBON_VALIDATE_PTR_RET(cmd, NULL);

    Carbon_Command *clone = calloc(1, sizeof(Carbon_Command));
    if (!clone) {
        carbon_set_error("carbon_command_clone: allocation failed");
        return NULL;
    }

    memcpy(clone, cmd, sizeof(Carbon_Command));
    /* Note: userdata is copied as pointer, not deep cloned */
    return clone;
}

void carbon_command_free(Carbon_Command *cmd) {
    free(cmd);
}

/*============================================================================
 * Command Parameters - Setters
 *============================================================================*/

void carbon_command_set_int(Carbon_Command *cmd, const char *key, int32_t value) {
    CARBON_VALIDATE_PTR(cmd);
    CARBON_VALIDATE_PTR(key);

    Carbon_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = CARBON_CMD_PARAM_INT;
        param->i32 = value;
    }
}

void carbon_command_set_int64(Carbon_Command *cmd, const char *key, int64_t value) {
    CARBON_VALIDATE_PTR(cmd);
    CARBON_VALIDATE_PTR(key);

    Carbon_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = CARBON_CMD_PARAM_INT64;
        param->i64 = value;
    }
}

void carbon_command_set_float(Carbon_Command *cmd, const char *key, float value) {
    CARBON_VALIDATE_PTR(cmd);
    CARBON_VALIDATE_PTR(key);

    Carbon_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = CARBON_CMD_PARAM_FLOAT;
        param->f32 = value;
    }
}

void carbon_command_set_double(Carbon_Command *cmd, const char *key, double value) {
    CARBON_VALIDATE_PTR(cmd);
    CARBON_VALIDATE_PTR(key);

    Carbon_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = CARBON_CMD_PARAM_DOUBLE;
        param->f64 = value;
    }
}

void carbon_command_set_bool(Carbon_Command *cmd, const char *key, bool value) {
    CARBON_VALIDATE_PTR(cmd);
    CARBON_VALIDATE_PTR(key);

    Carbon_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = CARBON_CMD_PARAM_BOOL;
        param->b = value;
    }
}

void carbon_command_set_entity(Carbon_Command *cmd, const char *key, uint32_t entity) {
    CARBON_VALIDATE_PTR(cmd);
    CARBON_VALIDATE_PTR(key);

    Carbon_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = CARBON_CMD_PARAM_ENTITY;
        param->entity = entity;
    }
}

void carbon_command_set_string(Carbon_Command *cmd, const char *key, const char *value) {
    CARBON_VALIDATE_PTR(cmd);
    CARBON_VALIDATE_PTR(key);

    Carbon_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = CARBON_CMD_PARAM_STRING;
        if (value) {
            strncpy(param->str, value, CARBON_COMMAND_MAX_PARAM_KEY - 1);
        } else {
            param->str[0] = '\0';
        }
    }
}

void carbon_command_set_ptr(Carbon_Command *cmd, const char *key, void *ptr) {
    CARBON_VALIDATE_PTR(cmd);
    CARBON_VALIDATE_PTR(key);

    Carbon_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = CARBON_CMD_PARAM_PTR;
        param->ptr = ptr;
    }
}

/*============================================================================
 * Command Parameters - Getters
 *============================================================================*/

bool carbon_command_has_param(const Carbon_Command *cmd, const char *key) {
    CARBON_VALIDATE_PTR_RET(cmd, false);
    CARBON_VALIDATE_PTR_RET(key, false);
    return find_param_const(cmd, key) != NULL;
}

Carbon_CommandParamType carbon_command_get_param_type(const Carbon_Command *cmd, const char *key) {
    CARBON_VALIDATE_PTR_RET(cmd, CARBON_CMD_PARAM_NONE);
    CARBON_VALIDATE_PTR_RET(key, CARBON_CMD_PARAM_NONE);

    const Carbon_CommandParam *param = find_param_const(cmd, key);
    return param ? param->type : CARBON_CMD_PARAM_NONE;
}

int32_t carbon_command_get_int(const Carbon_Command *cmd, const char *key) {
    return carbon_command_get_int_or(cmd, key, 0);
}

int32_t carbon_command_get_int_or(const Carbon_Command *cmd, const char *key, int32_t def) {
    CARBON_VALIDATE_PTR_RET(cmd, def);
    CARBON_VALIDATE_PTR_RET(key, def);

    const Carbon_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == CARBON_CMD_PARAM_INT) {
        return param->i32;
    }
    return def;
}

int64_t carbon_command_get_int64(const Carbon_Command *cmd, const char *key) {
    CARBON_VALIDATE_PTR_RET(cmd, 0);
    CARBON_VALIDATE_PTR_RET(key, 0);

    const Carbon_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == CARBON_CMD_PARAM_INT64) {
        return param->i64;
    }
    return 0;
}

float carbon_command_get_float(const Carbon_Command *cmd, const char *key) {
    return carbon_command_get_float_or(cmd, key, 0.0f);
}

float carbon_command_get_float_or(const Carbon_Command *cmd, const char *key, float def) {
    CARBON_VALIDATE_PTR_RET(cmd, def);
    CARBON_VALIDATE_PTR_RET(key, def);

    const Carbon_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == CARBON_CMD_PARAM_FLOAT) {
        return param->f32;
    }
    return def;
}

double carbon_command_get_double(const Carbon_Command *cmd, const char *key) {
    CARBON_VALIDATE_PTR_RET(cmd, 0.0);
    CARBON_VALIDATE_PTR_RET(key, 0.0);

    const Carbon_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == CARBON_CMD_PARAM_DOUBLE) {
        return param->f64;
    }
    return 0.0;
}

bool carbon_command_get_bool(const Carbon_Command *cmd, const char *key) {
    CARBON_VALIDATE_PTR_RET(cmd, false);
    CARBON_VALIDATE_PTR_RET(key, false);

    const Carbon_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == CARBON_CMD_PARAM_BOOL) {
        return param->b;
    }
    return false;
}

uint32_t carbon_command_get_entity(const Carbon_Command *cmd, const char *key) {
    CARBON_VALIDATE_PTR_RET(cmd, 0);
    CARBON_VALIDATE_PTR_RET(key, 0);

    const Carbon_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == CARBON_CMD_PARAM_ENTITY) {
        return param->entity;
    }
    return 0;
}

const char *carbon_command_get_string(const Carbon_Command *cmd, const char *key) {
    CARBON_VALIDATE_PTR_RET(cmd, NULL);
    CARBON_VALIDATE_PTR_RET(key, NULL);

    const Carbon_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == CARBON_CMD_PARAM_STRING) {
        return param->str;
    }
    return NULL;
}

void *carbon_command_get_ptr(const Carbon_Command *cmd, const char *key) {
    CARBON_VALIDATE_PTR_RET(cmd, NULL);
    CARBON_VALIDATE_PTR_RET(key, NULL);

    const Carbon_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == CARBON_CMD_PARAM_PTR) {
        return param->ptr;
    }
    return NULL;
}

/*============================================================================
 * Validation
 *============================================================================*/

Carbon_CommandResult carbon_command_validate(Carbon_CommandSystem *sys,
                                               const Carbon_Command *cmd,
                                               void *game_state) {
    Carbon_CommandResult result = {0};

    CARBON_VALIDATE_PTR_RET(sys, result);
    CARBON_VALIDATE_PTR_RET(cmd, result);

    result.command_type = cmd->type;
    result.sequence = cmd->sequence;

    /* Find registered type */
    CommandType *ct = find_type(sys, cmd->type);
    if (!ct) {
        result.success = false;
        snprintf(result.error, sizeof(result.error),
                 "Command type %d not registered", cmd->type);
        return result;
    }

    /* Call validator if present */
    if (ct->validator) {
        result.success = ct->validator(cmd, game_state, result.error, sizeof(result.error));
        if (!result.success) {
            sys->stats.total_invalid++;
        }
    } else {
        /* No validator = always valid */
        result.success = true;
    }

    return result;
}

/*============================================================================
 * Queue Operations
 *============================================================================*/

bool carbon_command_queue(Carbon_CommandSystem *sys, Carbon_Command *cmd) {
    CARBON_VALIDATE_PTR_RET(sys, false);
    CARBON_VALIDATE_PTR_RET(cmd, false);

    if (sys->queue_count >= CARBON_COMMAND_MAX_QUEUE) {
        carbon_set_error("carbon_command_queue: queue is full");
        return false;
    }

    /* Clone command */
    Carbon_Command *clone = carbon_command_clone(cmd);
    if (!clone) {
        return false;
    }

    clone->sequence = sys->next_sequence++;
    sys->queue[sys->queue_count] = clone;
    sys->queue_count++;

    return true;
}

Carbon_CommandResult carbon_command_queue_validated(Carbon_CommandSystem *sys,
                                                      Carbon_Command *cmd,
                                                      void *game_state) {
    Carbon_CommandResult result = carbon_command_validate(sys, cmd, game_state);
    if (result.success) {
        if (!carbon_command_queue(sys, cmd)) {
            result.success = false;
            snprintf(result.error, sizeof(result.error), "Failed to queue command");
        }
    }
    return result;
}

int carbon_command_queue_count(const Carbon_CommandSystem *sys) {
    CARBON_VALIDATE_PTR_RET(sys, 0);
    return sys->queue_count;
}

void carbon_command_queue_clear(Carbon_CommandSystem *sys) {
    CARBON_VALIDATE_PTR(sys);

    for (int i = 0; i < sys->queue_count; i++) {
        carbon_command_free(sys->queue[i]);
        sys->queue[i] = NULL;
    }
    sys->queue_count = 0;
}

const Carbon_Command *carbon_command_queue_get(const Carbon_CommandSystem *sys, int index) {
    CARBON_VALIDATE_PTR_RET(sys, NULL);

    if (index < 0 || index >= sys->queue_count) {
        return NULL;
    }
    return sys->queue[index];
}

bool carbon_command_queue_remove(Carbon_CommandSystem *sys, int index) {
    CARBON_VALIDATE_PTR_RET(sys, false);

    if (index < 0 || index >= sys->queue_count) {
        return false;
    }

    carbon_command_free(sys->queue[index]);

    /* Shift remaining */
    for (int i = index; i < sys->queue_count - 1; i++) {
        sys->queue[i] = sys->queue[i + 1];
    }
    sys->queue_count--;

    return true;
}

/*============================================================================
 * Execution
 *============================================================================*/

int carbon_command_execute_all(Carbon_CommandSystem *sys,
                                void *game_state,
                                Carbon_CommandResult *results,
                                int max) {
    CARBON_VALIDATE_PTR_RET(sys, 0);

    int executed = 0;

    while (sys->queue_count > 0 && (max <= 0 || executed < max)) {
        Carbon_CommandResult result = carbon_command_execute_next(sys, game_state);
        if (results && executed < max) {
            results[executed] = result;
        }
        executed++;
    }

    return executed;
}

Carbon_CommandResult carbon_command_execute(Carbon_CommandSystem *sys,
                                              const Carbon_Command *cmd,
                                              void *game_state) {
    Carbon_CommandResult result = {0};

    CARBON_VALIDATE_PTR_RET(sys, result);
    CARBON_VALIDATE_PTR_RET(cmd, result);

    result.command_type = cmd->type;
    result.sequence = cmd->sequence;

    /* Find registered type */
    CommandType *ct = find_type(sys, cmd->type);
    if (!ct) {
        result.success = false;
        snprintf(result.error, sizeof(result.error),
                 "Command type %d not registered", cmd->type);
        sys->stats.total_failed++;
        return result;
    }

    /* Validate first */
    if (ct->validator) {
        if (!ct->validator(cmd, game_state, result.error, sizeof(result.error))) {
            result.success = false;
            sys->stats.total_invalid++;
            notify_callback(sys, cmd, &result);
            return result;
        }
    }

    /* Execute */
    result.success = ct->executor(cmd, game_state);
    sys->stats.total_executed++;

    if (result.success) {
        sys->stats.total_succeeded++;
        /* Track per-type stats */
        if (cmd->type >= 0 && cmd->type < CARBON_COMMAND_MAX_TYPES) {
            sys->stats.commands_by_type[cmd->type]++;
        }
        /* Add to history */
        add_to_history(sys, cmd);
    } else {
        sys->stats.total_failed++;
        if (result.error[0] == '\0') {
            snprintf(result.error, sizeof(result.error), "Execution failed");
        }
    }

    notify_callback(sys, cmd, &result);

    return result;
}

Carbon_CommandResult carbon_command_execute_next(Carbon_CommandSystem *sys,
                                                   void *game_state) {
    Carbon_CommandResult result = {0};

    CARBON_VALIDATE_PTR_RET(sys, result);

    if (sys->queue_count <= 0) {
        result.success = false;
        snprintf(result.error, sizeof(result.error), "Queue is empty");
        return result;
    }

    /* Get and remove first command */
    Carbon_Command *cmd = sys->queue[0];
    for (int i = 0; i < sys->queue_count - 1; i++) {
        sys->queue[i] = sys->queue[i + 1];
    }
    sys->queue_count--;

    /* Execute */
    result = carbon_command_execute(sys, cmd, game_state);

    /* Free command */
    carbon_command_free(cmd);

    return result;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void carbon_command_set_callback(Carbon_CommandSystem *sys,
                                  Carbon_CommandCallback callback,
                                  void *userdata) {
    CARBON_VALIDATE_PTR(sys);
    sys->callback = callback;
    sys->callback_userdata = userdata;
}

/*============================================================================
 * History
 *============================================================================*/

void carbon_command_enable_history(Carbon_CommandSystem *sys, int max_commands) {
    CARBON_VALIDATE_PTR(sys);

    /* Clear existing history */
    carbon_command_clear_history(sys);

    if (max_commands > CARBON_COMMAND_MAX_HISTORY) {
        max_commands = CARBON_COMMAND_MAX_HISTORY;
    }

    sys->history_max = max_commands;
}

int carbon_command_get_history(const Carbon_CommandSystem *sys,
                                const Carbon_Command **out,
                                int max) {
    CARBON_VALIDATE_PTR_RET(sys, 0);
    CARBON_VALIDATE_PTR_RET(out, 0);

    if (sys->history_max <= 0) {
        return 0;
    }

    int count = 0;
    /* Return newest first */
    for (int i = sys->history_count - 1; i >= 0 && count < max; i--) {
        int idx = (sys->history_head + i) % sys->history_max;
        if (sys->history[idx]) {
            out[count++] = sys->history[idx];
        }
    }

    return count;
}

int carbon_command_get_history_count(const Carbon_CommandSystem *sys) {
    CARBON_VALIDATE_PTR_RET(sys, 0);
    return sys->history_count;
}

void carbon_command_clear_history(Carbon_CommandSystem *sys) {
    CARBON_VALIDATE_PTR(sys);

    if (sys->history_max <= 0) return;

    for (int i = 0; i < sys->history_count; i++) {
        int idx = (sys->history_head + i) % sys->history_max;
        if (sys->history[idx]) {
            carbon_command_free(sys->history[idx]);
            sys->history[idx] = NULL;
        }
    }

    sys->history_count = 0;
    sys->history_head = 0;
}

Carbon_CommandResult carbon_command_replay(Carbon_CommandSystem *sys,
                                             int index,
                                             void *game_state) {
    Carbon_CommandResult result = {0};

    CARBON_VALIDATE_PTR_RET(sys, result);

    if (sys->history_max <= 0 || index < 0 || index >= sys->history_count) {
        result.success = false;
        snprintf(result.error, sizeof(result.error), "Invalid history index");
        return result;
    }

    /* Get from end (newest first) */
    int actual_idx = (sys->history_head + sys->history_count - 1 - index) % sys->history_max;
    const Carbon_Command *cmd = sys->history[actual_idx];

    if (!cmd) {
        result.success = false;
        snprintf(result.error, sizeof(result.error), "History entry is null");
        return result;
    }

    return carbon_command_execute(sys, cmd, game_state);
}

/*============================================================================
 * Statistics
 *============================================================================*/

void carbon_command_get_stats(const Carbon_CommandSystem *sys, Carbon_CommandStats *out) {
    CARBON_VALIDATE_PTR(sys);
    CARBON_VALIDATE_PTR(out);
    *out = sys->stats;
}

void carbon_command_reset_stats(Carbon_CommandSystem *sys) {
    CARBON_VALIDATE_PTR(sys);
    memset(&sys->stats, 0, sizeof(sys->stats));
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

Carbon_CommandResult carbon_command_result_failure(int type, const char *error) {
    Carbon_CommandResult r = {0};
    r.success = false;
    r.command_type = type;
    if (error) {
        strncpy(r.error, error, CARBON_COMMAND_MAX_ERROR - 1);
    }
    return r;
}
