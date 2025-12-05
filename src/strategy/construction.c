/*
 * Carbon Game Engine - Construction Queue / Ghost Building System
 *
 * Planned buildings with progress tracking before actual construction.
 */

#include "carbon/construction.h"
#include "carbon/error.h"
#include "carbon/validate.h"

#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

typedef struct GhostSlot {
    Carbon_Ghost ghost;
    bool active;
} GhostSlot;

struct Carbon_ConstructionQueue {
    GhostSlot *slots;
    int capacity;
    int count;
    uint32_t next_id;

    /* Callbacks */
    Carbon_ConstructionCallback callback;
    void *callback_userdata;

    Carbon_ConstructionCondition condition;
    void *condition_userdata;
};

/*============================================================================
 * Helper Functions
 *============================================================================*/

static GhostSlot *find_slot(Carbon_ConstructionQueue *queue, uint32_t ghost_id) {
    if (!queue || ghost_id == CARBON_GHOST_INVALID) {
        return NULL;
    }

    for (int i = 0; i < queue->capacity; i++) {
        if (queue->slots[i].active && queue->slots[i].ghost.id == ghost_id) {
            return &queue->slots[i];
        }
    }

    return NULL;
}

static const GhostSlot *find_slot_const(const Carbon_ConstructionQueue *queue, uint32_t ghost_id) {
    return find_slot((Carbon_ConstructionQueue *)queue, ghost_id);
}

static void trigger_callback(Carbon_ConstructionQueue *queue, const Carbon_Ghost *ghost) {
    if (queue->callback) {
        queue->callback(queue, ghost, queue->callback_userdata);
    }
}

/*============================================================================
 * Queue Creation and Destruction
 *============================================================================*/

Carbon_ConstructionQueue *carbon_construction_create(int max_ghosts) {
    if (max_ghosts <= 0) {
        max_ghosts = 32;  /* Default capacity */
    }

    Carbon_ConstructionQueue *queue = calloc(1, sizeof(Carbon_ConstructionQueue));
    if (!queue) {
        carbon_set_error("Failed to allocate construction queue");
        return NULL;
    }

    queue->slots = calloc(max_ghosts, sizeof(GhostSlot));
    if (!queue->slots) {
        carbon_set_error("Failed to allocate ghost slots");
        free(queue);
        return NULL;
    }

    queue->capacity = max_ghosts;
    queue->next_id = 1;  /* Start at 1, 0 is invalid */

    return queue;
}

void carbon_construction_destroy(Carbon_ConstructionQueue *queue) {
    if (!queue) return;

    free(queue->slots);
    free(queue);
}

/*============================================================================
 * Ghost Management
 *============================================================================*/

uint32_t carbon_construction_add_ghost(
    Carbon_ConstructionQueue *queue,
    int x, int y,
    uint16_t building_type,
    uint8_t direction)
{
    return carbon_construction_add_ghost_ex(queue, x, y, building_type,
                                             direction, 10.0f, -1);
}

uint32_t carbon_construction_add_ghost_ex(
    Carbon_ConstructionQueue *queue,
    int x, int y,
    uint16_t building_type,
    uint8_t direction,
    float base_duration,
    int32_t faction_id)
{
    CARBON_VALIDATE_PTR_RET(queue, CARBON_GHOST_INVALID);

    if (queue->count >= queue->capacity) {
        carbon_set_error("Construction queue is full");
        return CARBON_GHOST_INVALID;
    }

    /* Find first empty slot */
    int slot_idx = -1;
    for (int i = 0; i < queue->capacity; i++) {
        if (!queue->slots[i].active) {
            slot_idx = i;
            break;
        }
    }

    if (slot_idx < 0) {
        carbon_set_error("No available slot in construction queue");
        return CARBON_GHOST_INVALID;
    }

    GhostSlot *slot = &queue->slots[slot_idx];
    Carbon_Ghost *ghost = &slot->ghost;

    /* Initialize ghost */
    memset(ghost, 0, sizeof(Carbon_Ghost));
    ghost->id = queue->next_id++;
    ghost->x = x;
    ghost->y = y;
    ghost->building_type = building_type;
    ghost->direction = direction & 3;
    ghost->status = CARBON_GHOST_PENDING;
    ghost->progress = 0.0f;
    ghost->base_duration = base_duration > 0.0f ? base_duration : 10.0f;
    ghost->speed_multiplier = 1.0f;
    ghost->faction_id = faction_id;
    ghost->builder_entity = -1;

    slot->active = true;
    queue->count++;

    return ghost->id;
}

