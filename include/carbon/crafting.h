/**
 * Carbon Crafting State Machine System
 *
 * Progress-based crafting with recipe definitions, batch support,
 * speed multipliers, and completion callbacks.
 *
 * Usage:
 *   // Create recipe registry
 *   Carbon_RecipeRegistry *recipes = carbon_recipe_create();
 *
 *   // Define a recipe
 *   Carbon_RecipeDef recipe = {
 *       .id = "iron_sword",
 *       .name = "Iron Sword",
 *       .craft_time = 5.0f,
 *       .input_count = 2,
 *       .output_count = 1,
 *   };
 *   recipe.inputs[0] = (Carbon_RecipeItem){ .item_type = ITEM_IRON, .quantity = 3 };
 *   recipe.inputs[1] = (Carbon_RecipeItem){ .item_type = ITEM_WOOD, .quantity = 1 };
 *   recipe.outputs[0] = (Carbon_RecipeItem){ .item_type = ITEM_IRON_SWORD, .quantity = 1 };
 *   carbon_recipe_register(recipes, &recipe);
 *
 *   // Create a crafter (per-entity or per-building)
 *   Carbon_Crafter *crafter = carbon_crafter_create(recipes);
 *
 *   // Start crafting
 *   carbon_crafter_start(crafter, "iron_sword", 2);  // Craft 2 swords
 *
 *   // In game loop:
 *   carbon_crafter_update(crafter, delta_time);
 *
 *   // Check for completion
 *   if (carbon_crafter_is_complete(crafter)) {
 *       // Get outputs, give to player
 *       carbon_crafter_collect(crafter);
 *   }
 *
 *   // Cleanup
 *   carbon_crafter_destroy(crafter);
 *   carbon_recipe_destroy(recipes);
 */

#ifndef CARBON_CRAFTING_H
#define CARBON_CRAFTING_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_RECIPE_MAX             256   /* Maximum recipes in registry */
#define CARBON_RECIPE_MAX_INPUTS      8     /* Maximum input items per recipe */
#define CARBON_RECIPE_MAX_OUTPUTS     4     /* Maximum output items per recipe */
#define CARBON_CRAFTER_MAX_QUEUE      16    /* Maximum queued crafts per crafter */
#define CARBON_RECIPE_INVALID         UINT32_MAX  /* Invalid recipe handle */

/*============================================================================
 * Types
 *============================================================================*/

/**
 * Crafting job status.
 */
typedef enum Carbon_CraftStatus {
    CARBON_CRAFT_IDLE = 0,          /* Not crafting */
    CARBON_CRAFT_IN_PROGRESS,       /* Crafting in progress */
    CARBON_CRAFT_COMPLETE,          /* Craft complete, awaiting collection */
    CARBON_CRAFT_PAUSED,            /* Crafting paused */
    CARBON_CRAFT_FAILED,            /* Crafting failed (missing resources) */
} Carbon_CraftStatus;

/**
 * An item in a recipe (input or output).
 */
typedef struct Carbon_RecipeItem {
    int32_t item_type;              /* Game-defined item type ID */
    int32_t quantity;               /* Amount required/produced */
    uint32_t metadata;              /* Game-defined extra data */
} Carbon_RecipeItem;

/**
 * Recipe definition (static data).
 */
typedef struct Carbon_RecipeDef {
    /* Identity */
    char id[64];                    /* Unique identifier */
    char name[128];                 /* Display name */
    char description[256];          /* Description text */

    /* Categorization */
    int32_t category;               /* Recipe category (game-defined) */
    int32_t tier;                   /* Recipe tier/level */
    int32_t required_station;       /* Required crafting station type (-1 = none) */

    /* Timing */
    float craft_time;               /* Base craft time in seconds */

    /* Inputs (consumed) */
    Carbon_RecipeItem inputs[CARBON_RECIPE_MAX_INPUTS];
    int input_count;

    /* Outputs (produced) */
    Carbon_RecipeItem outputs[CARBON_RECIPE_MAX_OUTPUTS];
    int output_count;

    /* Requirements */
    char required_tech[64];         /* Required technology ID (empty = none) */
    int32_t required_level;         /* Required crafting level (0 = none) */

    /* Flags */
    bool unlocked;                  /* Available by default */
    bool hidden;                    /* Hidden until unlocked */
} Carbon_RecipeDef;

