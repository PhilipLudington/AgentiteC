#ifndef CARBON_HTN_H
#define CARBON_HTN_H

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
 *   Carbon_HTNDomain *domain = carbon_htn_domain_create();
 *   Carbon_HTNWorldState *ws = carbon_htn_world_state_create();
 *
 *   // Set world state
 *   carbon_htn_ws_set_int(ws, "health", 100);
 *   carbon_htn_ws_set_bool(ws, "has_weapon", true);
 *   carbon_htn_ws_set_int(ws, "ammo", 10);
 *
 *   // Register primitive tasks
 *   carbon_htn_register_primitive(domain, "attack",
 *                                  execute_attack,
 *                                  precond_has_target,
 *                                  effect_damage_target);
 *
 *   // Register compound tasks with methods
 *   carbon_htn_register_compound(domain, "engage_enemy");
 *   const char *attack_seq[] = {"aim", "shoot"};
 *   carbon_htn_add_method(domain, "engage_enemy", precond_has_ammo,
 *                          attack_seq, 2);
 *   const char *melee_seq[] = {"approach", "melee_attack"};
 *   carbon_htn_add_method(domain, "engage_enemy", precond_no_ammo,
 *                          melee_seq, 2);
 *
 *   // Plan from root task
 *   Carbon_HTNPlan *plan = carbon_htn_plan(domain, ws, "engage_enemy", 1000);
 *   if (carbon_htn_plan_valid(plan)) {
 *       // Execute plan
 *       Carbon_HTNExecutor *exec = carbon_htn_executor_create(domain);
 *       carbon_htn_executor_set_plan(exec, plan);
 *       while (carbon_htn_executor_update(exec, ws, game_ctx) == CARBON_HTN_RUNNING) {
 *           // Wait for next tick
 *       }
 *   }
 *
 *   // Cleanup
 *   carbon_htn_plan_destroy(plan);
 *   carbon_htn_executor_destroy(exec);
 *   carbon_htn_world_state_destroy(ws);
 *   carbon_htn_domain_destroy(domain);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_HTN_MAX_TASKS        64    /* Maximum tasks in domain */
#define CARBON_HTN_MAX_METHODS      8     /* Maximum methods per compound task */
#define CARBON_HTN_MAX_SUBTASKS     8     /* Maximum subtasks per method */
#define CARBON_HTN_MAX_CONDITIONS   8     /* Maximum conditions per precondition */
#define CARBON_HTN_MAX_EFFECTS      8     /* Maximum effects per task */
#define CARBON_HTN_MAX_PLAN_LEN     32    /* Maximum plan length */
#define CARBON_HTN_MAX_STATE_VARS   64    /* Maximum world state variables */
#define CARBON_HTN_MAX_KEY_LEN      32    /* Maximum key length */
#define CARBON_HTN_MAX_STACK_DEPTH  32    /* Maximum decomposition stack */

/*============================================================================
 * Task Status
 *============================================================================*/

/**
 * Task execution status
 */
typedef enum Carbon_HTNStatus {
    CARBON_HTN_SUCCESS = 0,     /* Task/plan completed successfully */
    CARBON_HTN_FAILED,          /* Task/plan failed */
    CARBON_HTN_RUNNING,         /* Task/plan still executing */
    CARBON_HTN_INVALID,         /* Task/plan is invalid */
} Carbon_HTNStatus;

/*============================================================================
 * Condition Operators
 *============================================================================*/

/**
 * Operators for condition evaluation
 */
typedef enum Carbon_HTNOperator {
    CARBON_HTN_OP_EQ = 0,       /* == */
    CARBON_HTN_OP_NE,           /* != */
    CARBON_HTN_OP_GT,           /* > */
    CARBON_HTN_OP_GE,           /* >= */
    CARBON_HTN_OP_LT,           /* < */
    CARBON_HTN_OP_LE,           /* <= */
    CARBON_HTN_OP_HAS,          /* Key exists */
    CARBON_HTN_OP_NOT_HAS,      /* Key does not exist */
    CARBON_HTN_OP_TRUE,         /* Boolean is true */
    CARBON_HTN_OP_FALSE,        /* Boolean is false */
} Carbon_HTNOperator;

/*============================================================================
 * Value Types
 *============================================================================*/

/**
 * World state value types
 */
typedef enum Carbon_HTNValueType {
    CARBON_HTN_TYPE_NONE = 0,
    CARBON_HTN_TYPE_INT,
    CARBON_HTN_TYPE_FLOAT,
    CARBON_HTN_TYPE_BOOL,
    CARBON_HTN_TYPE_PTR,
} Carbon_HTNValueType;

