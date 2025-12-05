/**
 * Carbon Hierarchical Task Network (HTN) AI Planner
 *
 * A sophisticated AI planning system that decomposes high-level goals
 * into executable primitive tasks.
 */

#include "carbon/carbon.h"
#include "carbon/htn.h"
#include "carbon/error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================
 * Internal Data Structures
 *============================================================================*/

/**
 * World state variable
 */
typedef struct {
    char key[CARBON_HTN_MAX_KEY_LEN];
    Carbon_HTNValue value;
    bool used;
} WSVariable;

/**
 * World state
 */
struct Carbon_HTNWorldState {
    WSVariable vars[CARBON_HTN_MAX_STATE_VARS];
    int count;
};

/**
 * Method for compound task decomposition
 */
typedef struct {
    /* Preconditions */
    Carbon_HTNConditionFunc precond_fn;
    Carbon_HTNCondition conditions[CARBON_HTN_MAX_CONDITIONS];
    int condition_count;

    /* Subtask sequence */
    char subtasks[CARBON_HTN_MAX_SUBTASKS][CARBON_HTN_MAX_KEY_LEN];
    int subtask_count;

    bool used;
} HTNMethod;

/**
 * Task definition
 */
struct Carbon_HTNTask {
    char name[CARBON_HTN_MAX_KEY_LEN];
    bool is_primitive;

    /* Primitive task data */
    Carbon_HTNExecuteFunc execute_fn;
    Carbon_HTNConditionFunc precond_fn;
    Carbon_HTNEffectFunc effect_fn;
    Carbon_HTNCondition conditions[CARBON_HTN_MAX_CONDITIONS];
    int condition_count;
    Carbon_HTNEffect effects[CARBON_HTN_MAX_EFFECTS];
    int effect_count;

    /* Compound task data */
    HTNMethod methods[CARBON_HTN_MAX_METHODS];
    int method_count;

    bool used;
};

/**
 * Domain
 */
struct Carbon_HTNDomain {
    Carbon_HTNTask tasks[CARBON_HTN_MAX_TASKS];
    int task_count;
};

/**
 * Plan
 */
struct Carbon_HTNPlan {
    Carbon_HTNTask *tasks[CARBON_HTN_MAX_PLAN_LEN];
    int length;
    bool valid;
};

/**
 * Executor
 */
struct Carbon_HTNExecutor {
    Carbon_HTNDomain *domain;
    Carbon_HTNPlan *plan;
    int current_index;
    Carbon_HTNStatus status;
    bool running;
};

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Find variable by key
 */
static WSVariable *find_var(Carbon_HTNWorldState *ws, const char *key) {
    for (int i = 0; i < CARBON_HTN_MAX_STATE_VARS; i++) {
        if (ws->vars[i].used && strcmp(ws->vars[i].key, key) == 0) {
            return &ws->vars[i];
        }
    }
    return NULL;
}

/**
 * Find or create variable
 */
static WSVariable *get_or_create_var(Carbon_HTNWorldState *ws, const char *key) {
    WSVariable *var = find_var(ws, key);
    if (var) return var;

    for (int i = 0; i < CARBON_HTN_MAX_STATE_VARS; i++) {
        if (!ws->vars[i].used) {
            var = &ws->vars[i];
            var->used = true;
            strncpy(var->key, key, CARBON_HTN_MAX_KEY_LEN - 1);
            var->key[CARBON_HTN_MAX_KEY_LEN - 1] = '\0';
            memset(&var->value, 0, sizeof(Carbon_HTNValue));
            ws->count++;
            return var;
        }
    }

    return NULL;
}

/**
 * Find task by name
 */
static Carbon_HTNTask *find_task_mut(Carbon_HTNDomain *domain, const char *name) {
    for (int i = 0; i < CARBON_HTN_MAX_TASKS; i++) {
        if (domain->tasks[i].used && strcmp(domain->tasks[i].name, name) == 0) {
            return &domain->tasks[i];
        }
    }
    return NULL;
}

/**
 * Allocate task slot
 */
static Carbon_HTNTask *alloc_task(Carbon_HTNDomain *domain) {
    for (int i = 0; i < CARBON_HTN_MAX_TASKS; i++) {
        if (!domain->tasks[i].used) {
            return &domain->tasks[i];
        }
    }
    return NULL;
}

/**
 * Check if preconditions are met
 */
static bool check_preconditions(const Carbon_HTNWorldState *ws,
                                 const Carbon_HTNTask *task) {
    /* Check callback first */
    if (task->precond_fn) {
        if (!task->precond_fn(ws, NULL)) {
            return false;
        }
    }

    /* Check declarative conditions */
    if (task->condition_count > 0) {
        if (!carbon_htn_eval_conditions(ws, task->conditions, task->condition_count)) {
            return false;
        }
    }

    return true;
}

