/*
 * Carbon Game Engine - Crafting State Machine System
 *
 * Progress-based crafting with recipe definitions, batch support,
 * speed multipliers, and completion callbacks.
 */

#include "agentite/agentite.h"
#include "agentite/crafting.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

struct Agentite_RecipeRegistry {
    Agentite_RecipeDef *recipes;
    int capacity;
    int count;
};

struct Agentite_Crafter {
    Agentite_RecipeRegistry *registry;

    /* Job queue */
    Agentite_CraftJob queue[AGENTITE_CRAFTER_MAX_QUEUE];
    int queue_head;     /* Index of current job */
    int queue_tail;     /* Index to insert next job */
    int queue_count;

    /* Configuration */
    float speed_multiplier;
    int32_t station_type;
    int32_t entity_id;

    /* Callbacks */
    Agentite_CraftCallback callback;
    void *callback_userdata;

    Agentite_CraftResourceCheck resource_check;
    void *resource_check_userdata;

    Agentite_CraftResourceConsume resource_consume;
    void *resource_consume_userdata;

    Agentite_CraftResourceProduce resource_produce;
    void *resource_produce_userdata;

    /* Statistics */
    int total_crafted;
    float total_craft_time;
};

/*============================================================================
 * Recipe Registry
 *============================================================================*/

Agentite_RecipeRegistry *agentite_recipe_create(void) {
    Agentite_RecipeRegistry *registry = AGENTITE_ALLOC(Agentite_RecipeRegistry);
    if (!registry) {
        agentite_set_error("Failed to allocate recipe registry");
        return NULL;
    }

    registry->capacity = AGENTITE_RECIPE_MAX;
    registry->recipes = AGENTITE_ALLOC_ARRAY(Agentite_RecipeDef, registry->capacity);
    if (!registry->recipes) {
        agentite_set_error("Failed to allocate recipe storage");
        free(registry);
        return NULL;
    }

    return registry;
}

void agentite_recipe_destroy(Agentite_RecipeRegistry *registry) {
    if (!registry) return;

    free(registry->recipes);
    free(registry);
}

int agentite_recipe_register(Agentite_RecipeRegistry *registry, const Agentite_RecipeDef *def) {
    AGENTITE_VALIDATE_PTR_RET(registry, -1);
    AGENTITE_VALIDATE_PTR_RET(def, -1);

    if (registry->count >= registry->capacity) {
        agentite_set_error("Crafting: Recipe registry is full (%d/%d)", registry->count, registry->capacity);
        return -1;
    }

    /* Check for duplicate ID */
    for (int i = 0; i < registry->count; i++) {
        if (strcmp(registry->recipes[i].id, def->id) == 0) {
            agentite_set_error("Recipe with ID '%s' already exists", def->id);
            return -1;
        }
    }

    /* Copy recipe to registry */
    int index = registry->count;
    memcpy(&registry->recipes[index], def, sizeof(Agentite_RecipeDef));
    registry->count++;

    return index;
}

int agentite_recipe_count(const Agentite_RecipeRegistry *registry) {
    AGENTITE_VALIDATE_PTR_RET(registry, 0);
    return registry->count;
}

const Agentite_RecipeDef *agentite_recipe_get(const Agentite_RecipeRegistry *registry, int index) {
    AGENTITE_VALIDATE_PTR_RET(registry, NULL);

    if (index < 0 || index >= registry->count) {
        return NULL;
    }

    return &registry->recipes[index];
}

const Agentite_RecipeDef *agentite_recipe_find(const Agentite_RecipeRegistry *registry, const char *id) {
    AGENTITE_VALIDATE_PTR_RET(registry, NULL);
    AGENTITE_VALIDATE_STRING_RET(id, NULL);

    for (int i = 0; i < registry->count; i++) {
        if (strcmp(registry->recipes[i].id, id) == 0) {
            return &registry->recipes[i];
        }
    }

    return NULL;
}

