/*
 * Carbon Game Engine - Blueprint System Implementation
 *
 * Save and place building templates with relative positioning.
 */

#include "agentite/agentite.h"
#include "agentite/blueprint.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

struct Agentite_Blueprint {
    char name[AGENTITE_BLUEPRINT_MAX_NAME];
    Agentite_BlueprintEntry entries[AGENTITE_BLUEPRINT_MAX_ENTRIES];
    int entry_count;
};

typedef struct LibrarySlot {
    Agentite_Blueprint *blueprint;
    bool active;
} LibrarySlot;

struct Agentite_BlueprintLibrary {
    LibrarySlot *slots;
    int capacity;
    int count;
    uint32_t next_handle;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static int min_int(int a, int b) {
    return a < b ? a : b;
}

static int max_int(int a, int b) {
    return a > b ? a : b;
}

/* Rotate direction value (0-3) by rotation amount (0-3) */
static uint8_t rotate_direction(uint8_t dir, int rotation) {
    return (uint8_t)((dir + rotation) & 3);
}

/* ============================================================================
 * Blueprint Creation and Destruction
 * ============================================================================ */

Agentite_Blueprint *agentite_blueprint_create(const char *name) {
    Agentite_Blueprint *bp = AGENTITE_ALLOC(Agentite_Blueprint);
    if (!bp) {
        agentite_set_error("Failed to allocate blueprint");
        return NULL;
    }

    if (name) {
        strncpy(bp->name, name, AGENTITE_BLUEPRINT_MAX_NAME - 1);
        bp->name[AGENTITE_BLUEPRINT_MAX_NAME - 1] = '\0';
    }

    return bp;
}

void agentite_blueprint_destroy(Agentite_Blueprint *bp) {
    free(bp);
}

Agentite_Blueprint *agentite_blueprint_clone(const Agentite_Blueprint *bp) {
    AGENTITE_VALIDATE_PTR_RET(bp, NULL);

    Agentite_Blueprint *clone = AGENTITE_ALLOC(Agentite_Blueprint);
    if (!clone) {
        agentite_set_error("Failed to allocate blueprint clone");
        return NULL;
    }

    memcpy(clone, bp, sizeof(Agentite_Blueprint));
    return clone;
}

/* ============================================================================
 * Blueprint Building
 * ============================================================================ */

int agentite_blueprint_add_entry(
    Agentite_Blueprint *bp,
    int rel_x, int rel_y,
    uint16_t building_type,
    uint8_t direction)
{
    return agentite_blueprint_add_entry_ex(bp, rel_x, rel_y, building_type, direction, 0);
}

int agentite_blueprint_add_entry_ex(
    Agentite_Blueprint *bp,
    int rel_x, int rel_y,
    uint16_t building_type,
    uint8_t direction,
    uint32_t metadata)
{
    AGENTITE_VALIDATE_PTR_RET(bp, -1);

    if (bp->entry_count >= AGENTITE_BLUEPRINT_MAX_ENTRIES) {
        agentite_set_error("Blueprint entry limit reached (%d)", AGENTITE_BLUEPRINT_MAX_ENTRIES);
        return -1;
    }

    int idx = bp->entry_count++;
    Agentite_BlueprintEntry *entry = &bp->entries[idx];

    entry->rel_x = (int16_t)rel_x;
    entry->rel_y = (int16_t)rel_y;
    entry->building_type = building_type;
    entry->direction = direction & 3;
    entry->flags = 0;
    entry->metadata = metadata;

    return idx;
}

bool agentite_blueprint_remove_entry(Agentite_Blueprint *bp, int index) {
    AGENTITE_VALIDATE_PTR_RET(bp, false);

    if (index < 0 || index >= bp->entry_count) {
        return false;
    }

    /* Shift remaining entries down */
    for (int i = index; i < bp->entry_count - 1; i++) {
        bp->entries[i] = bp->entries[i + 1];
    }
    bp->entry_count--;

    return true;
}

void agentite_blueprint_clear(Agentite_Blueprint *bp) {
    AGENTITE_VALIDATE_PTR(bp);
    bp->entry_count = 0;
}

int agentite_blueprint_capture(
    Agentite_Blueprint *bp,
    int x1, int y1,
    int x2, int y2,
    Agentite_BlueprintCapturer capturer,
    void *userdata)
{
    AGENTITE_VALIDATE_PTR_RET(bp, 0);
    AGENTITE_VALIDATE_PTR_RET(capturer, 0);

    /* Clear existing entries */
    agentite_blueprint_clear(bp);

    /* Ensure x1,y1 is top-left and x2,y2 is bottom-right */
    int min_x = min_int(x1, x2);
    int min_y = min_int(y1, y2);
    int max_x = max_int(x1, x2);
    int max_y = max_int(y1, y2);

    int captured = 0;

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            uint16_t type;
            uint8_t direction;
            uint32_t metadata = 0;

            if (capturer(x, y, &type, &direction, &metadata, userdata)) {
                /* Calculate relative position from top-left */
                int rel_x = x - min_x;
                int rel_y = y - min_y;

                int idx = agentite_blueprint_add_entry_ex(bp, rel_x, rel_y,
                                                        type, direction, metadata);
                if (idx >= 0) {
                    captured++;
                }
            }
        }
    }

    return captured;
}

