/**
 * Carbon Command Queue Implementation
 *
 * Validated, atomic command execution for player actions.
 */

#include "agentite/agentite.h"
#include "agentite/command.h"
#include "agentite/error.h"
#include "agentite/validate.h"

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
    char name[AGENTITE_COMMAND_MAX_PARAM_KEY];
    Agentite_CommandValidator validator;
    Agentite_CommandExecutor executor;
    bool registered;
} CommandType;

/**
 * Command system.
 */
struct Agentite_CommandSystem {
    /* Type registry */
    CommandType types[AGENTITE_COMMAND_MAX_TYPES];
    int type_count;

    /* Command queue */
    Agentite_Command *queue[AGENTITE_COMMAND_MAX_QUEUE];
    int queue_count;
    uint32_t next_sequence;

    /* History */
    Agentite_Command *history[AGENTITE_COMMAND_MAX_HISTORY];
    int history_count;
    int history_head;  /* Circular buffer head */
    int history_max;   /* 0 = disabled */

    /* Callback */
    Agentite_CommandCallback callback;
    void *callback_userdata;

    /* Statistics */
    Agentite_CommandStats stats;
};

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static CommandType *find_type(Agentite_CommandSystem *sys, int type) {
    for (int i = 0; i < sys->type_count; i++) {
        if (sys->types[i].type == type && sys->types[i].registered) {
            return &sys->types[i];
        }
    }
    return NULL;
}

static Agentite_CommandParam *find_param(Agentite_Command *cmd, const char *key) {
    for (int i = 0; i < cmd->param_count; i++) {
        if (strcmp(cmd->params[i].key, key) == 0) {
            return &cmd->params[i];
        }
    }
    return NULL;
}

static const Agentite_CommandParam *find_param_const(const Agentite_Command *cmd, const char *key) {
    for (int i = 0; i < cmd->param_count; i++) {
        if (strcmp(cmd->params[i].key, key) == 0) {
            return &cmd->params[i];
        }
    }
    return NULL;
}

static Agentite_CommandParam *get_or_create_param(Agentite_Command *cmd, const char *key) {
    Agentite_CommandParam *param = find_param(cmd, key);
    if (param) return param;

    if (cmd->param_count >= AGENTITE_COMMAND_MAX_PARAMS) {
        return NULL;
    }

    param = &cmd->params[cmd->param_count];
    memset(param, 0, sizeof(*param));
    strncpy(param->key, key, AGENTITE_COMMAND_MAX_PARAM_KEY - 1);
    cmd->param_count++;
    return param;
}

static void add_to_history(Agentite_CommandSystem *sys, const Agentite_Command *cmd) {
    if (sys->history_max <= 0) return;

    /* Clone command */
    Agentite_Command *clone = agentite_command_clone(cmd);
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
            agentite_command_free(sys->history[oldest]);
        }
        sys->history[oldest] = clone;
        sys->history_head = (sys->history_head + 1) % sys->history_max;
    }
}

static void notify_callback(Agentite_CommandSystem *sys, const Agentite_Command *cmd,
                             const Agentite_CommandResult *result) {
    if (sys->callback) {
        sys->callback(sys, cmd, result, sys->callback_userdata);
    }
}

/*============================================================================
 * Lifecycle
 *============================================================================*/

