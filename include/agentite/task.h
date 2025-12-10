/**
 * Carbon Task Queue System
 *
 * Sequential task execution for autonomous AI agents. Provides a queue
 * of tasks with lifecycle management, pathfinding integration, and
 * completion callbacks.
 *
 * Usage:
 *   // Create task queue for an agent
 *   Agentite_TaskQueue *queue = agentite_task_queue_create(16);
 *
 *   // Queue tasks
 *   agentite_task_queue_add_move(queue, target_x, target_y);
 *   agentite_task_queue_add_collect(queue, item_x, item_y, RESOURCE_WOOD);
 *   agentite_task_queue_add_wait(queue, 2.0f);
 *
 *   // In game loop:
 *   Agentite_Task *current = agentite_task_queue_current(queue);
 *   if (current && current->status == AGENTITE_TASK_IN_PROGRESS) {
 *       switch (current->type) {
 *           case AGENTITE_TASK_MOVE:
 *               // Move agent toward target...
 *               if (at_destination) {
 *                   agentite_task_queue_complete(queue);
 *               }
 *               break;
 *           // ... handle other task types
 *       }
 *   }
 *
 *   // Cleanup
 *   agentite_task_queue_destroy(queue);
 */

#ifndef AGENTITE_TASK_H
#define AGENTITE_TASK_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_TASK_MAX_DATA    64   /* Maximum bytes for task-specific data */
#define AGENTITE_TASK_MAX_REASON  64   /* Maximum length of failure reason */

/*============================================================================
 * Task Types
 *============================================================================*/

/**
 * Built-in task types for common agent actions.
 */
typedef enum Agentite_TaskType {
    AGENTITE_TASK_NONE = 0,
    AGENTITE_TASK_MOVE,           /* Move to target position */
    AGENTITE_TASK_EXPLORE,        /* Explore area around position */
    AGENTITE_TASK_COLLECT,        /* Collect resource at position */
    AGENTITE_TASK_DEPOSIT,        /* Deposit carried items */
    AGENTITE_TASK_CRAFT,          /* Craft item using recipe */
    AGENTITE_TASK_BUILD,          /* Construct building */
    AGENTITE_TASK_ATTACK,         /* Attack target entity */
    AGENTITE_TASK_DEFEND,         /* Defend position */
    AGENTITE_TASK_FOLLOW,         /* Follow target entity */
    AGENTITE_TASK_FLEE,           /* Flee from danger */
    AGENTITE_TASK_WAIT,           /* Wait for duration */
    AGENTITE_TASK_INTERACT,       /* Interact with entity/object */
    AGENTITE_TASK_PATROL,         /* Patrol between waypoints */
    AGENTITE_TASK_WITHDRAW,       /* Withdraw resources from storage */
    AGENTITE_TASK_MINE,           /* Mine resource node */
    AGENTITE_TASK_COUNT,

    /* User-defined task types start here */
    AGENTITE_TASK_USER = 100,
} Agentite_TaskType;

/**
 * Task execution status.
 */
typedef enum Agentite_TaskStatus {
    AGENTITE_TASK_PENDING = 0,    /* Not yet started */
    AGENTITE_TASK_IN_PROGRESS,    /* Currently executing */
    AGENTITE_TASK_COMPLETED,      /* Successfully completed */
    AGENTITE_TASK_FAILED,         /* Failed to complete */
    AGENTITE_TASK_CANCELLED,      /* Cancelled before completion */
} Agentite_TaskStatus;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Move task parameters.
 */
typedef struct Agentite_TaskMove {
    int target_x;               /* Target grid X */
    int target_y;               /* Target grid Y */
    bool run;                   /* Use running speed if available */
} Agentite_TaskMove;

/**
 * Explore task parameters.
 */
typedef struct Agentite_TaskExplore {
    int center_x;               /* Center of exploration area */
    int center_y;
    int radius;                 /* Exploration radius */
    float duration;             /* Maximum exploration time (0 = until done) */
} Agentite_TaskExplore;

/**
 * Collect task parameters.
 */
typedef struct Agentite_TaskCollect {
    int target_x;               /* Resource position */
    int target_y;
    int resource_type;          /* Type of resource to collect */
    int quantity;               /* Amount to collect (0 = all available) */
} Agentite_TaskCollect;