/* ============================================================================
 * Blueprint Transformation
 * ============================================================================ */

void agentite_blueprint_rotate_cw(Agentite_Blueprint *bp) {
    agentite_blueprint_rotate(bp, AGENTITE_BLUEPRINT_ROT_90);
}

void agentite_blueprint_rotate_ccw(Agentite_Blueprint *bp) {
    agentite_blueprint_rotate(bp, AGENTITE_BLUEPRINT_ROT_270);
}

void agentite_blueprint_rotate(Agentite_Blueprint *bp, Agentite_BlueprintRotation rotation) {
    AGENTITE_VALIDATE_PTR(bp);

    if (rotation == AGENTITE_BLUEPRINT_ROT_0 || bp->entry_count == 0) {
        return;
    }

    /* Get current bounds to determine rotation center */
    int min_x, min_y, max_x, max_y;
    agentite_blueprint_get_extents(bp, &min_x, &min_y, &max_x, &max_y);

    int width = max_x - min_x;
    int height = max_y - min_y;

    for (int i = 0; i < bp->entry_count; i++) {
        Agentite_BlueprintEntry *entry = &bp->entries[i];

        /* Translate to origin */
        int x = entry->rel_x - min_x;
        int y = entry->rel_y - min_y;
        int new_x, new_y;

        switch (rotation) {
            case AGENTITE_BLUEPRINT_ROT_90:
                /* (x, y) -> (height - y, x) */
                new_x = height - y;
                new_y = x;
                break;
            case AGENTITE_BLUEPRINT_ROT_180:
                /* (x, y) -> (width - x, height - y) */
                new_x = width - x;
                new_y = height - y;
                break;
            case AGENTITE_BLUEPRINT_ROT_270:
                /* (x, y) -> (y, width - x) */
                new_x = y;
                new_y = width - x;
                break;
            default:
                new_x = x;
                new_y = y;
                break;
        }

        entry->rel_x = (int16_t)new_x;
        entry->rel_y = (int16_t)new_y;

        /* Rotate the building direction as well */
        entry->direction = rotate_direction(entry->direction, (int)rotation);
    }
}

void agentite_blueprint_mirror_x(Agentite_Blueprint *bp) {
    AGENTITE_VALIDATE_PTR(bp);

    if (bp->entry_count == 0) return;

    int min_x, max_x;
    agentite_blueprint_get_extents(bp, &min_x, NULL, &max_x, NULL);
    int width = max_x - min_x;

    for (int i = 0; i < bp->entry_count; i++) {
        Agentite_BlueprintEntry *entry = &bp->entries[i];
        int x = entry->rel_x - min_x;
        entry->rel_x = (int16_t)(width - x);

        /* Mirror direction: 1 <-> 3 (East <-> West) */
        if (entry->direction == 1) entry->direction = 3;
        else if (entry->direction == 3) entry->direction = 1;
    }
}

void agentite_blueprint_mirror_y(Agentite_Blueprint *bp) {
    AGENTITE_VALIDATE_PTR(bp);

    if (bp->entry_count == 0) return;

    int min_y, max_y;
    agentite_blueprint_get_extents(bp, NULL, &min_y, NULL, &max_y);
    int height = max_y - min_y;

    for (int i = 0; i < bp->entry_count; i++) {
        Agentite_BlueprintEntry *entry = &bp->entries[i];
        int y = entry->rel_y - min_y;
        entry->rel_y = (int16_t)(height - y);

        /* Mirror direction: 0 <-> 2 (North <-> South) */
        if (entry->direction == 0) entry->direction = 2;
        else if (entry->direction == 2) entry->direction = 0;
    }
}

