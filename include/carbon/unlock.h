#ifndef CARBON_UNLOCK_H
#define CARBON_UNLOCK_H

#include <stdbool.h>

#define CARBON_UNLOCK_MAX_PREREQS 8

// Unlock node definition (loadable from config)
typedef struct Carbon_UnlockDef {
    char id[64];
    char name[128];
    char description[256];
    char category[32];

    char prerequisites[CARBON_UNLOCK_MAX_PREREQS][64];
    int prereq_count;

    int cost;               // Research points, gold, etc.

    // Game-specific extra data (not used by engine)
    int effect_type;        // Game-defined enum
    float effect_value;     // Effect magnitude
} Carbon_UnlockDef;

// Progress tracking (for active research)
typedef struct Carbon_ResearchProgress {
    char current_id[64];
    int points_invested;
    int points_required;
} Carbon_ResearchProgress;

// Unlock tree manager
typedef struct Carbon_UnlockTree Carbon_UnlockTree;

Carbon_UnlockTree *carbon_unlock_create(void);
void carbon_unlock_destroy(Carbon_UnlockTree *tree);

// Register unlock nodes
void carbon_unlock_register(Carbon_UnlockTree *tree, const Carbon_UnlockDef *def);

// Mark as completed (unlocked)
void carbon_unlock_complete(Carbon_UnlockTree *tree, const char *id);

// Query state
bool carbon_unlock_is_completed(const Carbon_UnlockTree *tree, const char *id);
bool carbon_unlock_has_prerequisites(const Carbon_UnlockTree *tree, const char *id);
bool carbon_unlock_can_research(const Carbon_UnlockTree *tree, const char *id);

// Get available (researchable) unlocks
// Returns count of available unlocks, fills out_defs with pointers
int carbon_unlock_get_available(const Carbon_UnlockTree *tree,
                                 const Carbon_UnlockDef **out_defs,
                                 int max_count);

// Get unlocks by category
int carbon_unlock_get_by_category(const Carbon_UnlockTree *tree,
                                   const char *category,
                                   const Carbon_UnlockDef **out_defs,
                                   int max_count);

// Get completed unlocks
int carbon_unlock_get_completed(const Carbon_UnlockTree *tree,
                                 const Carbon_UnlockDef **out_defs,
                                 int max_count);

// Get all unlocks
int carbon_unlock_count(const Carbon_UnlockTree *tree);
const Carbon_UnlockDef *carbon_unlock_get_by_index(const Carbon_UnlockTree *tree, int index);
const Carbon_UnlockDef *carbon_unlock_find(const Carbon_UnlockTree *tree, const char *id);

// Reset all progress
void carbon_unlock_reset(Carbon_UnlockTree *tree);

// Research progress management
void carbon_unlock_start_research(Carbon_UnlockTree *tree,
                                   Carbon_ResearchProgress *progress,
                                   const char *id);

// Add research points, returns true if research completed
bool carbon_unlock_add_points(Carbon_UnlockTree *tree,
                               Carbon_ResearchProgress *progress,
                               int points);

// Get progress as percentage (0.0 to 1.0)
float carbon_unlock_get_progress_percent(const Carbon_ResearchProgress *progress);

// Check if actively researching something
bool carbon_unlock_is_researching(const Carbon_ResearchProgress *progress);

// Cancel active research
void carbon_unlock_cancel_research(Carbon_ResearchProgress *progress);

#endif // CARBON_UNLOCK_H
