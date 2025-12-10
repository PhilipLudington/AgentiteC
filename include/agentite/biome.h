/**
 * Carbon Biome System
 *
 * Terrain types affecting resource distribution and visuals.
 * Supports biome properties, resource spawn weights, and tilemap integration.
 *
 * Usage:
 *   // Create biome system
 *   Agentite_BiomeSystem *biomes = agentite_biome_create();
 *
 *   // Register biomes
 *   Agentite_BiomeDef forest = {
 *       .id = "forest",
 *       .name = "Forest",
 *       .color = 0xFF228B22,  // Forest green
 *       .movement_cost = 1.5f,
 *       .resource_multiplier = 1.2f,
 *   };
 *   agentite_biome_register(biomes, &forest);
 *
 *   // Set resource weights per biome
 *   agentite_biome_set_resource_weight(biomes, "forest", RESOURCE_WOOD, 2.0f);
 *   agentite_biome_set_resource_weight(biomes, "forest", RESOURCE_IRON, 0.3f);
 *
 *   // Query best biome for a resource
 *   int best = agentite_biome_get_best_for_resource(biomes, RESOURCE_WOOD);
 *
 *   // Cleanup
 *   agentite_biome_destroy(biomes);
 */

#ifndef AGENTITE_BIOME_H
#define AGENTITE_BIOME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_BIOME_MAX             64    /* Maximum biomes */
#define AGENTITE_BIOME_MAX_RESOURCES   32    /* Maximum resource types tracked */
#define AGENTITE_BIOME_INVALID         (-1)  /* Invalid biome ID */

/*============================================================================
 * Types
 *============================================================================*/

/**
 * Biome flags for special properties.
 */
typedef enum Agentite_BiomeFlags {
    AGENTITE_BIOME_FLAG_NONE       = 0,
    AGENTITE_BIOME_FLAG_PASSABLE   = (1 << 0),  /* Can be traversed */
    AGENTITE_BIOME_FLAG_BUILDABLE  = (1 << 1),  /* Can build structures */
    AGENTITE_BIOME_FLAG_FARMABLE   = (1 << 2),  /* Can grow crops */
    AGENTITE_BIOME_FLAG_WATER      = (1 << 3),  /* Is water (for naval units) */
    AGENTITE_BIOME_FLAG_HAZARDOUS  = (1 << 4),  /* Causes damage over time */
} Agentite_BiomeFlags;

/**
 * Biome definition (static data).
 */
typedef struct Agentite_BiomeDef {
    /* Identity */
    char id[64];                    /* Unique identifier */
    char name[128];                 /* Display name */
    char description[256];          /* Description text */

    /* Visuals */
    uint32_t color;                 /* Primary color (ABGR format) */
    uint32_t color_variant;         /* Secondary color for variation */
    int32_t base_tile;              /* Base tile ID for tilemap */
    int32_t tile_variants;          /* Number of tile variants */

    /* Gameplay */
    float movement_cost;            /* Movement speed multiplier (1.0 = normal, 2.0 = half speed) */
    float resource_multiplier;      /* Global resource yield multiplier */
    float visibility_modifier;      /* Vision range modifier */
    float defense_bonus;            /* Defense bonus for units in this biome */

    /* Resource spawn weights (per resource type) */
    float resource_weights[AGENTITE_BIOME_MAX_RESOURCES];

    /* Flags */
    uint32_t flags;                 /* Combination of Agentite_BiomeFlags */

    /* Temperature/climate (for weather/seasons) */
    float base_temperature;         /* Base temperature (-1.0 cold to 1.0 hot) */
    float humidity;                 /* Humidity level (0.0 dry to 1.0 wet) */

    /* Transition */
    int32_t transition_priority;    /* For blending edges (higher = on top) */

    /* User data */
    void *userdata;
} Agentite_BiomeDef;

/**
 * Forward declaration.
 */
typedef struct Agentite_BiomeSystem Agentite_BiomeSystem;

/**
 * Callback for biome-related events.
 */
typedef void (*Agentite_BiomeCallback)(
    Agentite_BiomeSystem *system,
    int biome_id,
    void *userdata
);