/**
 * Check method preconditions
 */
static bool check_method_preconditions(const Carbon_HTNWorldState *ws,
                                        const HTNMethod *method) {
    /* Check callback first */
    if (method->precond_fn) {
        if (!method->precond_fn(ws, NULL)) {
            return false;
        }
    }

    /* Check declarative conditions */
    if (method->condition_count > 0) {
        if (!carbon_htn_eval_conditions(ws, method->conditions, method->condition_count)) {
            return false;
        }
    }

    return true;
}

/**
 * Apply task effects to world state
 */
static void apply_task_effects(Carbon_HTNWorldState *ws, const Carbon_HTNTask *task) {
    /* Apply callback effects */
    if (task->effect_fn) {
        task->effect_fn(ws, NULL);
    }

    /* Apply declarative effects */
    carbon_htn_apply_effects(ws, task->effects, task->effect_count);
}

/*============================================================================
 * World State - Creation/Destruction
 *============================================================================*/

Carbon_HTNWorldState *carbon_htn_world_state_create(void) {
    Carbon_HTNWorldState *ws = CARBON_ALLOC(Carbon_HTNWorldState);
    if (!ws) {
        carbon_set_error("carbon_htn_world_state_create: allocation failed");
        return NULL;
    }
    return ws;
}

void carbon_htn_world_state_destroy(Carbon_HTNWorldState *ws) {
    if (ws) {
        free(ws);
    }
}

Carbon_HTNWorldState *carbon_htn_world_state_clone(const Carbon_HTNWorldState *ws) {
    if (!ws) return NULL;

    Carbon_HTNWorldState *clone = carbon_htn_world_state_create();
    if (clone) {
        carbon_htn_world_state_copy(clone, ws);
    }
    return clone;
}

void carbon_htn_world_state_copy(Carbon_HTNWorldState *dest,
                                  const Carbon_HTNWorldState *src) {
    if (!dest || !src) return;
    memcpy(dest, src, sizeof(Carbon_HTNWorldState));
}

void carbon_htn_world_state_clear(Carbon_HTNWorldState *ws) {
    if (!ws) return;
    memset(ws->vars, 0, sizeof(ws->vars));
    ws->count = 0;
}

/*============================================================================
 * World State - Value Access
 *============================================================================*/

void carbon_htn_ws_set_int(Carbon_HTNWorldState *ws, const char *key, int32_t value) {
    if (!ws || !key) return;

    WSVariable *var = get_or_create_var(ws, key);
    if (var) {
        var->value.type = CARBON_HTN_TYPE_INT;
        var->value.i32 = value;
    }
}

void carbon_htn_ws_set_float(Carbon_HTNWorldState *ws, const char *key, float value) {
    if (!ws || !key) return;

    WSVariable *var = get_or_create_var(ws, key);
    if (var) {
        var->value.type = CARBON_HTN_TYPE_FLOAT;
        var->value.f32 = value;
    }
}

void carbon_htn_ws_set_bool(Carbon_HTNWorldState *ws, const char *key, bool value) {
    if (!ws || !key) return;

    WSVariable *var = get_or_create_var(ws, key);
    if (var) {
        var->value.type = CARBON_HTN_TYPE_BOOL;
        var->value.b = value;
    }
}

void carbon_htn_ws_set_ptr(Carbon_HTNWorldState *ws, const char *key, void *value) {
    if (!ws || !key) return;

    WSVariable *var = get_or_create_var(ws, key);
    if (var) {
        var->value.type = CARBON_HTN_TYPE_PTR;
        var->value.ptr = value;
    }
}

int32_t carbon_htn_ws_get_int(const Carbon_HTNWorldState *ws, const char *key) {
    if (!ws || !key) return 0;

    WSVariable *var = find_var((Carbon_HTNWorldState *)ws, key);
    if (var && var->value.type == CARBON_HTN_TYPE_INT) {
        return var->value.i32;
    }
    return 0;
}

float carbon_htn_ws_get_float(const Carbon_HTNWorldState *ws, const char *key) {
    if (!ws || !key) return 0.0f;

    WSVariable *var = find_var((Carbon_HTNWorldState *)ws, key);
    if (var && var->value.type == CARBON_HTN_TYPE_FLOAT) {
        return var->value.f32;
    }
    return 0.0f;
}

bool carbon_htn_ws_get_bool(const Carbon_HTNWorldState *ws, const char *key) {
    if (!ws || !key) return false;

    WSVariable *var = find_var((Carbon_HTNWorldState *)ws, key);
    if (var && var->value.type == CARBON_HTN_TYPE_BOOL) {
        return var->value.b;
    }
    return false;
}