void agentite_blueprint_normalize(Agentite_Blueprint *bp) {
    AGENTITE_VALIDATE_PTR(bp);

    if (bp->entry_count == 0) return;

    int min_x, min_y;
    agentite_blueprint_get_extents(bp, &min_x, &min_y, NULL, NULL);

    /* Shift all entries so minimum is at (0, 0) */
    for (int i = 0; i < bp->entry_count; i++) {
        bp->entries[i].rel_x -= (int16_t)min_x;
        bp->entries[i].rel_y -= (int16_t)min_y;
    }
}

/* ============================================================================
 * Blueprint Queries
 * ============================================================================ */

const char *agentite_blueprint_get_name(const Agentite_Blueprint *bp) {
    AGENTITE_VALIDATE_PTR_RET(bp, NULL);
    return bp->name;
}

void agentite_blueprint_set_name(Agentite_Blueprint *bp, const char *name) {
    AGENTITE_VALIDATE_PTR(bp);

    if (name) {
        strncpy(bp->name, name, AGENTITE_BLUEPRINT_MAX_NAME - 1);
        bp->name[AGENTITE_BLUEPRINT_MAX_NAME - 1] = '\0';
    } else {
        bp->name[0] = '\0';
    }
}

int agentite_blueprint_get_entry_count(const Agentite_Blueprint *bp) {
    AGENTITE_VALIDATE_PTR_RET(bp, 0);
    return bp->entry_count;
}

const Agentite_BlueprintEntry *agentite_blueprint_get_entry(
    const Agentite_Blueprint *bp,
    int index)
{
    AGENTITE_VALIDATE_PTR_RET(bp, NULL);

    if (index < 0 || index >= bp->entry_count) {
        return NULL;
    }

    return &bp->entries[index];
}

int agentite_blueprint_get_entries(
    const Agentite_Blueprint *bp,
    Agentite_BlueprintEntry *out_entries,
    int max_entries)
{
    AGENTITE_VALIDATE_PTR_RET(bp, 0);
    AGENTITE_VALIDATE_PTR_RET(out_entries, 0);

    int count = min_int(bp->entry_count, max_entries);
    memcpy(out_entries, bp->entries, count * sizeof(Agentite_BlueprintEntry));

    return count;
}

void agentite_blueprint_get_bounds(
    const Agentite_Blueprint *bp,
    int *out_width,
    int *out_height)
{
    AGENTITE_VALIDATE_PTR(bp);

    if (bp->entry_count == 0) {
        if (out_width) *out_width = 0;
        if (out_height) *out_height = 0;
        return;
    }

    int min_x, min_y, max_x, max_y;
    agentite_blueprint_get_extents(bp, &min_x, &min_y, &max_x, &max_y);

    if (out_width) *out_width = max_x - min_x + 1;
    if (out_height) *out_height = max_y - min_y + 1;
}

void agentite_blueprint_get_extents(
    const Agentite_Blueprint *bp,
    int *out_min_x, int *out_min_y,
    int *out_max_x, int *out_max_y)
{
    AGENTITE_VALIDATE_PTR(bp);

    if (bp->entry_count == 0) {
        if (out_min_x) *out_min_x = 0;
        if (out_min_y) *out_min_y = 0;
        if (out_max_x) *out_max_x = 0;
        if (out_max_y) *out_max_y = 0;
        return;
    }

    int min_x = bp->entries[0].rel_x;
    int min_y = bp->entries[0].rel_y;
    int max_x = bp->entries[0].rel_x;
    int max_y = bp->entries[0].rel_y;

    for (int i = 1; i < bp->entry_count; i++) {
        const Agentite_BlueprintEntry *e = &bp->entries[i];
        if (e->rel_x < min_x) min_x = e->rel_x;
        if (e->rel_y < min_y) min_y = e->rel_y;
        if (e->rel_x > max_x) max_x = e->rel_x;
        if (e->rel_y > max_y) max_y = e->rel_y;
    }

    if (out_min_x) *out_min_x = min_x;
    if (out_min_y) *out_min_y = min_y;
    if (out_max_x) *out_max_x = max_x;
    if (out_max_y) *out_max_y = max_y;
}