/*============================================================================
 * System Creation and Destruction
 *============================================================================*/

/**
 * Create a new biome system.
 *
 * @return New system or NULL on failure
 */
Agentite_BiomeSystem *agentite_biome_create(void);

/**
 * Destroy a biome system and free resources.
 *
 * @param system System to destroy (safe if NULL)
 */
void agentite_biome_destroy(Agentite_BiomeSystem *system);

/*============================================================================
 * Biome Registration
 *============================================================================*/

/**
 * Register a biome definition.
 *
 * @param system Biome system
 * @param def    Biome definition (copied)
 * @return Biome ID (0+) or AGENTITE_BIOME_INVALID on failure
 */
int agentite_biome_register(Agentite_BiomeSystem *system, const Agentite_BiomeDef *def);

/**
 * Get the number of registered biomes.
 *
 * @param system Biome system
 * @return Number of biomes
 */
int agentite_biome_count(const Agentite_BiomeSystem *system);

/**
 * Get a biome by ID.
 *
 * @param system   Biome system
 * @param biome_id Biome ID
 * @return Biome definition or NULL
 */
const Agentite_BiomeDef *agentite_biome_get(const Agentite_BiomeSystem *system, int biome_id);

/**
 * Find a biome by string ID.
 *
 * @param system Biome system
 * @param id     Biome string ID
 * @return Biome definition or NULL if not found
 */
const Agentite_BiomeDef *agentite_biome_find(const Agentite_BiomeSystem *system, const char *id);

/**
 * Get the index of a biome by string ID.
 *
 * @param system Biome system
 * @param id     Biome string ID
 * @return Biome ID or AGENTITE_BIOME_INVALID if not found
 */
int agentite_biome_find_index(const Agentite_BiomeSystem *system, const char *id);

/*============================================================================
 * Resource Weights
 *============================================================================*/

/**
 * Set resource spawn weight for a biome.
 * Higher weight = more likely to spawn this resource.
 *
 * @param system        Biome system
 * @param biome_id      Biome ID
 * @param resource_type Resource type (game-defined)
 * @param weight        Spawn weight (0.0 = never, 1.0 = normal, 2.0 = double)
 * @return true if set
 */
bool agentite_biome_set_resource_weight(
    Agentite_BiomeSystem *system,
    int biome_id,
    int resource_type,
    float weight
);

/**
 * Set resource spawn weight by string ID.
 *
 * @param system        Biome system
 * @param id            Biome string ID
 * @param resource_type Resource type (game-defined)
 * @param weight        Spawn weight
 * @return true if set
 */
bool agentite_biome_set_resource_weight_by_id(
    Agentite_BiomeSystem *system,
    const char *id,
    int resource_type,
    float weight
);

/**
 * Get resource spawn weight for a biome.
 *
 * @param system        Biome system
 * @param biome_id      Biome ID
 * @param resource_type Resource type
 * @return Spawn weight (0.0 if not set or invalid)
 */
float agentite_biome_get_resource_weight(
    const Agentite_BiomeSystem *system,
    int biome_id,
    int resource_type
);

/**
 * Get the best biome for spawning a specific resource.
 *
 * @param system        Biome system
 * @param resource_type Resource type
 * @return Biome ID with highest weight, or AGENTITE_BIOME_INVALID if none
 */
int agentite_biome_get_best_for_resource(
    const Agentite_BiomeSystem *system,
    int resource_type
);

/**
 * Get all biomes that can spawn a resource.
 *
 * @param system         Biome system
 * @param resource_type  Resource type
 * @param out_biome_ids  Output array for biome IDs
 * @param max_count      Maximum number to return
 * @return Number of biomes found
 */
int agentite_biome_get_all_for_resource(
    const Agentite_BiomeSystem *system,
    int resource_type,
    int *out_biome_ids,
    int max_count
);

/*============================================================================
 * Biome Properties
 *============================================================================*/

/**
 * Get biome name.
 *
 * @param system   Biome system
 * @param biome_id Biome ID
 * @return Name string or NULL
 */
const char *agentite_biome_get_name(const Agentite_BiomeSystem *system, int biome_id);

