# Task Queue System

Sequential task execution for autonomous AI agents.

## Quick Start

```c
#include "agentite/task.h"

// Create task queue (capacity = max pending tasks)
Agentite_TaskQueue *queue = agentite_task_queue_create(16);
agentite_task_queue_set_assigned_entity(queue, worker_entity);

// Queue tasks
agentite_task_queue_add_move(queue, target_x, target_y);
agentite_task_queue_add_collect(queue, resource_x, resource_y, RESOURCE_WOOD);
agentite_task_queue_add_deposit(queue, storage_x, storage_y, -1);
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
Agentite_Task *current = agentite_task_queue_current(queue);
if (current) {
    if (current->status == AGENTITE_TASK_PENDING) {
        agentite_task_queue_start(queue);
    }

    if (current->status == AGENTITE_TASK_IN_PROGRESS) {
        switch (current->type) {
            case AGENTITE_TASK_MOVE:
                if (reached_destination()) {
                    agentite_task_queue_complete(queue);
                }
                break;
            case AGENTITE_TASK_WAIT:
                agentite_task_queue_update_wait(queue, delta_time);
                break;
        }
    }
}
```

## Completion Callback

```c
void on_task_done(Agentite_TaskQueue *queue, const Agentite_Task *task, void *userdata) {
    if (task->status == AGENTITE_TASK_COMPLETED) {
        printf("Completed %s\n", agentite_task_type_name(task->type));
    } else if (task->status == AGENTITE_TASK_FAILED) {
        printf("Failed: %s\n", task->fail_reason);
    }
}
agentite_task_queue_set_callback(queue, on_task_done, agent);
```

## Queue Management

```c
// Insert urgent task at front
agentite_task_queue_insert_front(queue, AGENTITE_TASK_MOVE, &move_data, sizeof(move_data));

// Cancel and clear
agentite_task_queue_cancel(queue);  // Cancel current
agentite_task_queue_clear(queue);   // Clear all

// Query state
int count = agentite_task_queue_count(queue);
bool idle = agentite_task_queue_is_idle(queue);
```

## Task Statuses

`PENDING`, `IN_PROGRESS`, `COMPLETED`, `FAILED`, `CANCELLED`

## Common Patterns

```c
// Worker gathering loop
agentite_task_queue_add_move(queue, resource_x, resource_y);
agentite_task_queue_add_collect(queue, resource_x, resource_y, RESOURCE_WOOD);
agentite_task_queue_add_move(queue, storage_x, storage_y);
agentite_task_queue_add_deposit(queue, storage_x, storage_y, -1);
// Re-queue on completion callback to loop

// Guard patrol
int waypoints[][2] = {{10, 10}, {30, 10}, {30, 30}, {10, 30}};
agentite_task_queue_add_patrol(queue, waypoints, 4, true);
```