/**
 * World state value
 */
typedef struct Carbon_HTNValue {
    Carbon_HTNValueType type;
    union {
        int32_t i32;
        float f32;
        bool b;
        void *ptr;
    };
} Carbon_HTNValue;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * A single condition in a precondition set
 */
typedef struct Carbon_HTNCondition {
    char key[CARBON_HTN_MAX_KEY_LEN];
    Carbon_HTNOperator op;
    Carbon_HTNValue value;
} Carbon_HTNCondition;

/**
 * A single effect applied to world state
 */
typedef struct Carbon_HTNEffect {
    char key[CARBON_HTN_MAX_KEY_LEN];
    Carbon_HTNValue value;
    bool is_increment;  /* If true, add to existing value */
} Carbon_HTNEffect;

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct Carbon_HTNWorldState Carbon_HTNWorldState;
typedef struct Carbon_HTNDomain Carbon_HTNDomain;
typedef struct Carbon_HTNPlan Carbon_HTNPlan;
typedef struct Carbon_HTNExecutor Carbon_HTNExecutor;
typedef struct Carbon_HTNTask Carbon_HTNTask;

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
typedef Carbon_HTNStatus (*Carbon_HTNExecuteFunc)(Carbon_HTNWorldState *ws,
                                                   void *userdata);

/**
 * Precondition check callback.
 * Can be used instead of declarative conditions.
 *
 * @param ws       World state (read-only)
 * @param userdata User context
 * @return true if preconditions are met
 */
typedef bool (*Carbon_HTNConditionFunc)(const Carbon_HTNWorldState *ws,
                                         void *userdata);

/**
 * Effect application callback.
 * Called when primitive task completes successfully.
 *
 * @param ws       World state to modify
 * @param userdata User context
 */
typedef void (*Carbon_HTNEffectFunc)(Carbon_HTNWorldState *ws, void *userdata);

/*============================================================================
 * World State
 *============================================================================*/

/**
 * Create a new world state.
 *
 * @return New world state or NULL on failure
 */
Carbon_HTNWorldState *carbon_htn_world_state_create(void);

/**
 * Destroy a world state.
 *
 * @param ws World state to destroy
 */
void carbon_htn_world_state_destroy(Carbon_HTNWorldState *ws);

/**
 * Clone a world state.
 *
 * @param ws World state to clone
 * @return New world state copy or NULL on failure
 */
Carbon_HTNWorldState *carbon_htn_world_state_clone(const Carbon_HTNWorldState *ws);

/**
 * Copy world state values.
 *
 * @param dest Destination
 * @param src  Source
 */
void carbon_htn_world_state_copy(Carbon_HTNWorldState *dest,
                                  const Carbon_HTNWorldState *src);

/**
 * Clear all world state values.
 *
 * @param ws World state
 */
void carbon_htn_world_state_clear(Carbon_HTNWorldState *ws);

/*============================================================================
 * World State - Value Access
 *============================================================================*/

/**
 * Set integer value.
 */
void carbon_htn_ws_set_int(Carbon_HTNWorldState *ws, const char *key, int32_t value);

/**
 * Set float value.
 */
void carbon_htn_ws_set_float(Carbon_HTNWorldState *ws, const char *key, float value);

/**
 * Set boolean value.
 */
void carbon_htn_ws_set_bool(Carbon_HTNWorldState *ws, const char *key, bool value);

/**
 * Set pointer value.
 */
void carbon_htn_ws_set_ptr(Carbon_HTNWorldState *ws, const char *key, void *value);

/**
 * Get integer value.
 */
int32_t carbon_htn_ws_get_int(const Carbon_HTNWorldState *ws, const char *key);

/**
 * Get float value.
 */
float carbon_htn_ws_get_float(const Carbon_HTNWorldState *ws, const char *key);

/**
 * Get boolean value.
 */
bool carbon_htn_ws_get_bool(const Carbon_HTNWorldState *ws, const char *key);

/**
 * Get pointer value.
 */
void *carbon_htn_ws_get_ptr(const Carbon_HTNWorldState *ws, const char *key);

/**
 * Check if key exists.
 */
bool carbon_htn_ws_has(const Carbon_HTNWorldState *ws, const char *key);

/**
 * Remove a key.
 */
void carbon_htn_ws_remove(Carbon_HTNWorldState *ws, const char *key);

/**
 * Get value as generic struct.
 */