bool agentite_blueprint_is_empty(const Agentite_Blueprint *bp) {
    AGENTITE_VALIDATE_PTR_RET(bp, true);
    return bp->entry_count == 0;
}

/* ============================================================================
 * Blueprint Placement
 * ============================================================================ */

Agentite_BlueprintPlacement agentite_blueprint_can_place(
    const Agentite_Blueprint *bp,
    int origin_x, int origin_y,
    Agentite_BlueprintValidator validator,
    void *userdata)
{
    Agentite_BlueprintPlacement result = {
        .valid = true,
        .valid_count = 0,
        .invalid_count = 0,
        .first_invalid_index = -1
    };

    if (!bp || !validator) {
        result.valid = false;
        return result;
    }

    for (int i = 0; i < bp->entry_count; i++) {
        const Agentite_BlueprintEntry *entry = &bp->entries[i];
        int world_x = origin_x + entry->rel_x;
        int world_y = origin_y + entry->rel_y;

        if (validator(world_x, world_y, entry->building_type,
                      entry->direction, userdata)) {
            result.valid_count++;
        } else {
            result.invalid_count++;
            if (result.first_invalid_index < 0) {
                result.first_invalid_index = i;
            }
            result.valid = false;
        }
    }

    return result;
}

int agentite_blueprint_place(
    const Agentite_Blueprint *bp,
    int origin_x, int origin_y,
    Agentite_BlueprintPlacer placer,
    void *userdata)
{
    AGENTITE_VALIDATE_PTR_RET(bp, 0);
    AGENTITE_VALIDATE_PTR_RET(placer, 0);

    for (int i = 0; i < bp->entry_count; i++) {
        const Agentite_BlueprintEntry *entry = &bp->entries[i];
        int world_x = origin_x + entry->rel_x;
        int world_y = origin_y + entry->rel_y;

        placer(world_x, world_y, entry->building_type,
               entry->direction, entry->metadata, userdata);
    }

    return bp->entry_count;
}

void agentite_blueprint_entry_to_world(
    const Agentite_BlueprintEntry *entry,
    int origin_x, int origin_y,
    int *out_x, int *out_y)
{
    AGENTITE_VALIDATE_PTR(entry);

    if (out_x) *out_x = origin_x + entry->rel_x;
    if (out_y) *out_y = origin_y + entry->rel_y;
}

/* ============================================================================
 * Blueprint Library
 * ============================================================================ */

Agentite_BlueprintLibrary *agentite_blueprint_library_create(int initial_capacity) {
    Agentite_BlueprintLibrary *library = AGENTITE_ALLOC(Agentite_BlueprintLibrary);
    if (!library) {
        agentite_set_error("Failed to allocate blueprint library");
        return NULL;
    }

    library->capacity = initial_capacity > 0 ? initial_capacity : 16;
    library->slots = AGENTITE_ALLOC_ARRAY(LibrarySlot, library->capacity);
    if (!library->slots) {
        agentite_set_error("Failed to allocate library slots");
        free(library);
        return NULL;
    }

    library->next_handle = 1;  /* Start at 1, 0 is invalid */

    return library;
}

void agentite_blueprint_library_destroy(Agentite_BlueprintLibrary *library) {
    if (!library) return;

    /* Free all blueprints */
    for (int i = 0; i < library->capacity; i++) {
        if (library->slots[i].active && library->slots[i].blueprint) {
            agentite_blueprint_destroy(library->slots[i].blueprint);
        }
    }

    free(library->slots);
    free(library);
}

static bool library_ensure_capacity(Agentite_BlueprintLibrary *library) {
    if (library->count < library->capacity) {
        return true;
    }

    int new_capacity = library->capacity * 2;
    LibrarySlot *new_slots = (LibrarySlot*)realloc(library->slots,
                                     new_capacity * sizeof(LibrarySlot));
    if (!new_slots) {
        agentite_set_error("Failed to grow blueprint library");
        return false;
    }

    /* Initialize new slots */
    memset(&new_slots[library->capacity], 0,
           (new_capacity - library->capacity) * sizeof(LibrarySlot));

    library->slots = new_slots;
    library->capacity = new_capacity;
    return true;
}

