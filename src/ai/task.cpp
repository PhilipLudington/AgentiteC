/**
 * Carbon Task Queue Implementation
 *
 * Sequential task execution for autonomous AI agents.
 */

#include "carbon/carbon.h"
#include "carbon/task.h"
#include "carbon/error.h"
#include "carbon/validate.h"

#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

struct Carbon_TaskQueue {
    Carbon_Task *tasks;             /* Task array */
    int capacity;                   /* Maximum tasks */
    int count;                      /* Current task count */
    int head;                       /* Index of current task */
    Carbon_TaskCallback callback;   /* Completion callback */
    void *callback_userdata;        /* Callback user data */
    int32_t assigned_entity;        /* Entity assigned to execute tasks */
};

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static int queue_next_index(Carbon_TaskQueue *queue, int index) {
    return (index + 1) % queue->capacity;
}

static int queue_actual_index(Carbon_TaskQueue *queue, int logical_index) {
    return (queue->head + logical_index) % queue->capacity;
}

static void notify_completion(Carbon_TaskQueue *queue, const Carbon_Task *task) {
    if (queue->callback) {
        queue->callback(queue, task, queue->callback_userdata);
    }
}

static void advance_queue(Carbon_TaskQueue *queue) {
    if (queue->count > 0) {
        queue->head = queue_next_index(queue, queue->head);
        queue->count--;
    }
}

static int add_task(Carbon_TaskQueue *queue, Carbon_TaskType type,
                    const void *data, size_t data_size) {
    CARBON_VALIDATE_PTR_RET(queue, -1);

    if (queue->count >= queue->capacity) {
        carbon_set_error("carbon_task_queue: queue is full");
        return -1;
    }

    /* Calculate tail position */
    int tail = queue_actual_index(queue, queue->count);

    Carbon_Task *task = &queue->tasks[tail];
    memset(task, 0, sizeof(Carbon_Task));

    task->type = type;
    task->status = CARBON_TASK_PENDING;
    task->progress = 0.0f;
    task->priority = 0.0f;
    task->assigned_entity = -1;

    if (data && data_size > 0) {
        size_t copy_size = data_size < sizeof(task->data) ? data_size : sizeof(task->data);
        memcpy(&task->data, data, copy_size);
    }

    int index = queue->count;
    queue->count++;

    return index;
}

/*============================================================================
 * Lifecycle
 *============================================================================*/

Carbon_TaskQueue *carbon_task_queue_create(int max_tasks) {
    if (max_tasks <= 0) {
        carbon_set_error("carbon_task_queue_create: max_tasks must be positive");
        return NULL;
    }

    Carbon_TaskQueue *queue = CARBON_ALLOC(Carbon_TaskQueue);
    if (!queue) {
        carbon_set_error("carbon_task_queue_create: allocation failed");
        return NULL;
    }

    queue->tasks = CARBON_ALLOC_ARRAY(Carbon_Task, (size_t)max_tasks);
    if (!queue->tasks) {
        carbon_set_error("carbon_task_queue_create: task array allocation failed");
        free(queue);
        return NULL;
    }

    queue->capacity = max_tasks;
    queue->count = 0;
    queue->head = 0;
    queue->callback = NULL;
    queue->callback_userdata = NULL;
    queue->assigned_entity = -1;

    return queue;
}

void carbon_task_queue_destroy(Carbon_TaskQueue *queue) {
    if (!queue) return;

    free(queue->tasks);
    free(queue);
}

/*============================================================================
 * Task Addition - Movement
 *============================================================================*/

int carbon_task_queue_add_move(Carbon_TaskQueue *queue, int target_x, int target_y) {
    return carbon_task_queue_add_move_ex(queue, target_x, target_y, false);
}

int carbon_task_queue_add_move_ex(Carbon_TaskQueue *queue,
                                   int target_x, int target_y, bool run) {
    Carbon_TaskMove data = {
        .target_x = target_x,
        .target_y = target_y,
        .run = run
    };
    return add_task(queue, CARBON_TASK_MOVE, &data, sizeof(data));
}

