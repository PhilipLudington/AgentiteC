/**
 * Carbon Hierarchical Task Network (HTN) AI Planner
 *
 * A sophisticated AI planning system that decomposes high-level goals
 * into executable primitive tasks.
 */

#include "agentite/agentite.h"
#include "agentite/htn.h"
#include "agentite/error.h"

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
    char key[AGENTITE_HTN_MAX_KEY_LEN];
    Agentite_HTNValue value;
    bool used;
} WSVariable;

/**
 * World state
 */
struct Agentite_HTNWorldState {
    WSVariable vars[AGENTITE_HTN_MAX_STATE_VARS];
    int count;
};

/**
 * Method for compound task decomposition
 */
typedef struct {
    /* Preconditions */
    Agentite_HTNConditionFunc precond_fn;
    Agentite_HTNCondition conditions[AGENTITE_HTN_MAX_CONDITIONS];
    int condition_count;

    /* Subtask sequence */
    char subtasks[AGENTITE_HTN_MAX_SUBTASKS][AGENTITE_HTN_MAX_KEY_LEN];
    int subtask_count;

    bool used;
} HTNMethod;

/**
 * Task definition
 */
struct Agentite_HTNTask {
    char name[AGENTITE_HTN_MAX_KEY_LEN];
    bool is_primitive;

    /* Primitive task data */
    Agentite_HTNExecuteFunc execute_fn;
    Agentite_HTNConditionFunc precond_fn;
    Agentite_HTNEffectFunc effect_fn;
    Agentite_HTNCondition conditions[AGENTITE_HTN_MAX_CONDITIONS];
    int condition_count;
    Agentite_HTNEffect effects[AGENTITE_HTN_MAX_EFFECTS];
    int effect_count;

    /* Compound task data */
    HTNMethod methods[AGENTITE_HTN_MAX_METHODS];
    int method_count;

    bool used;
};

/**
 * Domain
 */
struct Agentite_HTNDomain {
    Agentite_HTNTask tasks[AGENTITE_HTN_MAX_TASKS];
    int task_count;
};

/**
 * Plan
 */
struct Agentite_HTNPlan {
    Agentite_HTNTask *tasks[AGENTITE_HTN_MAX_PLAN_LEN];
    int length;
    bool valid;
};

/**
 * Executor
 */
struct Agentite_HTNExecutor {
    Agentite_HTNDomain *domain;
    Agentite_HTNPlan *plan;
    int current_index;
    Agentite_HTNStatus status;
    bool running;
};

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Find variable by key
 */
static WSVariable *find_var(Agentite_HTNWorldState *ws, const char *key) {
    for (int i = 0; i < AGENTITE_HTN_MAX_STATE_VARS; i++) {
        if (ws->vars[i].used && strcmp(ws->vars[i].key, key) == 0) {
            return &ws->vars[i];
        }
    }
    return NULL;
}

/**
 * Find or create variable
 */
static WSVariable *get_or_create_var(Agentite_HTNWorldState *ws, const char *key) {
    WSVariable *var = find_var(ws, key);
    if (var) return var;

    for (int i = 0; i < AGENTITE_HTN_MAX_STATE_VARS; i++) {
        if (!ws->vars[i].used) {
            var = &ws->vars[i];
            var->used = true;
            strncpy(var->key, key, AGENTITE_HTN_MAX_KEY_LEN - 1);
            var->key[AGENTITE_HTN_MAX_KEY_LEN - 1] = '\0';
            memset(&var->value, 0, sizeof(Agentite_HTNValue));
            ws->count++;
            return var;
        }
    }

    return NULL;
}

/**
 * Find task by name
 */
static Agentite_HTNTask *find_task_mut(Agentite_HTNDomain *domain, const char *name) {
    for (int i = 0; i < AGENTITE_HTN_MAX_TASKS; i++) {
        if (domain->tasks[i].used && strcmp(domain->tasks[i].name, name) == 0) {
            return &domain->tasks[i];
        }
    }
    return NULL;
}

/**
 * Allocate task slot
 */
static Agentite_HTNTask *alloc_task(Agentite_HTNDomain *domain) {
    for (int i = 0; i < AGENTITE_HTN_MAX_TASKS; i++) {
        if (!domain->tasks[i].used) {
            return &domain->tasks[i];
        }
    }
    return NULL;
}

/**
 * Check if preconditions are met
 */
static bool check_preconditions(const Agentite_HTNWorldState *ws,
                                 const Agentite_HTNTask *task) {
    /* Check callback first */
    if (task->precond_fn) {
        if (!task->precond_fn(ws, NULL)) {
            return false;
        }
    }

    /* Check declarative conditions */
    if (task->condition_count > 0) {
        if (!agentite_htn_eval_conditions(ws, task->conditions, task->condition_count)) {
            return false;
        }
    }

    return true;
}

