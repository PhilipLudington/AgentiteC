/**
 * Carbon Task Queue System
 *
 * Sequential task execution for autonomous AI agents. Provides a queue
 * of tasks with lifecycle management, pathfinding integration, and
 * completion callbacks.
 *
 * Usage:
 *   // Create task queue for an agent
 *   Carbon_TaskQueue *queue = carbon_task_queue_create(16);
 *
 *   // Queue tasks
 *   carbon_task_queue_add_move(queue, target_x, target_y);
 *   carbon_task_queue_add_collect(queue, item_x, item_y, RESOURCE_WOOD);
 *   carbon_task_queue_add_wait(queue, 2.0f);
 *
 *   // In game loop:
 *   Carbon_Task *current = carbon_task_queue_current(queue);
 *   if (current && current->status == CARBON_TASK_IN_PROGRESS) {
 *       switch (current->type) {
 *           case CARBON_TASK_MOVE:
 *               // Move agent toward target...
 *               if (at_destination) {
 *                   carbon_task_queue_complete(queue);
 *               }
 *               break;
 *           // ... handle other task types
 *       }
 *   }
 *
 *   // Cleanup
 *   carbon_task_queue_destroy(queue);
 */

#ifndef CARBON_TASK_H
#define CARBON_TASK_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_TASK_MAX_DATA    64   /* Maximum bytes for task-specific data */
#define CARBON_TASK_MAX_REASON  64   /* Maximum length of failure reason */

/*============================================================================
 * Task Types
 *============================================================================*/

/**
 * Built-in task types for common agent actions.
 */
typedef enum Carbon_TaskType {
    CARBON_TASK_NONE = 0,
    CARBON_TASK_MOVE,           /* Move to target position */
    CARBON_TASK_EXPLORE,        /* Explore area around position */
    CARBON_TASK_COLLECT,        /* Collect resource at position */
    CARBON_TASK_DEPOSIT,        /* Deposit carried items */
    CARBON_TASK_CRAFT,          /* Craft item using recipe */
    CARBON_TASK_BUILD,          /* Construct building */
    CARBON_TASK_ATTACK,         /* Attack target entity */
    CARBON_TASK_DEFEND,         /* Defend position */
    CARBON_TASK_FOLLOW,         /* Follow target entity */
    CARBON_TASK_FLEE,           /* Flee from danger */
    CARBON_TASK_WAIT,           /* Wait for duration */
    CARBON_TASK_INTERACT,       /* Interact with entity/object */
    CARBON_TASK_PATROL,         /* Patrol between waypoints */
    CARBON_TASK_WITHDRAW,       /* Withdraw resources from storage */
    CARBON_TASK_MINE,           /* Mine resource node */
    CARBON_TASK_COUNT,

    /* User-defined task types start here */
    CARBON_TASK_USER = 100,
} Carbon_TaskType;

/**
 * Task execution status.
 */
typedef enum Carbon_TaskStatus {
    CARBON_TASK_PENDING = 0,    /* Not yet started */
    CARBON_TASK_IN_PROGRESS,    /* Currently executing */
    CARBON_TASK_COMPLETED,      /* Successfully completed */
    CARBON_TASK_FAILED,         /* Failed to complete */
    CARBON_TASK_CANCELLED,      /* Cancelled before completion */
} Carbon_TaskStatus;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Move task parameters.
 */
typedef struct Carbon_TaskMove {
    int target_x;               /* Target grid X */
    int target_y;               /* Target grid Y */
    bool run;                   /* Use running speed if available */
} Carbon_TaskMove;

/**
 * Explore task parameters.
 */
typedef struct Carbon_TaskExplore {
    int center_x;               /* Center of exploration area */
    int center_y;
    int radius;                 /* Exploration radius */
    float duration;             /* Maximum exploration time (0 = until done) */
} Carbon_TaskExplore;

/**
 * Collect task parameters.
 */
typedef struct Carbon_TaskCollect {
    int target_x;               /* Resource position */
    int target_y;
    int resource_type;          /* Type of resource to collect */
    int quantity;               /* Amount to collect (0 = all available) */
} Carbon_TaskCollect;

/**
 * Deposit task parameters.
 */