int agentite_recipe_find_index(const Agentite_RecipeRegistry *registry, const char *id) {
    AGENTITE_VALIDATE_PTR_RET(registry, -1);
    AGENTITE_VALIDATE_STRING_RET(id, -1);

    for (int i = 0; i < registry->count; i++) {
        if (strcmp(registry->recipes[i].id, id) == 0) {
            return i;
        }
    }

    return -1;
}

int agentite_recipe_get_by_category(
    const Agentite_RecipeRegistry *registry,
    int32_t category,
    const Agentite_RecipeDef **out_defs,
    int max_count)
{
    AGENTITE_VALIDATE_PTR_RET(registry, 0);
    AGENTITE_VALIDATE_PTR_RET(out_defs, 0);

    int count = 0;
    for (int i = 0; i < registry->count && count < max_count; i++) {
        if (registry->recipes[i].category == category) {
            out_defs[count++] = &registry->recipes[i];
        }
    }

    return count;
}

int agentite_recipe_get_by_station(
    const Agentite_RecipeRegistry *registry,
    int32_t station_type,
    const Agentite_RecipeDef **out_defs,
    int max_count)
{
    AGENTITE_VALIDATE_PTR_RET(registry, 0);
    AGENTITE_VALIDATE_PTR_RET(out_defs, 0);

    int count = 0;
    for (int i = 0; i < registry->count && count < max_count; i++) {
        if (registry->recipes[i].required_station == station_type) {
            out_defs[count++] = &registry->recipes[i];
        }
    }

    return count;
}

bool agentite_recipe_set_unlocked(Agentite_RecipeRegistry *registry, const char *id, bool unlocked) {
    AGENTITE_VALIDATE_PTR_RET(registry, false);
    AGENTITE_VALIDATE_STRING_RET(id, false);

    for (int i = 0; i < registry->count; i++) {
        if (strcmp(registry->recipes[i].id, id) == 0) {
            registry->recipes[i].unlocked = unlocked;
            return true;
        }
    }

    return false;
}

bool agentite_recipe_is_unlocked(const Agentite_RecipeRegistry *registry, const char *id) {
    AGENTITE_VALIDATE_PTR_RET(registry, false);
    AGENTITE_VALIDATE_STRING_RET(id, false);

    const Agentite_RecipeDef *recipe = agentite_recipe_find(registry, id);
    return recipe ? recipe->unlocked : false;
}

/*============================================================================
 * Crafter - Creation and Destruction
 *============================================================================*/

Agentite_Crafter *agentite_crafter_create(Agentite_RecipeRegistry *registry) {
    AGENTITE_VALIDATE_PTR_RET(registry, NULL);

    Agentite_Crafter *crafter = AGENTITE_ALLOC(Agentite_Crafter);
    if (!crafter) {
        agentite_set_error("Failed to allocate crafter");
        return NULL;
    }

    crafter->registry = registry;
    crafter->speed_multiplier = 1.0f;
    crafter->station_type = -1;
    crafter->entity_id = -1;

    return crafter;
}

void agentite_crafter_destroy(Agentite_Crafter *crafter) {
    if (!crafter) return;
    free(crafter);
}

/*============================================================================
 * Crafter - Update
 *============================================================================*/