void *carbon_htn_ws_get_ptr(const Carbon_HTNWorldState *ws, const char *key) {
    if (!ws || !key) return NULL;

    WSVariable *var = find_var((Carbon_HTNWorldState *)ws, key);
    if (var && var->value.type == CARBON_HTN_TYPE_PTR) {
        return var->value.ptr;
    }
    return NULL;
}

bool carbon_htn_ws_has(const Carbon_HTNWorldState *ws, const char *key) {
    if (!ws || !key) return false;
    return find_var((Carbon_HTNWorldState *)ws, key) != NULL;
}

void carbon_htn_ws_remove(Carbon_HTNWorldState *ws, const char *key) {
    if (!ws || !key) return;

    WSVariable *var = find_var(ws, key);
    if (var) {
        var->used = false;
        ws->count--;
    }
}

const Carbon_HTNValue *carbon_htn_ws_get_value(const Carbon_HTNWorldState *ws,
                                                const char *key) {
    if (!ws || !key) return NULL;

    WSVariable *var = find_var((Carbon_HTNWorldState *)ws, key);
    return var ? &var->value : NULL;
}

void carbon_htn_ws_inc_int(Carbon_HTNWorldState *ws, const char *key, int32_t amount) {
    if (!ws || !key) return;

    WSVariable *var = find_var(ws, key);
    if (var && var->value.type == CARBON_HTN_TYPE_INT) {
        var->value.i32 += amount;
    } else {
        carbon_htn_ws_set_int(ws, key, amount);
    }
}

void carbon_htn_ws_inc_float(Carbon_HTNWorldState *ws, const char *key, float amount) {
    if (!ws || !key) return;

    WSVariable *var = find_var(ws, key);
    if (var && var->value.type == CARBON_HTN_TYPE_FLOAT) {
        var->value.f32 += amount;
    } else {
        carbon_htn_ws_set_float(ws, key, amount);
    }
}

/*============================================================================
 * Domain - Creation/Destruction
 *============================================================================*/

Carbon_HTNDomain *carbon_htn_domain_create(void) {
    Carbon_HTNDomain *domain = CARBON_ALLOC(Carbon_HTNDomain);
    if (!domain) {
        carbon_set_error("carbon_htn_domain_create: allocation failed");
        return NULL;
    }
    return domain;
}

void carbon_htn_domain_destroy(Carbon_HTNDomain *domain) {
    if (domain) {
        free(domain);
    }
}

/*============================================================================
 * Domain - Task Registration
 *============================================================================*/

int carbon_htn_register_primitive(Carbon_HTNDomain *domain, const char *name,
                                   Carbon_HTNExecuteFunc execute,
                                   Carbon_HTNConditionFunc precond,
                                   Carbon_HTNEffectFunc effect) {
    if (!domain || !name || !execute) {
        carbon_set_error("carbon_htn_register_primitive: invalid parameters");
        return -1;
    }

    /* Check for duplicate */
    if (find_task_mut(domain, name)) {
        carbon_set_error("carbon_htn_register_primitive: task '%s' already exists", name);
        return -1;
    }

    Carbon_HTNTask *task = alloc_task(domain);
    if (!task) {
        carbon_set_error("carbon_htn_register_primitive: max tasks reached");
        return -1;
    }

    memset(task, 0, sizeof(Carbon_HTNTask));
    strncpy(task->name, name, CARBON_HTN_MAX_KEY_LEN - 1);
    task->name[CARBON_HTN_MAX_KEY_LEN - 1] = '\0';
    task->is_primitive = true;
    task->execute_fn = execute;
    task->precond_fn = precond;
    task->effect_fn = effect;
    task->used = true;
    domain->task_count++;

    return domain->task_count - 1;
}

int carbon_htn_register_primitive_ex(Carbon_HTNDomain *domain, const char *name,
                                      Carbon_HTNExecuteFunc execute,
                                      const Carbon_HTNCondition *conditions,
                                      int cond_count,
                                      const Carbon_HTNEffect *effects,
                                      int effect_count) {
    if (!domain || !name || !execute) {
        carbon_set_error("carbon_htn_register_primitive_ex: invalid parameters");
        return -1;
    }

    int idx = carbon_htn_register_primitive(domain, name, execute, NULL, NULL);
    if (idx < 0) return idx;

    Carbon_HTNTask *task = find_task_mut(domain, name);
    if (!task) return -1;

    /* Copy conditions */
    if (conditions && cond_count > 0) {
        task->condition_count = (cond_count > CARBON_HTN_MAX_CONDITIONS) ?
                                 CARBON_HTN_MAX_CONDITIONS : cond_count;
        memcpy(task->conditions, conditions,
               task->condition_count * sizeof(Carbon_HTNCondition));
    }

    /* Copy effects */
    if (effects && effect_count > 0) {
        task->effect_count = (effect_count > CARBON_HTN_MAX_EFFECTS) ?
                              CARBON_HTN_MAX_EFFECTS : effect_count;
        memcpy(task->effects, effects,
               task->effect_count * sizeof(Carbon_HTNEffect));
    }

    return idx;
}

