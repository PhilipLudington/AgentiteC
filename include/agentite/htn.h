#ifndef AGENTITE_HTN_H
#define AGENTITE_HTN_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Carbon Hierarchical Task Network (HTN) AI Planner
 *
 * A sophisticated AI planning system that decomposes high-level goals
 * into executable primitive tasks. Significantly more powerful than
 * simple task queues for autonomous AI agents.
 *
 * Usage:
 *   // Create domain and world state
 *   Agentite_HTNDomain *domain = agentite_htn_domain_create();
 *   Agentite_HTNWorldState *ws = agentite_htn_world_state_create();
 *
 *   // Set world state
 *   agentite_htn_ws_set_int(ws, "health", 100);
 *   agentite_htn_ws_set_bool(ws, "has_weapon", true);
 *   agentite_htn_ws_set_int(ws, "ammo", 10);
 *
 *   // Register primitive tasks
 *   agentite_htn_register_primitive(domain, "attack",
 *                                  execute_attack,
 *                                  precond_has_target,
 *                                  effect_damage_target);
 *
 *   // Register compound tasks with methods
 *   agentite_htn_register_compound(domain, "engage_enemy");
 *   const char *attack_seq[] = {"aim", "shoot"};
 *   agentite_htn_add_method(domain, "engage_enemy", precond_has_ammo,
 *                          attack_seq, 2);
 *   const char *melee_seq[] = {"approach", "melee_attack"};
 *   agentite_htn_add_method(domain, "engage_enemy", precond_no_ammo,
 *                          melee_seq, 2);
 *
 *   // Plan from root task
 *   Agentite_HTNPlan *plan = agentite_htn_plan(domain, ws, "engage_enemy", 1000);
 *   if (agentite_htn_plan_valid(plan)) {
 *       // Execute plan
 *       Agentite_HTNExecutor *exec = agentite_htn_executor_create(domain);
 *       agentite_htn_executor_set_plan(exec, plan);
 *       while (agentite_htn_executor_update(exec, ws, game_ctx) == AGENTITE_HTN_RUNNING) {
 *           // Wait for next tick
 *       }
 *   }
 *
 *   // Cleanup
 *   agentite_htn_plan_destroy(plan);
 *   agentite_htn_executor_destroy(exec);
 *   agentite_htn_world_state_destroy(ws);
 *   agentite_htn_domain_destroy(domain);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_HTN_MAX_TASKS        64    /* Maximum tasks in domain */
#define AGENTITE_HTN_MAX_METHODS      8     /* Maximum methods per compound task */
#define AGENTITE_HTN_MAX_SUBTASKS     8     /* Maximum subtasks per method */
#define AGENTITE_HTN_MAX_CONDITIONS   8     /* Maximum conditions per precondition */
#define AGENTITE_HTN_MAX_EFFECTS      8     /* Maximum effects per task */
#define AGENTITE_HTN_MAX_PLAN_LEN     32    /* Maximum plan length */
#define AGENTITE_HTN_MAX_STATE_VARS   64    /* Maximum world state variables */
#define AGENTITE_HTN_MAX_KEY_LEN      32    /* Maximum key length */
#define AGENTITE_HTN_MAX_STACK_DEPTH  32    /* Maximum decomposition stack */

/*============================================================================
 * Task Status
 *============================================================================*/

/**
 * Task execution status
 */
typedef enum Agentite_HTNStatus {
    AGENTITE_HTN_SUCCESS = 0,     /* Task/plan completed successfully */
    AGENTITE_HTN_FAILED,          /* Task/plan failed */
    AGENTITE_HTN_RUNNING,         /* Task/plan still executing */
    AGENTITE_HTN_INVALID,         /* Task/plan is invalid */
} Agentite_HTNStatus;

/*============================================================================
 * Condition Operators
 *============================================================================*/

/**
 * Operators for condition evaluation
 */