void agentite_crafter_update(Agentite_Crafter *crafter, float delta_time) {
    AGENTITE_VALIDATE_PTR(crafter);

    if (delta_time <= 0.0f) return;
    if (crafter->queue_count == 0) return;

    Agentite_CraftJob *job = &crafter->queue[crafter->queue_head];

    /* Only update if in progress */
    if (job->status != AGENTITE_CRAFT_IN_PROGRESS) return;

    /* Get the recipe */
    const Agentite_RecipeDef *recipe = agentite_recipe_get(crafter->registry, job->recipe_index);
    if (!recipe) {
        job->status = AGENTITE_CRAFT_FAILED;
        return;
    }

    /* Calculate progress increment */
    float base_time = recipe->craft_time > 0.0f ? recipe->craft_time : 1.0f;
    float effective_speed = crafter->speed_multiplier > 0.0f ? crafter->speed_multiplier : 1.0f;
    float progress_per_second = effective_speed / base_time;
    float increment = progress_per_second * delta_time;

    job->progress += increment;
    crafter->total_craft_time += delta_time;

    /* Check for item completion */
    while (job->progress >= 1.0f && job->completed < job->quantity) {
        job->progress -= 1.0f;
        job->completed++;

        /* Produce the item */
        if (crafter->resource_produce) {
            crafter->resource_produce(crafter, recipe, 1, crafter->resource_produce_userdata);
        }

        /* Trigger callback */
        if (crafter->callback) {
            crafter->callback(crafter, recipe, 1, crafter->callback_userdata);
        }

        crafter->total_crafted++;

        /* Check if batch is complete */
        if (job->completed >= job->quantity) {
            job->progress = 1.0f;
            job->status = AGENTITE_CRAFT_COMPLETE;
            break;
        }

        /* Check for resources for next item */
        if (crafter->resource_check) {
            if (!crafter->resource_check(crafter, recipe, crafter->resource_check_userdata)) {
                job->status = AGENTITE_CRAFT_FAILED;
                break;
            }
        }

        /* Consume resources for next item */
        if (crafter->resource_consume) {
            crafter->resource_consume(crafter, recipe, crafter->resource_consume_userdata);
        }
    }
}

/*============================================================================
 * Crafter - Crafting Operations
 *============================================================================*/

static bool start_job(Agentite_Crafter *crafter, int recipe_index, int quantity) {
    /* Check recipe exists */
    const Agentite_RecipeDef *recipe = agentite_recipe_get(crafter->registry, recipe_index);
    if (!recipe) {
        agentite_set_error("Recipe not found at index %d", recipe_index);
        return false;
    }

    /* Check if recipe is unlocked */
    if (!recipe->unlocked) {
        agentite_set_error("Recipe '%s' is not unlocked", recipe->id);
        return false;
    }

    /* Check station requirement */
    if (recipe->required_station >= 0 && recipe->required_station != crafter->station_type) {
        agentite_set_error("Recipe '%s' requires different station", recipe->id);
        return false;
    }

    /* Check resources if callback is set */
    if (crafter->resource_check) {
        if (!crafter->resource_check(crafter, recipe, crafter->resource_check_userdata)) {
            agentite_set_error("Insufficient resources for recipe '%s'", recipe->id);
            return false;
        }
    }

    /* Consume resources for first item */
    if (crafter->resource_consume) {
        crafter->resource_consume(crafter, recipe, crafter->resource_consume_userdata);
    }

    /* Start the job */
    Agentite_CraftJob *job = &crafter->queue[crafter->queue_head];
    job->recipe_index = recipe_index;
    job->quantity = quantity > 0 ? quantity : 1;
    job->completed = 0;
    job->progress = 0.0f;
    job->status = AGENTITE_CRAFT_IN_PROGRESS;

    if (crafter->queue_count == 0) {
        crafter->queue_count = 1;
    }

    return true;
}

bool agentite_crafter_start(Agentite_Crafter *crafter, const char *id, int quantity) {
    AGENTITE_VALIDATE_PTR_RET(crafter, false);
    AGENTITE_VALIDATE_STRING_RET(id, false);

    int index = agentite_recipe_find_index(crafter->registry, id);
    if (index < 0) {
        agentite_set_error("Recipe '%s' not found", id);
        return false;
    }

    return agentite_crafter_start_index(crafter, index, quantity);
}