int carbon_htn_register_compound(Carbon_HTNDomain *domain, const char *name) {
    if (!domain || !name) {
        carbon_set_error("carbon_htn_register_compound: invalid parameters");
        return -1;
    }

    /* Check for duplicate */
    if (find_task_mut(domain, name)) {
        carbon_set_error("carbon_htn_register_compound: task '%s' already exists", name);
        return -1;
    }

    Carbon_HTNTask *task = alloc_task(domain);
    if (!task) {
        carbon_set_error("carbon_htn_register_compound: max tasks reached");
        return -1;
    }

    memset(task, 0, sizeof(Carbon_HTNTask));
    strncpy(task->name, name, CARBON_HTN_MAX_KEY_LEN - 1);
    task->name[CARBON_HTN_MAX_KEY_LEN - 1] = '\0';
    task->is_primitive = false;
    task->used = true;
    domain->task_count++;

    return domain->task_count - 1;
}

int carbon_htn_add_method(Carbon_HTNDomain *domain, const char *compound_name,
                           Carbon_HTNConditionFunc precond,
                           const char **subtasks, int subtask_count) {
    if (!domain || !compound_name || !subtasks || subtask_count <= 0) {
        carbon_set_error("carbon_htn_add_method: invalid parameters");
        return -1;
    }

    Carbon_HTNTask *task = find_task_mut(domain, compound_name);
    if (!task) {
        carbon_set_error("carbon_htn_add_method: compound task '%s' not found", compound_name);
        return -1;
    }

    if (task->is_primitive) {
        carbon_set_error("carbon_htn_add_method: task '%s' is not compound", compound_name);
        return -1;
    }

    if (task->method_count >= CARBON_HTN_MAX_METHODS) {
        carbon_set_error("carbon_htn_add_method: max methods reached for '%s'", compound_name);
        return -1;
    }

    HTNMethod *method = &task->methods[task->method_count];
    memset(method, 0, sizeof(HTNMethod));
    method->precond_fn = precond;
    method->used = true;

    /* Copy subtask names */
    method->subtask_count = (subtask_count > CARBON_HTN_MAX_SUBTASKS) ?
                             CARBON_HTN_MAX_SUBTASKS : subtask_count;
    for (int i = 0; i < method->subtask_count; i++) {
        strncpy(method->subtasks[i], subtasks[i], CARBON_HTN_MAX_KEY_LEN - 1);
        method->subtasks[i][CARBON_HTN_MAX_KEY_LEN - 1] = '\0';
    }

    return task->method_count++;
}

int carbon_htn_add_method_ex(Carbon_HTNDomain *domain, const char *compound_name,
                              const Carbon_HTNCondition *conditions, int cond_count,
                              const char **subtasks, int subtask_count) {
    int idx = carbon_htn_add_method(domain, compound_name, NULL, subtasks, subtask_count);
    if (idx < 0) return idx;

    Carbon_HTNTask *task = find_task_mut(domain, compound_name);
    if (!task) return -1;

    HTNMethod *method = &task->methods[idx];

    /* Copy conditions */
    if (conditions && cond_count > 0) {
        method->condition_count = (cond_count > CARBON_HTN_MAX_CONDITIONS) ?
                                   CARBON_HTN_MAX_CONDITIONS : cond_count;
        memcpy(method->conditions, conditions,
               method->condition_count * sizeof(Carbon_HTNCondition));
    }

    return idx;
}

const Carbon_HTNTask *carbon_htn_find_task(const Carbon_HTNDomain *domain,
                                            const char *name) {
    if (!domain || !name) return NULL;
    return find_task_mut((Carbon_HTNDomain *)domain, name);
}

int carbon_htn_task_count(const Carbon_HTNDomain *domain) {
    return domain ? domain->task_count : 0;
}

bool carbon_htn_task_is_primitive(const Carbon_HTNTask *task) {
    return task ? task->is_primitive : false;
}

const char *carbon_htn_task_name(const Carbon_HTNTask *task) {
    return task ? task->name : NULL;
}

/*============================================================================
 * Planning
 *============================================================================*/

