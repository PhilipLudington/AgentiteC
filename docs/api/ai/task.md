# Task Queue System

Sequential task execution for autonomous AI agents.

## Quick Start

```c
#include "carbon/task.h"

// Create task queue (capacity = max pending tasks)
Carbon_TaskQueue *queue = carbon_task_queue_create(16);
carbon_task_queue_set_assigned_entity(queue, worker_entity);

// Queue tasks
carbon_task_queue_add_move(queue, target_x, target_y);
carbon_task_queue_add_collect(queue, resource_x, resource_y, RESOURCE_WOOD);
carbon_task_queue_add_deposit(queue, storage_x, storage_y, -1);
```

## Task Types

| Function | Description |
|----------|-------------|
| `add_move` | Move to position |
| `add_move_ex` | Move with run option |
| `add_collect` | Collect resource |
| `add_deposit` | Deposit carried items |
| `add_withdraw` | Withdraw from storage |
| `add_mine` | Mine resource node |
| `add_craft` | Craft item |
| `add_build` | Construct building |
| `add_attack` | Attack target |
| `add_defend` | Defend position |
| `add_follow` | Follow entity |
| `add_flee` | Flee from danger |
| `add_wait` | Wait for duration |
| `add_interact` | Interact with object |
| `add_patrol` | Patrol waypoints |
| `add_explore` | Explore area |

## Processing Tasks

```c
Carbon_Task *current = carbon_task_queue_current(queue);
if (current) {
    if (current->status == CARBON_TASK_PENDING) {
        carbon_task_queue_start(queue);
    }

    if (current->status == CARBON_TASK_IN_PROGRESS) {
        switch (current->type) {
            case CARBON_TASK_MOVE:
                if (reached_destination()) {
                    carbon_task_queue_complete(queue);
                }
                break;
            case CARBON_TASK_WAIT:
                carbon_task_queue_update_wait(queue, delta_time);
                break;
        }
    }
}
```

## Completion Callback

```c
void on_task_done(Carbon_TaskQueue *queue, const Carbon_Task *task, void *userdata) {
    if (task->status == CARBON_TASK_COMPLETED) {
        printf("Completed %s\n", carbon_task_type_name(task->type));
    } else if (task->status == CARBON_TASK_FAILED) {
        printf("Failed: %s\n", task->fail_reason);
    }
}
carbon_task_queue_set_callback(queue, on_task_done, agent);
```

## Queue Management

```c
// Insert urgent task at front
carbon_task_queue_insert_front(queue, CARBON_TASK_MOVE, &move_data, sizeof(move_data));

// Cancel and clear
carbon_task_queue_cancel(queue);  // Cancel current
carbon_task_queue_clear(queue);   // Clear all

// Query state
int count = carbon_task_queue_count(queue);
bool idle = carbon_task_queue_is_idle(queue);
```

## Task Statuses

`PENDING`, `IN_PROGRESS`, `COMPLETED`, `FAILED`, `CANCELLED`

## Common Patterns

```c
// Worker gathering loop
carbon_task_queue_add_move(queue, resource_x, resource_y);
carbon_task_queue_add_collect(queue, resource_x, resource_y, RESOURCE_WOOD);
carbon_task_queue_add_move(queue, storage_x, storage_y);
carbon_task_queue_add_deposit(queue, storage_x, storage_y, -1);
// Re-queue on completion callback to loop

// Guard patrol
int waypoints[][2] = {{10, 10}, {30, 10}, {30, 30}, {10, 30}};
carbon_task_queue_add_patrol(queue, waypoints, 4, true);
```