/**
 * Get biome color.
 *
 * @param system   Biome system
 * @param biome_id Biome ID
 * @return Color in ABGR format (0 if invalid)
 */
uint32_t agentite_biome_get_color(const Agentite_BiomeSystem *system, int biome_id);

/**
 * Get movement cost for a biome.
 *
 * @param system   Biome system
 * @param biome_id Biome ID
 * @return Movement cost multiplier (1.0 if invalid)
 */
float agentite_biome_get_movement_cost(const Agentite_BiomeSystem *system, int biome_id);

/**
 * Get resource yield multiplier for a biome.
 *
 * @param system   Biome system
 * @param biome_id Biome ID
 * @return Resource multiplier (1.0 if invalid)
 */
float agentite_biome_get_resource_multiplier(const Agentite_BiomeSystem *system, int biome_id);

/**
 * Get visibility modifier for a biome.
 *
 * @param system   Biome system
 * @param biome_id Biome ID
 * @return Visibility modifier (1.0 if invalid)
 */
float agentite_biome_get_visibility_modifier(const Agentite_BiomeSystem *system, int biome_id);

/**
 * Get defense bonus for a biome.
 *
 * @param system   Biome system
 * @param biome_id Biome ID
 * @return Defense bonus (0.0 if invalid)
 */
float agentite_biome_get_defense_bonus(const Agentite_BiomeSystem *system, int biome_id);

/*============================================================================
 * Biome Flags
 *============================================================================*/

/**
 * Check if biome has a specific flag.
 *
 * @param system   Biome system
 * @param biome_id Biome ID
 * @param flag     Flag to check
 * @return true if flag is set
 */
bool agentite_biome_has_flag(const Agentite_BiomeSystem *system, int biome_id, Agentite_BiomeFlags flag);

/**
 * Check if biome is passable.
 *
 * @param system   Biome system
 * @param biome_id Biome ID
 * @return true if passable
 */
bool agentite_biome_is_passable(const Agentite_BiomeSystem *system, int biome_id);

/**
 * Check if biome is buildable.
 *
 * @param system   Biome system
 * @param biome_id Biome ID
 * @return true if can build structures
 */
bool agentite_biome_is_buildable(const Agentite_BiomeSystem *system, int biome_id);

/**
 * Check if biome is water.
 *
 * @param system   Biome system
 * @param biome_id Biome ID
 * @return true if water biome
 */
bool agentite_biome_is_water(const Agentite_BiomeSystem *system, int biome_id);

/**
 * Check if biome is hazardous.
 *
 * @param system   Biome system
 * @param biome_id Biome ID
 * @return true if hazardous
 */
bool agentite_biome_is_hazardous(const Agentite_BiomeSystem *system, int biome_id);

/*============================================================================
 * Biome Map
 *============================================================================*/

/**
 * Create a biome map for a world.
 *
 * @param system Biome system
 * @param width  Map width in tiles
 * @param height Map height in tiles
 * @return Map handle or NULL on failure
 */
typedef struct Agentite_BiomeMap Agentite_BiomeMap;

Agentite_BiomeMap *agentite_biome_map_create(
    Agentite_BiomeSystem *system,
    int width,
    int height
);

/**
 * Destroy a biome map.
 *
 * @param map Map to destroy (safe if NULL)
 */
void agentite_biome_map_destroy(Agentite_BiomeMap *map);

/**
 * Set biome at a position.
 *
 * @param map      Biome map
 * @param x        X coordinate
 * @param y        Y coordinate
 * @param biome_id Biome ID
 * @return true if set
 */
bool agentite_biome_map_set(Agentite_BiomeMap *map, int x, int y, int biome_id);

/**
 * Get biome at a position.
 *
 * @param map Biome map
 * @param x   X coordinate
 * @param y   Y coordinate
 * @return Biome ID or AGENTITE_BIOME_INVALID if out of bounds
 */
int agentite_biome_map_get(const Agentite_BiomeMap *map, int x, int y);

/**
 * Fill a rectangular region with a biome.
 *
 * @param map      Biome map
 * @param x        Start X
 * @param y        Start Y
 * @param width    Region width
 * @param height   Region height
 * @param biome_id Biome ID
 */
