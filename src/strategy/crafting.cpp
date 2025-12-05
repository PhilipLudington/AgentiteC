/*
 * Carbon Game Engine - Crafting State Machine System
 *
 * Progress-based crafting with recipe definitions, batch support,
 * speed multipliers, and completion callbacks.
 */

#include "carbon/carbon.h"
#include "carbon/crafting.h"
#include "carbon/error.h"
#include "carbon/validate.h"

#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

struct Carbon_RecipeRegistry {
    Carbon_RecipeDef *recipes;
    int capacity;
    int count;
};

struct Carbon_Crafter {
    Carbon_RecipeRegistry *registry;

    /* Job queue */
    Carbon_CraftJob queue[CARBON_CRAFTER_MAX_QUEUE];
    int queue_head;     /* Index of current job */
    int queue_tail;     /* Index to insert next job */
    int queue_count;

    /* Configuration */
    float speed_multiplier;
    int32_t station_type;
    int32_t entity_id;

    /* Callbacks */
    Carbon_CraftCallback callback;
    void *callback_userdata;

    Carbon_CraftResourceCheck resource_check;
    void *resource_check_userdata;

    Carbon_CraftResourceConsume resource_consume;
    void *resource_consume_userdata;

    Carbon_CraftResourceProduce resource_produce;
    void *resource_produce_userdata;

    /* Statistics */
    int total_crafted;
    float total_craft_time;
};

/*============================================================================
 * Recipe Registry
 *============================================================================*/

Carbon_RecipeRegistry *carbon_recipe_create(void) {
    Carbon_RecipeRegistry *registry = CARBON_ALLOC(Carbon_RecipeRegistry);
    if (!registry) {
        carbon_set_error("Failed to allocate recipe registry");
        return NULL;
    }

    registry->capacity = CARBON_RECIPE_MAX;
    registry->recipes = CARBON_ALLOC_ARRAY(Carbon_RecipeDef, registry->capacity);
    if (!registry->recipes) {
        carbon_set_error("Failed to allocate recipe storage");
        free(registry);
        return NULL;
    }

    return registry;
}

void carbon_recipe_destroy(Carbon_RecipeRegistry *registry) {
    if (!registry) return;

    free(registry->recipes);
    free(registry);
}

int carbon_recipe_register(Carbon_RecipeRegistry *registry, const Carbon_RecipeDef *def) {
    CARBON_VALIDATE_PTR_RET(registry, -1);
    CARBON_VALIDATE_PTR_RET(def, -1);

    if (registry->count >= registry->capacity) {
        carbon_set_error("Recipe registry is full");
        return -1;
    }

    /* Check for duplicate ID */
    for (int i = 0; i < registry->count; i++) {
        if (strcmp(registry->recipes[i].id, def->id) == 0) {
            carbon_set_error("Recipe with ID '%s' already exists", def->id);
            return -1;
        }
    }

    /* Copy recipe to registry */
    int index = registry->count;
    memcpy(&registry->recipes[index], def, sizeof(Carbon_RecipeDef));
    registry->count++;

    return index;
}

int carbon_recipe_count(const Carbon_RecipeRegistry *registry) {
    CARBON_VALIDATE_PTR_RET(registry, 0);
    return registry->count;
}

const Carbon_RecipeDef *carbon_recipe_get(const Carbon_RecipeRegistry *registry, int index) {
    CARBON_VALIDATE_PTR_RET(registry, NULL);

    if (index < 0 || index >= registry->count) {
        return NULL;
    }

    return &registry->recipes[index];
}

const Carbon_RecipeDef *carbon_recipe_find(const Carbon_RecipeRegistry *registry, const char *id) {
    CARBON_VALIDATE_PTR_RET(registry, NULL);
    CARBON_VALIDATE_STRING_RET(id, NULL);

    for (int i = 0; i < registry->count; i++) {
        if (strcmp(registry->recipes[i].id, id) == 0) {
            return &registry->recipes[i];
        }
    }

    return NULL;
}

