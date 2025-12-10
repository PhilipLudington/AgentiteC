/**
 * Carbon Task Queue Implementation
 *
 * Sequential task execution for autonomous AI agents.
 */

#include "agentite/agentite.h"
#include "agentite/task.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

struct Agentite_TaskQueue {
    Agentite_Task *tasks;             /* Task array */
    int capacity;                   /* Maximum tasks */
    int count;                      /* Current task count */
    int head;                       /* Index of current task */
    Agentite_TaskCallback callback;   /* Completion callback */
    void *callback_userdata;        /* Callback user data */
    int32_t assigned_entity;        /* Entity assigned to execute tasks */
};

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static int queue_next_index(Agentite_TaskQueue *queue, int index) {
    return (index + 1) % queue->capacity;
}

static int queue_actual_index(Agentite_TaskQueue *queue, int logical_index) {
    return (queue->head + logical_index) % queue->capacity;
}

static void notify_completion(Agentite_TaskQueue *queue, const Agentite_Task *task) {
    if (queue->callback) {
        queue->callback(queue, task, queue->callback_userdata);
    }
}

static void advance_queue(Agentite_TaskQueue *queue) {
    if (queue->count > 0) {
        queue->head = queue_next_index(queue, queue->head);
        queue->count--;
    }
}

static int add_task(Agentite_TaskQueue *queue, Agentite_TaskType type,
                    const void *data, size_t data_size) {
    AGENTITE_VALIDATE_PTR_RET(queue, -1);

    if (queue->count >= queue->capacity) {
        agentite_set_error("agentite_task_queue: queue is full");
        return -1;
    }

    /* Calculate tail position */
    int tail = queue_actual_index(queue, queue->count);

    Agentite_Task *task = &queue->tasks[tail];
    memset(task, 0, sizeof(Agentite_Task));

    task->type = type;
    task->status = AGENTITE_TASK_PENDING;
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

Agentite_TaskQueue *agentite_task_queue_create(int max_tasks) {
    if (max_tasks <= 0) {
        agentite_set_error("agentite_task_queue_create: max_tasks must be positive");
        return NULL;
    }

    Agentite_TaskQueue *queue = AGENTITE_ALLOC(Agentite_TaskQueue);
    if (!queue) {
        agentite_set_error("agentite_task_queue_create: allocation failed");
        return NULL;
    }

    queue->tasks = AGENTITE_ALLOC_ARRAY(Agentite_Task, (size_t)max_tasks);
    if (!queue->tasks) {
        agentite_set_error("agentite_task_queue_create: task array allocation failed");
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

void agentite_task_queue_destroy(Agentite_TaskQueue *queue) {
    if (!queue) return;

    free(queue->tasks);
    free(queue);
}

/*============================================================================
 * Task Addition - Movement
 *============================================================================*/

int agentite_task_queue_add_move(Agentite_TaskQueue *queue, int target_x, int target_y) {
    return agentite_task_queue_add_move_ex(queue, target_x, target_y, false);
}

int agentite_task_queue_add_move_ex(Agentite_TaskQueue *queue,
                                   int target_x, int target_y, bool run) {
    Agentite_TaskMove data = {
        .target_x = target_x,
        .target_y = target_y,
        .run = run
    };
    return add_task(queue, AGENTITE_TASK_MOVE, &data, sizeof(data));
}

/*============================================================================
 * Task Addition - Exploration
 *============================================================================*/

int agentite_task_queue_add_explore(Agentite_TaskQueue *queue,
                                   int area_x, int area_y, int radius) {
    Agentite_TaskExplore data = {
        .center_x = area_x,
        .center_y = area_y,
        .radius = radius,
        .duration = 0.0f
    };
    return add_task(queue, AGENTITE_TASK_EXPLORE, &data, sizeof(data));
}

/*============================================================================
 * Task Addition - Resources
 *============================================================================*/

int agentite_task_queue_add_collect(Agentite_TaskQueue *queue,
                                   int x, int y, int resource_type) {
    return agentite_task_queue_add_collect_ex(queue, x, y, resource_type, 0);
}

int agentite_task_queue_add_collect_ex(Agentite_TaskQueue *queue,
                                      int x, int y, int resource_type, int quantity) {
    Agentite_TaskCollect data = {
        .target_x = x,
        .target_y = y,
        .resource_type = resource_type,
        .quantity = quantity
    };
    return add_task(queue, AGENTITE_TASK_COLLECT, &data, sizeof(data));
}

int agentite_task_queue_add_deposit(Agentite_TaskQueue *queue,
                                   int storage_x, int storage_y, int resource_type) {
    Agentite_TaskDeposit data = {
        .storage_x = storage_x,
        .storage_y = storage_y,
        .resource_type = resource_type,
        .quantity = 0
    };
    return add_task(queue, AGENTITE_TASK_DEPOSIT, &data, sizeof(data));
}

int agentite_task_queue_add_withdraw(Agentite_TaskQueue *queue,
                                    int storage_x, int storage_y,
                                    int resource_type, int quantity) {
    Agentite_TaskWithdraw data = {
        .storage_x = storage_x,
        .storage_y = storage_y,
        .resource_type = resource_type,
        .quantity = quantity
    };
    return add_task(queue, AGENTITE_TASK_WITHDRAW, &data, sizeof(data));
}

int agentite_task_queue_add_mine(Agentite_TaskQueue *queue,
                                int target_x, int target_y, int quantity) {
    Agentite_TaskMine data = {
        .target_x = target_x,
        .target_y = target_y,
        .quantity = quantity
    };
    return add_task(queue, AGENTITE_TASK_MINE, &data, sizeof(data));
}

/*============================================================================
 * Task Addition - Crafting & Building
 *============================================================================*/

int agentite_task_queue_add_craft(Agentite_TaskQueue *queue, int recipe_id, int quantity) {
    Agentite_TaskCraft data = {
        .recipe_id = recipe_id,
        .quantity = quantity
    };
    return add_task(queue, AGENTITE_TASK_CRAFT, &data, sizeof(data));
}

int agentite_task_queue_add_build(Agentite_TaskQueue *queue,
                                 int x, int y, int building_type) {
    return agentite_task_queue_add_build_ex(queue, x, y, building_type, 0);
}

int agentite_task_queue_add_build_ex(Agentite_TaskQueue *queue,
                                    int x, int y, int building_type, int direction) {
    Agentite_TaskBuild data = {
        .target_x = x,
        .target_y = y,
        .building_type = building_type,
        .direction = direction
    };
    return add_task(queue, AGENTITE_TASK_BUILD, &data, sizeof(data));
}

/*============================================================================
 * Task Addition - Combat
 *============================================================================*/

int agentite_task_queue_add_attack(Agentite_TaskQueue *queue,
                                  uint32_t target_entity, bool pursue) {
    Agentite_TaskAttack data = {
        .target_entity = target_entity,
        .pursue = pursue
    };
    return add_task(queue, AGENTITE_TASK_ATTACK, &data, sizeof(data));
}

int agentite_task_queue_add_defend(Agentite_TaskQueue *queue,
                                  int center_x, int center_y, int radius) {
    Agentite_TaskDefend data = {
        .center_x = center_x,
        .center_y = center_y,
        .radius = radius,
        .duration = 0.0f
    };
    return add_task(queue, AGENTITE_TASK_DEFEND, &data, sizeof(data));
}

int agentite_task_queue_add_follow(Agentite_TaskQueue *queue,
                                  uint32_t target_entity,
                                  int min_distance, int max_distance) {
    Agentite_TaskFollow data = {
        .target_entity = target_entity,
        .min_distance = min_distance,
        .max_distance = max_distance
    };
    return add_task(queue, AGENTITE_TASK_FOLLOW, &data, sizeof(data));
}

/*============================================================================
 * Task Addition - Utility
 *============================================================================*/

int agentite_task_queue_add_wait(Agentite_TaskQueue *queue, float duration) {
    Agentite_TaskWait data = {
        .duration = duration,
        .elapsed = 0.0f
    };
    return add_task(queue, AGENTITE_TASK_WAIT, &data, sizeof(data));
}

int agentite_task_queue_add_interact(Agentite_TaskQueue *queue,
                                    int x, int y, int interaction_type) {
    Agentite_TaskInteract data = {
        .target_x = x,
        .target_y = y,
        .target_entity = 0,
        .interaction_type = interaction_type
    };
    return add_task(queue, AGENTITE_TASK_INTERACT, &data, sizeof(data));
}

int agentite_task_queue_add_interact_entity(Agentite_TaskQueue *queue,
                                           uint32_t target_entity,
                                           int interaction_type) {
    Agentite_TaskInteract data = {
        .target_x = 0,
        .target_y = 0,
        .target_entity = target_entity,
        .interaction_type = interaction_type
    };
    return add_task(queue, AGENTITE_TASK_INTERACT, &data, sizeof(data));
}

int agentite_task_queue_add_patrol(Agentite_TaskQueue *queue,
                                  const int waypoints[][2],
                                  int waypoint_count, bool loop) {
    AGENTITE_VALIDATE_PTR_RET(queue, -1);
    AGENTITE_VALIDATE_PTR_RET(waypoints, -1);

    if (waypoint_count <= 0 || waypoint_count > 8) {
        agentite_set_error("agentite_task_queue_add_patrol: waypoint_count must be 1-8");
        return -1;
    }

    Agentite_TaskPatrol data = {
        .waypoint_count = waypoint_count,
        .current_waypoint = 0,
        .loop = loop
    };

    for (int i = 0; i < waypoint_count; i++) {
        data.waypoints[i][0] = waypoints[i][0];
        data.waypoints[i][1] = waypoints[i][1];
    }

    return add_task(queue, AGENTITE_TASK_PATROL, &data, sizeof(data));
}

int agentite_task_queue_add_custom(Agentite_TaskQueue *queue,
                                  Agentite_TaskType type,
                                  const void *data, size_t size) {
    AGENTITE_VALIDATE_PTR_RET(queue, -1);

    if (type < AGENTITE_TASK_USER) {
        agentite_set_error("agentite_task_queue_add_custom: type must be >= AGENTITE_TASK_USER");
        return -1;
    }

    if (size > AGENTITE_TASK_MAX_DATA) {
        agentite_set_error("agentite_task_queue_add_custom: data too large");
        return -1;
    }

    return add_task(queue, type, data, size);
}

/*============================================================================
 * Queue Operations
 *============================================================================*/

Agentite_Task *agentite_task_queue_current(Agentite_TaskQueue *queue) {
    if (!queue || queue->count == 0) {
        return NULL;
    }
    return &queue->tasks[queue->head];
}

Agentite_Task *agentite_task_queue_get(Agentite_TaskQueue *queue, int index) {
    if (!queue || index < 0 || index >= queue->count) {
        return NULL;
    }
    int actual = queue_actual_index(queue, index);
    return &queue->tasks[actual];
}

bool agentite_task_queue_start(Agentite_TaskQueue *queue) {
    AGENTITE_VALIDATE_PTR_RET(queue, false);

    Agentite_Task *current = agentite_task_queue_current(queue);
    if (!current) {
        return false;
    }

    if (current->status != AGENTITE_TASK_PENDING) {
        return false;
    }

    current->status = AGENTITE_TASK_IN_PROGRESS;
    current->assigned_entity = queue->assigned_entity;
    return true;
}

void agentite_task_queue_complete(Agentite_TaskQueue *queue) {
    AGENTITE_VALIDATE_PTR(queue);

    Agentite_Task *current = agentite_task_queue_current(queue);
    if (!current) {
        return;
    }

    current->status = AGENTITE_TASK_COMPLETED;
    current->progress = 1.0f;

    notify_completion(queue, current);
    advance_queue(queue);
}

void agentite_task_queue_fail(Agentite_TaskQueue *queue, const char *reason) {
    AGENTITE_VALIDATE_PTR(queue);

    Agentite_Task *current = agentite_task_queue_current(queue);
    if (!current) {
        return;
    }

    current->status = AGENTITE_TASK_FAILED;

    if (reason) {
        strncpy(current->fail_reason, reason, AGENTITE_TASK_MAX_REASON - 1);
        current->fail_reason[AGENTITE_TASK_MAX_REASON - 1] = '\0';
    }

    notify_completion(queue, current);
    advance_queue(queue);
}

void agentite_task_queue_cancel(Agentite_TaskQueue *queue) {
    AGENTITE_VALIDATE_PTR(queue);

    Agentite_Task *current = agentite_task_queue_current(queue);
    if (!current) {
        return;
    }

    current->status = AGENTITE_TASK_CANCELLED;

    notify_completion(queue, current);
    advance_queue(queue);
}

void agentite_task_queue_set_progress(Agentite_TaskQueue *queue, float progress) {
    AGENTITE_VALIDATE_PTR(queue);

    Agentite_Task *current = agentite_task_queue_current(queue);
    if (!current) {
        return;
    }

    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    current->progress = progress;
}

void agentite_task_queue_clear(Agentite_TaskQueue *queue) {
    AGENTITE_VALIDATE_PTR(queue);

    /* Cancel current task if in progress */
    Agentite_Task *current = agentite_task_queue_current(queue);
    if (current && current->status == AGENTITE_TASK_IN_PROGRESS) {
        current->status = AGENTITE_TASK_CANCELLED;
        notify_completion(queue, current);
    }

    queue->count = 0;
    queue->head = 0;
}

bool agentite_task_queue_remove(Agentite_TaskQueue *queue, int index) {
    AGENTITE_VALIDATE_PTR_RET(queue, false);

    if (index < 0 || index >= queue->count) {
        return false;
    }

    /* If removing current task, handle like cancel */
    if (index == 0) {
        agentite_task_queue_cancel(queue);
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

int agentite_task_queue_insert_front(Agentite_TaskQueue *queue,
                                    Agentite_TaskType type,
                                    const void *data, size_t size) {
    AGENTITE_VALIDATE_PTR_RET(queue, -1);

    if (queue->count >= queue->capacity) {
        agentite_set_error("agentite_task_queue_insert_front: queue is full");
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
    Agentite_Task *task = &queue->tasks[insert_pos];
    memset(task, 0, sizeof(Agentite_Task));

    task->type = type;
    task->status = AGENTITE_TASK_PENDING;
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

int agentite_task_queue_count(Agentite_TaskQueue *queue) {
    return queue ? queue->count : 0;
}

bool agentite_task_queue_is_empty(Agentite_TaskQueue *queue) {
    return !queue || queue->count == 0;
}

bool agentite_task_queue_is_full(Agentite_TaskQueue *queue) {
    return queue && queue->count >= queue->capacity;
}

bool agentite_task_queue_is_idle(Agentite_TaskQueue *queue) {
    if (!queue || queue->count == 0) {
        return true;
    }

    Agentite_Task *current = &queue->tasks[queue->head];
    return current->status == AGENTITE_TASK_COMPLETED ||
           current->status == AGENTITE_TASK_FAILED ||
           current->status == AGENTITE_TASK_CANCELLED;
}

int agentite_task_queue_capacity(Agentite_TaskQueue *queue) {
    return queue ? queue->capacity : 0;
}

/*============================================================================
 * Wait Task Helper
 *============================================================================*/

bool agentite_task_queue_update_wait(Agentite_TaskQueue *queue, float delta_time) {
    AGENTITE_VALIDATE_PTR_RET(queue, false);

    Agentite_Task *current = agentite_task_queue_current(queue);
    if (!current || current->type != AGENTITE_TASK_WAIT) {
        return false;
    }

    if (current->status != AGENTITE_TASK_IN_PROGRESS) {
        return false;
    }

    Agentite_TaskWait *wait = &current->data.wait;
    wait->elapsed += delta_time;

    if (wait->duration > 0.0f) {
        current->progress = wait->elapsed / wait->duration;
        if (current->progress > 1.0f) {
            current->progress = 1.0f;
        }

        if (wait->elapsed >= wait->duration) {
            agentite_task_queue_complete(queue);
            return false;
        }
    }

    return true;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void agentite_task_queue_set_callback(Agentite_TaskQueue *queue,
                                     Agentite_TaskCallback callback,
                                     void *userdata) {
    AGENTITE_VALIDATE_PTR(queue);
    queue->callback = callback;
    queue->callback_userdata = userdata;
}

/*============================================================================
 * Assignment
 *============================================================================*/

void agentite_task_queue_set_assigned_entity(Agentite_TaskQueue *queue, int32_t entity_id) {
    AGENTITE_VALIDATE_PTR(queue);
    queue->assigned_entity = entity_id;
}

int32_t agentite_task_queue_get_assigned_entity(Agentite_TaskQueue *queue) {
    return queue ? queue->assigned_entity : -1;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *agentite_task_type_name(Agentite_TaskType type) {
    switch (type) {
        case AGENTITE_TASK_NONE:      return "None";
        case AGENTITE_TASK_MOVE:      return "Move";
        case AGENTITE_TASK_EXPLORE:   return "Explore";
        case AGENTITE_TASK_COLLECT:   return "Collect";
        case AGENTITE_TASK_DEPOSIT:   return "Deposit";
        case AGENTITE_TASK_CRAFT:     return "Craft";
        case AGENTITE_TASK_BUILD:     return "Build";
        case AGENTITE_TASK_ATTACK:    return "Attack";
        case AGENTITE_TASK_DEFEND:    return "Defend";
        case AGENTITE_TASK_FOLLOW:    return "Follow";
        case AGENTITE_TASK_FLEE:      return "Flee";
        case AGENTITE_TASK_WAIT:      return "Wait";
        case AGENTITE_TASK_INTERACT:  return "Interact";
        case AGENTITE_TASK_PATROL:    return "Patrol";
        case AGENTITE_TASK_WITHDRAW:  return "Withdraw";
        case AGENTITE_TASK_MINE:      return "Mine";
        default:
            if (type >= AGENTITE_TASK_USER) {
                return "Custom";
            }
            return "Unknown";
    }
}

const char *agentite_task_status_name(Agentite_TaskStatus status) {
    switch (status) {
        case AGENTITE_TASK_PENDING:     return "Pending";
        case AGENTITE_TASK_IN_PROGRESS: return "In Progress";
        case AGENTITE_TASK_COMPLETED:   return "Completed";
        case AGENTITE_TASK_FAILED:      return "Failed";
        case AGENTITE_TASK_CANCELLED:   return "Cancelled";
        default:                      return "Unknown";
    }
}