typedef struct Carbon_TaskDeposit {
    int storage_x;              /* Storage position */
    int storage_y;
    int resource_type;          /* Type to deposit (-1 = all) */
    int quantity;               /* Amount to deposit (0 = all carried) */
} Carbon_TaskDeposit;

/**
 * Craft task parameters.
 */
typedef struct Carbon_TaskCraft {
    int recipe_id;              /* Recipe to craft */
    int quantity;               /* Number to craft */
} Carbon_TaskCraft;

/**
 * Build task parameters.
 */
typedef struct Carbon_TaskBuild {
    int target_x;               /* Building position */
    int target_y;
    int building_type;          /* Type of building to construct */
    int direction;              /* Building orientation (0-3) */
} Carbon_TaskBuild;

/**
 * Attack task parameters.
 */
typedef struct Carbon_TaskAttack {
    uint32_t target_entity;     /* Entity to attack */
    bool pursue;                /* Chase if target moves */
} Carbon_TaskAttack;

/**
 * Defend task parameters.
 */
typedef struct Carbon_TaskDefend {
    int center_x;               /* Defense position */
    int center_y;
    int radius;                 /* Defense radius */
    float duration;             /* How long to defend (0 = indefinite) */
} Carbon_TaskDefend;

/**
 * Follow task parameters.
 */
typedef struct Carbon_TaskFollow {
    uint32_t target_entity;     /* Entity to follow */
    int min_distance;           /* Minimum distance to maintain */
    int max_distance;           /* Maximum distance before giving up */
} Carbon_TaskFollow;

/**
 * Wait task parameters.
 */
typedef struct Carbon_TaskWait {
    float duration;             /* Seconds to wait */
    float elapsed;              /* Time already waited */
} Carbon_TaskWait;

/**
 * Interact task parameters.
 */
typedef struct Carbon_TaskInteract {
    int target_x;               /* Interaction target position */
    int target_y;
    uint32_t target_entity;     /* Or entity to interact with (0 = use position) */
    int interaction_type;       /* Game-defined interaction type */
} Carbon_TaskInteract;

/**
 * Patrol task parameters.
 */
typedef struct Carbon_TaskPatrol {
    int waypoints[8][2];        /* Up to 8 waypoints [x,y] */
    int waypoint_count;         /* Number of waypoints */
    int current_waypoint;       /* Current target waypoint */
    bool loop;                  /* Loop patrol or stop at end */
} Carbon_TaskPatrol;

/**
 * Withdraw task parameters.
 */
typedef struct Carbon_TaskWithdraw {
    int storage_x;              /* Storage position */
    int storage_y;
    int resource_type;          /* Type to withdraw */
    int quantity;               /* Amount to withdraw */
} Carbon_TaskWithdraw;

/**
 * Mine task parameters.
 */
typedef struct Carbon_TaskMine {
    int target_x;               /* Resource node position */
    int target_y;
    int quantity;               /* Amount to mine (0 = until full) */
} Carbon_TaskMine;

/**
 * Union of all task parameter types.
 */
typedef union Carbon_TaskData {
    Carbon_TaskMove move;
    Carbon_TaskExplore explore;
    Carbon_TaskCollect collect;
    Carbon_TaskDeposit deposit;
    Carbon_TaskCraft craft;
    Carbon_TaskBuild build;
    Carbon_TaskAttack attack;
    Carbon_TaskDefend defend;
    Carbon_TaskFollow follow;
    Carbon_TaskWait wait;
    Carbon_TaskInteract interact;
    Carbon_TaskPatrol patrol;
    Carbon_TaskWithdraw withdraw;
    Carbon_TaskMine mine;
    uint8_t raw[CARBON_TASK_MAX_DATA];  /* For custom tasks */
} Carbon_TaskData;

/**
 * A single task in the queue.
 */
typedef struct Carbon_Task {
    Carbon_TaskType type;           /* Task type */
    Carbon_TaskStatus status;       /* Current status */
    Carbon_TaskData data;           /* Task-specific parameters */
    float progress;                 /* 0.0 to 1.0 completion */
    float priority;                 /* Higher = more important */
    char fail_reason[CARBON_TASK_MAX_REASON];  /* Failure description */
    int32_t assigned_entity;        /* Entity assigned to this task (-1 = none) */
    void *userdata;                 /* User-defined data pointer */
} Carbon_Task;