Carbon_HTNPlan *carbon_htn_plan(Carbon_HTNDomain *domain,
                                 const Carbon_HTNWorldState *ws,
                                 const char *root_task,
                                 int max_iterations) {
    if (!domain || !ws || !root_task) {
        carbon_set_error("carbon_htn_plan: invalid parameters");
        return NULL;
    }

    if (max_iterations <= 0) {
        max_iterations = 1000;
    }

    /* Create plan */
    Carbon_HTNPlan *plan = CARBON_ALLOC(Carbon_HTNPlan);
    if (!plan) {
        carbon_set_error("carbon_htn_plan: allocation failed");
        return NULL;
    }

    /* Clone world state for simulation */
    Carbon_HTNWorldState *sim_ws = carbon_htn_world_state_clone(ws);
    if (!sim_ws) {
        free(plan);
        return NULL;
    }

    /* Task decomposition stack */
    const char *stack[CARBON_HTN_MAX_STACK_DEPTH];
    int stack_size = 0;

    /* Push root task */
    stack[stack_size++] = root_task;

    int iterations = 0;

    /* Process task stack */
    while (stack_size > 0 && iterations < max_iterations) {
        iterations++;

        /* Pop task from stack */
        const char *task_name = stack[--stack_size];
        Carbon_HTNTask *task = find_task_mut(domain, task_name);

        if (!task) {
            carbon_set_error("carbon_htn_plan: unknown task '%s'", task_name);
            goto fail;
        }

        if (task->is_primitive) {
            /* Check preconditions */
            if (!check_preconditions(sim_ws, task)) {
                carbon_set_error("carbon_htn_plan: preconditions failed for '%s'", task_name);
                goto fail;
            }

            /* Add to plan */
            if (plan->length >= CARBON_HTN_MAX_PLAN_LEN) {
                carbon_set_error("carbon_htn_plan: max plan length reached");
                goto fail;
            }
            plan->tasks[plan->length++] = task;

            /* Apply effects to simulated world state */
            apply_task_effects(sim_ws, task);
        } else {
            /* Compound task - find applicable method */
            bool found_method = false;

            for (int m = 0; m < task->method_count; m++) {
                HTNMethod *method = &task->methods[m];

                if (check_method_preconditions(sim_ws, method)) {
                    /* Push subtasks in reverse order (so first is processed first) */
                    if (stack_size + method->subtask_count > CARBON_HTN_MAX_STACK_DEPTH) {
                        carbon_set_error("carbon_htn_plan: stack overflow");
                        goto fail;
                    }

                    for (int s = method->subtask_count - 1; s >= 0; s--) {
                        stack[stack_size++] = method->subtasks[s];
                    }

                    found_method = true;
                    break;
                }
            }

            if (!found_method) {
                carbon_set_error("carbon_htn_plan: no applicable method for '%s'", task_name);
                goto fail;
            }
        }
    }

    if (iterations >= max_iterations) {
        carbon_set_error("carbon_htn_plan: max iterations reached");
        goto fail;
    }

    carbon_htn_world_state_destroy(sim_ws);
    plan->valid = true;
    return plan;

fail:
    carbon_htn_world_state_destroy(sim_ws);
    plan->valid = false;
    return plan;
}

bool carbon_htn_plan_valid(const Carbon_HTNPlan *plan) {
    return plan && plan->valid;
}

int carbon_htn_plan_length(const Carbon_HTNPlan *plan) {
    return plan ? plan->length : 0;
}

const Carbon_HTNTask *carbon_htn_plan_get_task(const Carbon_HTNPlan *plan, int index) {
    if (!plan || index < 0 || index >= plan->length) return NULL;
    return plan->tasks[index];
}

const char *carbon_htn_plan_get_task_name(const Carbon_HTNPlan *plan, int index) {
    const Carbon_HTNTask *task = carbon_htn_plan_get_task(plan, index);
    return task ? task->name : NULL;
}

void carbon_htn_plan_destroy(Carbon_HTNPlan *plan) {
    if (plan) {
        free(plan);
    }
}

/*============================================================================
 * Execution
 *============================================================================*/

Carbon_HTNExecutor *carbon_htn_executor_create(Carbon_HTNDomain *domain) {
    if (!domain) {
        carbon_set_error("carbon_htn_executor_create: null domain");
        return NULL;
    }

    Carbon_HTNExecutor *exec = CARBON_ALLOC(Carbon_HTNExecutor);
    if (!exec) {
        carbon_set_error("carbon_htn_executor_create: allocation failed");
        return NULL;
    }

    exec->domain = domain;
    exec->current_index = -1;
    exec->status = CARBON_HTN_INVALID;

    return exec;
}

void carbon_htn_executor_destroy(Carbon_HTNExecutor *exec) {
    if (exec) {
        if (exec->plan) {
            carbon_htn_plan_destroy(exec->plan);
        }
        free(exec);
    }
}

void carbon_htn_executor_set_plan(Carbon_HTNExecutor *exec, Carbon_HTNPlan *plan) {
    if (!exec) return;

    if (exec->plan) {
        carbon_htn_plan_destroy(exec->plan);
    }

    exec->plan = plan;
    exec->current_index = 0;
    exec->status = (plan && plan->valid && plan->length > 0) ?
                    CARBON_HTN_RUNNING : CARBON_HTN_INVALID;
    exec->running = (exec->status == CARBON_HTN_RUNNING);
}