int carbon_recipe_find_index(const Carbon_RecipeRegistry *registry, const char *id) {
    CARBON_VALIDATE_PTR_RET(registry, -1);
    CARBON_VALIDATE_STRING_RET(id, -1);

    for (int i = 0; i < registry->count; i++) {
        if (strcmp(registry->recipes[i].id, id) == 0) {
            return i;
        }
    }

    return -1;
}

int carbon_recipe_get_by_category(
    const Carbon_RecipeRegistry *registry,
    int32_t category,
    const Carbon_RecipeDef **out_defs,
    int max_count)
{
    CARBON_VALIDATE_PTR_RET(registry, 0);
    CARBON_VALIDATE_PTR_RET(out_defs, 0);

    int count = 0;
    for (int i = 0; i < registry->count && count < max_count; i++) {
        if (registry->recipes[i].category == category) {
            out_defs[count++] = &registry->recipes[i];
        }
    }

    return count;
}

int carbon_recipe_get_by_station(
    const Carbon_RecipeRegistry *registry,
    int32_t station_type,
    const Carbon_RecipeDef **out_defs,
    int max_count)
{
    CARBON_VALIDATE_PTR_RET(registry, 0);
    CARBON_VALIDATE_PTR_RET(out_defs, 0);

    int count = 0;
    for (int i = 0; i < registry->count && count < max_count; i++) {
        if (registry->recipes[i].required_station == station_type) {
            out_defs[count++] = &registry->recipes[i];
        }
    }

    return count;
}

bool carbon_recipe_set_unlocked(Carbon_RecipeRegistry *registry, const char *id, bool unlocked) {
    CARBON_VALIDATE_PTR_RET(registry, false);
    CARBON_VALIDATE_STRING_RET(id, false);

    for (int i = 0; i < registry->count; i++) {
        if (strcmp(registry->recipes[i].id, id) == 0) {
            registry->recipes[i].unlocked = unlocked;
            return true;
        }
    }

    return false;
}

bool carbon_recipe_is_unlocked(const Carbon_RecipeRegistry *registry, const char *id) {
    CARBON_VALIDATE_PTR_RET(registry, false);
    CARBON_VALIDATE_STRING_RET(id, false);

    const Carbon_RecipeDef *recipe = carbon_recipe_find(registry, id);
    return recipe ? recipe->unlocked : false;
}

/*============================================================================
 * Crafter - Creation and Destruction
 *============================================================================*/

Carbon_Crafter *carbon_crafter_create(Carbon_RecipeRegistry *registry) {
    CARBON_VALIDATE_PTR_RET(registry, NULL);

    Carbon_Crafter *crafter = CARBON_ALLOC(Carbon_Crafter);
    if (!crafter) {
        carbon_set_error("Failed to allocate crafter");
        return NULL;
    }

    crafter->registry = registry;
    crafter->speed_multiplier = 1.0f;
    crafter->station_type = -1;
    crafter->entity_id = -1;

    return crafter;
}

void carbon_crafter_destroy(Carbon_Crafter *crafter) {
    if (!crafter) return;
    free(crafter);
}

/*============================================================================
 * Crafter - Update
 *============================================================================*/