/*============================================================================
 * Task Queue Forward Declaration
 *============================================================================*/

typedef struct Carbon_TaskQueue Carbon_TaskQueue;

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
typedef void (*Carbon_TaskCallback)(Carbon_TaskQueue *queue,
                                     const Carbon_Task *task,
                                     void *userdata);

/**
 * Create a new task queue.
 *
 * @param max_tasks Maximum tasks the queue can hold
 * @return New task queue or NULL on failure
 */
Carbon_TaskQueue *carbon_task_queue_create(int max_tasks);

/**
 * Destroy a task queue and free resources.
 *
 * @param queue Queue to destroy
 */
void carbon_task_queue_destroy(Carbon_TaskQueue *queue);

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
int carbon_task_queue_add_move(Carbon_TaskQueue *queue, int target_x, int target_y);

/**
 * Add a move task with run option.
 *
 * @param queue    Task queue
 * @param target_x Destination X coordinate
 * @param target_y Destination Y coordinate
 * @param run      Whether to run
 * @return Index of added task or -1 on failure
 */
int carbon_task_queue_add_move_ex(Carbon_TaskQueue *queue,
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
int carbon_task_queue_add_explore(Carbon_TaskQueue *queue,
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
int carbon_task_queue_add_collect(Carbon_TaskQueue *queue,
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
int carbon_task_queue_add_collect_ex(Carbon_TaskQueue *queue,
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
int carbon_task_queue_add_deposit(Carbon_TaskQueue *queue,
                                   int storage_x, int storage_y, int resource_type);

/**
 * Add a craft task to the queue.
 *
 * @param queue     Task queue
 * @param recipe_id Recipe to craft
 * @param quantity  Number to craft
 * @return Index of added task or -1 on failure
 */
int carbon_task_queue_add_craft(Carbon_TaskQueue *queue, int recipe_id, int quantity);

/**
 * Add a build task to the queue.
 *
 * @param queue         Task queue
 * @param x             Build X position
 * @param y             Build Y position
 * @param building_type Building type to construct
 * @return Index of added task or -1 on failure
 */
int carbon_task_queue_add_build(Carbon_TaskQueue *queue,
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
int carbon_task_queue_add_build_ex(Carbon_TaskQueue *queue,
                                    int x, int y, int building_type, int direction);

/**
 * Add an attack task to the queue.
 *
 * @param queue         Task queue
 * @param target_entity Entity to attack
 * @param pursue        Chase if target moves
 * @return Index of added task or -1 on failure
 */
int carbon_task_queue_add_attack(Carbon_TaskQueue *queue,
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
int carbon_task_queue_add_defend(Carbon_TaskQueue *queue,
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
int carbon_task_queue_add_follow(Carbon_TaskQueue *queue,
                                  uint32_t target_entity,
                                  int min_distance, int max_distance);

/**
 * Add a wait task to the queue.
 *
 * @param queue    Task queue
 * @param duration Seconds to wait
 * @return Index of added task or -1 on failure
 */
int carbon_task_queue_add_wait(Carbon_TaskQueue *queue, float duration);

/**
 * Add an interact task to the queue.
 *
 * @param queue            Task queue
 * @param x                Target X position
 * @param y                Target Y position
 * @param interaction_type Game-defined interaction type
 * @return Index of added task or -1 on failure
 */
int carbon_task_queue_add_interact(Carbon_TaskQueue *queue,
                                    int x, int y, int interaction_type);

/**
 * Add an interact task with entity target.
 *
 * @param queue            Task queue
 * @param target_entity    Entity to interact with
 * @param interaction_type Game-defined interaction type
 * @return Index of added task or -1 on failure
 */
int carbon_task_queue_add_interact_entity(Carbon_TaskQueue *queue,
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
int carbon_task_queue_add_patrol(Carbon_TaskQueue *queue,
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
int carbon_task_queue_add_withdraw(Carbon_TaskQueue *queue,
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
int carbon_task_queue_add_mine(Carbon_TaskQueue *queue,
                                int target_x, int target_y, int quantity);

/**
 * Add a custom task to the queue.
 *
 * @param queue Task queue
 * @param type  Custom task type (>= CARBON_TASK_USER)
 * @param data  Task data (copied, up to CARBON_TASK_MAX_DATA bytes)
 * @param size  Size of task data
 * @return Index of added task or -1 on failure
 */
int carbon_task_queue_add_custom(Carbon_TaskQueue *queue,
                                  Carbon_TaskType type,
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
Carbon_Task *carbon_task_queue_current(Carbon_TaskQueue *queue);

/**
 * Get task at specific index.
 *
 * @param queue Task queue
 * @param index Task index
 * @return Task at index or NULL if out of bounds
 */
Carbon_Task *carbon_task_queue_get(Carbon_TaskQueue *queue, int index);

/**
 * Start the current task (set status to IN_PROGRESS).
 * Call this when an agent begins working on a task.
 *
 * @param queue Task queue
 * @return true if task was started
 */
bool carbon_task_queue_start(Carbon_TaskQueue *queue);

/**
 * Mark the current task as complete and advance to next task.
 * Triggers completion callback if set.
 *
 * @param queue Task queue
 */
void carbon_task_queue_complete(Carbon_TaskQueue *queue);

/**
 * Mark the current task as failed and advance to next task.
 * Triggers completion callback if set.
 *
 * @param queue  Task queue
 * @param reason Failure reason (copied, max CARBON_TASK_MAX_REASON chars)
 */
void carbon_task_queue_fail(Carbon_TaskQueue *queue, const char *reason);

/**
 * Cancel the current task and advance to next task.
 * Triggers completion callback if set.
 *
 * @param queue Task queue
 */
void carbon_task_queue_cancel(Carbon_TaskQueue *queue);

/**
 * Update task progress.
 *
 * @param queue    Task queue
 * @param progress Progress value (0.0 to 1.0)
 */
void carbon_task_queue_set_progress(Carbon_TaskQueue *queue, float progress);

/**
 * Clear all tasks from the queue.
 * Cancels current task if in progress.
 *
 * @param queue Task queue
 */
void carbon_task_queue_clear(Carbon_TaskQueue *queue);

/**
 * Remove a specific task from the queue by index.
 *
 * @param queue Task queue
 * @param index Task index to remove
 * @return true if task was removed
 */
bool carbon_task_queue_remove(Carbon_TaskQueue *queue, int index);

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
int carbon_task_queue_insert_front(Carbon_TaskQueue *queue,
                                    Carbon_TaskType type,
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
int carbon_task_queue_count(Carbon_TaskQueue *queue);

/**
 * Check if queue is empty.
 *
 * @param queue Task queue
 * @return true if queue is empty
 */
bool carbon_task_queue_is_empty(Carbon_TaskQueue *queue);

/**
 * Check if queue is full.
 *
 * @param queue Task queue
 * @return true if queue is full
 */
bool carbon_task_queue_is_full(Carbon_TaskQueue *queue);

/**
 * Check if agent is currently idle (no task or current task complete).
 *
 * @param queue Task queue
 * @return true if idle
 */
bool carbon_task_queue_is_idle(Carbon_TaskQueue *queue);

/**
 * Get maximum capacity of the queue.
 *
 * @param queue Task queue
 * @return Maximum tasks
 */
int carbon_task_queue_capacity(Carbon_TaskQueue *queue);

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
bool carbon_task_queue_update_wait(Carbon_TaskQueue *queue, float delta_time);

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
void carbon_task_queue_set_callback(Carbon_TaskQueue *queue,
                                     Carbon_TaskCallback callback,
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
void carbon_task_queue_set_assigned_entity(Carbon_TaskQueue *queue, int32_t entity_id);

/**
 * Get the assigned entity for this queue.
 *
 * @param queue Task queue
 * @return Assigned entity ID or -1 if none
 */
int32_t carbon_task_queue_get_assigned_entity(Carbon_TaskQueue *queue);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get human-readable name for a task type.
 *
 * @param type Task type
 * @return Static string name
 */
const char *carbon_task_type_name(Carbon_TaskType type);

/**
 * Get human-readable name for a task status.
 *
 * @param status Task status
 * @return Static string name
 */
const char *carbon_task_status_name(Carbon_TaskStatus status);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_TASK_H */