/**
 * A single crafting job in the queue.
 */
typedef struct Carbon_CraftJob {
    uint32_t recipe_index;          /* Recipe being crafted */
    int32_t quantity;               /* Total items to craft */
    int32_t completed;              /* Items completed */
    float progress;                 /* Current item progress (0.0 to 1.0) */
    Carbon_CraftStatus status;      /* Current status */
} Carbon_CraftJob;

/**
 * Forward declarations.
 */
typedef struct Carbon_RecipeRegistry Carbon_RecipeRegistry;
typedef struct Carbon_Crafter Carbon_Crafter;

/**
 * Callback when a single item is crafted.
 *
 * @param crafter   The crafter
 * @param recipe    Recipe that was crafted
 * @param quantity  Number of items just completed (usually 1)
 * @param userdata  User data passed to set_callback
 */
typedef void (*Carbon_CraftCallback)(
    Carbon_Crafter *crafter,
    const Carbon_RecipeDef *recipe,
    int quantity,
    void *userdata
);

/**
 * Callback to check if resources are available.
 *
 * @param crafter   The crafter
 * @param recipe    Recipe to check
 * @param userdata  User data passed to set_resource_callback
 * @return true if all inputs are available
 */
typedef bool (*Carbon_CraftResourceCheck)(
    Carbon_Crafter *crafter,
    const Carbon_RecipeDef *recipe,
    void *userdata
);

/**
 * Callback to consume resources when crafting starts.
 *
 * @param crafter   The crafter
 * @param recipe    Recipe being crafted
 * @param userdata  User data passed to set_resource_callback
 */
typedef void (*Carbon_CraftResourceConsume)(
    Carbon_Crafter *crafter,
    const Carbon_RecipeDef *recipe,
    void *userdata
);

/**
 * Callback to produce items when crafting completes.
 *
 * @param crafter   The crafter
 * @param recipe    Recipe that completed
 * @param quantity  Number of items produced
 * @param userdata  User data passed to set_resource_callback
 */
typedef void (*Carbon_CraftResourceProduce)(
    Carbon_Crafter *crafter,
    const Carbon_RecipeDef *recipe,
    int quantity,
    void *userdata
);

/*============================================================================
 * Recipe Registry
 *============================================================================*/

/**
 * Create a new recipe registry.
 *
 * @return New registry or NULL on failure
 */
Carbon_RecipeRegistry *carbon_recipe_create(void);

/**
 * Destroy a recipe registry and free resources.
 *
 * @param registry Registry to destroy (safe if NULL)
 */
void carbon_recipe_destroy(Carbon_RecipeRegistry *registry);

/**
 * Register a recipe definition.
 *
 * @param registry Recipe registry
 * @param def      Recipe definition (copied)
 * @return Recipe index (0+) or -1 on failure
 */
int carbon_recipe_register(Carbon_RecipeRegistry *registry, const Carbon_RecipeDef *def);

/**
 * Get the number of registered recipes.
 *
 * @param registry Recipe registry
 * @return Number of recipes
 */
int carbon_recipe_count(const Carbon_RecipeRegistry *registry);

/**
 * Get a recipe by index.
 *
 * @param registry Recipe registry
 * @param index    Recipe index
 * @return Recipe definition or NULL
 */
const Carbon_RecipeDef *carbon_recipe_get(const Carbon_RecipeRegistry *registry, int index);

/**
 * Find a recipe by ID.
 *
 * @param registry Recipe registry
 * @param id       Recipe ID
 * @return Recipe definition or NULL if not found
 */
const Carbon_RecipeDef *carbon_recipe_find(const Carbon_RecipeRegistry *registry, const char *id);

/**
 * Get the index of a recipe by ID.
 *
 * @param registry Recipe registry
 * @param id       Recipe ID
 * @return Recipe index or -1 if not found
 */
