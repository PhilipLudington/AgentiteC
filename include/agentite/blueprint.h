/*
 * Carbon Game Engine - Blueprint System
 *
 * Save and place building templates with relative positioning.
 * Supports capturing selections, rotation, and placement validation.
 */

#ifndef AGENTITE_BLUEPRINT_H
#define AGENTITE_BLUEPRINT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#ifndef AGENTITE_BLUEPRINT_MAX_ENTRIES
#define AGENTITE_BLUEPRINT_MAX_ENTRIES 64    /* Max objects per blueprint */
#endif

#ifndef AGENTITE_BLUEPRINT_MAX_NAME
#define AGENTITE_BLUEPRINT_MAX_NAME 64       /* Max name length */
#endif

#ifndef AGENTITE_BLUEPRINT_INVALID
#define AGENTITE_BLUEPRINT_INVALID 0         /* Invalid blueprint handle */
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

/* Forward declarations */
typedef struct Agentite_Blueprint Agentite_Blueprint;
typedef struct Agentite_BlueprintLibrary Agentite_BlueprintLibrary;

/**
 * A single entry in a blueprint (one building/object)
 */
typedef struct Agentite_BlueprintEntry {
    int16_t rel_x;              /* X offset from blueprint origin */
    int16_t rel_y;              /* Y offset from blueprint origin */
    uint16_t building_type;     /* Building/object type ID */
    uint8_t direction;          /* Direction/rotation (0-3 for 90-degree increments) */
    uint8_t flags;              /* Additional flags (game-defined) */
    uint32_t metadata;          /* Extra data (game-defined) */
} Agentite_BlueprintEntry;

/**
 * Rotation direction for blueprints
 */
typedef enum Agentite_BlueprintRotation {
    AGENTITE_BLUEPRINT_ROT_0 = 0,     /* No rotation */
    AGENTITE_BLUEPRINT_ROT_90 = 1,    /* 90 degrees clockwise */
    AGENTITE_BLUEPRINT_ROT_180 = 2,   /* 180 degrees */
    AGENTITE_BLUEPRINT_ROT_270 = 3    /* 270 degrees clockwise (90 counter-clockwise) */
} Agentite_BlueprintRotation;

/**
 * Placement validation result
 */
typedef struct Agentite_BlueprintPlacement {
    bool valid;                 /* True if all entries can be placed */
    int valid_count;            /* Number of entries that can be placed */
    int invalid_count;          /* Number of entries that cannot be placed */
    int first_invalid_index;    /* Index of first invalid entry, or -1 */
} Agentite_BlueprintPlacement;

/**
 * Callback for validating placement of a single entry.
 * Return true if the entry can be placed at (world_x, world_y).
 */
typedef bool (*Agentite_BlueprintValidator)(
    int world_x, int world_y,
    uint16_t building_type,
    uint8_t direction,
    void *userdata
);

/**
 * Callback for placing a single entry.
 * Called for each entry when placing a blueprint.
 */
typedef void (*Agentite_BlueprintPlacer)(
    int world_x, int world_y,
    uint16_t building_type,
    uint8_t direction,
    uint32_t metadata,
    void *userdata
);

/**
 * Callback for capturing buildings from the world.
 * Return true and fill out_type/out_direction if there's a building at (x, y).
 */
typedef bool (*Agentite_BlueprintCapturer)(
    int world_x, int world_y,
    uint16_t *out_type,
    uint8_t *out_direction,
    uint32_t *out_metadata,
    void *userdata
);

/* ============================================================================
 * Blueprint Creation and Destruction
 * ============================================================================ */

/**
 * Create an empty blueprint.
 *
 * @param name Display name for the blueprint
 * @return New blueprint, or NULL on failure.
 *         Caller OWNS the returned pointer and MUST call agentite_blueprint_destroy().
 */
Agentite_Blueprint *agentite_blueprint_create(const char *name);

/**
 * Destroy a blueprint and free all resources.
 *
 * @param bp Blueprint to destroy (safe if NULL)
 */
void agentite_blueprint_destroy(Agentite_Blueprint *bp);

/**
 * Create a deep copy of a blueprint.
 *
 * @param bp Blueprint to copy
 * @return New blueprint copy, or NULL on failure
 */
Agentite_Blueprint *agentite_blueprint_clone(const Agentite_Blueprint *bp);

/* ============================================================================
 * Blueprint Building
 * ============================================================================ */

/**
 * Add an entry to the blueprint.
 *
 * @param bp Blueprint to modify
 * @param rel_x X offset from blueprint origin
 * @param rel_y Y offset from blueprint origin
 * @param building_type Building/object type ID
 * @param direction Direction (0-3)
 * @return Index of added entry, or -1 on failure
 */
int agentite_blueprint_add_entry(
    Agentite_Blueprint *bp,
    int rel_x, int rel_y,
    uint16_t building_type,
    uint8_t direction
);