Carbon_HTNStatus carbon_htn_executor_update(Carbon_HTNExecutor *exec,
                                             Carbon_HTNWorldState *ws,
                                             void *userdata) {
    if (!exec || !exec->plan || !exec->running) {
        return CARBON_HTN_INVALID;
    }

    if (exec->current_index >= exec->plan->length) {
        exec->status = CARBON_HTN_SUCCESS;
        exec->running = false;
        return CARBON_HTN_SUCCESS;
    }

    Carbon_HTNTask *task = exec->plan->tasks[exec->current_index];
    if (!task) {
        exec->status = CARBON_HTN_FAILED;
        exec->running = false;
        return CARBON_HTN_FAILED;
    }

    /* Execute current task */
    Carbon_HTNStatus task_status = CARBON_HTN_SUCCESS;
    if (task->execute_fn) {
        task_status = task->execute_fn(ws, userdata);
    }

    switch (task_status) {
        case CARBON_HTN_RUNNING:
            return CARBON_HTN_RUNNING;

        case CARBON_HTN_SUCCESS:
            /* Apply effects */
            apply_task_effects(ws, task);

            /* Move to next task */
            exec->current_index++;
            if (exec->current_index >= exec->plan->length) {
                exec->status = CARBON_HTN_SUCCESS;
                exec->running = false;
                return CARBON_HTN_SUCCESS;
            }
            return CARBON_HTN_RUNNING;

        case CARBON_HTN_FAILED:
        default:
            exec->status = CARBON_HTN_FAILED;
            exec->running = false;
            return CARBON_HTN_FAILED;
    }
}

void carbon_htn_executor_reset(Carbon_HTNExecutor *exec) {
    if (!exec) return;

    exec->current_index = 0;
    if (exec->plan && exec->plan->valid && exec->plan->length > 0) {
        exec->status = CARBON_HTN_RUNNING;
        exec->running = true;
    }
}

bool carbon_htn_executor_is_running(const Carbon_HTNExecutor *exec) {
    return exec ? exec->running : false;
}

int carbon_htn_executor_get_current_index(const Carbon_HTNExecutor *exec) {
    return (exec && exec->running) ? exec->current_index : -1;
}

const char *carbon_htn_executor_get_current_task(const Carbon_HTNExecutor *exec) {
    if (!exec || !exec->plan || !exec->running) return NULL;
    if (exec->current_index < 0 || exec->current_index >= exec->plan->length) return NULL;

    return exec->plan->tasks[exec->current_index]->name;
}

float carbon_htn_executor_get_progress(const Carbon_HTNExecutor *exec) {
    if (!exec || !exec->plan || exec->plan->length == 0) return 0.0f;
    return (float)exec->current_index / (float)exec->plan->length;
}

void carbon_htn_executor_abort(Carbon_HTNExecutor *exec) {
    if (exec) {
        exec->status = CARBON_HTN_FAILED;
        exec->running = false;
    }
}

/*============================================================================
 * Condition Helpers
 *============================================================================*/

Carbon_HTNCondition carbon_htn_cond_int(const char *key, Carbon_HTNOperator op,
                                         int32_t value) {
    Carbon_HTNCondition cond = {0};
    if (key) {
        strncpy(cond.key, key, CARBON_HTN_MAX_KEY_LEN - 1);
    }
    cond.op = op;
    cond.value.type = CARBON_HTN_TYPE_INT;
    cond.value.i32 = value;
    return cond;
}

Carbon_HTNCondition carbon_htn_cond_float(const char *key, Carbon_HTNOperator op,
                                           float value) {
    Carbon_HTNCondition cond = {0};
    if (key) {
        strncpy(cond.key, key, CARBON_HTN_MAX_KEY_LEN - 1);
    }
    cond.op = op;
    cond.value.type = CARBON_HTN_TYPE_FLOAT;
    cond.value.f32 = value;
    return cond;
}

Carbon_HTNCondition carbon_htn_cond_bool(const char *key, bool value) {
    Carbon_HTNCondition cond = {0};
    if (key) {
        strncpy(cond.key, key, CARBON_HTN_MAX_KEY_LEN - 1);
    }
    cond.op = value ? CARBON_HTN_OP_TRUE : CARBON_HTN_OP_FALSE;
    cond.value.type = CARBON_HTN_TYPE_BOOL;
    cond.value.b = value;
    return cond;
}

Carbon_HTNCondition carbon_htn_cond_has(const char *key) {
    Carbon_HTNCondition cond = {0};
    if (key) {
        strncpy(cond.key, key, CARBON_HTN_MAX_KEY_LEN - 1);
    }
    cond.op = CARBON_HTN_OP_HAS;
    return cond;
}