void carbon_crafter_update(Carbon_Crafter *crafter, float delta_time) {
    CARBON_VALIDATE_PTR(crafter);

    if (delta_time <= 0.0f) return;
    if (crafter->queue_count == 0) return;

    Carbon_CraftJob *job = &crafter->queue[crafter->queue_head];

    /* Only update if in progress */
    if (job->status != CARBON_CRAFT_IN_PROGRESS) return;

    /* Get the recipe */
    const Carbon_RecipeDef *recipe = carbon_recipe_get(crafter->registry, job->recipe_index);
    if (!recipe) {
        job->status = CARBON_CRAFT_FAILED;
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
            job->status = CARBON_CRAFT_COMPLETE;
            break;
        }

        /* Check for resources for next item */
        if (crafter->resource_check) {
            if (!crafter->resource_check(crafter, recipe, crafter->resource_check_userdata)) {
                job->status = CARBON_CRAFT_FAILED;
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

static bool start_job(Carbon_Crafter *crafter, int recipe_index, int quantity) {
    /* Check recipe exists */
    const Carbon_RecipeDef *recipe = carbon_recipe_get(crafter->registry, recipe_index);
    if (!recipe) {
        carbon_set_error("Recipe not found at index %d", recipe_index);
        return false;
    }

    /* Check if recipe is unlocked */
    if (!recipe->unlocked) {
        carbon_set_error("Recipe '%s' is not unlocked", recipe->id);
        return false;
    }

    /* Check station requirement */
    if (recipe->required_station >= 0 && recipe->required_station != crafter->station_type) {
        carbon_set_error("Recipe '%s' requires different station", recipe->id);
        return false;
    }

    /* Check resources if callback is set */
    if (crafter->resource_check) {
        if (!crafter->resource_check(crafter, recipe, crafter->resource_check_userdata)) {
            carbon_set_error("Insufficient resources for recipe '%s'", recipe->id);
            return false;
        }
    }

    /* Consume resources for first item */
    if (crafter->resource_consume) {
        crafter->resource_consume(crafter, recipe, crafter->resource_consume_userdata);
    }

    /* Start the job */
    Carbon_CraftJob *job = &crafter->queue[crafter->queue_head];
    job->recipe_index = recipe_index;
    job->quantity = quantity > 0 ? quantity : 1;
    job->completed = 0;
    job->progress = 0.0f;
    job->status = CARBON_CRAFT_IN_PROGRESS;

    if (crafter->queue_count == 0) {
        crafter->queue_count = 1;
    }

    return true;
}

bool carbon_crafter_start(Carbon_Crafter *crafter, const char *id, int quantity) {
    CARBON_VALIDATE_PTR_RET(crafter, false);
    CARBON_VALIDATE_STRING_RET(id, false);

    int index = carbon_recipe_find_index(crafter->registry, id);
    if (index < 0) {
        carbon_set_error("Recipe '%s' not found", id);
        return false;
    }

    return carbon_crafter_start_index(crafter, index, quantity);
}

bool carbon_crafter_start_index(Carbon_Crafter *crafter, int recipe_index, int quantity) {
    CARBON_VALIDATE_PTR_RET(crafter, false);

    /* If already crafting, fail */
    if (crafter->queue_count > 0) {
        Carbon_CraftJob *current = &crafter->queue[crafter->queue_head];
        if (current->status == CARBON_CRAFT_IN_PROGRESS ||
            current->status == CARBON_CRAFT_PAUSED) {
            carbon_set_error("Already crafting");
            return false;
        }
    }

    /* Reset queue */
    crafter->queue_head = 0;
    crafter->queue_tail = 0;
    crafter->queue_count = 0;

    return start_job(crafter, recipe_index, quantity);
}

bool carbon_crafter_queue(Carbon_Crafter *crafter, const char *id, int quantity) {
    CARBON_VALIDATE_PTR_RET(crafter, false);
    CARBON_VALIDATE_STRING_RET(id, false);

    if (crafter->queue_count >= CARBON_CRAFTER_MAX_QUEUE) {
        carbon_set_error("Craft queue is full");
        return false;
    }

    int index = carbon_recipe_find_index(crafter->registry, id);
    if (index < 0) {
        carbon_set_error("Recipe '%s' not found", id);
        return false;
    }

    /* If nothing in queue, start immediately */
    if (crafter->queue_count == 0) {
        return start_job(crafter, index, quantity);
    }

    /* Add to queue */
    int queue_pos = (crafter->queue_head + crafter->queue_count) % CARBON_CRAFTER_MAX_QUEUE;
    Carbon_CraftJob *job = &crafter->queue[queue_pos];
    job->recipe_index = index;
    job->quantity = quantity > 0 ? quantity : 1;
    job->completed = 0;
    job->progress = 0.0f;
    job->status = CARBON_CRAFT_IDLE;

    crafter->queue_count++;

    return true;
}

void carbon_crafter_pause(Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR(crafter);

    if (crafter->queue_count == 0) return;

    Carbon_CraftJob *job = &crafter->queue[crafter->queue_head];
    if (job->status == CARBON_CRAFT_IN_PROGRESS) {
        job->status = CARBON_CRAFT_PAUSED;
    }
}

void carbon_crafter_resume(Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR(crafter);

    if (crafter->queue_count == 0) return;

    Carbon_CraftJob *job = &crafter->queue[crafter->queue_head];
    if (job->status == CARBON_CRAFT_PAUSED) {
        job->status = CARBON_CRAFT_IN_PROGRESS;
    }
}

bool carbon_crafter_cancel(Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, false);

    if (crafter->queue_count == 0) return false;

    /* Remove current job */
    crafter->queue_head = (crafter->queue_head + 1) % CARBON_CRAFTER_MAX_QUEUE;
    crafter->queue_count--;

    /* Start next job if available */
    if (crafter->queue_count > 0) {
        Carbon_CraftJob *next = &crafter->queue[crafter->queue_head];
        const Carbon_RecipeDef *recipe = carbon_recipe_get(crafter->registry, next->recipe_index);

        /* Check and consume resources */
        if (crafter->resource_check && recipe) {
            if (!crafter->resource_check(crafter, recipe, crafter->resource_check_userdata)) {
                next->status = CARBON_CRAFT_FAILED;
                return true;
            }
        }

        if (crafter->resource_consume && recipe) {
            crafter->resource_consume(crafter, recipe, crafter->resource_consume_userdata);
        }

        next->status = CARBON_CRAFT_IN_PROGRESS;
    }

    return true;
}

void carbon_crafter_cancel_all(Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR(crafter);

    crafter->queue_head = 0;
    crafter->queue_tail = 0;
    crafter->queue_count = 0;
}

int carbon_crafter_collect(Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, 0);

    if (crafter->queue_count == 0) return 0;

    Carbon_CraftJob *job = &crafter->queue[crafter->queue_head];
    if (job->status != CARBON_CRAFT_COMPLETE) return 0;

    int collected = job->completed;

    /* Advance to next job */
    crafter->queue_head = (crafter->queue_head + 1) % CARBON_CRAFTER_MAX_QUEUE;
    crafter->queue_count--;

    /* Start next job if available */
    if (crafter->queue_count > 0) {
        Carbon_CraftJob *next = &crafter->queue[crafter->queue_head];
        const Carbon_RecipeDef *recipe = carbon_recipe_get(crafter->registry, next->recipe_index);

        /* Check and consume resources */
        if (crafter->resource_check && recipe) {
            if (!crafter->resource_check(crafter, recipe, crafter->resource_check_userdata)) {
                next->status = CARBON_CRAFT_FAILED;
                return collected;
            }
        }

        if (crafter->resource_consume && recipe) {
            crafter->resource_consume(crafter, recipe, crafter->resource_consume_userdata);
        }

        next->status = CARBON_CRAFT_IN_PROGRESS;
    }

    return collected;
}

/*============================================================================
 * Crafter - Speed and Modifiers
 *============================================================================*/

void carbon_crafter_set_speed(Carbon_Crafter *crafter, float multiplier) {
    CARBON_VALIDATE_PTR(crafter);
    crafter->speed_multiplier = multiplier > 0.0f ? multiplier : 0.0f;
}

float carbon_crafter_get_speed(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, 1.0f);
    return crafter->speed_multiplier;
}