/**
 * Deposit task parameters.
 */
typedef struct Agentite_TaskDeposit {
    int storage_x;              /* Storage position */
    int storage_y;
    int resource_type;          /* Type to deposit (-1 = all) */
    int quantity;               /* Amount to deposit (0 = all carried) */
} Agentite_TaskDeposit;

/**
 * Craft task parameters.
 */
typedef struct Agentite_TaskCraft {
    int recipe_id;              /* Recipe to craft */
    int quantity;               /* Number to craft */
} Agentite_TaskCraft;

/**
 * Build task parameters.
 */
typedef struct Agentite_TaskBuild {
    int target_x;               /* Building position */
    int target_y;
    int building_type;          /* Type of building to construct */
    int direction;              /* Building orientation (0-3) */
} Agentite_TaskBuild;

/**
 * Attack task parameters.
 */
typedef struct Agentite_TaskAttack {
    uint32_t target_entity;     /* Entity to attack */
    bool pursue;                /* Chase if target moves */
} Agentite_TaskAttack;

/**
 * Defend task parameters.
 */
typedef struct Agentite_TaskDefend {
    int center_x;               /* Defense position */
    int center_y;
    int radius;                 /* Defense radius */
    float duration;             /* How long to defend (0 = indefinite) */
} Agentite_TaskDefend;

/**
 * Follow task parameters.
 */
typedef struct Agentite_TaskFollow {
    uint32_t target_entity;     /* Entity to follow */
    int min_distance;           /* Minimum distance to maintain */
    int max_distance;           /* Maximum distance before giving up */
} Agentite_TaskFollow;

/**
 * Wait task parameters.
 */
typedef struct Agentite_TaskWait {
    float duration;             /* Seconds to wait */
    float elapsed;              /* Time already waited */
} Agentite_TaskWait;

/**
 * Interact task parameters.
 */
typedef struct Agentite_TaskInteract {
    int target_x;               /* Interaction target position */
    int target_y;
    uint32_t target_entity;     /* Or entity to interact with (0 = use position) */
    int interaction_type;       /* Game-defined interaction type */
} Agentite_TaskInteract;

/**
 * Patrol task parameters.
 */
typedef struct Agentite_TaskPatrol {
    int waypoints[8][2];        /* Up to 8 waypoints [x,y] */
    int waypoint_count;         /* Number of waypoints */
    int current_waypoint;       /* Current target waypoint */
    bool loop;                  /* Loop patrol or stop at end */
} Agentite_TaskPatrol;

/**
 * Withdraw task parameters.
 */
typedef struct Agentite_TaskWithdraw {
    int storage_x;              /* Storage position */
    int storage_y;
    int resource_type;          /* Type to withdraw */
    int quantity;               /* Amount to withdraw */
} Agentite_TaskWithdraw;

/**
 * Mine task parameters.
 */
typedef struct Agentite_TaskMine {
    int target_x;               /* Resource node position */
    int target_y;
    int quantity;               /* Amount to mine (0 = until full) */
} Agentite_TaskMine;

/**
 * Union of all task parameter types.
 */
typedef union Agentite_TaskData {
    Agentite_TaskMove move;
    Agentite_TaskExplore explore;
    Agentite_TaskCollect collect;
    Agentite_TaskDeposit deposit;
    Agentite_TaskCraft craft;
    Agentite_TaskBuild build;
    Agentite_TaskAttack attack;
    Agentite_TaskDefend defend;
    Agentite_TaskFollow follow;
    Agentite_TaskWait wait;
    Agentite_TaskInteract interact;
    Agentite_TaskPatrol patrol;
    Agentite_TaskWithdraw withdraw;
    Agentite_TaskMine mine;
    uint8_t raw[AGENTITE_TASK_MAX_DATA];  /* For custom tasks */
} Agentite_TaskData;

/**
 * A single task in the queue.
 */