bool carbon_construction_remove_ghost(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    slot->active = false;
    queue->count--;

    return true;
}

bool carbon_construction_cancel_ghost(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    slot->ghost.status = CARBON_GHOST_CANCELLED;
    trigger_callback(queue, &slot->ghost);

    return true;
}

Carbon_Ghost *carbon_construction_get_ghost(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, NULL);

    GhostSlot *slot = find_slot(queue, ghost);
    return slot ? &slot->ghost : NULL;
}

const Carbon_Ghost *carbon_construction_get_ghost_const(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, NULL);

    const GhostSlot *slot = find_slot_const(queue, ghost);
    return slot ? &slot->ghost : NULL;
}

uint32_t carbon_construction_find_at(
    const Carbon_ConstructionQueue *queue,
    int x, int y)
{
    CARBON_VALIDATE_PTR_RET(queue, CARBON_GHOST_INVALID);

    for (int i = 0; i < queue->capacity; i++) {
        if (queue->slots[i].active) {
            const Carbon_Ghost *g = &queue->slots[i].ghost;
            if (g->x == x && g->y == y) {
                return g->id;
            }
        }
    }

    return CARBON_GHOST_INVALID;
}

bool carbon_construction_has_ghost_at(
    const Carbon_ConstructionQueue *queue,
    int x, int y)
{
    return carbon_construction_find_at(queue, x, y) != CARBON_GHOST_INVALID;
}

/*============================================================================
 * Construction Progress
 *============================================================================*/

void carbon_construction_update(
    Carbon_ConstructionQueue *queue,
    float delta_time)
{
    CARBON_VALIDATE_PTR(queue);

    if (delta_time <= 0.0f) return;

    for (int i = 0; i < queue->capacity; i++) {
        if (!queue->slots[i].active) continue;

        Carbon_Ghost *ghost = &queue->slots[i].ghost;

        /* Only update ghosts that are actively constructing */
        if (ghost->status != CARBON_GHOST_CONSTRUCTING) continue;

        /* Check condition callback if set */
        if (queue->condition) {
            if (!queue->condition(queue, ghost, queue->condition_userdata)) {
                continue;  /* Condition not met, skip this ghost */
            }
        }

        /* Calculate progress increment */
        float progress_per_second = 1.0f / ghost->base_duration;
        float increment = progress_per_second * ghost->speed_multiplier * delta_time;

        ghost->progress += increment;

        /* Check for completion */
        if (ghost->progress >= 1.0f) {
            ghost->progress = 1.0f;
            ghost->status = CARBON_GHOST_COMPLETE;
            trigger_callback(queue, ghost);
        }
    }
}

bool carbon_construction_start(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    if (slot->ghost.status != CARBON_GHOST_PENDING) {
        return false;
    }

    slot->ghost.status = CARBON_GHOST_CONSTRUCTING;
    return true;
}

bool carbon_construction_pause(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    if (slot->ghost.status != CARBON_GHOST_CONSTRUCTING) {
        return false;
    }

    slot->ghost.status = CARBON_GHOST_PAUSED;
    return true;
}

bool carbon_construction_resume(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    if (slot->ghost.status != CARBON_GHOST_PAUSED) {
        return false;
    }

    slot->ghost.status = CARBON_GHOST_CONSTRUCTING;
    return true;
}

float carbon_construction_get_progress(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, -1.0f);

    const GhostSlot *slot = find_slot_const(queue, ghost);
    return slot ? slot->ghost.progress : -1.0f;
}

bool carbon_construction_set_progress(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    float progress)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    /* Clamp progress to 0.0 - 1.0 */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    slot->ghost.progress = progress;

    /* Check for completion */
    if (progress >= 1.0f && slot->ghost.status == CARBON_GHOST_CONSTRUCTING) {
        slot->ghost.status = CARBON_GHOST_COMPLETE;
        trigger_callback(queue, &slot->ghost);
    }

    return true;
}