typedef enum Agentite_HTNOperator {
    AGENTITE_HTN_OP_EQ = 0,       /* == */
    AGENTITE_HTN_OP_NE,           /* != */
    AGENTITE_HTN_OP_GT,           /* > */
    AGENTITE_HTN_OP_GE,           /* >= */
    AGENTITE_HTN_OP_LT,           /* < */
    AGENTITE_HTN_OP_LE,           /* <= */
    AGENTITE_HTN_OP_HAS,          /* Key exists */
    AGENTITE_HTN_OP_NOT_HAS,      /* Key does not exist */
    AGENTITE_HTN_OP_TRUE,         /* Boolean is true */
    AGENTITE_HTN_OP_FALSE,        /* Boolean is false */
} Agentite_HTNOperator;

/*============================================================================
 * Value Types
 *============================================================================*/

/**
 * World state value types
 */
typedef enum Agentite_HTNValueType {
    AGENTITE_HTN_TYPE_NONE = 0,
    AGENTITE_HTN_TYPE_INT,
    AGENTITE_HTN_TYPE_FLOAT,
    AGENTITE_HTN_TYPE_BOOL,
    AGENTITE_HTN_TYPE_PTR,
} Agentite_HTNValueType;

/**
 * World state value
 */
typedef struct Agentite_HTNValue {
    Agentite_HTNValueType type;
    union {
        int32_t i32;
        float f32;
        bool b;
        void *ptr;
    };
} Agentite_HTNValue;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * A single condition in a precondition set
 */
typedef struct Agentite_HTNCondition {
    char key[AGENTITE_HTN_MAX_KEY_LEN];
    Agentite_HTNOperator op;
    Agentite_HTNValue value;
} Agentite_HTNCondition;

/**
 * A single effect applied to world state
 */
typedef struct Agentite_HTNEffect {
    char key[AGENTITE_HTN_MAX_KEY_LEN];
    Agentite_HTNValue value;
    bool is_increment;  /* If true, add to existing value */
} Agentite_HTNEffect;

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct Agentite_HTNWorldState Agentite_HTNWorldState;
typedef struct Agentite_HTNDomain Agentite_HTNDomain;
typedef struct Agentite_HTNPlan Agentite_HTNPlan;
typedef struct Agentite_HTNExecutor Agentite_HTNExecutor;
typedef struct Agentite_HTNTask Agentite_HTNTask;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Primitive task execution callback.
 * Called each frame while task is running.
 *
 * @param ws       World state (can be modified)
 * @param userdata User context
 * @return Status: RUNNING to continue, SUCCESS/FAILED to complete
 */
typedef Agentite_HTNStatus (*Agentite_HTNExecuteFunc)(Agentite_HTNWorldState *ws,
                                                   void *userdata);

/**
 * Precondition check callback.
 * Can be used instead of declarative conditions.
 *
 * @param ws       World state (read-only)
 * @param userdata User context
 * @return true if preconditions are met
 */
typedef bool (*Agentite_HTNConditionFunc)(const Agentite_HTNWorldState *ws,
                                         void *userdata);

/**
 * Effect application callback.
 * Called when primitive task completes successfully.
 *
 * @param ws       World state to modify
 * @param userdata User context
 */
typedef void (*Agentite_HTNEffectFunc)(Agentite_HTNWorldState *ws, void *userdata);

/*============================================================================
 * World State
 *============================================================================*/

/**
 * Create a new world state.
 *
 * @return New world state or NULL on failure
 */
Agentite_HTNWorldState *agentite_htn_world_state_create(void);

/**
 * Destroy a world state.
 *
 * @param ws World state to destroy
 */
void agentite_htn_world_state_destroy(Agentite_HTNWorldState *ws);

/**
 * Clone a world state.
 *
 * @param ws World state to clone
 * @return New world state copy or NULL on failure
 */
Agentite_HTNWorldState *agentite_htn_world_state_clone(const Agentite_HTNWorldState *ws);

/**
 * Copy world state values.
 *
 * @param dest Destination
 * @param src  Source
 */
void agentite_htn_world_state_copy(Agentite_HTNWorldState *dest,
                                  const Agentite_HTNWorldState *src);

/**
 * Clear all world state values.
 *
 * @param ws World state
 */
void agentite_htn_world_state_clear(Agentite_HTNWorldState *ws);

/*============================================================================
 * World State - Value Access
 *============================================================================*/

/**
 * Set integer value.
 */
void agentite_htn_ws_set_int(Agentite_HTNWorldState *ws, const char *key, int32_t value);