/*============================================================================
 * Task Addition - Exploration
 *============================================================================*/

int carbon_task_queue_add_explore(Carbon_TaskQueue *queue,
                                   int area_x, int area_y, int radius) {
    Carbon_TaskExplore data = {
        .center_x = area_x,
        .center_y = area_y,
        .radius = radius,
        .duration = 0.0f
    };
    return add_task(queue, CARBON_TASK_EXPLORE, &data, sizeof(data));
}

/*============================================================================
 * Task Addition - Resources
 *============================================================================*/

int carbon_task_queue_add_collect(Carbon_TaskQueue *queue,
                                   int x, int y, int resource_type) {
    return carbon_task_queue_add_collect_ex(queue, x, y, resource_type, 0);
}

int carbon_task_queue_add_collect_ex(Carbon_TaskQueue *queue,
                                      int x, int y, int resource_type, int quantity) {
    Carbon_TaskCollect data = {
        .target_x = x,
        .target_y = y,
        .resource_type = resource_type,
        .quantity = quantity
    };
    return add_task(queue, CARBON_TASK_COLLECT, &data, sizeof(data));
}

int carbon_task_queue_add_deposit(Carbon_TaskQueue *queue,
                                   int storage_x, int storage_y, int resource_type) {
    Carbon_TaskDeposit data = {
        .storage_x = storage_x,
        .storage_y = storage_y,
        .resource_type = resource_type,
        .quantity = 0
    };
    return add_task(queue, CARBON_TASK_DEPOSIT, &data, sizeof(data));
}

int carbon_task_queue_add_withdraw(Carbon_TaskQueue *queue,
                                    int storage_x, int storage_y,
                                    int resource_type, int quantity) {
    Carbon_TaskWithdraw data = {
        .storage_x = storage_x,
        .storage_y = storage_y,
        .resource_type = resource_type,
        .quantity = quantity
    };
    return add_task(queue, CARBON_TASK_WITHDRAW, &data, sizeof(data));
}

int carbon_task_queue_add_mine(Carbon_TaskQueue *queue,
                                int target_x, int target_y, int quantity) {
    Carbon_TaskMine data = {
        .target_x = target_x,
        .target_y = target_y,
        .quantity = quantity
    };
    return add_task(queue, CARBON_TASK_MINE, &data, sizeof(data));
}

/*============================================================================
 * Task Addition - Crafting & Building
 *============================================================================*/

int carbon_task_queue_add_craft(Carbon_TaskQueue *queue, int recipe_id, int quantity) {
    Carbon_TaskCraft data = {
        .recipe_id = recipe_id,
        .quantity = quantity
    };
    return add_task(queue, CARBON_TASK_CRAFT, &data, sizeof(data));
}

int carbon_task_queue_add_build(Carbon_TaskQueue *queue,
                                 int x, int y, int building_type) {
    return carbon_task_queue_add_build_ex(queue, x, y, building_type, 0);
}

int carbon_task_queue_add_build_ex(Carbon_TaskQueue *queue,
                                    int x, int y, int building_type, int direction) {
    Carbon_TaskBuild data = {
        .target_x = x,
        .target_y = y,
        .building_type = building_type,
        .direction = direction
    };
    return add_task(queue, CARBON_TASK_BUILD, &data, sizeof(data));
}

/*============================================================================
 * Task Addition - Combat
 *============================================================================*/

int carbon_task_queue_add_attack(Carbon_TaskQueue *queue,
                                  uint32_t target_entity, bool pursue) {
    Carbon_TaskAttack data = {
        .target_entity = target_entity,
        .pursue = pursue
    };
    return add_task(queue, CARBON_TASK_ATTACK, &data, sizeof(data));
}

int carbon_task_queue_add_defend(Carbon_TaskQueue *queue,
                                  int center_x, int center_y, int radius) {
    Carbon_TaskDefend data = {
        .center_x = center_x,
        .center_y = center_y,
        .radius = radius,
        .duration = 0.0f
    };
    return add_task(queue, CARBON_TASK_DEFEND, &data, sizeof(data));
}