/**
 * Check method preconditions
 */
static bool check_method_preconditions(const Agentite_HTNWorldState *ws,
                                        const HTNMethod *method) {
    /* Check callback first */
    if (method->precond_fn) {
        if (!method->precond_fn(ws, NULL)) {
            return false;
        }
    }

    /* Check declarative conditions */
    if (method->condition_count > 0) {
        if (!agentite_htn_eval_conditions(ws, method->conditions, method->condition_count)) {
            return false;
        }
    }

    return true;
}

/**
 * Apply task effects to world state
 */
static void apply_task_effects(Agentite_HTNWorldState *ws, const Agentite_HTNTask *task) {
    /* Apply callback effects */
    if (task->effect_fn) {
        task->effect_fn(ws, NULL);
    }

    /* Apply declarative effects */
    agentite_htn_apply_effects(ws, task->effects, task->effect_count);
}

/*============================================================================
 * World State - Creation/Destruction
 *============================================================================*/

Agentite_HTNWorldState *agentite_htn_world_state_create(void) {
    Agentite_HTNWorldState *ws = AGENTITE_ALLOC(Agentite_HTNWorldState);
    if (!ws) {
        agentite_set_error("agentite_htn_world_state_create: allocation failed");
        return NULL;
    }
    return ws;
}

void agentite_htn_world_state_destroy(Agentite_HTNWorldState *ws) {
    if (ws) {
        free(ws);
    }
}

Agentite_HTNWorldState *agentite_htn_world_state_clone(const Agentite_HTNWorldState *ws) {
    if (!ws) return NULL;

    Agentite_HTNWorldState *clone = agentite_htn_world_state_create();
    if (clone) {
        agentite_htn_world_state_copy(clone, ws);
    }
    return clone;
}

void agentite_htn_world_state_copy(Agentite_HTNWorldState *dest,
                                  const Agentite_HTNWorldState *src) {
    if (!dest || !src) return;
    memcpy(dest, src, sizeof(Agentite_HTNWorldState));
}

void agentite_htn_world_state_clear(Agentite_HTNWorldState *ws) {
    if (!ws) return;
    memset(ws->vars, 0, sizeof(ws->vars));
    ws->count = 0;
}

/*============================================================================
 * World State - Value Access
 *============================================================================*/

void agentite_htn_ws_set_int(Agentite_HTNWorldState *ws, const char *key, int32_t value) {
    if (!ws || !key) return;

    WSVariable *var = get_or_create_var(ws, key);
    if (var) {
        var->value.type = AGENTITE_HTN_TYPE_INT;
        var->value.i32 = value;
    }
}

void agentite_htn_ws_set_float(Agentite_HTNWorldState *ws, const char *key, float value) {
    if (!ws || !key) return;

    WSVariable *var = get_or_create_var(ws, key);
    if (var) {
        var->value.type = AGENTITE_HTN_TYPE_FLOAT;
        var->value.f32 = value;
    }
}

void agentite_htn_ws_set_bool(Agentite_HTNWorldState *ws, const char *key, bool value) {
    if (!ws || !key) return;

    WSVariable *var = get_or_create_var(ws, key);
    if (var) {
        var->value.type = AGENTITE_HTN_TYPE_BOOL;
        var->value.b = value;
    }
}

void agentite_htn_ws_set_ptr(Agentite_HTNWorldState *ws, const char *key, void *value) {
    if (!ws || !key) return;

    WSVariable *var = get_or_create_var(ws, key);
    if (var) {
        var->value.type = AGENTITE_HTN_TYPE_PTR;
        var->value.ptr = value;
    }
}

int32_t agentite_htn_ws_get_int(const Agentite_HTNWorldState *ws, const char *key) {
    if (!ws || !key) return 0;

    WSVariable *var = find_var((Agentite_HTNWorldState *)ws, key);
    if (var && var->value.type == AGENTITE_HTN_TYPE_INT) {
        return var->value.i32;
    }
    return 0;
}

float agentite_htn_ws_get_float(const Agentite_HTNWorldState *ws, const char *key) {
    if (!ws || !key) return 0.0f;

    WSVariable *var = find_var((Agentite_HTNWorldState *)ws, key);
    if (var && var->value.type == AGENTITE_HTN_TYPE_FLOAT) {
        return var->value.f32;
    }
    return 0.0f;
}

bool agentite_htn_ws_get_bool(const Agentite_HTNWorldState *ws, const char *key) {
    if (!ws || !key) return false;

    WSVariable *var = find_var((Agentite_HTNWorldState *)ws, key);
    if (var && var->value.type == AGENTITE_HTN_TYPE_BOOL) {
        return var->value.b;
    }
    return false;
}