int carbon_recipe_find_index(const Carbon_RecipeRegistry *registry, const char *id);

/**
 * Get recipes by category.
 *
 * @param registry  Recipe registry
 * @param category  Category ID (game-defined)
 * @param out_defs  Output array for recipe definitions
 * @param max_count Maximum number to return
 * @return Number of recipes in category
 */
int carbon_recipe_get_by_category(
    const Carbon_RecipeRegistry *registry,
    int32_t category,
    const Carbon_RecipeDef **out_defs,
    int max_count
);

/**
 * Get recipes by required station.
 *
 * @param registry     Recipe registry
 * @param station_type Station type ID (-1 for no station required)
 * @param out_defs     Output array for recipe definitions
 * @param max_count    Maximum number to return
 * @return Number of matching recipes
 */
int carbon_recipe_get_by_station(
    const Carbon_RecipeRegistry *registry,
    int32_t station_type,
    const Carbon_RecipeDef **out_defs,
    int max_count
);

/**
 * Set a recipe's unlocked state.
 *
 * @param registry Recipe registry
 * @param id       Recipe ID
 * @param unlocked true to unlock, false to lock
 * @return true if recipe found and updated
 */
bool carbon_recipe_set_unlocked(Carbon_RecipeRegistry *registry, const char *id, bool unlocked);

/**
 * Check if a recipe is unlocked.
 *
 * @param registry Recipe registry
 * @param id       Recipe ID
 * @return true if unlocked
 */
bool carbon_recipe_is_unlocked(const Carbon_RecipeRegistry *registry, const char *id);

/*============================================================================
 * Crafter
 *============================================================================*/

/**
 * Create a new crafter.
 *
 * @param registry Recipe registry to use
 * @return New crafter or NULL on failure
 */
Carbon_Crafter *carbon_crafter_create(Carbon_RecipeRegistry *registry);

/**
 * Destroy a crafter and free resources.
 *
 * @param crafter Crafter to destroy (safe if NULL)
 */
void carbon_crafter_destroy(Carbon_Crafter *crafter);

/**
 * Update the crafter (advance crafting progress).
 *
 * @param crafter    Crafter to update
 * @param delta_time Time elapsed this frame
 */
void carbon_crafter_update(Carbon_Crafter *crafter, float delta_time);

/*============================================================================
 * Crafting Operations
 *============================================================================*/

/**
 * Start crafting a recipe.
 *
 * @param crafter  Crafter
 * @param id       Recipe ID
 * @param quantity Number of items to craft
 * @return true if crafting started
 */
bool carbon_crafter_start(Carbon_Crafter *crafter, const char *id, int quantity);

/**
 * Start crafting a recipe by index.
 *
 * @param crafter      Crafter
 * @param recipe_index Recipe index
 * @param quantity     Number of items to craft
 * @return true if crafting started
 */
bool carbon_crafter_start_index(Carbon_Crafter *crafter, int recipe_index, int quantity);

/**
 * Queue a recipe to craft after current job completes.
 *
 * @param crafter  Crafter
 * @param id       Recipe ID
 * @param quantity Number of items to craft
 * @return true if queued successfully
 */
bool carbon_crafter_queue(Carbon_Crafter *crafter, const char *id, int quantity);

/**
 * Pause crafting.
 *
 * @param crafter Crafter to pause
 */
void carbon_crafter_pause(Carbon_Crafter *crafter);

/**
 * Resume crafting.
 *
 * @param crafter Crafter to resume
 */
void carbon_crafter_resume(Carbon_Crafter *crafter);

/**
 * Cancel the current crafting job.
 *
 * @param crafter Crafter
 * @return true if there was a job to cancel
 */
bool carbon_crafter_cancel(Carbon_Crafter *crafter);

/**
 * Cancel all crafting jobs (current and queued).
 *
 * @param crafter Crafter
 */
void carbon_crafter_cancel_all(Carbon_Crafter *crafter);

/**
 * Collect completed items (marks job as collected).
 * This transitions from COMPLETE back to processing queue or IDLE.
 *
 * @param crafter Crafter
 * @return Number of items collected
 */