Agentite_CommandSystem *agentite_command_create(void) {
    Agentite_CommandSystem *sys = AGENTITE_ALLOC(Agentite_CommandSystem);
    if (!sys) {
        agentite_set_error("agentite_command_create: allocation failed");
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

void agentite_command_destroy(Agentite_CommandSystem *sys) {
    if (!sys) return;

    /* Free queued commands */
    for (int i = 0; i < sys->queue_count; i++) {
        agentite_command_free(sys->queue[i]);
    }

    /* Free history */
    for (int i = 0; i < sys->history_count; i++) {
        int idx = (sys->history_head + i) % sys->history_max;
        if (sys->history[idx]) {
            agentite_command_free(sys->history[idx]);
        }
    }

    free(sys);
}

/*============================================================================
 * Command Type Registration
 *============================================================================*/

bool agentite_command_register(Agentite_CommandSystem *sys,
                              int type,
                              Agentite_CommandValidator validator,
                              Agentite_CommandExecutor executor) {
    return agentite_command_register_named(sys, type, NULL, validator, executor);
}

bool agentite_command_register_named(Agentite_CommandSystem *sys,
                                    int type,
                                    const char *name,
                                    Agentite_CommandValidator validator,
                                    Agentite_CommandExecutor executor) {
    AGENTITE_VALIDATE_PTR_RET(sys, false);
    AGENTITE_VALIDATE_PTR_RET(executor, false);

    /* Check if already registered */
    if (find_type(sys, type)) {
        agentite_set_error("agentite_command_register: type %d already registered", type);
        return false;
    }

    /* Check capacity */
    if (sys->type_count >= AGENTITE_COMMAND_MAX_TYPES) {
        agentite_set_error("agentite_command_register: max types reached");
        return false;
    }

    CommandType *ct = &sys->types[sys->type_count];
    ct->type = type;
    ct->validator = validator;
    ct->executor = executor;
    ct->registered = true;

    if (name) {
        strncpy(ct->name, name, AGENTITE_COMMAND_MAX_PARAM_KEY - 1);
    } else {
        snprintf(ct->name, AGENTITE_COMMAND_MAX_PARAM_KEY, "Command_%d", type);
    }

    sys->type_count++;
    return true;
}

bool agentite_command_is_registered(const Agentite_CommandSystem *sys, int type) {
    AGENTITE_VALIDATE_PTR_RET(sys, false);
    return find_type((Agentite_CommandSystem*)sys, type) != NULL;
}

const char *agentite_command_get_type_name(const Agentite_CommandSystem *sys, int type) {
    AGENTITE_VALIDATE_PTR_RET(sys, NULL);
    CommandType *ct = find_type((Agentite_CommandSystem*)sys, type);
    return ct ? ct->name : NULL;
}

/*============================================================================
 * Command Creation
 *============================================================================*/

Agentite_Command *agentite_command_new(int type) {
    return agentite_command_new_ex(type, -1);
}

Agentite_Command *agentite_command_new_ex(int type, int32_t faction) {
    Agentite_Command *cmd = AGENTITE_ALLOC(Agentite_Command);
    if (!cmd) {
        agentite_set_error("agentite_command_new: allocation failed");
        return NULL;
    }

    cmd->type = type;
    cmd->param_count = 0;
    cmd->sequence = 0;  /* Set when queued */
    cmd->source_faction = faction;
    cmd->userdata = NULL;

    return cmd;
}

Agentite_Command *agentite_command_clone(const Agentite_Command *cmd) {
    AGENTITE_VALIDATE_PTR_RET(cmd, NULL);

    Agentite_Command *clone = AGENTITE_ALLOC(Agentite_Command);
    if (!clone) {
        agentite_set_error("agentite_command_clone: allocation failed");
        return NULL;
    }

    memcpy(clone, cmd, sizeof(Agentite_Command));
    /* Note: userdata is copied as pointer, not deep cloned */
    return clone;
}

void agentite_command_free(Agentite_Command *cmd) {
    free(cmd);
}

/*============================================================================
 * Command Parameters - Setters
 *============================================================================*/

void agentite_command_set_int(Agentite_Command *cmd, const char *key, int32_t value) {
    AGENTITE_VALIDATE_PTR(cmd);
    AGENTITE_VALIDATE_PTR(key);

    Agentite_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = AGENTITE_CMD_PARAM_INT;
        param->i32 = value;
    }
}

void agentite_command_set_int64(Agentite_Command *cmd, const char *key, int64_t value) {
    AGENTITE_VALIDATE_PTR(cmd);
    AGENTITE_VALIDATE_PTR(key);

    Agentite_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = AGENTITE_CMD_PARAM_INT64;
        param->i64 = value;
    }
}

void agentite_command_set_float(Agentite_Command *cmd, const char *key, float value) {
    AGENTITE_VALIDATE_PTR(cmd);
    AGENTITE_VALIDATE_PTR(key);

    Agentite_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = AGENTITE_CMD_PARAM_FLOAT;
        param->f32 = value;
    }
}

void agentite_command_set_double(Agentite_Command *cmd, const char *key, double value) {
    AGENTITE_VALIDATE_PTR(cmd);
    AGENTITE_VALIDATE_PTR(key);

    Agentite_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = AGENTITE_CMD_PARAM_DOUBLE;
        param->f64 = value;
    }
}

void agentite_command_set_bool(Agentite_Command *cmd, const char *key, bool value) {
    AGENTITE_VALIDATE_PTR(cmd);
    AGENTITE_VALIDATE_PTR(key);

    Agentite_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = AGENTITE_CMD_PARAM_BOOL;
        param->b = value;
    }
}