bool carbon_construction_add_progress(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    float amount)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    /* Only add progress to constructing ghosts */
    if (slot->ghost.status != CARBON_GHOST_CONSTRUCTING) {
        return false;
    }

    slot->ghost.progress += amount;

    /* Check for completion */
    if (slot->ghost.progress >= 1.0f) {
        slot->ghost.progress = 1.0f;
        slot->ghost.status = CARBON_GHOST_COMPLETE;
        trigger_callback(queue, &slot->ghost);
    }

    return true;
}

bool carbon_construction_is_complete(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    const GhostSlot *slot = find_slot_const(queue, ghost);
    return slot && slot->ghost.status == CARBON_GHOST_COMPLETE;
}

bool carbon_construction_complete_instant(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    /* Can only complete pending or constructing ghosts */
    if (slot->ghost.status != CARBON_GHOST_PENDING &&
        slot->ghost.status != CARBON_GHOST_CONSTRUCTING) {
        return false;
    }

    slot->ghost.progress = 1.0f;
    slot->ghost.status = CARBON_GHOST_COMPLETE;
    trigger_callback(queue, &slot->ghost);

    return true;
}

/*============================================================================
 * Speed and Modifiers
 *============================================================================*/

bool carbon_construction_set_speed(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    float multiplier)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    slot->ghost.speed_multiplier = multiplier > 0.0f ? multiplier : 0.0f;
    return true;
}

float carbon_construction_get_speed(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, 0.0f);

    const GhostSlot *slot = find_slot_const(queue, ghost);
    return slot ? slot->ghost.speed_multiplier : 0.0f;
}

bool carbon_construction_set_duration(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    float duration)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    slot->ghost.base_duration = duration > 0.0f ? duration : 1.0f;
    return true;
}

float carbon_construction_get_remaining_time(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, -1.0f);

    const GhostSlot *slot = find_slot_const(queue, ghost);
    if (!slot) return -1.0f;

    const Carbon_Ghost *g = &slot->ghost;
    float remaining_progress = 1.0f - g->progress;
    float effective_speed = g->speed_multiplier > 0.0f ? g->speed_multiplier : 1.0f;

    return (remaining_progress * g->base_duration) / effective_speed;
}

/*============================================================================
 * Builder Assignment
 *============================================================================*/

bool carbon_construction_set_builder(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    int32_t builder_entity)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    slot->ghost.builder_entity = builder_entity;
    return true;
}

int32_t carbon_construction_get_builder(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, -1);

    const GhostSlot *slot = find_slot_const(queue, ghost);
    return slot ? slot->ghost.builder_entity : -1;
}

int carbon_construction_find_by_builder(
    const Carbon_ConstructionQueue *queue,
    int32_t builder_entity,
    uint32_t *out_handles,
    int max_handles)
{
    CARBON_VALIDATE_PTR_RET(queue, 0);
    CARBON_VALIDATE_PTR_RET(out_handles, 0);

    int count = 0;
    for (int i = 0; i < queue->capacity && count < max_handles; i++) {
        if (queue->slots[i].active &&
            queue->slots[i].ghost.builder_entity == builder_entity) {
            out_handles[count++] = queue->slots[i].ghost.id;
        }
    }

    return count;
}

/*============================================================================
 * Faction Queries
 *============================================================================*/

int carbon_construction_get_by_faction(
    const Carbon_ConstructionQueue *queue,
    int32_t faction_id,
    uint32_t *out_handles,
    int max_handles)
{
    CARBON_VALIDATE_PTR_RET(queue, 0);
    CARBON_VALIDATE_PTR_RET(out_handles, 0);

    int count = 0;
    for (int i = 0; i < queue->capacity && count < max_handles; i++) {
        if (queue->slots[i].active &&
            queue->slots[i].ghost.faction_id == faction_id) {
            out_handles[count++] = queue->slots[i].ghost.id;
        }
    }

    return count;
}

int carbon_construction_count_by_faction(
    const Carbon_ConstructionQueue *queue,
    int32_t faction_id)
{
    CARBON_VALIDATE_PTR_RET(queue, 0);

    int count = 0;
    for (int i = 0; i < queue->capacity; i++) {
        if (queue->slots[i].active &&
            queue->slots[i].ghost.faction_id == faction_id) {
            count++;
        }
    }

    return count;
}

int carbon_construction_count_active_by_faction(
    const Carbon_ConstructionQueue *queue,
    int32_t faction_id)
{
    CARBON_VALIDATE_PTR_RET(queue, 0);

    int count = 0;
    for (int i = 0; i < queue->capacity; i++) {
        if (queue->slots[i].active &&
            queue->slots[i].ghost.faction_id == faction_id &&
            queue->slots[i].ghost.status == CARBON_GHOST_CONSTRUCTING) {
            count++;
        }
    }

    return count;
}