/**
 * Add an entry with metadata.
 *
 * @param bp Blueprint to modify
 * @param rel_x X offset from blueprint origin
 * @param rel_y Y offset from blueprint origin
 * @param building_type Building/object type ID
 * @param direction Direction (0-3)
 * @param metadata Extra data
 * @return Index of added entry, or -1 on failure
 */
int agentite_blueprint_add_entry_ex(
    Agentite_Blueprint *bp,
    int rel_x, int rel_y,
    uint16_t building_type,
    uint8_t direction,
    uint32_t metadata
);

/**
 * Remove an entry from the blueprint by index.
 *
 * @param bp Blueprint to modify
 * @param index Entry index to remove
 * @return True if removed, false if index invalid
 */
bool agentite_blueprint_remove_entry(Agentite_Blueprint *bp, int index);

/**
 * Clear all entries from the blueprint.
 *
 * @param bp Blueprint to clear
 */
void agentite_blueprint_clear(Agentite_Blueprint *bp);

/**
 * Capture buildings from the world into a blueprint.
 *
 * @param bp Blueprint to fill (cleared first)
 * @param x1, y1 Top-left corner of capture region
 * @param x2, y2 Bottom-right corner of capture region
 * @param capturer Callback to get building at each cell
 * @param userdata User data for callback
 * @return Number of entries captured
 */
int agentite_blueprint_capture(
    Agentite_Blueprint *bp,
    int x1, int y1,
    int x2, int y2,
    Agentite_BlueprintCapturer capturer,
    void *userdata
);

/* ============================================================================
 * Blueprint Transformation
 * ============================================================================ */

/**
 * Rotate the blueprint 90 degrees clockwise.
 *
 * @param bp Blueprint to rotate
 */
void agentite_blueprint_rotate_cw(Agentite_Blueprint *bp);

/**
 * Rotate the blueprint 90 degrees counter-clockwise.
 *
 * @param bp Blueprint to rotate
 */
void agentite_blueprint_rotate_ccw(Agentite_Blueprint *bp);

/**
 * Rotate the blueprint by the specified amount.
 *
 * @param bp Blueprint to rotate
 * @param rotation Rotation amount
 */
void agentite_blueprint_rotate(Agentite_Blueprint *bp, Agentite_BlueprintRotation rotation);

/**
 * Mirror the blueprint horizontally (flip X).
 *
 * @param bp Blueprint to mirror
 */
void agentite_blueprint_mirror_x(Agentite_Blueprint *bp);

/**
 * Mirror the blueprint vertically (flip Y).
 *
 * @param bp Blueprint to mirror
 */
void agentite_blueprint_mirror_y(Agentite_Blueprint *bp);

/**
 * Normalize the blueprint so the minimum X and Y offsets are 0.
 * This moves the origin to the top-left corner of the bounding box.
 *
 * @param bp Blueprint to normalize
 */
void agentite_blueprint_normalize(Agentite_Blueprint *bp);

/* ============================================================================
 * Blueprint Queries
 * ============================================================================ */

/**
 * Get the blueprint name.
 *
 * @param bp Blueprint to query
 * @return Name string, or NULL if invalid
 */
const char *agentite_blueprint_get_name(const Agentite_Blueprint *bp);

/**
 * Set the blueprint name.
 *
 * @param bp Blueprint to modify
 * @param name New name
 */
void agentite_blueprint_set_name(Agentite_Blueprint *bp, const char *name);

/**
 * Get the number of entries in the blueprint.
 *
 * @param bp Blueprint to query
 * @return Entry count
 */
int agentite_blueprint_get_entry_count(const Agentite_Blueprint *bp);

/**
 * Get an entry by index.
 *
 * @param bp Blueprint to query
 * @param index Entry index
 * @return Pointer to entry, or NULL if invalid
 */
const Agentite_BlueprintEntry *agentite_blueprint_get_entry(
    const Agentite_Blueprint *bp,
    int index
);

/**
 * Get all entries.
 *
 * @param bp Blueprint to query
 * @param out_entries Output array (must have space for entry_count entries)
 * @param max_entries Maximum entries to copy
 * @return Number of entries copied
 */
int agentite_blueprint_get_entries(
    const Agentite_Blueprint *bp,
    Agentite_BlueprintEntry *out_entries,
    int max_entries
);

/**
 * Get the bounding box dimensions of the blueprint.
 *
 * @param bp Blueprint to query
 * @param out_width Output width (nullable)
 * @param out_height Output height (nullable)
 */
void agentite_blueprint_get_bounds(
    const Agentite_Blueprint *bp,
    int *out_width,
    int *out_height
);