bool agentite_crafter_start_index(Agentite_Crafter *crafter, int recipe_index, int quantity) {
    AGENTITE_VALIDATE_PTR_RET(crafter, false);

    /* If already crafting, fail */
    if (crafter->queue_count > 0) {
        Agentite_CraftJob *current = &crafter->queue[crafter->queue_head];
        if (current->status == AGENTITE_CRAFT_IN_PROGRESS ||
            current->status == AGENTITE_CRAFT_PAUSED) {
            agentite_set_error("Already crafting");
            return false;
        }
    }

    /* Reset queue */
    crafter->queue_head = 0;
    crafter->queue_tail = 0;
    crafter->queue_count = 0;

    return start_job(crafter, recipe_index, quantity);
}

bool agentite_crafter_queue(Agentite_Crafter *crafter, const char *id, int quantity) {
    AGENTITE_VALIDATE_PTR_RET(crafter, false);
    AGENTITE_VALIDATE_STRING_RET(id, false);

    if (crafter->queue_count >= AGENTITE_CRAFTER_MAX_QUEUE) {
        agentite_set_error("Crafting: Queue is full (%d/%d)", crafter->queue_count, AGENTITE_CRAFTER_MAX_QUEUE);
        return false;
    }

    int index = agentite_recipe_find_index(crafter->registry, id);
    if (index < 0) {
        agentite_set_error("Recipe '%s' not found", id);
        return false;
    }

    /* If nothing in queue, start immediately */
    if (crafter->queue_count == 0) {
        return start_job(crafter, index, quantity);
    }

    /* Add to queue */
    int queue_pos = (crafter->queue_head + crafter->queue_count) % AGENTITE_CRAFTER_MAX_QUEUE;
    Agentite_CraftJob *job = &crafter->queue[queue_pos];
    job->recipe_index = index;
    job->quantity = quantity > 0 ? quantity : 1;
    job->completed = 0;
    job->progress = 0.0f;
    job->status = AGENTITE_CRAFT_IDLE;

    crafter->queue_count++;

    return true;
}

void agentite_crafter_pause(Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR(crafter);

    if (crafter->queue_count == 0) return;

    Agentite_CraftJob *job = &crafter->queue[crafter->queue_head];
    if (job->status == AGENTITE_CRAFT_IN_PROGRESS) {
        job->status = AGENTITE_CRAFT_PAUSED;
    }
}

void agentite_crafter_resume(Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR(crafter);

    if (crafter->queue_count == 0) return;

    Agentite_CraftJob *job = &crafter->queue[crafter->queue_head];
    if (job->status == AGENTITE_CRAFT_PAUSED) {
        job->status = AGENTITE_CRAFT_IN_PROGRESS;
    }
}

bool agentite_crafter_cancel(Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, false);

    if (crafter->queue_count == 0) return false;

    /* Remove current job */
    crafter->queue_head = (crafter->queue_head + 1) % AGENTITE_CRAFTER_MAX_QUEUE;
    crafter->queue_count--;

    /* Start next job if available */
    if (crafter->queue_count > 0) {
        Agentite_CraftJob *next = &crafter->queue[crafter->queue_head];
        const Agentite_RecipeDef *recipe = agentite_recipe_get(crafter->registry, next->recipe_index);

        /* Check and consume resources */
        if (crafter->resource_check && recipe) {
            if (!crafter->resource_check(crafter, recipe, crafter->resource_check_userdata)) {
                next->status = AGENTITE_CRAFT_FAILED;
                return true;
            }
        }

        if (crafter->resource_consume && recipe) {
            crafter->resource_consume(crafter, recipe, crafter->resource_consume_userdata);
        }

        next->status = AGENTITE_CRAFT_IN_PROGRESS;
    }

    return true;
}

void agentite_crafter_cancel_all(Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR(crafter);

    crafter->queue_head = 0;
    crafter->queue_tail = 0;
    crafter->queue_count = 0;
}