/**
 * Set float value.
 */
void agentite_htn_ws_set_float(Agentite_HTNWorldState *ws, const char *key, float value);

/**
 * Set boolean value.
 */
void agentite_htn_ws_set_bool(Agentite_HTNWorldState *ws, const char *key, bool value);

/**
 * Set pointer value.
 */
void agentite_htn_ws_set_ptr(Agentite_HTNWorldState *ws, const char *key, void *value);

/**
 * Get integer value.
 */
int32_t agentite_htn_ws_get_int(const Agentite_HTNWorldState *ws, const char *key);

/**
 * Get float value.
 */
float agentite_htn_ws_get_float(const Agentite_HTNWorldState *ws, const char *key);

/**
 * Get boolean value.
 */
bool agentite_htn_ws_get_bool(const Agentite_HTNWorldState *ws, const char *key);

/**
 * Get pointer value.
 */
void *agentite_htn_ws_get_ptr(const Agentite_HTNWorldState *ws, const char *key);

/**
 * Check if key exists.
 */
bool agentite_htn_ws_has(const Agentite_HTNWorldState *ws, const char *key);

/**
 * Remove a key.
 */
void agentite_htn_ws_remove(Agentite_HTNWorldState *ws, const char *key);

/**
 * Get value as generic struct.
 */
const Agentite_HTNValue *agentite_htn_ws_get_value(const Agentite_HTNWorldState *ws,
                                                const char *key);

/**
 * Increment integer value.
 */
void agentite_htn_ws_inc_int(Agentite_HTNWorldState *ws, const char *key, int32_t amount);

/**
 * Increment float value.
 */
void agentite_htn_ws_inc_float(Agentite_HTNWorldState *ws, const char *key, float amount);

/*============================================================================
 * Domain
 *============================================================================*/

/**
 * Create a new HTN domain.
 *
 * @return New domain or NULL on failure
 */
Agentite_HTNDomain *agentite_htn_domain_create(void);

/**
 * Destroy a domain.
 *
 * @param domain Domain to destroy
 */
void agentite_htn_domain_destroy(Agentite_HTNDomain *domain);

/**
 * Register a primitive task.
 * Primitive tasks are the leaf actions that actually do things.
 *
 * @param domain    Domain
 * @param name      Task name (unique)
 * @param execute   Execution callback
 * @param precond   Precondition callback (or NULL)
 * @param effect    Effect callback (or NULL)
 * @return Task index or -1 on failure
 */
int agentite_htn_register_primitive(Agentite_HTNDomain *domain, const char *name,
                                   Agentite_HTNExecuteFunc execute,
                                   Agentite_HTNConditionFunc precond,
                                   Agentite_HTNEffectFunc effect);

/**
 * Register a primitive task with declarative conditions/effects.
 *
 * @param domain     Domain
 * @param name       Task name
 * @param execute    Execution callback
 * @param conditions Condition array (copied)
 * @param cond_count Number of conditions
 * @param effects    Effect array (copied)
 * @param effect_count Number of effects
 * @return Task index or -1 on failure
 */
int agentite_htn_register_primitive_ex(Agentite_HTNDomain *domain, const char *name,
                                      Agentite_HTNExecuteFunc execute,
                                      const Agentite_HTNCondition *conditions,
                                      int cond_count,
                                      const Agentite_HTNEffect *effects,
                                      int effect_count);

/**
 * Register a compound task.
 * Compound tasks decompose into sequences of subtasks via methods.
 *
 * @param domain Domain
 * @param name   Task name (unique)
 * @return Task index or -1 on failure
 */
int agentite_htn_register_compound(Agentite_HTNDomain *domain, const char *name);

/**
 * Add a method to a compound task.
 * Methods are tried in order; first one with satisfied preconditions is used.
 *
 * @param domain        Domain
 * @param compound_name Compound task name
 * @param precond       Precondition callback (or NULL for always valid)
 * @param subtasks      Array of subtask names
 * @param subtask_count Number of subtasks
 * @return Method index or -1 on failure
 */
int agentite_htn_add_method(Agentite_HTNDomain *domain, const char *compound_name,
                           Agentite_HTNConditionFunc precond,
                           const char **subtasks, int subtask_count);

