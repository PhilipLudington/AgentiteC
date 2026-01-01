/**
 * Agentite Engine - ECS Entity Inspector
 *
 * A debug tool for inspecting ECS entities and their components at runtime.
 * Displays entity list and component field values using the reflection system.
 *
 * Usage:
 *   Agentite_Inspector *inspector = agentite_inspector_create(world, registry, NULL);
 *
 *   // In your render loop:
 *   agentite_inspector_draw(inspector, ui, x, y, w, h);
 *
 *   agentite_inspector_destroy(inspector);
 */

#ifndef AGENTITE_ECS_INSPECTOR_H
#define AGENTITE_ECS_INSPECTOR_H

#include "agentite/ecs.h"
#include "agentite/ecs_reflect.h"
#include "agentite/ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

/* Inspector context (opaque) */
typedef struct Agentite_Inspector Agentite_Inspector;

/* Inspector configuration */
typedef struct Agentite_InspectorConfig {
    float entity_list_width;    /* Width of entity list panel (default: 200) */
    float inspector_width;      /* Width of inspector panel (default: 300) */
    bool show_entity_ids;       /* Show raw entity IDs (default: false) */
    bool show_component_sizes;  /* Show component byte sizes (default: false) */
    bool show_field_types;      /* Show field type names (default: false) */
    int max_entities;           /* Maximum entities to display (default: 1000) */
} Agentite_InspectorConfig;

#define AGENTITE_INSPECTOR_CONFIG_DEFAULT { \
    .entity_list_width = 200.0f, \
    .inspector_width = 300.0f, \
    .show_entity_ids = false, \
    .show_component_sizes = false, \
    .show_field_types = false, \
    .max_entities = 1000 \
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * Create an inspector instance.
 *
 * @param world    The ECS world to inspect
 * @param registry Component reflection registry
 * @param config   Configuration (NULL for defaults)
 * @return New inspector, or NULL on failure
 */
Agentite_Inspector *agentite_inspector_create(
    Agentite_World *world,
    Agentite_ReflectRegistry *registry,
    const Agentite_InspectorConfig *config);

/**
 * Destroy an inspector instance.
 * Safe to pass NULL.
 */
void agentite_inspector_destroy(Agentite_Inspector *inspector);

/* ============================================================================
 * Entity Selection
 * ============================================================================ */

/**
 * Set the currently selected entity.
 */
void agentite_inspector_select(Agentite_Inspector *inspector, ecs_entity_t entity);

/**
 * Get the currently selected entity (0 if none).
 */
ecs_entity_t agentite_inspector_get_selected(const Agentite_Inspector *inspector);

/**
 * Clear selection.
 */
void agentite_inspector_clear_selection(Agentite_Inspector *inspector);

/* ============================================================================
 * UI Drawing (Immediate Mode)
 * ============================================================================ */

/**
 * Draw the entity list panel.
 *
 * Shows a scrollable list of all entities with optional name filter.
 * Clicking an entity selects it.
 *
 * @param inspector The inspector instance
 * @param ui        UI context
 * @param x, y      Panel position
 * @param w, h      Panel size
 * @return true if selection changed this frame
 */
bool agentite_inspector_entity_list(Agentite_Inspector *inspector,
                                     AUI_Context *ui,
                                     float x, float y, float w, float h);

/**
 * Draw the inspector panel for the selected entity.
 *
 * Shows entity info and all components with their field values.
 * Components are collapsible.
 *
 * @param inspector The inspector instance
 * @param ui        UI context
 * @param x, y      Panel position
 * @param w, h      Panel size
 */
void agentite_inspector_panel(Agentite_Inspector *inspector,
                               AUI_Context *ui,
                               float x, float y, float w, float h);

/**
 * Draw combined inspector (entity list + inspector side by side).
 *
 * Convenience function that draws both panels.
 *
 * @param inspector The inspector instance
 * @param ui        UI context
 * @param x, y      Overall position
 * @param w, h      Overall size (divided between panels)
 */
void agentite_inspector_draw(Agentite_Inspector *inspector,
                              AUI_Context *ui,
                              float x, float y, float w, float h);

/* ============================================================================
 * Entity Filtering
 * ============================================================================ */

/**
 * Filter entities by name substring (case-insensitive).
 * Pass NULL or empty string to clear filter.
 */
void agentite_inspector_set_name_filter(Agentite_Inspector *inspector,
                                         const char *filter);

/**
 * Filter entities to only those with a specific component.
 * Pass 0 to clear component filter.
 */
void agentite_inspector_require_component(Agentite_Inspector *inspector,
                                           ecs_entity_t component_id);

/**
 * Clear all filters.
 */
void agentite_inspector_clear_filters(Agentite_Inspector *inspector);

/* ============================================================================
 * Cache Control
 * ============================================================================ */

/**
 * Force refresh of the entity cache.
 * Normally the cache updates automatically, but this can be used
 * after bulk entity operations.
 */
void agentite_inspector_refresh(Agentite_Inspector *inspector);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_ECS_INSPECTOR_H */