void agentite_command_set_entity(Agentite_Command *cmd, const char *key, uint32_t entity) {
    AGENTITE_VALIDATE_PTR(cmd);
    AGENTITE_VALIDATE_PTR(key);

    Agentite_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = AGENTITE_CMD_PARAM_ENTITY;
        param->entity = entity;
    }
}

void agentite_command_set_string(Agentite_Command *cmd, const char *key, const char *value) {
    AGENTITE_VALIDATE_PTR(cmd);
    AGENTITE_VALIDATE_PTR(key);

    Agentite_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = AGENTITE_CMD_PARAM_STRING;
        if (value) {
            strncpy(param->str, value, AGENTITE_COMMAND_MAX_PARAM_KEY - 1);
        } else {
            param->str[0] = '\0';
        }
    }
}

void agentite_command_set_ptr(Agentite_Command *cmd, const char *key, void *ptr) {
    AGENTITE_VALIDATE_PTR(cmd);
    AGENTITE_VALIDATE_PTR(key);

    Agentite_CommandParam *param = get_or_create_param(cmd, key);
    if (param) {
        param->type = AGENTITE_CMD_PARAM_PTR;
        param->ptr = ptr;
    }
}

/*============================================================================
 * Command Parameters - Getters
 *============================================================================*/

bool agentite_command_has_param(const Agentite_Command *cmd, const char *key) {
    AGENTITE_VALIDATE_PTR_RET(cmd, false);
    AGENTITE_VALIDATE_PTR_RET(key, false);
    return find_param_const(cmd, key) != NULL;
}

Agentite_CommandParamType agentite_command_get_param_type(const Agentite_Command *cmd, const char *key) {
    AGENTITE_VALIDATE_PTR_RET(cmd, AGENTITE_CMD_PARAM_NONE);
    AGENTITE_VALIDATE_PTR_RET(key, AGENTITE_CMD_PARAM_NONE);

    const Agentite_CommandParam *param = find_param_const(cmd, key);
    return param ? param->type : AGENTITE_CMD_PARAM_NONE;
}

int32_t agentite_command_get_int(const Agentite_Command *cmd, const char *key) {
    return agentite_command_get_int_or(cmd, key, 0);
}

int32_t agentite_command_get_int_or(const Agentite_Command *cmd, const char *key, int32_t def) {
    AGENTITE_VALIDATE_PTR_RET(cmd, def);
    AGENTITE_VALIDATE_PTR_RET(key, def);

    const Agentite_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == AGENTITE_CMD_PARAM_INT) {
        return param->i32;
    }
    return def;
}

int64_t agentite_command_get_int64(const Agentite_Command *cmd, const char *key) {
    AGENTITE_VALIDATE_PTR_RET(cmd, 0);
    AGENTITE_VALIDATE_PTR_RET(key, 0);

    const Agentite_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == AGENTITE_CMD_PARAM_INT64) {
        return param->i64;
    }
    return 0;
}

float agentite_command_get_float(const Agentite_Command *cmd, const char *key) {
    return agentite_command_get_float_or(cmd, key, 0.0f);
}

float agentite_command_get_float_or(const Agentite_Command *cmd, const char *key, float def) {
    AGENTITE_VALIDATE_PTR_RET(cmd, def);
    AGENTITE_VALIDATE_PTR_RET(key, def);

    const Agentite_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == AGENTITE_CMD_PARAM_FLOAT) {
        return param->f32;
    }
    return def;
}

double agentite_command_get_double(const Agentite_Command *cmd, const char *key) {
    AGENTITE_VALIDATE_PTR_RET(cmd, 0.0);
    AGENTITE_VALIDATE_PTR_RET(key, 0.0);

    const Agentite_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == AGENTITE_CMD_PARAM_DOUBLE) {
        return param->f64;
    }
    return 0.0;
}

bool agentite_command_get_bool(const Agentite_Command *cmd, const char *key) {
    AGENTITE_VALIDATE_PTR_RET(cmd, false);
    AGENTITE_VALIDATE_PTR_RET(key, false);

    const Agentite_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == AGENTITE_CMD_PARAM_BOOL) {
        return param->b;
    }
    return false;
}