void *agentite_htn_ws_get_ptr(const Agentite_HTNWorldState *ws, const char *key) {
    if (!ws || !key) return NULL;

    WSVariable *var = find_var((Agentite_HTNWorldState *)ws, key);
    if (var && var->value.type == AGENTITE_HTN_TYPE_PTR) {
        return var->value.ptr;
    }
    return NULL;
}

bool agentite_htn_ws_has(const Agentite_HTNWorldState *ws, const char *key) {
    if (!ws || !key) return false;
    return find_var((Agentite_HTNWorldState *)ws, key) != NULL;
}

void agentite_htn_ws_remove(Agentite_HTNWorldState *ws, const char *key) {
    if (!ws || !key) return;

    WSVariable *var = find_var(ws, key);
    if (var) {
        var->used = false;
        ws->count--;
    }
}

const Agentite_HTNValue *agentite_htn_ws_get_value(const Agentite_HTNWorldState *ws,
                                                const char *key) {
    if (!ws || !key) return NULL;

    WSVariable *var = find_var((Agentite_HTNWorldState *)ws, key);
    return var ? &var->value : NULL;
}

void agentite_htn_ws_inc_int(Agentite_HTNWorldState *ws, const char *key, int32_t amount) {
    if (!ws || !key) return;

    WSVariable *var = find_var(ws, key);
    if (var && var->value.type == AGENTITE_HTN_TYPE_INT) {
        var->value.i32 += amount;
    } else {
        agentite_htn_ws_set_int(ws, key, amount);
    }
}

void agentite_htn_ws_inc_float(Agentite_HTNWorldState *ws, const char *key, float amount) {
    if (!ws || !key) return;

    WSVariable *var = find_var(ws, key);
    if (var && var->value.type == AGENTITE_HTN_TYPE_FLOAT) {
        var->value.f32 += amount;
    } else {
        agentite_htn_ws_set_float(ws, key, amount);
    }
}

/*============================================================================
 * Domain - Creation/Destruction
 *============================================================================*/

Agentite_HTNDomain *agentite_htn_domain_create(void) {
    Agentite_HTNDomain *domain = AGENTITE_ALLOC(Agentite_HTNDomain);
    if (!domain) {
        agentite_set_error("agentite_htn_domain_create: allocation failed");
        return NULL;
    }
    return domain;
}

void agentite_htn_domain_destroy(Agentite_HTNDomain *domain) {
    if (domain) {
        free(domain);
    }
}

/*============================================================================
 * Domain - Task Registration
 *============================================================================*/

int agentite_htn_register_primitive(Agentite_HTNDomain *domain, const char *name,
                                   Agentite_HTNExecuteFunc execute,
                                   Agentite_HTNConditionFunc precond,
                                   Agentite_HTNEffectFunc effect) {
    if (!domain || !name || !execute) {
        agentite_set_error("agentite_htn_register_primitive: invalid parameters");
        return -1;
    }

    /* Check for duplicate */
    if (find_task_mut(domain, name)) {
        agentite_set_error("agentite_htn_register_primitive: task '%s' already exists", name);
        return -1;
    }

    Agentite_HTNTask *task = alloc_task(domain);
    if (!task) {
        agentite_set_error("HTN: Maximum tasks reached (%d/%d) when adding '%s'", domain->task_count, AGENTITE_HTN_MAX_TASKS, name);
        return -1;
    }

    memset(task, 0, sizeof(Agentite_HTNTask));
    strncpy(task->name, name, AGENTITE_HTN_MAX_KEY_LEN - 1);
    task->name[AGENTITE_HTN_MAX_KEY_LEN - 1] = '\0';
    task->is_primitive = true;
    task->execute_fn = execute;
    task->precond_fn = precond;
    task->effect_fn = effect;
    task->used = true;
    domain->task_count++;

    return domain->task_count - 1;
}

int agentite_htn_register_primitive_ex(Agentite_HTNDomain *domain, const char *name,
                                      Agentite_HTNExecuteFunc execute,
                                      const Agentite_HTNCondition *conditions,
                                      int cond_count,
                                      const Agentite_HTNEffect *effects,
                                      int effect_count) {
    if (!domain || !name || !execute) {
        agentite_set_error("agentite_htn_register_primitive_ex: invalid parameters");
        return -1;
    }

    int idx = agentite_htn_register_primitive(domain, name, execute, NULL, NULL);
    if (idx < 0) return idx;

    Agentite_HTNTask *task = find_task_mut(domain, name);
    if (!task) return -1;

    /* Copy conditions */
    if (conditions && cond_count > 0) {
        task->condition_count = (cond_count > AGENTITE_HTN_MAX_CONDITIONS) ?
                                 AGENTITE_HTN_MAX_CONDITIONS : cond_count;
        memcpy(task->conditions, conditions,
               task->condition_count * sizeof(Agentite_HTNCondition));
    }

    /* Copy effects */
    if (effects && effect_count > 0) {
        task->effect_count = (effect_count > AGENTITE_HTN_MAX_EFFECTS) ?
                              AGENTITE_HTN_MAX_EFFECTS : effect_count;
        memcpy(task->effects, effects,
               task->effect_count * sizeof(Agentite_HTNEffect));
    }

    return idx;
}