typedef struct Agentite_Task {
    Agentite_TaskType type;           /* Task type */
    Agentite_TaskStatus status;       /* Current status */
    Agentite_TaskData data;           /* Task-specific parameters */
    float progress;                 /* 0.0 to 1.0 completion */
    float priority;                 /* Higher = more important */
    char fail_reason[AGENTITE_TASK_MAX_REASON];  /* Failure description */
    int32_t assigned_entity;        /* Entity assigned to this task (-1 = none) */
    void *userdata;                 /* User-defined data pointer */
} Agentite_Task;

/*============================================================================
 * Task Queue Forward Declaration
 *============================================================================*/

typedef struct Agentite_TaskQueue Agentite_TaskQueue;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Task completion callback.
 * Called when a task transitions to COMPLETED, FAILED, or CANCELLED.
 *
 * @param queue    The task queue
 * @param task     The completed task
 * @param userdata User data passed to set_callback
 */
typedef void (*Agentite_TaskCallback)(Agentite_TaskQueue *queue,
                                     const Agentite_Task *task,
                                     void *userdata);

/**
 * Create a new task queue.
 *
 * @param max_tasks Maximum tasks the queue can hold
 * @return New task queue or NULL on failure
 */
Agentite_TaskQueue *agentite_task_queue_create(int max_tasks);

/**
 * Destroy a task queue and free resources.
 *
 * @param queue Queue to destroy
 */
void agentite_task_queue_destroy(Agentite_TaskQueue *queue);

/*============================================================================
 * Task Addition
 *============================================================================*/