int agentite_crafter_collect(Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, 0);

    if (crafter->queue_count == 0) return 0;

    Agentite_CraftJob *job = &crafter->queue[crafter->queue_head];
    if (job->status != AGENTITE_CRAFT_COMPLETE) return 0;

    int collected = job->completed;

    /* Advance to next job */
    crafter->queue_head = (crafter->queue_head + 1) % AGENTITE_CRAFTER_MAX_QUEUE;
    crafter->queue_count--;

    /* Start next job if available */
    if (crafter->queue_count > 0) {
        Agentite_CraftJob *next = &crafter->queue[crafter->queue_head];
        const Agentite_RecipeDef *recipe = agentite_recipe_get(crafter->registry, next->recipe_index);

        /* Check and consume resources */
        if (crafter->resource_check && recipe) {
            if (!crafter->resource_check(crafter, recipe, crafter->resource_check_userdata)) {
                next->status = AGENTITE_CRAFT_FAILED;
                return collected;
            }
        }

        if (crafter->resource_consume && recipe) {
            crafter->resource_consume(crafter, recipe, crafter->resource_consume_userdata);
        }

        next->status = AGENTITE_CRAFT_IN_PROGRESS;
    }

    return collected;
}

/*============================================================================
 * Crafter - Speed and Modifiers
 *============================================================================*/

void agentite_crafter_set_speed(Agentite_Crafter *crafter, float multiplier) {
    AGENTITE_VALIDATE_PTR(crafter);
    crafter->speed_multiplier = multiplier > 0.0f ? multiplier : 0.0f;
}

float agentite_crafter_get_speed(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, 1.0f);
    return crafter->speed_multiplier;
}

void agentite_crafter_set_station(Agentite_Crafter *crafter, int32_t station_type) {
    AGENTITE_VALIDATE_PTR(crafter);
    crafter->station_type = station_type;
}

int32_t agentite_crafter_get_station(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, -1);
    return crafter->station_type;
}

/*============================================================================
 * Crafter - State Queries
 *============================================================================*/

Agentite_CraftStatus agentite_crafter_get_status(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, AGENTITE_CRAFT_IDLE);

    if (crafter->queue_count == 0) {
        return AGENTITE_CRAFT_IDLE;
    }

    return crafter->queue[crafter->queue_head].status;
}

bool agentite_crafter_is_idle(const Agentite_Crafter *crafter) {
    return agentite_crafter_get_status(crafter) == AGENTITE_CRAFT_IDLE;
}

bool agentite_crafter_is_active(const Agentite_Crafter *crafter) {
    Agentite_CraftStatus status = agentite_crafter_get_status(crafter);
    return status == AGENTITE_CRAFT_IN_PROGRESS || status == AGENTITE_CRAFT_PAUSED;
}

bool agentite_crafter_is_complete(const Agentite_Crafter *crafter) {
    return agentite_crafter_get_status(crafter) == AGENTITE_CRAFT_COMPLETE;
}

float agentite_crafter_get_progress(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, 0.0f);

    if (crafter->queue_count == 0) {
        return 0.0f;
    }

    return crafter->queue[crafter->queue_head].progress;
}

float agentite_crafter_get_batch_progress(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, 0.0f);

    if (crafter->queue_count == 0) {
        return 0.0f;
    }

    const Agentite_CraftJob *job = &crafter->queue[crafter->queue_head];
    if (job->quantity <= 0) {
        return 0.0f;
    }

    float completed = (float)job->completed;
    float partial = job->progress;
    return (completed + partial) / (float)job->quantity;
}

const Agentite_CraftJob *agentite_crafter_get_current_job(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, NULL);

    if (crafter->queue_count == 0) {
        return NULL;
    }

    return &crafter->queue[crafter->queue_head];
}

const Agentite_RecipeDef *agentite_crafter_get_current_recipe(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, NULL);

    if (crafter->queue_count == 0) {
        return NULL;
    }

    const Agentite_CraftJob *job = &crafter->queue[crafter->queue_head];
    return agentite_recipe_get(crafter->registry, job->recipe_index);
}