int carbon_task_queue_add_follow(Carbon_TaskQueue *queue,
                                  uint32_t target_entity,
                                  int min_distance, int max_distance) {
    Carbon_TaskFollow data = {
        .target_entity = target_entity,
        .min_distance = min_distance,
        .max_distance = max_distance
    };
    return add_task(queue, CARBON_TASK_FOLLOW, &data, sizeof(data));
}

/*============================================================================
 * Task Addition - Utility
 *============================================================================*/

int carbon_task_queue_add_wait(Carbon_TaskQueue *queue, float duration) {
    Carbon_TaskWait data = {
        .duration = duration,
        .elapsed = 0.0f
    };
    return add_task(queue, CARBON_TASK_WAIT, &data, sizeof(data));
}

int carbon_task_queue_add_interact(Carbon_TaskQueue *queue,
                                    int x, int y, int interaction_type) {
    Carbon_TaskInteract data = {
        .target_x = x,
        .target_y = y,
        .target_entity = 0,
        .interaction_type = interaction_type
    };
    return add_task(queue, CARBON_TASK_INTERACT, &data, sizeof(data));
}

int carbon_task_queue_add_interact_entity(Carbon_TaskQueue *queue,
                                           uint32_t target_entity,
                                           int interaction_type) {
    Carbon_TaskInteract data = {
        .target_x = 0,
        .target_y = 0,
        .target_entity = target_entity,
        .interaction_type = interaction_type
    };
    return add_task(queue, CARBON_TASK_INTERACT, &data, sizeof(data));
}

int carbon_task_queue_add_patrol(Carbon_TaskQueue *queue,
                                  const int waypoints[][2],
                                  int waypoint_count, bool loop) {
    CARBON_VALIDATE_PTR_RET(queue, -1);
    CARBON_VALIDATE_PTR_RET(waypoints, -1);

    if (waypoint_count <= 0 || waypoint_count > 8) {
        carbon_set_error("carbon_task_queue_add_patrol: waypoint_count must be 1-8");
        return -1;
    }

    Carbon_TaskPatrol data = {
        .waypoint_count = waypoint_count,
        .current_waypoint = 0,
        .loop = loop
    };

    for (int i = 0; i < waypoint_count; i++) {
        data.waypoints[i][0] = waypoints[i][0];
        data.waypoints[i][1] = waypoints[i][1];
    }

    return add_task(queue, CARBON_TASK_PATROL, &data, sizeof(data));
}

int carbon_task_queue_add_custom(Carbon_TaskQueue *queue,
                                  Carbon_TaskType type,
                                  const void *data, size_t size) {
    CARBON_VALIDATE_PTR_RET(queue, -1);

    if (type < CARBON_TASK_USER) {
        carbon_set_error("carbon_task_queue_add_custom: type must be >= CARBON_TASK_USER");
        return -1;
    }

    if (size > CARBON_TASK_MAX_DATA) {
        carbon_set_error("carbon_task_queue_add_custom: data too large");
        return -1;
    }

    return add_task(queue, type, data, size);
}

/*============================================================================
 * Queue Operations
 *============================================================================*/

Carbon_Task *carbon_task_queue_current(Carbon_TaskQueue *queue) {
    if (!queue || queue->count == 0) {
        return NULL;
    }
    return &queue->tasks[queue->head];
}

Carbon_Task *carbon_task_queue_get(Carbon_TaskQueue *queue, int index) {
    if (!queue || index < 0 || index >= queue->count) {
        return NULL;
    }
    int actual = queue_actual_index(queue, index);
    return &queue->tasks[actual];
}

bool carbon_task_queue_start(Carbon_TaskQueue *queue) {
    CARBON_VALIDATE_PTR_RET(queue, false);

    Carbon_Task *current = carbon_task_queue_current(queue);
    if (!current) {
        return false;
    }

    if (current->status != CARBON_TASK_PENDING) {
        return false;
    }

    current->status = CARBON_TASK_IN_PROGRESS;
    current->assigned_entity = queue->assigned_entity;
    return true;
}