/**
 * Add a move task to the queue.
 *
 * @param queue    Task queue
 * @param target_x Destination X coordinate
 * @param target_y Destination Y coordinate
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_move(Agentite_TaskQueue *queue, int target_x, int target_y);

/**
 * Add a move task with run option.
 *
 * @param queue    Task queue
 * @param target_x Destination X coordinate
 * @param target_y Destination Y coordinate
 * @param run      Whether to run
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_move_ex(Agentite_TaskQueue *queue,
                                   int target_x, int target_y, bool run);

/**
 * Add an explore task to the queue.
 *
 * @param queue   Task queue
 * @param area_x  Center X of exploration area
 * @param area_y  Center Y of exploration area
 * @param radius  Exploration radius
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_explore(Agentite_TaskQueue *queue,
                                   int area_x, int area_y, int radius);

/**
 * Add a collect task to the queue.
 *
 * @param queue         Task queue
 * @param x             Resource X position
 * @param y             Resource Y position
 * @param resource_type Resource type to collect
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_collect(Agentite_TaskQueue *queue,
                                   int x, int y, int resource_type);

/**
 * Add a collect task with quantity.
 *
 * @param queue         Task queue
 * @param x             Resource X position
 * @param y             Resource Y position
 * @param resource_type Resource type to collect
 * @param quantity      Amount to collect (0 = all)
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_collect_ex(Agentite_TaskQueue *queue,
                                      int x, int y, int resource_type, int quantity);

/**
 * Add a deposit task to the queue.
 *
 * @param queue         Task queue
 * @param storage_x     Storage X position
 * @param storage_y     Storage Y position
 * @param resource_type Resource type to deposit (-1 = all)
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_deposit(Agentite_TaskQueue *queue,
                                   int storage_x, int storage_y, int resource_type);

/**
 * Add a craft task to the queue.
 *
 * @param queue     Task queue
 * @param recipe_id Recipe to craft
 * @param quantity  Number to craft
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_craft(Agentite_TaskQueue *queue, int recipe_id, int quantity);

/**
 * Add a build task to the queue.
 *
 * @param queue         Task queue
 * @param x             Build X position
 * @param y             Build Y position
 * @param building_type Building type to construct
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_build(Agentite_TaskQueue *queue,
                                 int x, int y, int building_type);

/**
 * Add a build task with direction.
 *
 * @param queue         Task queue
 * @param x             Build X position
 * @param y             Build Y position
 * @param building_type Building type to construct
 * @param direction     Building orientation (0-3)
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_build_ex(Agentite_TaskQueue *queue,
                                    int x, int y, int building_type, int direction);

/**
 * Add an attack task to the queue.
 *
 * @param queue         Task queue
 * @param target_entity Entity to attack
 * @param pursue        Chase if target moves
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_attack(Agentite_TaskQueue *queue,
                                  uint32_t target_entity, bool pursue);

/**
 * Add a defend task to the queue.
 *
 * @param queue    Task queue
 * @param center_x Defense center X
 * @param center_y Defense center Y
 * @param radius   Defense radius
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_defend(Agentite_TaskQueue *queue,
                                  int center_x, int center_y, int radius);

/**
 * Add a follow task to the queue.
 *
 * @param queue         Task queue
 * @param target_entity Entity to follow
 * @param min_distance  Minimum distance to maintain
 * @param max_distance  Maximum distance before giving up
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_follow(Agentite_TaskQueue *queue,
                                  uint32_t target_entity,
                                  int min_distance, int max_distance);

/**
 * Add a wait task to the queue.
 *
 * @param queue    Task queue
 * @param duration Seconds to wait
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_wait(Agentite_TaskQueue *queue, float duration);

/**
 * Add an interact task to the queue.
 *
 * @param queue            Task queue
 * @param x                Target X position
 * @param y                Target Y position
 * @param interaction_type Game-defined interaction type
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_interact(Agentite_TaskQueue *queue,
                                    int x, int y, int interaction_type);

/**
 * Add an interact task with entity target.
 *
 * @param queue            Task queue
 * @param target_entity    Entity to interact with
 * @param interaction_type Game-defined interaction type
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_interact_entity(Agentite_TaskQueue *queue,
                                           uint32_t target_entity,
                                           int interaction_type);

/**
 * Add a patrol task with waypoints.
 *
 * @param queue          Task queue
 * @param waypoints      Array of [x,y] waypoint coordinates
 * @param waypoint_count Number of waypoints (max 8)
 * @param loop           Loop patrol or stop at end
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_patrol(Agentite_TaskQueue *queue,
                                  const int waypoints[][2],
                                  int waypoint_count, bool loop);

/**
 * Add a withdraw task to the queue.
 *
 * @param queue         Task queue
 * @param storage_x     Storage X position
 * @param storage_y     Storage Y position
 * @param resource_type Resource type to withdraw
 * @param quantity      Amount to withdraw
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_withdraw(Agentite_TaskQueue *queue,
                                    int storage_x, int storage_y,
                                    int resource_type, int quantity);

/**
 * Add a mine task to the queue.
 *
 * @param queue    Task queue
 * @param target_x Resource node X position
 * @param target_y Resource node Y position
 * @param quantity Amount to mine (0 = until full)
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_mine(Agentite_TaskQueue *queue,
                                int target_x, int target_y, int quantity);

/**
 * Add a custom task to the queue.
 *
 * @param queue Task queue
 * @param type  Custom task type (>= AGENTITE_TASK_USER)
 * @param data  Task data (copied, up to AGENTITE_TASK_MAX_DATA bytes)
 * @param size  Size of task data
 * @return Index of added task or -1 on failure
 */
int agentite_task_queue_add_custom(Agentite_TaskQueue *queue,
                                  Agentite_TaskType type,
                                  const void *data, size_t size);

/*============================================================================
 * Queue Operations
 *============================================================================*/

/**
 * Get the current (front) task in the queue.
 *
 * @param queue Task queue
 * @return Current task or NULL if queue is empty
 */
Agentite_Task *agentite_task_queue_current(Agentite_TaskQueue *queue);

/**
 * Get task at specific index.
 *
 * @param queue Task queue
 * @param index Task index
 * @return Task at index or NULL if out of bounds
 */
Agentite_Task *agentite_task_queue_get(Agentite_TaskQueue *queue, int index);

/**
 * Start the current task (set status to IN_PROGRESS).
 * Call this when an agent begins working on a task.
 *
 * @param queue Task queue
 * @return true if task was started
 */
bool agentite_task_queue_start(Agentite_TaskQueue *queue);

/**
 * Mark the current task as complete and advance to next task.
 * Triggers completion callback if set.
 *
 * @param queue Task queue
 */
void agentite_task_queue_complete(Agentite_TaskQueue *queue);

/**
 * Mark the current task as failed and advance to next task.
 * Triggers completion callback if set.
 *
 * @param queue  Task queue
 * @param reason Failure reason (copied, max AGENTITE_TASK_MAX_REASON chars)
 */