/**
 * Get the minimum and maximum offsets in the blueprint.
 *
 * @param bp Blueprint to query
 * @param out_min_x, out_min_y Minimum offsets (nullable)
 * @param out_max_x, out_max_y Maximum offsets (nullable)
 */
void agentite_blueprint_get_extents(
    const Agentite_Blueprint *bp,
    int *out_min_x, int *out_min_y,
    int *out_max_x, int *out_max_y
);

/**
 * Check if the blueprint is empty (no entries).
 *
 * @param bp Blueprint to query
 * @return True if empty
 */
bool agentite_blueprint_is_empty(const Agentite_Blueprint *bp);

/* ============================================================================
 * Blueprint Placement
 * ============================================================================ */

/**
 * Check if the blueprint can be placed at the given position.
 *
 * @param bp Blueprint to check
 * @param origin_x, origin_y World position for blueprint origin
 * @param validator Callback to validate each entry
 * @param userdata User data for callback
 * @return Placement result with validity info
 */
Agentite_BlueprintPlacement agentite_blueprint_can_place(
    const Agentite_Blueprint *bp,
    int origin_x, int origin_y,
    Agentite_BlueprintValidator validator,
    void *userdata
);

/**
 * Place the blueprint at the given position.
 * Does not check validity - call can_place first if needed.
 *
 * @param bp Blueprint to place
 * @param origin_x, origin_y World position for blueprint origin
 * @param placer Callback to place each entry
 * @param userdata User data for callback
 * @return Number of entries placed
 */
int agentite_blueprint_place(
    const Agentite_Blueprint *bp,
    int origin_x, int origin_y,
    Agentite_BlueprintPlacer placer,
    void *userdata
);

/**
 * Get the world position for an entry at a given origin.
 *
 * @param entry Entry to transform
 * @param origin_x, origin_y Blueprint origin
 * @param out_x, out_y Output world position
 */
void agentite_blueprint_entry_to_world(
    const Agentite_BlueprintEntry *entry,
    int origin_x, int origin_y,
    int *out_x, int *out_y
);

/* ============================================================================
 * Blueprint Library
 * ============================================================================ */

/**
 * Create a blueprint library.
 *
 * @param initial_capacity Initial capacity (0 for default)
 * @return New library, or NULL on failure
 */
Agentite_BlueprintLibrary *agentite_blueprint_library_create(int initial_capacity);

/**
 * Destroy a blueprint library and all contained blueprints.
 *
 * @param library Library to destroy (safe if NULL)
 */
void agentite_blueprint_library_destroy(Agentite_BlueprintLibrary *library);

/**
 * Add a blueprint to the library. The library takes ownership.
 *
 * @param library Library to add to
 * @param bp Blueprint to add (ownership transferred)
 * @return Handle to the blueprint, or AGENTITE_BLUEPRINT_INVALID on failure
 */
uint32_t agentite_blueprint_library_add(
    Agentite_BlueprintLibrary *library,
    Agentite_Blueprint *bp
);

/**
 * Remove a blueprint from the library.
 *
 * @param library Library to modify
 * @param handle Blueprint handle
 * @return True if removed
 */
bool agentite_blueprint_library_remove(
    Agentite_BlueprintLibrary *library,
    uint32_t handle
);

/**
 * Get a blueprint by handle.
 *
 * @param library Library to query
 * @param handle Blueprint handle
 * @return Blueprint pointer, or NULL if not found
 */
Agentite_Blueprint *agentite_blueprint_library_get(
    Agentite_BlueprintLibrary *library,
    uint32_t handle
);

/**
 * Get a blueprint by handle (const version).
 *
 * @param library Library to query
 * @param handle Blueprint handle
 * @return Blueprint pointer, or NULL if not found
 */
const Agentite_Blueprint *agentite_blueprint_library_get_const(
    const Agentite_BlueprintLibrary *library,
    uint32_t handle
);

/**
 * Find a blueprint by name.
 *
 * @param library Library to search
 * @param name Name to find
 * @return Blueprint handle, or AGENTITE_BLUEPRINT_INVALID if not found
 */
uint32_t agentite_blueprint_library_find(
    const Agentite_BlueprintLibrary *library,
    const char *name
);

/**
 * Get the number of blueprints in the library.
 *
 * @param library Library to query
 * @return Blueprint count
 */
int agentite_blueprint_library_count(const Agentite_BlueprintLibrary *library);

/**
 * Get all blueprint handles in the library.
 *
 * @param library Library to query
 * @param out_handles Output array
 * @param max_handles Maximum handles to return
 * @return Number of handles written
 */
int agentite_blueprint_library_get_all(
    const Agentite_BlueprintLibrary *library,
    uint32_t *out_handles,
    int max_handles
);

/**
 * Clear all blueprints from the library.
 *
 * @param library Library to clear
 */
void agentite_blueprint_library_clear(Agentite_BlueprintLibrary *library);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_BLUEPRINT_H */