int carbon_crafter_collect(Carbon_Crafter *crafter);

/*============================================================================
 * Speed and Modifiers
 *============================================================================*/

/**
 * Set crafting speed multiplier.
 *
 * @param crafter    Crafter
 * @param multiplier Speed multiplier (1.0 = normal, 2.0 = twice as fast)
 */
void carbon_crafter_set_speed(Carbon_Crafter *crafter, float multiplier);

/**
 * Get crafting speed multiplier.
 *
 * @param crafter Crafter
 * @return Speed multiplier
 */
float carbon_crafter_get_speed(const Carbon_Crafter *crafter);

/**
 * Set the crafting station type this crafter represents.
 * This affects which recipes can be crafted.
 *
 * @param crafter      Crafter
 * @param station_type Station type ID (-1 for no station)
 */
void carbon_crafter_set_station(Carbon_Crafter *crafter, int32_t station_type);

/**
 * Get the crafting station type.
 *
 * @param crafter Crafter
 * @return Station type ID
 */
int32_t carbon_crafter_get_station(const Carbon_Crafter *crafter);

/*============================================================================
 * State Queries
 *============================================================================*/

/**
 * Get current crafting status.
 *
 * @param crafter Crafter
 * @return Current status
 */
Carbon_CraftStatus carbon_crafter_get_status(const Carbon_Crafter *crafter);

/**
 * Check if crafter is idle (not crafting anything).
 *
 * @param crafter Crafter
 * @return true if idle
 */
bool carbon_crafter_is_idle(const Carbon_Crafter *crafter);

/**
 * Check if crafting is in progress.
 *
 * @param crafter Crafter
 * @return true if crafting
 */
bool carbon_crafter_is_active(const Carbon_Crafter *crafter);

/**
 * Check if current job is complete and awaiting collection.
 *
 * @param crafter Crafter
 * @return true if complete
 */
bool carbon_crafter_is_complete(const Carbon_Crafter *crafter);

/**
 * Get current crafting progress (0.0 to 1.0) for current item.
 *
 * @param crafter Crafter
 * @return Progress (0.0 if idle)
 */
float carbon_crafter_get_progress(const Carbon_Crafter *crafter);

/**
 * Get overall batch progress (completed / total items).
 *
 * @param crafter Crafter
 * @return Batch progress (0.0 to 1.0)
 */
float carbon_crafter_get_batch_progress(const Carbon_Crafter *crafter);

/**
 * Get the current job.
 *
 * @param crafter Crafter
 * @return Current job or NULL if idle
 */
const Carbon_CraftJob *carbon_crafter_get_current_job(const Carbon_Crafter *crafter);

/**
 * Get the recipe being crafted.
 *
 * @param crafter Crafter
 * @return Recipe or NULL if idle
 */
const Carbon_RecipeDef *carbon_crafter_get_current_recipe(const Carbon_Crafter *crafter);

/**
 * Get remaining time for current item.
 *
 * @param crafter Crafter
 * @return Remaining time in seconds (0.0 if idle)
 */
float carbon_crafter_get_remaining_time(const Carbon_Crafter *crafter);

/**
 * Get total remaining time for all items in current job.
 *
 * @param crafter Crafter
 * @return Total remaining time in seconds
 */
float carbon_crafter_get_total_remaining_time(const Carbon_Crafter *crafter);

/*============================================================================
 * Queue Management
 *============================================================================*/

/**
 * Get number of items in queue (including current job).
 *
 * @param crafter Crafter
 * @return Queue length
 */
int carbon_crafter_get_queue_length(const Carbon_Crafter *crafter);

/**
 * Get a queued job by index.
 *
 * @param crafter Crafter
 * @param index   Queue index (0 = current)
 * @return Job or NULL if index out of range
 */
const Carbon_CraftJob *carbon_crafter_get_queued_job(const Carbon_Crafter *crafter, int index);

/**
 * Remove a queued job by index.
 *
 * @param crafter Crafter
 * @param index   Queue index (0 = current job, cannot be removed this way)
 * @return true if removed
 */
bool carbon_crafter_remove_queued(Carbon_Crafter *crafter, int index);