/*============================================================================
 * Queue State
 *============================================================================*/

int carbon_construction_count(const Carbon_ConstructionQueue *queue) {
    CARBON_VALIDATE_PTR_RET(queue, 0);
    return queue->count;
}

int carbon_construction_count_active(const Carbon_ConstructionQueue *queue) {
    CARBON_VALIDATE_PTR_RET(queue, 0);

    int count = 0;
    for (int i = 0; i < queue->capacity; i++) {
        if (queue->slots[i].active &&
            queue->slots[i].ghost.status == CARBON_GHOST_CONSTRUCTING) {
            count++;
        }
    }

    return count;
}

int carbon_construction_count_complete(const Carbon_ConstructionQueue *queue) {
    CARBON_VALIDATE_PTR_RET(queue, 0);

    int count = 0;
    for (int i = 0; i < queue->capacity; i++) {
        if (queue->slots[i].active &&
            queue->slots[i].ghost.status == CARBON_GHOST_COMPLETE) {
            count++;
        }
    }

    return count;
}

bool carbon_construction_is_full(const Carbon_ConstructionQueue *queue) {
    CARBON_VALIDATE_PTR_RET(queue, true);
    return queue->count >= queue->capacity;
}

int carbon_construction_capacity(const Carbon_ConstructionQueue *queue) {
    CARBON_VALIDATE_PTR_RET(queue, 0);
    return queue->capacity;
}

int carbon_construction_get_all(
    const Carbon_ConstructionQueue *queue,
    uint32_t *out_handles,
    int max_handles)
{
    CARBON_VALIDATE_PTR_RET(queue, 0);
    CARBON_VALIDATE_PTR_RET(out_handles, 0);

    int count = 0;
    for (int i = 0; i < queue->capacity && count < max_handles; i++) {
        if (queue->slots[i].active) {
            out_handles[count++] = queue->slots[i].ghost.id;
        }
    }

    return count;
}

void carbon_construction_clear(Carbon_ConstructionQueue *queue) {
    CARBON_VALIDATE_PTR(queue);

    for (int i = 0; i < queue->capacity; i++) {
        queue->slots[i].active = false;
    }

    queue->count = 0;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void carbon_construction_set_callback(
    Carbon_ConstructionQueue *queue,
    Carbon_ConstructionCallback callback,
    void *userdata)
{
    CARBON_VALIDATE_PTR(queue);

    queue->callback = callback;
    queue->callback_userdata = userdata;
}

void carbon_construction_set_condition_callback(
    Carbon_ConstructionQueue *queue,
    Carbon_ConstructionCondition callback,
    void *userdata)
{
    CARBON_VALIDATE_PTR(queue);

    queue->condition = callback;
    queue->condition_userdata = userdata;
}

/*============================================================================
 * Metadata
 *============================================================================*/

bool carbon_construction_set_metadata(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    uint32_t metadata)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    slot->ghost.metadata = metadata;
    return true;
}

uint32_t carbon_construction_get_metadata(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, 0);

    const GhostSlot *slot = find_slot_const(queue, ghost);
    return slot ? slot->ghost.metadata : 0;
}

bool carbon_construction_set_userdata(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    void *userdata)
{
    CARBON_VALIDATE_PTR_RET(queue, false);

    GhostSlot *slot = find_slot(queue, ghost);
    if (!slot) return false;

    slot->ghost.userdata = userdata;
    return true;
}

void *carbon_construction_get_userdata(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost)
{
    CARBON_VALIDATE_PTR_RET(queue, NULL);

    const GhostSlot *slot = find_slot_const(queue, ghost);
    return slot ? slot->ghost.userdata : NULL;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *carbon_ghost_status_name(Carbon_GhostStatus status) {
    switch (status) {
        case CARBON_GHOST_PENDING:      return "Pending";
        case CARBON_GHOST_CONSTRUCTING: return "Constructing";
        case CARBON_GHOST_COMPLETE:     return "Complete";
        case CARBON_GHOST_CANCELLED:    return "Cancelled";
        case CARBON_GHOST_PAUSED:       return "Paused";
        default:                        return "Unknown";
    }
}