/**
 * Add a method with declarative conditions.
 *
 * @param domain        Domain
 * @param compound_name Compound task name
 * @param conditions    Condition array (copied)
 * @param cond_count    Number of conditions
 * @param subtasks      Array of subtask names
 * @param subtask_count Number of subtasks
 * @return Method index or -1 on failure
 */
int agentite_htn_add_method_ex(Agentite_HTNDomain *domain, const char *compound_name,
                              const Agentite_HTNCondition *conditions, int cond_count,
                              const char **subtasks, int subtask_count);

/**
 * Find a task by name.
 *
 * @param domain Domain
 * @param name   Task name
 * @return Task pointer or NULL
 */
const Agentite_HTNTask *agentite_htn_find_task(const Agentite_HTNDomain *domain,
                                            const char *name);

/**
 * Get task count.
 */
int agentite_htn_task_count(const Agentite_HTNDomain *domain);

/**
 * Check if task is primitive.
 */
bool agentite_htn_task_is_primitive(const Agentite_HTNTask *task);

/**
 * Get task name.
 */
const char *agentite_htn_task_name(const Agentite_HTNTask *task);

/*============================================================================
 * Planning
 *============================================================================*/

/**
 * Generate a plan from a root task.
 *
 * @param domain         Domain with task definitions
 * @param ws             Current world state
 * @param root_task      Name of root task to plan from
 * @param max_iterations Maximum planning iterations (0 = default 1000)
 * @return Plan or NULL on failure (check agentite_get_last_error())
 */
Agentite_HTNPlan *agentite_htn_plan(Agentite_HTNDomain *domain,
                                 const Agentite_HTNWorldState *ws,
                                 const char *root_task,
                                 int max_iterations);

/**
 * Check if plan is valid.
 *
 * @param plan Plan to check
 * @return true if plan is valid and executable
 */
bool agentite_htn_plan_valid(const Agentite_HTNPlan *plan);

/**
 * Get plan length (number of primitive tasks).
 *
 * @param plan Plan
 * @return Number of tasks in plan
 */
int agentite_htn_plan_length(const Agentite_HTNPlan *plan);

/**
 * Get task at index in plan.
 *
 * @param plan  Plan
 * @param index Task index
 * @return Task pointer or NULL
 */
const Agentite_HTNTask *agentite_htn_plan_get_task(const Agentite_HTNPlan *plan, int index);

/**
 * Get task name at index in plan.
 *
 * @param plan  Plan
 * @param index Task index
 * @return Task name or NULL
 */
const char *agentite_htn_plan_get_task_name(const Agentite_HTNPlan *plan, int index);

/**
 * Destroy a plan.
 *
 * @param plan Plan to destroy
 */
void agentite_htn_plan_destroy(Agentite_HTNPlan *plan);

/*============================================================================
 * Execution
 *============================================================================*/

/**
 * Create a plan executor.
 *
 * @param domain Domain with task definitions
 * @return Executor or NULL on failure
 */
Agentite_HTNExecutor *agentite_htn_executor_create(Agentite_HTNDomain *domain);

/**
 * Destroy an executor.
 *
 * @param exec Executor to destroy
 */
void agentite_htn_executor_destroy(Agentite_HTNExecutor *exec);

/**
 * Set the plan to execute.
 *
 * @param exec Executor
 * @param plan Plan (ownership transferred)
 */
void agentite_htn_executor_set_plan(Agentite_HTNExecutor *exec, Agentite_HTNPlan *plan);

/**
 * Update execution (call each tick).
 *
 * @param exec     Executor
 * @param ws       World state (will be modified by effects)
 * @param userdata User context passed to task callbacks
 * @return Status: RUNNING, SUCCESS, or FAILED
 */
Agentite_HTNStatus agentite_htn_executor_update(Agentite_HTNExecutor *exec,
                                             Agentite_HTNWorldState *ws,
                                             void *userdata);

/**
 * Reset executor to start of plan.
 *
 * @param exec Executor
 */
void agentite_htn_executor_reset(Agentite_HTNExecutor *exec);

/**
 * Check if executor is running.
 *
 * @param exec Executor
 * @return true if currently executing a plan
 */