void carbon_crafter_set_station(Carbon_Crafter *crafter, int32_t station_type) {
    CARBON_VALIDATE_PTR(crafter);
    crafter->station_type = station_type;
}

int32_t carbon_crafter_get_station(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, -1);
    return crafter->station_type;
}

/*============================================================================
 * Crafter - State Queries
 *============================================================================*/

Carbon_CraftStatus carbon_crafter_get_status(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, CARBON_CRAFT_IDLE);

    if (crafter->queue_count == 0) {
        return CARBON_CRAFT_IDLE;
    }

    return crafter->queue[crafter->queue_head].status;
}

bool carbon_crafter_is_idle(const Carbon_Crafter *crafter) {
    return carbon_crafter_get_status(crafter) == CARBON_CRAFT_IDLE;
}

bool carbon_crafter_is_active(const Carbon_Crafter *crafter) {
    Carbon_CraftStatus status = carbon_crafter_get_status(crafter);
    return status == CARBON_CRAFT_IN_PROGRESS || status == CARBON_CRAFT_PAUSED;
}

bool carbon_crafter_is_complete(const Carbon_Crafter *crafter) {
    return carbon_crafter_get_status(crafter) == CARBON_CRAFT_COMPLETE;
}

float carbon_crafter_get_progress(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, 0.0f);

    if (crafter->queue_count == 0) {
        return 0.0f;
    }

    return crafter->queue[crafter->queue_head].progress;
}