Carbon_HTNCondition carbon_htn_cond_not_has(const char *key) {
    Carbon_HTNCondition cond = {0};
    if (key) {
        strncpy(cond.key, key, CARBON_HTN_MAX_KEY_LEN - 1);
    }
    cond.op = CARBON_HTN_OP_NOT_HAS;
    return cond;
}

/*============================================================================
 * Effect Helpers
 *============================================================================*/

Carbon_HTNEffect carbon_htn_effect_set_int(const char *key, int32_t value) {
    Carbon_HTNEffect effect = {0};
    if (key) {
        strncpy(effect.key, key, CARBON_HTN_MAX_KEY_LEN - 1);
    }
    effect.value.type = CARBON_HTN_TYPE_INT;
    effect.value.i32 = value;
    effect.is_increment = false;
    return effect;
}

Carbon_HTNEffect carbon_htn_effect_set_float(const char *key, float value) {
    Carbon_HTNEffect effect = {0};
    if (key) {
        strncpy(effect.key, key, CARBON_HTN_MAX_KEY_LEN - 1);
    }
    effect.value.type = CARBON_HTN_TYPE_FLOAT;
    effect.value.f32 = value;
    effect.is_increment = false;
    return effect;
}

Carbon_HTNEffect carbon_htn_effect_set_bool(const char *key, bool value) {
    Carbon_HTNEffect effect = {0};
    if (key) {
        strncpy(effect.key, key, CARBON_HTN_MAX_KEY_LEN - 1);
    }
    effect.value.type = CARBON_HTN_TYPE_BOOL;
    effect.value.b = value;
    effect.is_increment = false;
    return effect;
}

Carbon_HTNEffect carbon_htn_effect_inc_int(const char *key, int32_t amount) {
    Carbon_HTNEffect effect = {0};
    if (key) {
        strncpy(effect.key, key, CARBON_HTN_MAX_KEY_LEN - 1);
    }
    effect.value.type = CARBON_HTN_TYPE_INT;
    effect.value.i32 = amount;
    effect.is_increment = true;
    return effect;
}

Carbon_HTNEffect carbon_htn_effect_inc_float(const char *key, float amount) {
    Carbon_HTNEffect effect = {0};
    if (key) {
        strncpy(effect.key, key, CARBON_HTN_MAX_KEY_LEN - 1);
    }
    effect.value.type = CARBON_HTN_TYPE_FLOAT;
    effect.value.f32 = amount;
    effect.is_increment = true;
    return effect;
}

/*============================================================================
 * Condition Evaluation
 *============================================================================*/

bool carbon_htn_eval_condition(const Carbon_HTNWorldState *ws,
                                const Carbon_HTNCondition *cond) {
    if (!ws || !cond) return false;

    const Carbon_HTNValue *val = carbon_htn_ws_get_value(ws, cond->key);

    switch (cond->op) {
        case CARBON_HTN_OP_HAS:
            return val != NULL;

        case CARBON_HTN_OP_NOT_HAS:
            return val == NULL;

        case CARBON_HTN_OP_TRUE:
            return val && val->type == CARBON_HTN_TYPE_BOOL && val->b;

        case CARBON_HTN_OP_FALSE:
            return val && val->type == CARBON_HTN_TYPE_BOOL && !val->b;

        case CARBON_HTN_OP_EQ:
        case CARBON_HTN_OP_NE:
        case CARBON_HTN_OP_GT:
        case CARBON_HTN_OP_GE:
        case CARBON_HTN_OP_LT:
        case CARBON_HTN_OP_LE:
            if (!val) return false;

            /* Compare based on type */
            if (cond->value.type == CARBON_HTN_TYPE_INT && val->type == CARBON_HTN_TYPE_INT) {
                int32_t a = val->i32;
                int32_t b = cond->value.i32;
                switch (cond->op) {
                    case CARBON_HTN_OP_EQ: return a == b;
                    case CARBON_HTN_OP_NE: return a != b;
                    case CARBON_HTN_OP_GT: return a > b;
                    case CARBON_HTN_OP_GE: return a >= b;
                    case CARBON_HTN_OP_LT: return a < b;
                    case CARBON_HTN_OP_LE: return a <= b;
                    default: return false;
                }
            } else if (cond->value.type == CARBON_HTN_TYPE_FLOAT &&
                       val->type == CARBON_HTN_TYPE_FLOAT) {
                float a = val->f32;
                float b = cond->value.f32;
                switch (cond->op) {
                    case CARBON_HTN_OP_EQ: return a == b;
                    case CARBON_HTN_OP_NE: return a != b;
                    case CARBON_HTN_OP_GT: return a > b;
                    case CARBON_HTN_OP_GE: return a >= b;
                    case CARBON_HTN_OP_LT: return a < b;
                    case CARBON_HTN_OP_LE: return a <= b;
                    default: return false;
                }
            }
            return false;

        default:
            return false;
    }
}