int agentite_htn_register_compound(Agentite_HTNDomain *domain, const char *name) {
    if (!domain || !name) {
        agentite_set_error("agentite_htn_register_compound: invalid parameters");
        return -1;
    }

    /* Check for duplicate */
    if (find_task_mut(domain, name)) {
        agentite_set_error("agentite_htn_register_compound: task '%s' already exists", name);
        return -1;
    }

    Agentite_HTNTask *task = alloc_task(domain);
    if (!task) {
        agentite_set_error("HTN: Maximum tasks reached (%d/%d) when adding '%s'", domain->task_count, AGENTITE_HTN_MAX_TASKS, name);
        return -1;
    }

    memset(task, 0, sizeof(Agentite_HTNTask));
    strncpy(task->name, name, AGENTITE_HTN_MAX_KEY_LEN - 1);
    task->name[AGENTITE_HTN_MAX_KEY_LEN - 1] = '\0';
    task->is_primitive = false;
    task->used = true;
    domain->task_count++;

    return domain->task_count - 1;
}

int agentite_htn_add_method(Agentite_HTNDomain *domain, const char *compound_name,
                           Agentite_HTNConditionFunc precond,
                           const char **subtasks, int subtask_count) {
    if (!domain || !compound_name || !subtasks || subtask_count <= 0) {
        agentite_set_error("agentite_htn_add_method: invalid parameters");
        return -1;
    }

    Agentite_HTNTask *task = find_task_mut(domain, compound_name);
    if (!task) {
        agentite_set_error("agentite_htn_add_method: compound task '%s' not found", compound_name);
        return -1;
    }

    if (task->is_primitive) {
        agentite_set_error("agentite_htn_add_method: task '%s' is not compound", compound_name);
        return -1;
    }

    if (task->method_count >= AGENTITE_HTN_MAX_METHODS) {
        agentite_set_error("HTN: Maximum methods reached (%d/%d) for compound task '%s'", task->method_count, AGENTITE_HTN_MAX_METHODS, compound_name);
        return -1;
    }

    HTNMethod *method = &task->methods[task->method_count];
    memset(method, 0, sizeof(HTNMethod));
    method->precond_fn = precond;
    method->used = true;

    /* Copy subtask names */
    method->subtask_count = (subtask_count > AGENTITE_HTN_MAX_SUBTASKS) ?
                             AGENTITE_HTN_MAX_SUBTASKS : subtask_count;
    for (int i = 0; i < method->subtask_count; i++) {
        strncpy(method->subtasks[i], subtasks[i], AGENTITE_HTN_MAX_KEY_LEN - 1);
        method->subtasks[i][AGENTITE_HTN_MAX_KEY_LEN - 1] = '\0';
    }

    return task->method_count++;
}

int agentite_htn_add_method_ex(Agentite_HTNDomain *domain, const char *compound_name,
                              const Agentite_HTNCondition *conditions, int cond_count,
                              const char **subtasks, int subtask_count) {
    int idx = agentite_htn_add_method(domain, compound_name, NULL, subtasks, subtask_count);
    if (idx < 0) return idx;

    Agentite_HTNTask *task = find_task_mut(domain, compound_name);
    if (!task) return -1;

    HTNMethod *method = &task->methods[idx];

    /* Copy conditions */
    if (conditions && cond_count > 0) {
        method->condition_count = (cond_count > AGENTITE_HTN_MAX_CONDITIONS) ?
                                   AGENTITE_HTN_MAX_CONDITIONS : cond_count;
        memcpy(method->conditions, conditions,
               method->condition_count * sizeof(Agentite_HTNCondition));
    }

    return idx;
}

const Agentite_HTNTask *agentite_htn_find_task(const Agentite_HTNDomain *domain,
                                            const char *name) {
    if (!domain || !name) return NULL;
    return find_task_mut((Agentite_HTNDomain *)domain, name);
}

int agentite_htn_task_count(const Agentite_HTNDomain *domain) {
    return domain ? domain->task_count : 0;
}

bool agentite_htn_task_is_primitive(const Agentite_HTNTask *task) {
    return task ? task->is_primitive : false;
}

const char *agentite_htn_task_name(const Agentite_HTNTask *task) {
    return task ? task->name : NULL;
}

/*============================================================================
 * Planning
 *============================================================================*/

