#ifndef AGENTITE_UNLOCK_H
#define AGENTITE_UNLOCK_H

#include <stdbool.h>

#define AGENTITE_UNLOCK_MAX_PREREQS 8

// Unlock node definition (loadable from config)
typedef struct Agentite_UnlockDef {
    char id[64];
    char name[128];
    char description[256];
    char category[32];

    char prerequisites[AGENTITE_UNLOCK_MAX_PREREQS][64];
    int prereq_count;

    int cost;               // Research points, gold, etc.

    // Game-specific extra data (not used by engine)
    int effect_type;        // Game-defined enum
    float effect_value;     // Effect magnitude
} Agentite_UnlockDef;

// Progress tracking (for active research)
typedef struct Agentite_ResearchProgress {
    char current_id[64];
    int points_invested;
    int points_required;
} Agentite_ResearchProgress;

// Unlock tree manager
typedef struct Agentite_UnlockTree Agentite_UnlockTree;

Agentite_UnlockTree *agentite_unlock_create(void);
void agentite_unlock_destroy(Agentite_UnlockTree *tree);

// Register unlock nodes
void agentite_unlock_register(Agentite_UnlockTree *tree, const Agentite_UnlockDef *def);

// Mark as completed (unlocked)
void agentite_unlock_complete(Agentite_UnlockTree *tree, const char *id);

// Query state
bool agentite_unlock_is_completed(const Agentite_UnlockTree *tree, const char *id);
bool agentite_unlock_has_prerequisites(const Agentite_UnlockTree *tree, const char *id);
bool agentite_unlock_can_research(const Agentite_UnlockTree *tree, const char *id);

// Get available (researchable) unlocks
// Returns count of available unlocks, fills out_defs with pointers
int agentite_unlock_get_available(const Agentite_UnlockTree *tree,
                                 const Agentite_UnlockDef **out_defs,
                                 int max_count);

// Get unlocks by category
int agentite_unlock_get_by_category(const Agentite_UnlockTree *tree,
                                   const char *category,
                                   const Agentite_UnlockDef **out_defs,
                                   int max_count);

// Get completed unlocks
int agentite_unlock_get_completed(const Agentite_UnlockTree *tree,
                                 const Agentite_UnlockDef **out_defs,
                                 int max_count);

// Get all unlocks
int agentite_unlock_count(const Agentite_UnlockTree *tree);
const Agentite_UnlockDef *agentite_unlock_get_by_index(const Agentite_UnlockTree *tree, int index);
const Agentite_UnlockDef *agentite_unlock_find(const Agentite_UnlockTree *tree, const char *id);

// Reset all progress
void agentite_unlock_reset(Agentite_UnlockTree *tree);

// Research progress management
void agentite_unlock_start_research(Agentite_UnlockTree *tree,
                                   Agentite_ResearchProgress *progress,
                                   const char *id);

// Add research points, returns true if research completed
bool agentite_unlock_add_points(Agentite_UnlockTree *tree,
                               Agentite_ResearchProgress *progress,
                               int points);

// Get progress as percentage (0.0 to 1.0)
float agentite_unlock_get_progress_percent(const Agentite_ResearchProgress *progress);

// Check if actively researching something
bool agentite_unlock_is_researching(const Agentite_ResearchProgress *progress);

// Cancel active research
void agentite_unlock_cancel_research(Agentite_ResearchProgress *progress);

#endif // AGENTITE_UNLOCK_H