float carbon_crafter_get_batch_progress(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, 0.0f);

    if (crafter->queue_count == 0) {
        return 0.0f;
    }

    const Carbon_CraftJob *job = &crafter->queue[crafter->queue_head];
    if (job->quantity <= 0) {
        return 0.0f;
    }

    float completed = (float)job->completed;
    float partial = job->progress;
    return (completed + partial) / (float)job->quantity;
}

const Carbon_CraftJob *carbon_crafter_get_current_job(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, NULL);

    if (crafter->queue_count == 0) {
        return NULL;
    }

    return &crafter->queue[crafter->queue_head];
}

const Carbon_RecipeDef *carbon_crafter_get_current_recipe(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, NULL);

    if (crafter->queue_count == 0) {
        return NULL;
    }

    const Carbon_CraftJob *job = &crafter->queue[crafter->queue_head];
    return carbon_recipe_get(crafter->registry, job->recipe_index);
}

float carbon_crafter_get_remaining_time(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, 0.0f);

    if (crafter->queue_count == 0) {
        return 0.0f;
    }

    const Carbon_CraftJob *job = &crafter->queue[crafter->queue_head];
    const Carbon_RecipeDef *recipe = carbon_recipe_get(crafter->registry, job->recipe_index);
    if (!recipe) {
        return 0.0f;
    }

    float remaining_progress = 1.0f - job->progress;
    float effective_speed = crafter->speed_multiplier > 0.0f ? crafter->speed_multiplier : 1.0f;
    float base_time = recipe->craft_time > 0.0f ? recipe->craft_time : 1.0f;

    return (remaining_progress * base_time) / effective_speed;
}

float carbon_crafter_get_total_remaining_time(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, 0.0f);

    if (crafter->queue_count == 0) {
        return 0.0f;
    }

    const Carbon_CraftJob *job = &crafter->queue[crafter->queue_head];
    const Carbon_RecipeDef *recipe = carbon_recipe_get(crafter->registry, job->recipe_index);
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

int carbon_crafter_get_queue_length(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, 0);
    return crafter->queue_count;
}

const Carbon_CraftJob *carbon_crafter_get_queued_job(const Carbon_Crafter *crafter, int index) {
    CARBON_VALIDATE_PTR_RET(crafter, NULL);

    if (index < 0 || index >= crafter->queue_count) {
        return NULL;
    }

    int actual_index = (crafter->queue_head + index) % CARBON_CRAFTER_MAX_QUEUE;
    return &crafter->queue[actual_index];
}

bool carbon_crafter_remove_queued(Carbon_Crafter *crafter, int index) {
    CARBON_VALIDATE_PTR_RET(crafter, false);

    /* Cannot remove current job this way */
    if (index <= 0 || index >= crafter->queue_count) {
        return false;
    }

    /* Shift remaining jobs */
    for (int i = index; i < crafter->queue_count - 1; i++) {
        int from_idx = (crafter->queue_head + i + 1) % CARBON_CRAFTER_MAX_QUEUE;
        int to_idx = (crafter->queue_head + i) % CARBON_CRAFTER_MAX_QUEUE;
        crafter->queue[to_idx] = crafter->queue[from_idx];
    }

    crafter->queue_count--;

    return true;
}