Agentite_HTNPlan *agentite_htn_plan(Agentite_HTNDomain *domain,
                                 const Agentite_HTNWorldState *ws,
                                 const char *root_task,
                                 int max_iterations) {
    if (!domain || !ws || !root_task) {
        agentite_set_error("agentite_htn_plan: invalid parameters");
        return NULL;
    }

    if (max_iterations <= 0) {
        max_iterations = 1000;
    }

    /* Create plan */
    Agentite_HTNPlan *plan = AGENTITE_ALLOC(Agentite_HTNPlan);
    if (!plan) {
        agentite_set_error("agentite_htn_plan: allocation failed");
        return NULL;
    }

    /* Clone world state for simulation */
    Agentite_HTNWorldState *sim_ws = agentite_htn_world_state_clone(ws);
    if (!sim_ws) {
        free(plan);
        return NULL;
    }

    /* Task decomposition stack */
    const char *stack[AGENTITE_HTN_MAX_STACK_DEPTH];
    int stack_size = 0;

    /* Push root task */
    stack[stack_size++] = root_task;

    int iterations = 0;

    /* Process task stack */
    while (stack_size > 0 && iterations < max_iterations) {
        iterations++;

        /* Pop task from stack */
        const char *task_name = stack[--stack_size];
        Agentite_HTNTask *task = find_task_mut(domain, task_name);

        if (!task) {
            agentite_set_error("agentite_htn_plan: unknown task '%s'", task_name);
            goto fail;
        }

        if (task->is_primitive) {
            /* Check preconditions */
            if (!check_preconditions(sim_ws, task)) {
                agentite_set_error("agentite_htn_plan: preconditions failed for '%s'", task_name);
                goto fail;
            }

            /* Add to plan */
            if (plan->length >= AGENTITE_HTN_MAX_PLAN_LEN) {
                agentite_set_error("agentite_htn_plan: max plan length reached");
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
                    if (stack_size + method->subtask_count > AGENTITE_HTN_MAX_STACK_DEPTH) {
                        agentite_set_error("agentite_htn_plan: stack overflow");
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
                agentite_set_error("agentite_htn_plan: no applicable method for '%s'", task_name);
                goto fail;
            }
        }
    }

    if (iterations >= max_iterations) {
        agentite_set_error("agentite_htn_plan: max iterations reached");
        goto fail;
    }

    agentite_htn_world_state_destroy(sim_ws);
    plan->valid = true;
    return plan;

fail:
    agentite_htn_world_state_destroy(sim_ws);
    plan->valid = false;
    return plan;
}

bool agentite_htn_plan_valid(const Agentite_HTNPlan *plan) {
    return plan && plan->valid;
}

int agentite_htn_plan_length(const Agentite_HTNPlan *plan) {
    return plan ? plan->length : 0;
}

const Agentite_HTNTask *agentite_htn_plan_get_task(const Agentite_HTNPlan *plan, int index) {
    if (!plan || index < 0 || index >= plan->length) return NULL;
    return plan->tasks[index];
}

const char *agentite_htn_plan_get_task_name(const Agentite_HTNPlan *plan, int index) {
    const Agentite_HTNTask *task = agentite_htn_plan_get_task(plan, index);
    return task ? task->name : NULL;
}

void agentite_htn_plan_destroy(Agentite_HTNPlan *plan) {
    if (plan) {
        free(plan);
    }
}

/*============================================================================
 * Execution
 *============================================================================*/

Agentite_HTNExecutor *agentite_htn_executor_create(Agentite_HTNDomain *domain) {
    if (!domain) {
        agentite_set_error("agentite_htn_executor_create: null domain");
        return NULL;
    }

    Agentite_HTNExecutor *exec = AGENTITE_ALLOC(Agentite_HTNExecutor);
    if (!exec) {
        agentite_set_error("agentite_htn_executor_create: allocation failed");
        return NULL;
    }

    exec->domain = domain;
    exec->current_index = -1;
    exec->status = AGENTITE_HTN_INVALID;

    return exec;
}

void agentite_htn_executor_destroy(Agentite_HTNExecutor *exec) {
    if (exec) {
        if (exec->plan) {
            agentite_htn_plan_destroy(exec->plan);
        }
        free(exec);
    }
}

void agentite_htn_executor_set_plan(Agentite_HTNExecutor *exec, Agentite_HTNPlan *plan) {
    if (!exec) return;

    if (exec->plan) {
        agentite_htn_plan_destroy(exec->plan);
    }

    exec->plan = plan;
    exec->current_index = 0;
    exec->status = (plan && plan->valid && plan->length > 0) ?
                    AGENTITE_HTN_RUNNING : AGENTITE_HTN_INVALID;
    exec->running = (exec->status == AGENTITE_HTN_RUNNING);
}

Agentite_HTNStatus agentite_htn_executor_update(Agentite_HTNExecutor *exec,
                                             Agentite_HTNWorldState *ws,
                                             void *userdata) {
    if (!exec || !exec->plan || !exec->running) {
        return AGENTITE_HTN_INVALID;
    }

    if (exec->current_index >= exec->plan->length) {
        exec->status = AGENTITE_HTN_SUCCESS;
        exec->running = false;
        return AGENTITE_HTN_SUCCESS;
    }

    Agentite_HTNTask *task = exec->plan->tasks[exec->current_index];
    if (!task) {
        exec->status = AGENTITE_HTN_FAILED;
        exec->running = false;
        return AGENTITE_HTN_FAILED;
    }

    /* Execute current task */
    Agentite_HTNStatus task_status = AGENTITE_HTN_SUCCESS;
    if (task->execute_fn) {
        task_status = task->execute_fn(ws, userdata);
    }

    switch (task_status) {
        case AGENTITE_HTN_RUNNING:
            return AGENTITE_HTN_RUNNING;

        case AGENTITE_HTN_SUCCESS:
            /* Apply effects */
            apply_task_effects(ws, task);

            /* Move to next task */
            exec->current_index++;
            if (exec->current_index >= exec->plan->length) {
                exec->status = AGENTITE_HTN_SUCCESS;
                exec->running = false;
                return AGENTITE_HTN_SUCCESS;
            }
            return AGENTITE_HTN_RUNNING;

        case AGENTITE_HTN_FAILED:
        default:
            exec->status = AGENTITE_HTN_FAILED;
            exec->running = false;
            return AGENTITE_HTN_FAILED;
    }
}

void agentite_htn_executor_reset(Agentite_HTNExecutor *exec) {
    if (!exec) return;

    exec->current_index = 0;
    if (exec->plan && exec->plan->valid && exec->plan->length > 0) {
        exec->status = AGENTITE_HTN_RUNNING;
        exec->running = true;
    }
}

bool agentite_htn_executor_is_running(const Agentite_HTNExecutor *exec) {
    return exec ? exec->running : false;
}

int agentite_htn_executor_get_current_index(const Agentite_HTNExecutor *exec) {
    return (exec && exec->running) ? exec->current_index : -1;
}

const char *agentite_htn_executor_get_current_task(const Agentite_HTNExecutor *exec) {
    if (!exec || !exec->plan || !exec->running) return NULL;
    if (exec->current_index < 0 || exec->current_index >= exec->plan->length) return NULL;

    return exec->plan->tasks[exec->current_index]->name;
}

float agentite_htn_executor_get_progress(const Agentite_HTNExecutor *exec) {
    if (!exec || !exec->plan || exec->plan->length == 0) return 0.0f;
    return (float)exec->current_index / (float)exec->plan->length;
}

void agentite_htn_executor_abort(Agentite_HTNExecutor *exec) {
    if (exec) {
        exec->status = AGENTITE_HTN_FAILED;
        exec->running = false;
    }
}

/*============================================================================
 * Condition Helpers
 *============================================================================*/

Agentite_HTNCondition agentite_htn_cond_int(const char *key, Agentite_HTNOperator op,
                                         int32_t value) {
    Agentite_HTNCondition cond = {0};
    if (key) {
        strncpy(cond.key, key, AGENTITE_HTN_MAX_KEY_LEN - 1);
    }
    cond.op = op;
    cond.value.type = AGENTITE_HTN_TYPE_INT;
    cond.value.i32 = value;
    return cond;
}

Agentite_HTNCondition agentite_htn_cond_float(const char *key, Agentite_HTNOperator op,
                                           float value) {
    Agentite_HTNCondition cond = {0};
    if (key) {
        strncpy(cond.key, key, AGENTITE_HTN_MAX_KEY_LEN - 1);
    }
    cond.op = op;
    cond.value.type = AGENTITE_HTN_TYPE_FLOAT;
    cond.value.f32 = value;
    return cond;
}

Agentite_HTNCondition agentite_htn_cond_bool(const char *key, bool value) {
    Agentite_HTNCondition cond = {0};
    if (key) {
        strncpy(cond.key, key, AGENTITE_HTN_MAX_KEY_LEN - 1);
    }
    cond.op = value ? AGENTITE_HTN_OP_TRUE : AGENTITE_HTN_OP_FALSE;
    cond.value.type = AGENTITE_HTN_TYPE_BOOL;
    cond.value.b = value;
    return cond;
}

Agentite_HTNCondition agentite_htn_cond_has(const char *key) {
    Agentite_HTNCondition cond = {0};
    if (key) {
        strncpy(cond.key, key, AGENTITE_HTN_MAX_KEY_LEN - 1);
    }
    cond.op = AGENTITE_HTN_OP_HAS;
    return cond;
}

Agentite_HTNCondition agentite_htn_cond_not_has(const char *key) {
    Agentite_HTNCondition cond = {0};
    if (key) {
        strncpy(cond.key, key, AGENTITE_HTN_MAX_KEY_LEN - 1);
    }
    cond.op = AGENTITE_HTN_OP_NOT_HAS;
    return cond;
}

/*============================================================================
 * Effect Helpers
 *============================================================================*/

Agentite_HTNEffect agentite_htn_effect_set_int(const char *key, int32_t value) {
    Agentite_HTNEffect effect = {0};
    if (key) {
        strncpy(effect.key, key, AGENTITE_HTN_MAX_KEY_LEN - 1);
    }
    effect.value.type = AGENTITE_HTN_TYPE_INT;
    effect.value.i32 = value;
    effect.is_increment = false;
    return effect;
}

Agentite_HTNEffect agentite_htn_effect_set_float(const char *key, float value) {
    Agentite_HTNEffect effect = {0};
    if (key) {
        strncpy(effect.key, key, AGENTITE_HTN_MAX_KEY_LEN - 1);
    }
    effect.value.type = AGENTITE_HTN_TYPE_FLOAT;
    effect.value.f32 = value;
    effect.is_increment = false;
    return effect;
}

Agentite_HTNEffect agentite_htn_effect_set_bool(const char *key, bool value) {
    Agentite_HTNEffect effect = {0};
    if (key) {
        strncpy(effect.key, key, AGENTITE_HTN_MAX_KEY_LEN - 1);
    }
    effect.value.type = AGENTITE_HTN_TYPE_BOOL;
    effect.value.b = value;
    effect.is_increment = false;
    return effect;
}

Agentite_HTNEffect agentite_htn_effect_inc_int(const char *key, int32_t amount) {
    Agentite_HTNEffect effect = {0};
    if (key) {
        strncpy(effect.key, key, AGENTITE_HTN_MAX_KEY_LEN - 1);
    }
    effect.value.type = AGENTITE_HTN_TYPE_INT;
    effect.value.i32 = amount;
    effect.is_increment = true;
    return effect;
}

Agentite_HTNEffect agentite_htn_effect_inc_float(const char *key, float amount) {
    Agentite_HTNEffect effect = {0};
    if (key) {
        strncpy(effect.key, key, AGENTITE_HTN_MAX_KEY_LEN - 1);
    }
    effect.value.type = AGENTITE_HTN_TYPE_FLOAT;
    effect.value.f32 = amount;
    effect.is_increment = true;
    return effect;
}

/*============================================================================
 * Condition Evaluation
 *============================================================================*/

bool agentite_htn_eval_condition(const Agentite_HTNWorldState *ws,
                                const Agentite_HTNCondition *cond) {
    if (!ws || !cond) return false;

    const Agentite_HTNValue *val = agentite_htn_ws_get_value(ws, cond->key);

    switch (cond->op) {
        case AGENTITE_HTN_OP_HAS:
            return val != NULL;

        case AGENTITE_HTN_OP_NOT_HAS:
            return val == NULL;

        case AGENTITE_HTN_OP_TRUE:
            return val && val->type == AGENTITE_HTN_TYPE_BOOL && val->b;

        case AGENTITE_HTN_OP_FALSE:
            return val && val->type == AGENTITE_HTN_TYPE_BOOL && !val->b;

        case AGENTITE_HTN_OP_EQ:
        case AGENTITE_HTN_OP_NE:
        case AGENTITE_HTN_OP_GT:
        case AGENTITE_HTN_OP_GE:
        case AGENTITE_HTN_OP_LT:
        case AGENTITE_HTN_OP_LE:
            if (!val) return false;

            /* Compare based on type */
            if (cond->value.type == AGENTITE_HTN_TYPE_INT && val->type == AGENTITE_HTN_TYPE_INT) {
                int32_t a = val->i32;
                int32_t b = cond->value.i32;
                switch (cond->op) {
                    case AGENTITE_HTN_OP_EQ: return a == b;
                    case AGENTITE_HTN_OP_NE: return a != b;
                    case AGENTITE_HTN_OP_GT: return a > b;
                    case AGENTITE_HTN_OP_GE: return a >= b;
                    case AGENTITE_HTN_OP_LT: return a < b;
                    case AGENTITE_HTN_OP_LE: return a <= b;
                    default: return false;
                }
            } else if (cond->value.type == AGENTITE_HTN_TYPE_FLOAT &&
                       val->type == AGENTITE_HTN_TYPE_FLOAT) {
                float a = val->f32;
                float b = cond->value.f32;
                switch (cond->op) {
                    case AGENTITE_HTN_OP_EQ: return a == b;
                    case AGENTITE_HTN_OP_NE: return a != b;
                    case AGENTITE_HTN_OP_GT: return a > b;
                    case AGENTITE_HTN_OP_GE: return a >= b;
                    case AGENTITE_HTN_OP_LT: return a < b;
                    case AGENTITE_HTN_OP_LE: return a <= b;
                    default: return false;
                }
            }
            return false;

        default:
            return false;
    }
}

bool agentite_htn_eval_conditions(const Agentite_HTNWorldState *ws,
                                 const Agentite_HTNCondition *conds, int count) {
    if (!ws || !conds) return true;

    for (int i = 0; i < count; i++) {
        if (!agentite_htn_eval_condition(ws, &conds[i])) {
            return false;
        }
    }
    return true;
}

/*============================================================================
 * Effect Application
 *============================================================================*/

void agentite_htn_apply_effect(Agentite_HTNWorldState *ws, const Agentite_HTNEffect *effect) {
    if (!ws || !effect) return;

    if (effect->is_increment) {
        if (effect->value.type == AGENTITE_HTN_TYPE_INT) {
            agentite_htn_ws_inc_int(ws, effect->key, effect->value.i32);
        } else if (effect->value.type == AGENTITE_HTN_TYPE_FLOAT) {
            agentite_htn_ws_inc_float(ws, effect->key, effect->value.f32);
        }
    } else {
        switch (effect->value.type) {
            case AGENTITE_HTN_TYPE_INT:
                agentite_htn_ws_set_int(ws, effect->key, effect->value.i32);
                break;
            case AGENTITE_HTN_TYPE_FLOAT:
                agentite_htn_ws_set_float(ws, effect->key, effect->value.f32);
                break;
            case AGENTITE_HTN_TYPE_BOOL:
                agentite_htn_ws_set_bool(ws, effect->key, effect->value.b);
                break;
            case AGENTITE_HTN_TYPE_PTR:
                agentite_htn_ws_set_ptr(ws, effect->key, effect->value.ptr);
                break;
            default:
                break;
        }
    }
}

void agentite_htn_apply_effects(Agentite_HTNWorldState *ws,
                               const Agentite_HTNEffect *effects, int count) {
    if (!ws || !effects) return;

    for (int i = 0; i < count; i++) {
        agentite_htn_apply_effect(ws, &effects[i]);
    }
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *agentite_htn_operator_name(Agentite_HTNOperator op) {
    switch (op) {
        case AGENTITE_HTN_OP_EQ:      return "==";
        case AGENTITE_HTN_OP_NE:      return "!=";
        case AGENTITE_HTN_OP_GT:      return ">";
        case AGENTITE_HTN_OP_GE:      return ">=";
        case AGENTITE_HTN_OP_LT:      return "<";
        case AGENTITE_HTN_OP_LE:      return "<=";
        case AGENTITE_HTN_OP_HAS:     return "has";
        case AGENTITE_HTN_OP_NOT_HAS: return "not_has";
        case AGENTITE_HTN_OP_TRUE:    return "true";
        case AGENTITE_HTN_OP_FALSE:   return "false";
        default:                    return "unknown";
    }
}

const char *agentite_htn_status_name(Agentite_HTNStatus status) {
    switch (status) {
        case AGENTITE_HTN_SUCCESS: return "Success";
        case AGENTITE_HTN_FAILED:  return "Failed";
        case AGENTITE_HTN_RUNNING: return "Running";
        case AGENTITE_HTN_INVALID: return "Invalid";
        default:                 return "Unknown";
    }
}

void agentite_htn_ws_debug_print(const Agentite_HTNWorldState *ws) {
    if (!ws) {
        printf("WorldState: NULL\n");
        return;
    }

    printf("WorldState (%d vars):\n", ws->count);
    for (int i = 0; i < AGENTITE_HTN_MAX_STATE_VARS; i++) {
        if (!ws->vars[i].used) continue;

        const WSVariable *var = &ws->vars[i];
        printf("  %s = ", var->key);

        switch (var->value.type) {
            case AGENTITE_HTN_TYPE_INT:
                printf("%d (int)\n", var->value.i32);
                break;
            case AGENTITE_HTN_TYPE_FLOAT:
                printf("%.2f (float)\n", var->value.f32);
                break;
            case AGENTITE_HTN_TYPE_BOOL:
                printf("%s (bool)\n", var->value.b ? "true" : "false");
                break;
            case AGENTITE_HTN_TYPE_PTR:
                printf("%p (ptr)\n", var->value.ptr);
                break;
            default:
                printf("(unknown type)\n");
                break;
        }
    }
}

void agentite_htn_plan_debug_print(const Agentite_HTNPlan *plan) {
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