/**
 * Check if queue is full.
 *
 * @param crafter Crafter
 * @return true if queue is full
 */
bool carbon_crafter_is_queue_full(const Carbon_Crafter *crafter);

/**
 * Clear the queue (keeps current job).
 *
 * @param crafter Crafter
 */
void carbon_crafter_clear_queue(Carbon_Crafter *crafter);

/*============================================================================
 * Recipe Availability
 *============================================================================*/

/**
 * Check if a recipe can be crafted by this crafter.
 * Considers station type, unlock status, but NOT resource availability.
 *
 * @param crafter Crafter
 * @param id      Recipe ID
 * @return true if recipe can be crafted
 */
bool carbon_crafter_can_craft(const Carbon_Crafter *crafter, const char *id);

/**
 * Get all recipes available to this crafter.
 *
 * @param crafter   Crafter
 * @param out_defs  Output array for recipe definitions
 * @param max_count Maximum number to return
 * @return Number of available recipes
 */
int carbon_crafter_get_available_recipes(
    const Carbon_Crafter *crafter,
    const Carbon_RecipeDef **out_defs,
    int max_count
);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * Set callback for when items are crafted.
 *
 * @param crafter  Crafter
 * @param callback Callback function (NULL to clear)
 * @param userdata User data for callback
 */
void carbon_crafter_set_callback(
    Carbon_Crafter *crafter,
    Carbon_CraftCallback callback,
    void *userdata
);

/**
 * Set resource check callback.
 * If set, called before consuming resources.
 *
 * @param crafter  Crafter
 * @param check    Check function (NULL to clear)
 * @param userdata User data for callback
 */
void carbon_crafter_set_resource_check(
    Carbon_Crafter *crafter,
    Carbon_CraftResourceCheck check,
    void *userdata
);

/**
 * Set resource consume callback.
 * Called when crafting starts to deduct inputs.
 *
 * @param crafter  Crafter
 * @param consume  Consume function (NULL to clear)
 * @param userdata User data for callback
 */
void carbon_crafter_set_resource_consume(
    Carbon_Crafter *crafter,
    Carbon_CraftResourceConsume consume,
    void *userdata
);

/**
 * Set resource produce callback.
 * Called when crafting completes to add outputs.
 *
 * @param crafter  Crafter
 * @param produce  Produce function (NULL to clear)
 * @param userdata User data for callback
 */
void carbon_crafter_set_resource_produce(
    Carbon_Crafter *crafter,
    Carbon_CraftResourceProduce produce,
    void *userdata
);

/*============================================================================
 * Entity Association
 *============================================================================*/

/**
 * Set the entity this crafter is associated with.
 *
 * @param crafter Crafter
 * @param entity  Entity ID (-1 for none)
 */
void carbon_crafter_set_entity(Carbon_Crafter *crafter, int32_t entity);

/**
 * Get the associated entity.
 *
 * @param crafter Crafter
 * @return Entity ID or -1 if none
 */
int32_t carbon_crafter_get_entity(const Carbon_Crafter *crafter);

/*============================================================================
 * Statistics
 *============================================================================*/

/**
 * Get total items crafted by this crafter.
 *
 * @param crafter Crafter
 * @return Total items crafted
 */
int carbon_crafter_get_total_crafted(const Carbon_Crafter *crafter);

/**
 * Get total time spent crafting.
 *
 * @param crafter Crafter
 * @return Total craft time in seconds
 */
float carbon_crafter_get_total_craft_time(const Carbon_Crafter *crafter);

/**
 * Reset statistics.
 *
 * @param crafter Crafter
 */
void carbon_crafter_reset_stats(Carbon_Crafter *crafter);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get human-readable name for a craft status.
 *
 * @param status Craft status
 * @return Static string name
 */
const char *carbon_craft_status_name(Carbon_CraftStatus status);

/**
 * Calculate craft time with speed modifier.
 *
 * @param base_time  Base craft time
 * @param multiplier Speed multiplier
 * @return Actual craft time
 */
float carbon_craft_time_with_speed(float base_time, float multiplier);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_CRAFTING_H */
