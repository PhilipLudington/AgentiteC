#include "carbon/carbon.h"
#include "carbon/unlock.h"
#include <stdlib.h>
#include <string.h>

#define MAX_UNLOCKS 256

struct Carbon_UnlockTree {
    Carbon_UnlockDef unlocks[MAX_UNLOCKS];
    int unlock_count;

    // Completion state stored as bitfield for speed
    // (assumes unlock indices are stable)
    bool completed[MAX_UNLOCKS];
};

Carbon_UnlockTree *carbon_unlock_create(void) {
    Carbon_UnlockTree *tree = CARBON_ALLOC(Carbon_UnlockTree);
    return tree;
}

void carbon_unlock_destroy(Carbon_UnlockTree *tree) {
    free(tree);
}

void carbon_unlock_register(Carbon_UnlockTree *tree, const Carbon_UnlockDef *def) {
    if (!tree || !def || tree->unlock_count >= MAX_UNLOCKS) return;

    tree->unlocks[tree->unlock_count] = *def;
    tree->unlock_count++;
}

// Find unlock index by ID
static int find_unlock_index(const Carbon_UnlockTree *tree, const char *id) {
    if (!tree || !id) return -1;

    for (int i = 0; i < tree->unlock_count; i++) {
        if (strcmp(tree->unlocks[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

void carbon_unlock_complete(Carbon_UnlockTree *tree, const char *id) {
    if (!tree || !id) return;

    int idx = find_unlock_index(tree, id);
    if (idx >= 0) {
        tree->completed[idx] = true;
    }
}

bool carbon_unlock_is_completed(const Carbon_UnlockTree *tree, const char *id) {
    if (!tree || !id) return false;

    int idx = find_unlock_index(tree, id);
    if (idx < 0) return false;

    return tree->completed[idx];
}

bool carbon_unlock_has_prerequisites(const Carbon_UnlockTree *tree, const char *id) {
    if (!tree || !id) return false;

    int idx = find_unlock_index(tree, id);
    if (idx < 0) return false;

    const Carbon_UnlockDef *def = &tree->unlocks[idx];

    // Check all prerequisites are completed
    for (int i = 0; i < def->prereq_count; i++) {
        if (!carbon_unlock_is_completed(tree, def->prerequisites[i])) {
            return false;
        }
    }

    return true;
}

bool carbon_unlock_can_research(const Carbon_UnlockTree *tree, const char *id) {
    if (!tree || !id) return false;

    // Can't research if already completed
    if (carbon_unlock_is_completed(tree, id)) {
        return false;
    }

    // Must have all prerequisites
    return carbon_unlock_has_prerequisites(tree, id);
}

int carbon_unlock_get_available(const Carbon_UnlockTree *tree,
                                 const Carbon_UnlockDef **out_defs,
                                 int max_count) {
    if (!tree || !out_defs || max_count <= 0) return 0;

    int count = 0;
    for (int i = 0; i < tree->unlock_count && count < max_count; i++) {
        if (carbon_unlock_can_research(tree, tree->unlocks[i].id)) {
            out_defs[count++] = &tree->unlocks[i];
        }
    }
    return count;
}

int carbon_unlock_get_by_category(const Carbon_UnlockTree *tree,
                                   const char *category,
                                   const Carbon_UnlockDef **out_defs,
                                   int max_count) {
    if (!tree || !category || !out_defs || max_count <= 0) return 0;

    int count = 0;
    for (int i = 0; i < tree->unlock_count && count < max_count; i++) {
        if (strcmp(tree->unlocks[i].category, category) == 0) {
            out_defs[count++] = &tree->unlocks[i];
        }
    }
    return count;
}

int carbon_unlock_get_completed(const Carbon_UnlockTree *tree,
                                 const Carbon_UnlockDef **out_defs,
                                 int max_count) {
    if (!tree || !out_defs || max_count <= 0) return 0;

    int count = 0;
    for (int i = 0; i < tree->unlock_count && count < max_count; i++) {
        if (tree->completed[i]) {
            out_defs[count++] = &tree->unlocks[i];
        }
    }
    return count;
}

int carbon_unlock_count(const Carbon_UnlockTree *tree) {
    if (!tree) return 0;
    return tree->unlock_count;
}

const Carbon_UnlockDef *carbon_unlock_get_by_index(const Carbon_UnlockTree *tree, int index) {
    if (!tree || index < 0 || index >= tree->unlock_count) return NULL;
    return &tree->unlocks[index];
}

const Carbon_UnlockDef *carbon_unlock_find(const Carbon_UnlockTree *tree, const char *id) {
    if (!tree || !id) return NULL;

    int idx = find_unlock_index(tree, id);
    if (idx < 0) return NULL;

    return &tree->unlocks[idx];
}

void carbon_unlock_reset(Carbon_UnlockTree *tree) {
    if (!tree) return;
    memset(tree->completed, 0, sizeof(tree->completed));
}

void carbon_unlock_start_research(Carbon_UnlockTree *tree,
                                   Carbon_ResearchProgress *progress,
                                   const char *id) {
    if (!tree || !progress || !id) return;

    const Carbon_UnlockDef *def = carbon_unlock_find(tree, id);
    if (!def) return;

    strncpy(progress->current_id, id, sizeof(progress->current_id) - 1);
    progress->current_id[sizeof(progress->current_id) - 1] = '\0';
    progress->points_invested = 0;
    progress->points_required = def->cost;
}

bool carbon_unlock_add_points(Carbon_UnlockTree *tree,
                               Carbon_ResearchProgress *progress,
                               int points) {
    if (!tree || !progress || points <= 0) return false;
    if (progress->current_id[0] == '\0') return false;

    progress->points_invested += points;

    if (progress->points_invested >= progress->points_required) {
        // Research complete!
        carbon_unlock_complete(tree, progress->current_id);

        // Clear progress
        progress->current_id[0] = '\0';
        progress->points_invested = 0;
        progress->points_required = 0;

        return true;
    }

    return false;
}

float carbon_unlock_get_progress_percent(const Carbon_ResearchProgress *progress) {
    if (!progress || progress->points_required <= 0) return 0.0f;

    float percent = (float)progress->points_invested / (float)progress->points_required;
    if (percent > 1.0f) percent = 1.0f;
    if (percent < 0.0f) percent = 0.0f;

    return percent;
}

bool carbon_unlock_is_researching(const Carbon_ResearchProgress *progress) {
    if (!progress) return false;
    return progress->current_id[0] != '\0';
}

void carbon_unlock_cancel_research(Carbon_ResearchProgress *progress) {
    if (!progress) return;
    progress->current_id[0] = '\0';
    progress->points_invested = 0;
    progress->points_required = 0;
}