float agentite_crafter_get_remaining_time(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, 0.0f);

    if (crafter->queue_count == 0) {
        return 0.0f;
    }

    const Agentite_CraftJob *job = &crafter->queue[crafter->queue_head];
    const Agentite_RecipeDef *recipe = agentite_recipe_get(crafter->registry, job->recipe_index);
    if (!recipe) {
        return 0.0f;
    }

    float remaining_progress = 1.0f - job->progress;
    float effective_speed = crafter->speed_multiplier > 0.0f ? crafter->speed_multiplier : 1.0f;
    float base_time = recipe->craft_time > 0.0f ? recipe->craft_time : 1.0f;

    return (remaining_progress * base_time) / effective_speed;
}

float agentite_crafter_get_total_remaining_time(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, 0.0f);

    if (crafter->queue_count == 0) {
        return 0.0f;
    }

    const Agentite_CraftJob *job = &crafter->queue[crafter->queue_head];
    const Agentite_RecipeDef *recipe = agentite_recipe_get(crafter->registry, job->recipe_index);
    if (!recipe) {
        return 0.0f;
    }

    float effective_speed = crafter->speed_multiplier > 0.0f ? crafter->speed_multiplier : 1.0f;
    float base_time = recipe->craft_time > 0.0f ? recipe->craft_time : 1.0f;
    float time_per_item = base_time / effective_speed;

    /* Current item remaining */
    float remaining = (1.0f - job->progress) * time_per_item;

    /* Remaining items in batch */
    int items_left = job->quantity - job->completed - 1;
    if (items_left > 0) {
        remaining += items_left * time_per_item;
    }

    return remaining;
}

/*============================================================================
 * Crafter - Queue Management
 *============================================================================*/

int agentite_crafter_get_queue_length(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, 0);
    return crafter->queue_count;
}

const Agentite_CraftJob *agentite_crafter_get_queued_job(const Agentite_Crafter *crafter, int index) {
    AGENTITE_VALIDATE_PTR_RET(crafter, NULL);

    if (index < 0 || index >= crafter->queue_count) {
        return NULL;
    }

    int actual_index = (crafter->queue_head + index) % AGENTITE_CRAFTER_MAX_QUEUE;
    return &crafter->queue[actual_index];
}

bool agentite_crafter_remove_queued(Agentite_Crafter *crafter, int index) {
    AGENTITE_VALIDATE_PTR_RET(crafter, false);

    /* Cannot remove current job this way */
    if (index <= 0 || index >= crafter->queue_count) {
        return false;
    }

    /* Shift remaining jobs */
    for (int i = index; i < crafter->queue_count - 1; i++) {
        int from_idx = (crafter->queue_head + i + 1) % AGENTITE_CRAFTER_MAX_QUEUE;
        int to_idx = (crafter->queue_head + i) % AGENTITE_CRAFTER_MAX_QUEUE;
        crafter->queue[to_idx] = crafter->queue[from_idx];
    }

    crafter->queue_count--;

    return true;
}

bool agentite_crafter_is_queue_full(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, true);
    return crafter->queue_count >= AGENTITE_CRAFTER_MAX_QUEUE;
}

void agentite_crafter_clear_queue(Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR(crafter);

    /* Keep only current job */
    if (crafter->queue_count > 1) {
        crafter->queue_count = 1;
    }
}

/*============================================================================
 * Crafter - Recipe Availability
 *============================================================================*/

bool agentite_crafter_can_craft(const Agentite_Crafter *crafter, const char *id) {
    AGENTITE_VALIDATE_PTR_RET(crafter, false);
    AGENTITE_VALIDATE_STRING_RET(id, false);

    const Agentite_RecipeDef *recipe = agentite_recipe_find(crafter->registry, id);
    if (!recipe) {
        return false;
    }

    /* Check unlock status */
    if (!recipe->unlocked) {
        return false;
    }

    /* Check station requirement */
    if (recipe->required_station >= 0 && recipe->required_station != crafter->station_type) {
        return false;
    }

    return true;
}