void carbon_task_queue_complete(Carbon_TaskQueue *queue) {
    CARBON_VALIDATE_PTR(queue);

    Carbon_Task *current = carbon_task_queue_current(queue);
    if (!current) {
        return;
    }

    current->status = CARBON_TASK_COMPLETED;
    current->progress = 1.0f;

    notify_completion(queue, current);
    advance_queue(queue);
}

void carbon_task_queue_fail(Carbon_TaskQueue *queue, const char *reason) {
    CARBON_VALIDATE_PTR(queue);

    Carbon_Task *current = carbon_task_queue_current(queue);
    if (!current) {
        return;
    }

    current->status = CARBON_TASK_FAILED;

    if (reason) {
        strncpy(current->fail_reason, reason, CARBON_TASK_MAX_REASON - 1);
        current->fail_reason[CARBON_TASK_MAX_REASON - 1] = '\0';
    }

    notify_completion(queue, current);
    advance_queue(queue);
}

void carbon_task_queue_cancel(Carbon_TaskQueue *queue) {
    CARBON_VALIDATE_PTR(queue);

    Carbon_Task *current = carbon_task_queue_current(queue);
    if (!current) {
        return;
    }

    current->status = CARBON_TASK_CANCELLED;

    notify_completion(queue, current);
    advance_queue(queue);
}

void carbon_task_queue_set_progress(Carbon_TaskQueue *queue, float progress) {
    CARBON_VALIDATE_PTR(queue);

    Carbon_Task *current = carbon_task_queue_current(queue);
    if (!current) {
        return;
    }

    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    current->progress = progress;
}

void carbon_task_queue_clear(Carbon_TaskQueue *queue) {
    CARBON_VALIDATE_PTR(queue);

    /* Cancel current task if in progress */
    Carbon_Task *current = carbon_task_queue_current(queue);
    if (current && current->status == CARBON_TASK_IN_PROGRESS) {
        current->status = CARBON_TASK_CANCELLED;
        notify_completion(queue, current);
    }

    queue->count = 0;
    queue->head = 0;
}

bool carbon_task_queue_remove(Carbon_TaskQueue *queue, int index) {
    CARBON_VALIDATE_PTR_RET(queue, false);

    if (index < 0 || index >= queue->count) {
        return false;
    }

    /* If removing current task, handle like cancel */
    if (index == 0) {
        carbon_task_queue_cancel(queue);
        return true;
    }

    /* Shift tasks down */
    for (int i = index; i < queue->count - 1; i++) {
        int src = queue_actual_index(queue, i + 1);
        int dst = queue_actual_index(queue, i);
        queue->tasks[dst] = queue->tasks[src];
    }

    queue->count--;
    return true;
}

int carbon_task_queue_insert_front(Carbon_TaskQueue *queue,
                                    Carbon_TaskType type,
                                    const void *data, size_t size) {
    CARBON_VALIDATE_PTR_RET(queue, -1);

    if (queue->count >= queue->capacity) {
        carbon_set_error("carbon_task_queue_insert_front: queue is full");
        return -1;
    }

    if (queue->count == 0) {
        /* Empty queue, just add normally */
        return add_task(queue, type, data, size);
    }

    /* Shift all tasks back by one */
    for (int i = queue->count; i > 1; i--) {
        int src = queue_actual_index(queue, i - 1);
        int dst = queue_actual_index(queue, i);
        queue->tasks[dst] = queue->tasks[src];
    }

    /* Insert at position 1 (after current) */
    int insert_pos = queue_actual_index(queue, 1);
    Carbon_Task *task = &queue->tasks[insert_pos];
    memset(task, 0, sizeof(Carbon_Task));

    task->type = type;
    task->status = CARBON_TASK_PENDING;
    task->progress = 0.0f;
    task->priority = 0.0f;
    task->assigned_entity = -1;

    if (data && size > 0) {
        size_t copy_size = size < sizeof(task->data) ? size : sizeof(task->data);
        memcpy(&task->data, data, copy_size);
    }

    queue->count++;
    return 1;
}

/*============================================================================
 * Queue State
 *============================================================================*/

int carbon_task_queue_count(Carbon_TaskQueue *queue) {
    return queue ? queue->count : 0;
}