uint32_t agentite_blueprint_library_add(
    Agentite_BlueprintLibrary *library,
    Agentite_Blueprint *bp)
{
    AGENTITE_VALIDATE_PTR_RET(library, AGENTITE_BLUEPRINT_INVALID);
    AGENTITE_VALIDATE_PTR_RET(bp, AGENTITE_BLUEPRINT_INVALID);

    if (!library_ensure_capacity(library)) {
        return AGENTITE_BLUEPRINT_INVALID;
    }

    /* Find first empty slot */
    int slot = -1;
    for (int i = 0; i < library->capacity; i++) {
        if (!library->slots[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        agentite_set_error("No available slot in blueprint library");
        return AGENTITE_BLUEPRINT_INVALID;
    }

    library->slots[slot].blueprint = bp;
    library->slots[slot].active = true;
    library->count++;

    /* Return handle: slot index + 1 (so 0 is invalid) */
    return (uint32_t)(slot + 1);
}

bool agentite_blueprint_library_remove(
    Agentite_BlueprintLibrary *library,
    uint32_t handle)
{
    AGENTITE_VALIDATE_PTR_RET(library, false);

    if (handle == AGENTITE_BLUEPRINT_INVALID) return false;

    int slot = (int)handle - 1;
    if (slot < 0 || slot >= library->capacity) return false;
    if (!library->slots[slot].active) return false;

    agentite_blueprint_destroy(library->slots[slot].blueprint);
    library->slots[slot].blueprint = NULL;
    library->slots[slot].active = false;
    library->count--;

    return true;
}

Agentite_Blueprint *agentite_blueprint_library_get(
    Agentite_BlueprintLibrary *library,
    uint32_t handle)
{
    AGENTITE_VALIDATE_PTR_RET(library, NULL);

    if (handle == AGENTITE_BLUEPRINT_INVALID) return NULL;

    int slot = (int)handle - 1;
    if (slot < 0 || slot >= library->capacity) return NULL;
    if (!library->slots[slot].active) return NULL;

    return library->slots[slot].blueprint;
}

const Agentite_Blueprint *agentite_blueprint_library_get_const(
    const Agentite_BlueprintLibrary *library,
    uint32_t handle)
{
    return agentite_blueprint_library_get((Agentite_BlueprintLibrary *)library, handle);
}

uint32_t agentite_blueprint_library_find(
    const Agentite_BlueprintLibrary *library,
    const char *name)
{
    AGENTITE_VALIDATE_PTR_RET(library, AGENTITE_BLUEPRINT_INVALID);
    AGENTITE_VALIDATE_PTR_RET(name, AGENTITE_BLUEPRINT_INVALID);

    for (int i = 0; i < library->capacity; i++) {
        if (library->slots[i].active && library->slots[i].blueprint) {
            if (strcmp(library->slots[i].blueprint->name, name) == 0) {
                return (uint32_t)(i + 1);
            }
        }
    }

    return AGENTITE_BLUEPRINT_INVALID;
}

int agentite_blueprint_library_count(const Agentite_BlueprintLibrary *library) {
    AGENTITE_VALIDATE_PTR_RET(library, 0);
    return library->count;
}

int agentite_blueprint_library_get_all(
    const Agentite_BlueprintLibrary *library,
    uint32_t *out_handles,
    int max_handles)
{
    AGENTITE_VALIDATE_PTR_RET(library, 0);
    AGENTITE_VALIDATE_PTR_RET(out_handles, 0);

    int count = 0;
    for (int i = 0; i < library->capacity && count < max_handles; i++) {
        if (library->slots[i].active) {
            out_handles[count++] = (uint32_t)(i + 1);
        }
    }

    return count;
}

void agentite_blueprint_library_clear(Agentite_BlueprintLibrary *library) {
    AGENTITE_VALIDATE_PTR(library);

    for (int i = 0; i < library->capacity; i++) {
        if (library->slots[i].active && library->slots[i].blueprint) {
            agentite_blueprint_destroy(library->slots[i].blueprint);
            library->slots[i].blueprint = NULL;
            library->slots[i].active = false;
        }
    }

    library->count = 0;
}