int agentite_crafter_get_available_recipes(
    const Agentite_Crafter *crafter,
    const Agentite_RecipeDef **out_defs,
    int max_count)
{
    AGENTITE_VALIDATE_PTR_RET(crafter, 0);
    AGENTITE_VALIDATE_PTR_RET(out_defs, 0);

    int count = 0;
    int recipe_count = agentite_recipe_count(crafter->registry);

    for (int i = 0; i < recipe_count && count < max_count; i++) {
        const Agentite_RecipeDef *recipe = agentite_recipe_get(crafter->registry, i);
        if (!recipe) continue;

        /* Check unlock status */
        if (!recipe->unlocked) continue;

        /* Check station requirement */
        if (recipe->required_station >= 0 && recipe->required_station != crafter->station_type) {
            continue;
        }

        out_defs[count++] = recipe;
    }

    return count;
}

/*============================================================================
 * Crafter - Callbacks
 *============================================================================*/

void agentite_crafter_set_callback(
    Agentite_Crafter *crafter,
    Agentite_CraftCallback callback,
    void *userdata)
{
    AGENTITE_VALIDATE_PTR(crafter);
    crafter->callback = callback;
    crafter->callback_userdata = userdata;
}

void agentite_crafter_set_resource_check(
    Agentite_Crafter *crafter,
    Agentite_CraftResourceCheck check,
    void *userdata)
{
    AGENTITE_VALIDATE_PTR(crafter);
    crafter->resource_check = check;
    crafter->resource_check_userdata = userdata;
}

void agentite_crafter_set_resource_consume(
    Agentite_Crafter *crafter,
    Agentite_CraftResourceConsume consume,
    void *userdata)
{
    AGENTITE_VALIDATE_PTR(crafter);
    crafter->resource_consume = consume;
    crafter->resource_consume_userdata = userdata;
}

void agentite_crafter_set_resource_produce(
    Agentite_Crafter *crafter,
    Agentite_CraftResourceProduce produce,
    void *userdata)
{
    AGENTITE_VALIDATE_PTR(crafter);
    crafter->resource_produce = produce;
    crafter->resource_produce_userdata = userdata;
}

/*============================================================================
 * Crafter - Entity Association
 *============================================================================*/

void agentite_crafter_set_entity(Agentite_Crafter *crafter, int32_t entity) {
    AGENTITE_VALIDATE_PTR(crafter);
    crafter->entity_id = entity;
}

int32_t agentite_crafter_get_entity(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, -1);
    return crafter->entity_id;
}

/*============================================================================
 * Crafter - Statistics
 *============================================================================*/

int agentite_crafter_get_total_crafted(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, 0);
    return crafter->total_crafted;
}

float agentite_crafter_get_total_craft_time(const Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR_RET(crafter, 0.0f);
    return crafter->total_craft_time;
}

void agentite_crafter_reset_stats(Agentite_Crafter *crafter) {
    AGENTITE_VALIDATE_PTR(crafter);
    crafter->total_crafted = 0;
    crafter->total_craft_time = 0.0f;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *agentite_craft_status_name(Agentite_CraftStatus status) {
    switch (status) {
        case AGENTITE_CRAFT_IDLE:         return "Idle";
        case AGENTITE_CRAFT_IN_PROGRESS:  return "In Progress";
        case AGENTITE_CRAFT_COMPLETE:     return "Complete";
        case AGENTITE_CRAFT_PAUSED:       return "Paused";
        case AGENTITE_CRAFT_FAILED:       return "Failed";
        default:                        return "Unknown";
    }
}

float agentite_craft_time_with_speed(float base_time, float multiplier) {
    if (multiplier <= 0.0f) return base_time;
    return base_time / multiplier;
}