uint32_t agentite_command_get_entity(const Agentite_Command *cmd, const char *key) {
    AGENTITE_VALIDATE_PTR_RET(cmd, 0);
    AGENTITE_VALIDATE_PTR_RET(key, 0);

    const Agentite_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == AGENTITE_CMD_PARAM_ENTITY) {
        return param->entity;
    }
    return 0;
}

const char *agentite_command_get_string(const Agentite_Command *cmd, const char *key) {
    AGENTITE_VALIDATE_PTR_RET(cmd, NULL);
    AGENTITE_VALIDATE_PTR_RET(key, NULL);

    const Agentite_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == AGENTITE_CMD_PARAM_STRING) {
        return param->str;
    }
    return NULL;
}

void *agentite_command_get_ptr(const Agentite_Command *cmd, const char *key) {
    AGENTITE_VALIDATE_PTR_RET(cmd, NULL);
    AGENTITE_VALIDATE_PTR_RET(key, NULL);

    const Agentite_CommandParam *param = find_param_const(cmd, key);
    if (param && param->type == AGENTITE_CMD_PARAM_PTR) {
        return param->ptr;
    }
    return NULL;
}

/*============================================================================
 * Validation
 *============================================================================*/

Agentite_CommandResult agentite_command_validate(Agentite_CommandSystem *sys,
                                               const Agentite_Command *cmd,
                                               void *game_state) {
    Agentite_CommandResult result = {0};

    AGENTITE_VALIDATE_PTR_RET(sys, result);
    AGENTITE_VALIDATE_PTR_RET(cmd, result);

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

bool agentite_command_queue(Agentite_CommandSystem *sys, Agentite_Command *cmd) {
    AGENTITE_VALIDATE_PTR_RET(sys, false);
    AGENTITE_VALIDATE_PTR_RET(cmd, false);

    if (sys->queue_count >= AGENTITE_COMMAND_MAX_QUEUE) {
        agentite_set_error("agentite_command_queue: queue is full");
        return false;
    }

    /* Clone command */
    Agentite_Command *clone = agentite_command_clone(cmd);
    if (!clone) {
        return false;
    }

    clone->sequence = sys->next_sequence++;
    sys->queue[sys->queue_count] = clone;
    sys->queue_count++;

    return true;
}

Agentite_CommandResult agentite_command_queue_validated(Agentite_CommandSystem *sys,
                                                      Agentite_Command *cmd,
                                                      void *game_state) {
    Agentite_CommandResult result = agentite_command_validate(sys, cmd, game_state);
    if (result.success) {
        if (!agentite_command_queue(sys, cmd)) {
            result.success = false;
            snprintf(result.error, sizeof(result.error), "Failed to queue command");
        }
    }
    return result;
}

int agentite_command_queue_count(const Agentite_CommandSystem *sys) {
    AGENTITE_VALIDATE_PTR_RET(sys, 0);
    return sys->queue_count;
}

void agentite_command_queue_clear(Agentite_CommandSystem *sys) {
    AGENTITE_VALIDATE_PTR(sys);

    for (int i = 0; i < sys->queue_count; i++) {
        agentite_command_free(sys->queue[i]);
        sys->queue[i] = NULL;
    }
    sys->queue_count = 0;
}

const Agentite_Command *agentite_command_queue_get(const Agentite_CommandSystem *sys, int index) {
    AGENTITE_VALIDATE_PTR_RET(sys, NULL);

    if (index < 0 || index >= sys->queue_count) {
        return NULL;
    }
    return sys->queue[index];
}

bool agentite_command_queue_remove(Agentite_CommandSystem *sys, int index) {
    AGENTITE_VALIDATE_PTR_RET(sys, false);

    if (index < 0 || index >= sys->queue_count) {
        return false;
    }

    agentite_command_free(sys->queue[index]);

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

int agentite_command_execute_all(Agentite_CommandSystem *sys,
                                void *game_state,
                                Agentite_CommandResult *results,
                                int max) {
    AGENTITE_VALIDATE_PTR_RET(sys, 0);

    int executed = 0;

    while (sys->queue_count > 0 && (max <= 0 || executed < max)) {
        Agentite_CommandResult result = agentite_command_execute_next(sys, game_state);
        if (results && executed < max) {
            results[executed] = result;
        }
        executed++;
    }

    return executed;
}

Agentite_CommandResult agentite_command_execute(Agentite_CommandSystem *sys,
                                              const Agentite_Command *cmd,
                                              void *game_state) {
    Agentite_CommandResult result = {0};

    AGENTITE_VALIDATE_PTR_RET(sys, result);
    AGENTITE_VALIDATE_PTR_RET(cmd, result);

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
        if (cmd->type >= 0 && cmd->type < AGENTITE_COMMAND_MAX_TYPES) {
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

Agentite_CommandResult agentite_command_execute_next(Agentite_CommandSystem *sys,
                                                   void *game_state) {
    Agentite_CommandResult result = {0};

    AGENTITE_VALIDATE_PTR_RET(sys, result);

    if (sys->queue_count <= 0) {
        result.success = false;
        snprintf(result.error, sizeof(result.error), "Queue is empty");
        return result;
    }

    /* Get and remove first command */
    Agentite_Command *cmd = sys->queue[0];
    for (int i = 0; i < sys->queue_count - 1; i++) {
        sys->queue[i] = sys->queue[i + 1];
    }
    sys->queue_count--;

    /* Execute */
    result = agentite_command_execute(sys, cmd, game_state);

    /* Free command */
    agentite_command_free(cmd);

    return result;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void agentite_command_set_callback(Agentite_CommandSystem *sys,
                                  Agentite_CommandCallback callback,
                                  void *userdata) {
    AGENTITE_VALIDATE_PTR(sys);
    sys->callback = callback;
    sys->callback_userdata = userdata;
}

/*============================================================================
 * History
 *============================================================================*/

void agentite_command_enable_history(Agentite_CommandSystem *sys, int max_commands) {
    AGENTITE_VALIDATE_PTR(sys);

    /* Clear existing history */
    agentite_command_clear_history(sys);

    if (max_commands > AGENTITE_COMMAND_MAX_HISTORY) {
        max_commands = AGENTITE_COMMAND_MAX_HISTORY;
    }

    sys->history_max = max_commands;
}

int agentite_command_get_history(const Agentite_CommandSystem *sys,
                                const Agentite_Command **out,
                                int max) {
    AGENTITE_VALIDATE_PTR_RET(sys, 0);
    AGENTITE_VALIDATE_PTR_RET(out, 0);

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

int agentite_command_get_history_count(const Agentite_CommandSystem *sys) {
    AGENTITE_VALIDATE_PTR_RET(sys, 0);
    return sys->history_count;
}

void agentite_command_clear_history(Agentite_CommandSystem *sys) {
    AGENTITE_VALIDATE_PTR(sys);

    if (sys->history_max <= 0) return;

    for (int i = 0; i < sys->history_count; i++) {
        int idx = (sys->history_head + i) % sys->history_max;
        if (sys->history[idx]) {
            agentite_command_free(sys->history[idx]);
            sys->history[idx] = NULL;
        }
    }

    sys->history_count = 0;
    sys->history_head = 0;
}

Agentite_CommandResult agentite_command_replay(Agentite_CommandSystem *sys,
                                             int index,
                                             void *game_state) {
    Agentite_CommandResult result = {0};

    AGENTITE_VALIDATE_PTR_RET(sys, result);

    if (sys->history_max <= 0 || index < 0 || index >= sys->history_count) {
        result.success = false;
        snprintf(result.error, sizeof(result.error), "Invalid history index");
        return result;
    }

    /* Get from end (newest first) */
    int actual_idx = (sys->history_head + sys->history_count - 1 - index) % sys->history_max;
    const Agentite_Command *cmd = sys->history[actual_idx];

    if (!cmd) {
        result.success = false;
        snprintf(result.error, sizeof(result.error), "History entry is null");
        return result;
    }

    return agentite_command_execute(sys, cmd, game_state);
}

/*============================================================================
 * Statistics
 *============================================================================*/

void agentite_command_get_stats(const Agentite_CommandSystem *sys, Agentite_CommandStats *out) {
    AGENTITE_VALIDATE_PTR(sys);
    AGENTITE_VALIDATE_PTR(out);
    *out = sys->stats;
}

void agentite_command_reset_stats(Agentite_CommandSystem *sys) {
    AGENTITE_VALIDATE_PTR(sys);
    memset(&sys->stats, 0, sizeof(sys->stats));
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

Agentite_CommandResult agentite_command_result_failure(int type, const char *error) {
    Agentite_CommandResult r = {0};
    r.success = false;
    r.command_type = type;
    if (error) {
        strncpy(r.error, error, AGENTITE_COMMAND_MAX_ERROR - 1);
    }
    return r;
}