bool agentite_htn_executor_is_running(const Agentite_HTNExecutor *exec);

/**
 * Get current task index.
 *
 * @param exec Executor
 * @return Current task index or -1 if not running
 */
int agentite_htn_executor_get_current_index(const Agentite_HTNExecutor *exec);

/**
 * Get current task name.
 *
 * @param exec Executor
 * @return Current task name or NULL
 */
const char *agentite_htn_executor_get_current_task(const Agentite_HTNExecutor *exec);

/**
 * Get execution progress.
 *
 * @param exec Executor
 * @return Progress 0.0-1.0
 */
float agentite_htn_executor_get_progress(const Agentite_HTNExecutor *exec);

/**
 * Abort current execution.
 *
 * @param exec Executor
 */
void agentite_htn_executor_abort(Agentite_HTNExecutor *exec);

/*============================================================================
 * Condition Helpers
 *============================================================================*/

/**
 * Create a condition for value comparison.
 *
 * @param key   World state key
 * @param op    Comparison operator
 * @param value Value to compare against
 * @return Condition struct
 */
Agentite_HTNCondition agentite_htn_cond_int(const char *key, Agentite_HTNOperator op,
                                         int32_t value);

Agentite_HTNCondition agentite_htn_cond_float(const char *key, Agentite_HTNOperator op,
                                           float value);

Agentite_HTNCondition agentite_htn_cond_bool(const char *key, bool value);

Agentite_HTNCondition agentite_htn_cond_has(const char *key);

Agentite_HTNCondition agentite_htn_cond_not_has(const char *key);

/*============================================================================
 * Effect Helpers
 *============================================================================*/

/**
 * Create an effect to set a value.
 */
Agentite_HTNEffect agentite_htn_effect_set_int(const char *key, int32_t value);

Agentite_HTNEffect agentite_htn_effect_set_float(const char *key, float value);

Agentite_HTNEffect agentite_htn_effect_set_bool(const char *key, bool value);

/**
 * Create an effect to increment a value.
 */
Agentite_HTNEffect agentite_htn_effect_inc_int(const char *key, int32_t amount);

Agentite_HTNEffect agentite_htn_effect_inc_float(const char *key, float amount);

/*============================================================================
 * Condition Evaluation
 *============================================================================*/

/**
 * Evaluate a single condition against world state.
 *
 * @param ws   World state
 * @param cond Condition to evaluate
 * @return true if condition is satisfied
 */
bool agentite_htn_eval_condition(const Agentite_HTNWorldState *ws,
                                const Agentite_HTNCondition *cond);

/**
 * Evaluate multiple conditions (AND).
 *
 * @param ws    World state
 * @param conds Condition array
 * @param count Number of conditions
 * @return true if all conditions are satisfied
 */
bool agentite_htn_eval_conditions(const Agentite_HTNWorldState *ws,
                                 const Agentite_HTNCondition *conds, int count);

/*============================================================================
 * Effect Application
 *============================================================================*/

/**
 * Apply an effect to world state.
 *
 * @param ws     World state to modify
 * @param effect Effect to apply
 */
void agentite_htn_apply_effect(Agentite_HTNWorldState *ws, const Agentite_HTNEffect *effect);

/**
 * Apply multiple effects.
 *
 * @param ws      World state to modify
 * @param effects Effect array
 * @param count   Number of effects
 */
void agentite_htn_apply_effects(Agentite_HTNWorldState *ws,
                               const Agentite_HTNEffect *effects, int count);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get operator name.
 *
 * @param op Operator
 * @return Static string name
 */
const char *agentite_htn_operator_name(Agentite_HTNOperator op);

/**
 * Get status name.
 *
 * @param status Status
 * @return Static string name
 */
const char *agentite_htn_status_name(Agentite_HTNStatus status);

/**
 * Debug: Print world state.
 *
 * @param ws World state
 */
void agentite_htn_ws_debug_print(const Agentite_HTNWorldState *ws);

/**
 * Debug: Print plan.
 *
 * @param plan Plan
 */
void agentite_htn_plan_debug_print(const Agentite_HTNPlan *plan);

#endif /* AGENTITE_HTN_H */