void agentite_task_queue_fail(Agentite_TaskQueue *queue, const char *reason);

/**
 * Cancel the current task and advance to next task.
 * Triggers completion callback if set.
 *
 * @param queue Task queue
 */
void agentite_task_queue_cancel(Agentite_TaskQueue *queue);

/**
 * Update task progress.
 *
 * @param queue    Task queue
 * @param progress Progress value (0.0 to 1.0)
 */
void agentite_task_queue_set_progress(Agentite_TaskQueue *queue, float progress);

/**
 * Clear all tasks from the queue.
 * Cancels current task if in progress.
 *
 * @param queue Task queue
 */
void agentite_task_queue_clear(Agentite_TaskQueue *queue);

/**
 * Remove a specific task from the queue by index.
 *
 * @param queue Task queue
 * @param index Task index to remove
 * @return true if task was removed
 */
bool agentite_task_queue_remove(Agentite_TaskQueue *queue, int index);

/**
 * Insert a task at the front of the queue (after current task).
 * Useful for interrupt tasks that should execute immediately.
 *
 * @param queue Task queue
 * @param type  Task type
 * @param data  Task data
 * @param size  Size of task data
 * @return Index of inserted task or -1 on failure
 */
int agentite_task_queue_insert_front(Agentite_TaskQueue *queue,
                                    Agentite_TaskType type,
                                    const void *data, size_t size);

/*============================================================================
 * Queue State
 *============================================================================*/

/**
 * Get number of tasks in the queue.
 *
 * @param queue Task queue
 * @return Task count
 */
int agentite_task_queue_count(Agentite_TaskQueue *queue);

/**
 * Check if queue is empty.
 *
 * @param queue Task queue
 * @return true if queue is empty
 */
bool agentite_task_queue_is_empty(Agentite_TaskQueue *queue);

/**
 * Check if queue is full.
 *
 * @param queue Task queue
 * @return true if queue is full
 */
bool agentite_task_queue_is_full(Agentite_TaskQueue *queue);

/**
 * Check if agent is currently idle (no task or current task complete).
 *
 * @param queue Task queue
 * @return true if idle
 */
bool agentite_task_queue_is_idle(Agentite_TaskQueue *queue);

/**
 * Get maximum capacity of the queue.
 *
 * @param queue Task queue
 * @return Maximum tasks
 */
int agentite_task_queue_capacity(Agentite_TaskQueue *queue);

/*============================================================================
 * Wait Task Helper
 *============================================================================*/

/**
 * Update wait task with elapsed time.
 * Automatically completes the task when duration is reached.
 *
 * @param queue      Task queue
 * @param delta_time Time elapsed this frame
 * @return true if wait task is still active, false if completed or not a wait task
 */
bool agentite_task_queue_update_wait(Agentite_TaskQueue *queue, float delta_time);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * Set completion callback.
 * Called when any task completes, fails, or is cancelled.
 *
 * @param queue    Task queue
 * @param callback Callback function (NULL to clear)
 * @param userdata User data passed to callback
 */
void agentite_task_queue_set_callback(Agentite_TaskQueue *queue,
                                     Agentite_TaskCallback callback,
                                     void *userdata);

/*============================================================================
 * Assignment
 *============================================================================*/

/**
 * Assign an entity to execute this queue's tasks.
 *
 * @param queue     Task queue
 * @param entity_id Entity ID to assign (-1 to clear)
 */
void agentite_task_queue_set_assigned_entity(Agentite_TaskQueue *queue, int32_t entity_id);

/**
 * Get the assigned entity for this queue.
 *
 * @param queue Task queue
 * @return Assigned entity ID or -1 if none
 */
int32_t agentite_task_queue_get_assigned_entity(Agentite_TaskQueue *queue);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get human-readable name for a task type.
 *
 * @param type Task type
 * @return Static string name
 */
const char *agentite_task_type_name(Agentite_TaskType type);

/**
 * Get human-readable name for a task status.
 *
 * @param status Task status
 * @return Static string name
 */
const char *agentite_task_status_name(Agentite_TaskStatus status);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_TASK_H */