bool carbon_htn_eval_conditions(const Carbon_HTNWorldState *ws,
                                 const Carbon_HTNCondition *conds, int count) {
    if (!ws || !conds) return true;

    for (int i = 0; i < count; i++) {
        if (!carbon_htn_eval_condition(ws, &conds[i])) {
            return false;
        }
    }
    return true;
}

/*============================================================================
 * Effect Application
 *============================================================================*/

void carbon_htn_apply_effect(Carbon_HTNWorldState *ws, const Carbon_HTNEffect *effect) {
    if (!ws || !effect) return;

    if (effect->is_increment) {
        if (effect->value.type == CARBON_HTN_TYPE_INT) {
            carbon_htn_ws_inc_int(ws, effect->key, effect->value.i32);
        } else if (effect->value.type == CARBON_HTN_TYPE_FLOAT) {
            carbon_htn_ws_inc_float(ws, effect->key, effect->value.f32);
        }
    } else {
        switch (effect->value.type) {
            case CARBON_HTN_TYPE_INT:
                carbon_htn_ws_set_int(ws, effect->key, effect->value.i32);
                break;
            case CARBON_HTN_TYPE_FLOAT:
                carbon_htn_ws_set_float(ws, effect->key, effect->value.f32);
                break;
            case CARBON_HTN_TYPE_BOOL:
                carbon_htn_ws_set_bool(ws, effect->key, effect->value.b);
                break;
            case CARBON_HTN_TYPE_PTR:
                carbon_htn_ws_set_ptr(ws, effect->key, effect->value.ptr);
                break;
            default:
                break;
        }
    }
}

void carbon_htn_apply_effects(Carbon_HTNWorldState *ws,
                               const Carbon_HTNEffect *effects, int count) {
    if (!ws || !effects) return;

    for (int i = 0; i < count; i++) {
        carbon_htn_apply_effect(ws, &effects[i]);
    }
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *carbon_htn_operator_name(Carbon_HTNOperator op) {
    switch (op) {
        case CARBON_HTN_OP_EQ:      return "==";
        case CARBON_HTN_OP_NE:      return "!=";
        case CARBON_HTN_OP_GT:      return ">";
        case CARBON_HTN_OP_GE:      return ">=";
        case CARBON_HTN_OP_LT:      return "<";
        case CARBON_HTN_OP_LE:      return "<=";
        case CARBON_HTN_OP_HAS:     return "has";
        case CARBON_HTN_OP_NOT_HAS: return "not_has";
        case CARBON_HTN_OP_TRUE:    return "true";
        case CARBON_HTN_OP_FALSE:   return "false";
        default:                    return "unknown";
    }
}

const char *carbon_htn_status_name(Carbon_HTNStatus status) {
    switch (status) {
        case CARBON_HTN_SUCCESS: return "Success";
        case CARBON_HTN_FAILED:  return "Failed";
        case CARBON_HTN_RUNNING: return "Running";
        case CARBON_HTN_INVALID: return "Invalid";
        default:                 return "Unknown";
    }
}

void carbon_htn_ws_debug_print(const Carbon_HTNWorldState *ws) {
    if (!ws) {
        printf("WorldState: NULL\n");
        return;
    }

    printf("WorldState (%d vars):\n", ws->count);
    for (int i = 0; i < CARBON_HTN_MAX_STATE_VARS; i++) {
        if (!ws->vars[i].used) continue;

        const WSVariable *var = &ws->vars[i];
        printf("  %s = ", var->key);

        switch (var->value.type) {
            case CARBON_HTN_TYPE_INT:
                printf("%d (int)\n", var->value.i32);
                break;
            case CARBON_HTN_TYPE_FLOAT:
                printf("%.2f (float)\n", var->value.f32);
                break;
            case CARBON_HTN_TYPE_BOOL:
                printf("%s (bool)\n", var->value.b ? "true" : "false");
                break;
            case CARBON_HTN_TYPE_PTR:
                printf("%p (ptr)\n", var->value.ptr);
                break;
            default:
                printf("(unknown type)\n");
                break;
        }
    }
}

void carbon_htn_plan_debug_print(const Carbon_HTNPlan *plan) {
    if (!plan) {
        printf("Plan: NULL\n");
        return;
    }

    printf("Plan (valid=%s, length=%d):\n",
           plan->valid ? "true" : "false", plan->length);

    for (int i = 0; i < plan->length; i++) {
        printf("  [%d] %s\n", i, plan->tasks[i]->name);
    }
}