bool carbon_crafter_is_queue_full(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, true);
    return crafter->queue_count >= CARBON_CRAFTER_MAX_QUEUE;
}

void carbon_crafter_clear_queue(Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR(crafter);

    /* Keep only current job */
    if (crafter->queue_count > 1) {
        crafter->queue_count = 1;
    }
}

/*============================================================================
 * Crafter - Recipe Availability
 *============================================================================*/

bool carbon_crafter_can_craft(const Carbon_Crafter *crafter, const char *id) {
    CARBON_VALIDATE_PTR_RET(crafter, false);
    CARBON_VALIDATE_STRING_RET(id, false);

    const Carbon_RecipeDef *recipe = carbon_recipe_find(crafter->registry, id);
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

int carbon_crafter_get_available_recipes(
    const Carbon_Crafter *crafter,
    const Carbon_RecipeDef **out_defs,
    int max_count)
{
    CARBON_VALIDATE_PTR_RET(crafter, 0);
    CARBON_VALIDATE_PTR_RET(out_defs, 0);

    int count = 0;
    int recipe_count = carbon_recipe_count(crafter->registry);

    for (int i = 0; i < recipe_count && count < max_count; i++) {
        const Carbon_RecipeDef *recipe = carbon_recipe_get(crafter->registry, i);
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

void carbon_crafter_set_callback(
    Carbon_Crafter *crafter,
    Carbon_CraftCallback callback,
    void *userdata)
{
    CARBON_VALIDATE_PTR(crafter);
    crafter->callback = callback;
    crafter->callback_userdata = userdata;
}

void carbon_crafter_set_resource_check(
    Carbon_Crafter *crafter,
    Carbon_CraftResourceCheck check,
    void *userdata)
{
    CARBON_VALIDATE_PTR(crafter);
    crafter->resource_check = check;
    crafter->resource_check_userdata = userdata;
}

void carbon_crafter_set_resource_consume(
    Carbon_Crafter *crafter,
    Carbon_CraftResourceConsume consume,
    void *userdata)
{
    CARBON_VALIDATE_PTR(crafter);
    crafter->resource_consume = consume;
    crafter->resource_consume_userdata = userdata;
}

void carbon_crafter_set_resource_produce(
    Carbon_Crafter *crafter,
    Carbon_CraftResourceProduce produce,
    void *userdata)
{
    CARBON_VALIDATE_PTR(crafter);
    crafter->resource_produce = produce;
    crafter->resource_produce_userdata = userdata;
}

/*============================================================================
 * Crafter - Entity Association
 *============================================================================*/

void carbon_crafter_set_entity(Carbon_Crafter *crafter, int32_t entity) {
    CARBON_VALIDATE_PTR(crafter);
    crafter->entity_id = entity;
}

int32_t carbon_crafter_get_entity(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, -1);
    return crafter->entity_id;
}

/*============================================================================
 * Crafter - Statistics
 *============================================================================*/

int carbon_crafter_get_total_crafted(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, 0);
    return crafter->total_crafted;
}

float carbon_crafter_get_total_craft_time(const Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR_RET(crafter, 0.0f);
    return crafter->total_craft_time;
}

void carbon_crafter_reset_stats(Carbon_Crafter *crafter) {
    CARBON_VALIDATE_PTR(crafter);
    crafter->total_crafted = 0;
    crafter->total_craft_time = 0.0f;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *carbon_craft_status_name(Carbon_CraftStatus status) {
    switch (status) {
        case CARBON_CRAFT_IDLE:         return "Idle";
        case CARBON_CRAFT_IN_PROGRESS:  return "In Progress";
        case CARBON_CRAFT_COMPLETE:     return "Complete";
        case CARBON_CRAFT_PAUSED:       return "Paused";
        case CARBON_CRAFT_FAILED:       return "Failed";
        default:                        return "Unknown";
    }
}

float carbon_craft_time_with_speed(float base_time, float multiplier) {
    if (multiplier <= 0.0f) return base_time;
    return base_time / multiplier;
}