void agentite_biome_map_fill_rect(
    Agentite_BiomeMap *map,
    int x, int y,
    int width, int height,
    int biome_id
);

/**
 * Fill a circular region with a biome.
 *
 * @param map      Biome map
 * @param center_x Center X
 * @param center_y Center Y
 * @param radius   Circle radius
 * @param biome_id Biome ID
 */
void agentite_biome_map_fill_circle(
    Agentite_BiomeMap *map,
    int center_x, int center_y,
    int radius,
    int biome_id
);

/**
 * Get the biome definition at a position.
 *
 * @param map Biome map
 * @param x   X coordinate
 * @param y   Y coordinate
 * @return Biome definition or NULL
 */
const Agentite_BiomeDef *agentite_biome_map_get_def(const Agentite_BiomeMap *map, int x, int y);

/**
 * Get movement cost at a position.
 *
 * @param map Biome map
 * @param x   X coordinate
 * @param y   Y coordinate
 * @return Movement cost (1.0 if invalid or out of bounds)
 */
float agentite_biome_map_get_movement_cost(const Agentite_BiomeMap *map, int x, int y);

/**
 * Get resource weight at a position for a specific resource.
 *
 * @param map           Biome map
 * @param x             X coordinate
 * @param y             Y coordinate
 * @param resource_type Resource type
 * @return Resource weight (0.0 if invalid or out of bounds)
 */
float agentite_biome_map_get_resource_weight(
    const Agentite_BiomeMap *map,
    int x, int y,
    int resource_type
);

/**
 * Check if position is passable.
 *
 * @param map Biome map
 * @param x   X coordinate
 * @param y   Y coordinate
 * @return true if passable
 */
bool agentite_biome_map_is_passable(const Agentite_BiomeMap *map, int x, int y);

/**
 * Check if position is buildable.
 *
 * @param map Biome map
 * @param x   X coordinate
 * @param y   Y coordinate
 * @return true if buildable
 */
bool agentite_biome_map_is_buildable(const Agentite_BiomeMap *map, int x, int y);

/**
 * Get map dimensions.
 *
 * @param map        Biome map
 * @param out_width  Output for width (can be NULL)
 * @param out_height Output for height (can be NULL)
 */
void agentite_biome_map_get_size(const Agentite_BiomeMap *map, int *out_width, int *out_height);

/**
 * Count cells of a specific biome.
 *
 * @param map      Biome map
 * @param biome_id Biome ID
 * @return Cell count
 */
int agentite_biome_map_count_biome(const Agentite_BiomeMap *map, int biome_id);

/**
 * Get statistics for all biomes in the map.
 *
 * @param map        Biome map
 * @param out_counts Output array (size should be AGENTITE_BIOME_MAX)
 */
void agentite_biome_map_get_stats(const Agentite_BiomeMap *map, int *out_counts);

/*============================================================================
 * Generation Helpers
 *============================================================================*/

/**
 * Simple noise-based biome generation.
 * Uses a basic noise function to distribute biomes.
 *
 * @param map        Biome map to fill
 * @param biome_ids  Array of biome IDs to use
 * @param thresholds Noise thresholds for each biome (ascending order)
 * @param count      Number of biomes
 * @param seed       Random seed
 */
void agentite_biome_map_generate_noise(
    Agentite_BiomeMap *map,
    const int *biome_ids,
    const float *thresholds,
    int count,
    uint32_t seed
);

/**
 * Blend biome borders for smoother transitions.
 * Creates transition tiles at biome edges.
 *
 * @param map    Biome map
 * @param passes Number of smoothing passes
 */
void agentite_biome_map_smooth(Agentite_BiomeMap *map, int passes);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Create a default biome definition with reasonable defaults.
 *
 * @return Default biome definition
 */
Agentite_BiomeDef agentite_biome_default_def(void);

/**
 * Convert RGB to ABGR color format.
 *
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @return ABGR color with full alpha
 */
uint32_t agentite_biome_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * Convert RGBA to ABGR color format.
 *
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @param a Alpha (0-255)
 * @return ABGR color
 */
uint32_t agentite_biome_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_BIOME_H */