const Carbon_HTNValue *carbon_htn_ws_get_value(const Carbon_HTNWorldState *ws,
                                                const char *key);

/**
 * Increment integer value.
 */
void carbon_htn_ws_inc_int(Carbon_HTNWorldState *ws, const char *key, int32_t amount);

/**
 * Increment float value.
 */
void carbon_htn_ws_inc_float(Carbon_HTNWorldState *ws, const char *key, float amount);

/*============================================================================
 * Domain
 *============================================================================*/

/**
 * Create a new HTN domain.
 *
 * @return New domain or NULL on failure
 */
Carbon_HTNDomain *carbon_htn_domain_create(void);

/**
 * Destroy a domain.
 *
 * @param domain Domain to destroy
 */
void carbon_htn_domain_destroy(Carbon_HTNDomain *domain);

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
int carbon_htn_register_primitive(Carbon_HTNDomain *domain, const char *name,
                                   Carbon_HTNExecuteFunc execute,
                                   Carbon_HTNConditionFunc precond,
                                   Carbon_HTNEffectFunc effect);

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
int carbon_htn_register_primitive_ex(Carbon_HTNDomain *domain, const char *name,
                                      Carbon_HTNExecuteFunc execute,
                                      const Carbon_HTNCondition *conditions,
                                      int cond_count,
                                      const Carbon_HTNEffect *effects,
                                      int effect_count);

/**
 * Register a compound task.
 * Compound tasks decompose into sequences of subtasks via methods.
 *
 * @param domain Domain
 * @param name   Task name (unique)
 * @return Task index or -1 on failure
 */
