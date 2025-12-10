#include "agentite/agentite.h"
#include "agentite/unlock.h"
#include <stdlib.h>
#include <string.h>

#define MAX_UNLOCKS 256

struct Agentite_UnlockTree {
    Agentite_UnlockDef unlocks[MAX_UNLOCKS];
    int unlock_count;

    // Completion state stored as bitfield for speed
    // (assumes unlock indices are stable)
    bool completed[MAX_UNLOCKS];
};

Agentite_UnlockTree *agentite_unlock_create(void) {
    Agentite_UnlockTree *tree = AGENTITE_ALLOC(Agentite_UnlockTree);
    return tree;
}

void agentite_unlock_destroy(Agentite_UnlockTree *tree) {
    free(tree);
}

void agentite_unlock_register(Agentite_UnlockTree *tree, const Agentite_UnlockDef *def) {
    if (!tree || !def || tree->unlock_count >= MAX_UNLOCKS) return;

    tree->unlocks[tree->unlock_count] = *def;
    tree->unlock_count++;
}

// Find unlock index by ID
static int find_unlock_index(const Agentite_UnlockTree *tree, const char *id) {
    if (!tree || !id) return -1;

    for (int i = 0; i < tree->unlock_count; i++) {
        if (strcmp(tree->unlocks[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

void agentite_unlock_complete(Agentite_UnlockTree *tree, const char *id) {
    if (!tree || !id) return;

    int idx = find_unlock_index(tree, id);
    if (idx >= 0) {
        tree->completed[idx] = true;
    }
}

bool agentite_unlock_is_completed(const Agentite_UnlockTree *tree, const char *id) {
    if (!tree || !id) return false;

    int idx = find_unlock_index(tree, id);
    if (idx < 0) return false;

    return tree->completed[idx];
}

bool agentite_unlock_has_prerequisites(const Agentite_UnlockTree *tree, const char *id) {
    if (!tree || !id) return false;

    int idx = find_unlock_index(tree, id);
    if (idx < 0) return false;

    const Agentite_UnlockDef *def = &tree->unlocks[idx];

    // Check all prerequisites are completed
    for (int i = 0; i < def->prereq_count; i++) {
        if (!agentite_unlock_is_completed(tree, def->prerequisites[i])) {
            return false;
        }
    }

    return true;
}

bool agentite_unlock_can_research(const Agentite_UnlockTree *tree, const char *id) {
    if (!tree || !id) return false;

    // Can't research if already completed
    if (agentite_unlock_is_completed(tree, id)) {
        return false;
    }

    // Must have all prerequisites
    return agentite_unlock_has_prerequisites(tree, id);
}

int agentite_unlock_get_available(const Agentite_UnlockTree *tree,
                                 const Agentite_UnlockDef **out_defs,
                                 int max_count) {
    if (!tree || !out_defs || max_count <= 0) return 0;

    int count = 0;
    for (int i = 0; i < tree->unlock_count && count < max_count; i++) {
        if (agentite_unlock_can_research(tree, tree->unlocks[i].id)) {
            out_defs[count++] = &tree->unlocks[i];
        }
    }
    return count;
}

int agentite_unlock_get_by_category(const Agentite_UnlockTree *tree,
                                   const char *category,
                                   const Agentite_UnlockDef **out_defs,
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

int agentite_unlock_get_completed(const Agentite_UnlockTree *tree,
                                 const Agentite_UnlockDef **out_defs,
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

int agentite_unlock_count(const Agentite_UnlockTree *tree) {
    if (!tree) return 0;
    return tree->unlock_count;
}

const Agentite_UnlockDef *agentite_unlock_get_by_index(const Agentite_UnlockTree *tree, int index) {
    if (!tree || index < 0 || index >= tree->unlock_count) return NULL;
    return &tree->unlocks[index];
}

const Agentite_UnlockDef *agentite_unlock_find(const Agentite_UnlockTree *tree, const char *id) {
    if (!tree || !id) return NULL;

    int idx = find_unlock_index(tree, id);
    if (idx < 0) return NULL;

    return &tree->unlocks[idx];
}

void agentite_unlock_reset(Agentite_UnlockTree *tree) {
    if (!tree) return;
    memset(tree->completed, 0, sizeof(tree->completed));
}

void agentite_unlock_start_research(Agentite_UnlockTree *tree,
                                   Agentite_ResearchProgress *progress,
                                   const char *id) {
    if (!tree || !progress || !id) return;

    const Agentite_UnlockDef *def = agentite_unlock_find(tree, id);
    if (!def) return;

    strncpy(progress->current_id, id, sizeof(progress->current_id) - 1);
    progress->current_id[sizeof(progress->current_id) - 1] = '\0';
    progress->points_invested = 0;
    progress->points_required = def->cost;
}

bool agentite_unlock_add_points(Agentite_UnlockTree *tree,
                               Agentite_ResearchProgress *progress,
                               int points) {
    if (!tree || !progress || points <= 0) return false;
    if (progress->current_id[0] == '\0') return false;

    progress->points_invested += points;

    if (progress->points_invested >= progress->points_required) {
        // Research complete!
        agentite_unlock_complete(tree, progress->current_id);

        // Clear progress
        progress->current_id[0] = '\0';
        progress->points_invested = 0;
        progress->points_required = 0;

        return true;
    }

    return false;
}

float agentite_unlock_get_progress_percent(const Agentite_ResearchProgress *progress) {
    if (!progress || progress->points_required <= 0) return 0.0f;

    float percent = (float)progress->points_invested / (float)progress->points_required;
    if (percent > 1.0f) percent = 1.0f;
    if (percent < 0.0f) percent = 0.0f;

    return percent;
}

bool agentite_unlock_is_researching(const Agentite_ResearchProgress *progress) {
    if (!progress) return false;
    return progress->current_id[0] != '\0';
}

void agentite_unlock_cancel_research(Agentite_ResearchProgress *progress) {
    if (!progress) return;
    progress->current_id[0] = '\0';
    progress->points_invested = 0;
    progress->points_required = 0;
}