bool carbon_task_queue_is_empty(Carbon_TaskQueue *queue) {
    return !queue || queue->count == 0;
}

bool carbon_task_queue_is_full(Carbon_TaskQueue *queue) {
    return queue && queue->count >= queue->capacity;
}

bool carbon_task_queue_is_idle(Carbon_TaskQueue *queue) {
    if (!queue || queue->count == 0) {
        return true;
    }

    Carbon_Task *current = &queue->tasks[queue->head];
    return current->status == CARBON_TASK_COMPLETED ||
           current->status == CARBON_TASK_FAILED ||
           current->status == CARBON_TASK_CANCELLED;
}

int carbon_task_queue_capacity(Carbon_TaskQueue *queue) {
    return queue ? queue->capacity : 0;
}

/*============================================================================
 * Wait Task Helper
 *============================================================================*/

bool carbon_task_queue_update_wait(Carbon_TaskQueue *queue, float delta_time) {
    CARBON_VALIDATE_PTR_RET(queue, false);

    Carbon_Task *current = carbon_task_queue_current(queue);
    if (!current || current->type != CARBON_TASK_WAIT) {
        return false;
    }

    if (current->status != CARBON_TASK_IN_PROGRESS) {
        return false;
    }

    Carbon_TaskWait *wait = &current->data.wait;
    wait->elapsed += delta_time;

    if (wait->duration > 0.0f) {
        current->progress = wait->elapsed / wait->duration;
        if (current->progress > 1.0f) {
            current->progress = 1.0f;
        }

        if (wait->elapsed >= wait->duration) {
            carbon_task_queue_complete(queue);
            return false;
        }
    }

    return true;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void carbon_task_queue_set_callback(Carbon_TaskQueue *queue,
                                     Carbon_TaskCallback callback,
                                     void *userdata) {
    CARBON_VALIDATE_PTR(queue);
    queue->callback = callback;
    queue->callback_userdata = userdata;
}

/*============================================================================
 * Assignment
 *============================================================================*/

void carbon_task_queue_set_assigned_entity(Carbon_TaskQueue *queue, int32_t entity_id) {
    CARBON_VALIDATE_PTR(queue);
    queue->assigned_entity = entity_id;
}

int32_t carbon_task_queue_get_assigned_entity(Carbon_TaskQueue *queue) {
    return queue ? queue->assigned_entity : -1;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *carbon_task_type_name(Carbon_TaskType type) {
    switch (type) {
        case CARBON_TASK_NONE:      return "None";
        case CARBON_TASK_MOVE:      return "Move";
        case CARBON_TASK_EXPLORE:   return "Explore";
        case CARBON_TASK_COLLECT:   return "Collect";
        case CARBON_TASK_DEPOSIT:   return "Deposit";
        case CARBON_TASK_CRAFT:     return "Craft";
        case CARBON_TASK_BUILD:     return "Build";
        case CARBON_TASK_ATTACK:    return "Attack";
        case CARBON_TASK_DEFEND:    return "Defend";
        case CARBON_TASK_FOLLOW:    return "Follow";
        case CARBON_TASK_FLEE:      return "Flee";
        case CARBON_TASK_WAIT:      return "Wait";
        case CARBON_TASK_INTERACT:  return "Interact";
        case CARBON_TASK_PATROL:    return "Patrol";
        case CARBON_TASK_WITHDRAW:  return "Withdraw";
        case CARBON_TASK_MINE:      return "Mine";
        default:
            if (type >= CARBON_TASK_USER) {
                return "Custom";
            }
            return "Unknown";
    }
}

const char *carbon_task_status_name(Carbon_TaskStatus status) {
    switch (status) {
        case CARBON_TASK_PENDING:     return "Pending";
        case CARBON_TASK_IN_PROGRESS: return "In Progress";
        case CARBON_TASK_COMPLETED:   return "Completed";
        case CARBON_TASK_FAILED:      return "Failed";
        case CARBON_TASK_CANCELLED:   return "Cancelled";
        default:                      return "Unknown";
    }
}