int carbon_htn_register_compound(Carbon_HTNDomain *domain, const char *name);

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
int carbon_htn_add_method(Carbon_HTNDomain *domain, const char *compound_name,
                           Carbon_HTNConditionFunc precond,
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
int carbon_htn_add_method_ex(Carbon_HTNDomain *domain, const char *compound_name,
                              const Carbon_HTNCondition *conditions, int cond_count,
                              const char **subtasks, int subtask_count);

/**
 * Find a task by name.
 *
 * @param domain Domain
 * @param name   Task name
 * @return Task pointer or NULL
 */
const Carbon_HTNTask *carbon_htn_find_task(const Carbon_HTNDomain *domain,
                                            const char *name);

/**
 * Get task count.
 */
int carbon_htn_task_count(const Carbon_HTNDomain *domain);

/**
 * Check if task is primitive.
 */
bool carbon_htn_task_is_primitive(const Carbon_HTNTask *task);

/**
 * Get task name.
 */
const char *carbon_htn_task_name(const Carbon_HTNTask *task);

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
 * @return Plan or NULL on failure (check carbon_get_last_error())
 */
Carbon_HTNPlan *carbon_htn_plan(Carbon_HTNDomain *domain,
                                 const Carbon_HTNWorldState *ws,
                                 const char *root_task,
                                 int max_iterations);

/**
 * Check if plan is valid.
 *
 * @param plan Plan to check
 * @return true if plan is valid and executable
 */
bool carbon_htn_plan_valid(const Carbon_HTNPlan *plan);

/**
 * Get plan length (number of primitive tasks).
 *
 * @param plan Plan
 * @return Number of tasks in plan
 */
int carbon_htn_plan_length(const Carbon_HTNPlan *plan);

/**
 * Get task at index in plan.
 *
 * @param plan  Plan
 * @param index Task index
 * @return Task pointer or NULL
 */
const Carbon_HTNTask *carbon_htn_plan_get_task(const Carbon_HTNPlan *plan, int index);

/**
 * Get task name at index in plan.
 *
 * @param plan  Plan
 * @param index Task index
 * @return Task name or NULL
 */
const char *carbon_htn_plan_get_task_name(const Carbon_HTNPlan *plan, int index);

/**
 * Destroy a plan.
 *
 * @param plan Plan to destroy
 */
void carbon_htn_plan_destroy(Carbon_HTNPlan *plan);

/*============================================================================
 * Execution
 *============================================================================*/

/**
 * Create a plan executor.
 *
 * @param domain Domain with task definitions
 * @return Executor or NULL on failure
 */
Carbon_HTNExecutor *carbon_htn_executor_create(Carbon_HTNDomain *domain);

/**
 * Destroy an executor.
 *
 * @param exec Executor to destroy
 */
void carbon_htn_executor_destroy(Carbon_HTNExecutor *exec);

/**
 * Set the plan to execute.
 *
 * @param exec Executor
 * @param plan Plan (ownership transferred)
 */
void carbon_htn_executor_set_plan(Carbon_HTNExecutor *exec, Carbon_HTNPlan *plan);

/**
 * Update execution (call each tick).
 *
 * @param exec     Executor
 * @param ws       World state (will be modified by effects)
 * @param userdata User context passed to task callbacks
 * @return Status: RUNNING, SUCCESS, or FAILED
 */
Carbon_HTNStatus carbon_htn_executor_update(Carbon_HTNExecutor *exec,
                                             Carbon_HTNWorldState *ws,
                                             void *userdata);

/**
 * Reset executor to start of plan.
 *
 * @param exec Executor
 */
void carbon_htn_executor_reset(Carbon_HTNExecutor *exec);

/**
 * Check if executor is running.
 *
 * @param exec Executor
 * @return true if currently executing a plan
 */
bool carbon_htn_executor_is_running(const Carbon_HTNExecutor *exec);

/**
 * Get current task index.
 *
 * @param exec Executor
 * @return Current task index or -1 if not running
 */
int carbon_htn_executor_get_current_index(const Carbon_HTNExecutor *exec);

/**
 * Get current task name.
 *
 * @param exec Executor
 * @return Current task name or NULL
 */
const char *carbon_htn_executor_get_current_task(const Carbon_HTNExecutor *exec);

/**
 * Get execution progress.
 *
 * @param exec Executor
 * @return Progress 0.0-1.0
 */
float carbon_htn_executor_get_progress(const Carbon_HTNExecutor *exec);

/**
 * Abort current execution.
 *
 * @param exec Executor
 */
void carbon_htn_executor_abort(Carbon_HTNExecutor *exec);

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
Carbon_HTNCondition carbon_htn_cond_int(const char *key, Carbon_HTNOperator op,
                                         int32_t value);

Carbon_HTNCondition carbon_htn_cond_float(const char *key, Carbon_HTNOperator op,
                                           float value);

Carbon_HTNCondition carbon_htn_cond_bool(const char *key, bool value);

Carbon_HTNCondition carbon_htn_cond_has(const char *key);

Carbon_HTNCondition carbon_htn_cond_not_has(const char *key);

/*============================================================================
 * Effect Helpers
 *============================================================================*/

/**
 * Create an effect to set a value.
 */
Carbon_HTNEffect carbon_htn_effect_set_int(const char *key, int32_t value);

Carbon_HTNEffect carbon_htn_effect_set_float(const char *key, float value);

Carbon_HTNEffect carbon_htn_effect_set_bool(const char *key, bool value);

/**
 * Create an effect to increment a value.
 */
Carbon_HTNEffect carbon_htn_effect_inc_int(const char *key, int32_t amount);

Carbon_HTNEffect carbon_htn_effect_inc_float(const char *key, float amount);

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
bool carbon_htn_eval_condition(const Carbon_HTNWorldState *ws,
                                const Carbon_HTNCondition *cond);

/**
 * Evaluate multiple conditions (AND).
 *
 * @param ws    World state
 * @param conds Condition array
 * @param count Number of conditions
 * @return true if all conditions are satisfied
 */
bool carbon_htn_eval_conditions(const Carbon_HTNWorldState *ws,
                                 const Carbon_HTNCondition *conds, int count);

/*============================================================================
 * Effect Application
 *============================================================================*/

/**
 * Apply an effect to world state.
 *
 * @param ws     World state to modify
 * @param effect Effect to apply
 */
void carbon_htn_apply_effect(Carbon_HTNWorldState *ws, const Carbon_HTNEffect *effect);

/**
 * Apply multiple effects.
 *
 * @param ws      World state to modify
 * @param effects Effect array
 * @param count   Number of effects
 */
void carbon_htn_apply_effects(Carbon_HTNWorldState *ws,
                               const Carbon_HTNEffect *effects, int count);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get operator name.
 *
 * @param op Operator
 * @return Static string name
 */
const char *carbon_htn_operator_name(Carbon_HTNOperator op);

/**
 * Get status name.
 *
 * @param status Status
 * @return Static string name
 */
const char *carbon_htn_status_name(Carbon_HTNStatus status);

/**
 * Debug: Print world state.
 *
 * @param ws World state
 */
void carbon_htn_ws_debug_print(const Carbon_HTNWorldState *ws);

/**
 * Debug: Print plan.
 *
 * @param plan Plan
 */
void carbon_htn_plan_debug_print(const Carbon_HTNPlan *plan);

#endif /* CARBON_HTN_H */
