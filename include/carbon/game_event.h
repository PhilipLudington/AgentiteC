#ifndef CARBON_GAME_EVENT_H
#define CARBON_GAME_EVENT_H

#include <stdbool.h>

// Maximum values
#define CARBON_EVENT_MAX_CHOICES 4
#define CARBON_EVENT_MAX_EFFECTS 16
#define CARBON_EVENT_MAX_VARS 16

// Effect types are game-defined via indices
typedef struct Carbon_EventEffect {
    int type;             // Game-defined effect type enum
    float value;          // Effect magnitude
} Carbon_EventEffect;

// Player choice
typedef struct Carbon_EventChoice {
    char label[64];
    char description[256];
    Carbon_EventEffect effects[CARBON_EVENT_MAX_EFFECTS];
    int effect_count;
} Carbon_EventChoice;

// Event definition (loadable from config)
typedef struct Carbon_EventDef {
    char id[64];
    char name[128];
    char description[512];
    char trigger[256];    // Expression: "health < 0.2" or "turn > 10"
    Carbon_EventChoice choices[CARBON_EVENT_MAX_CHOICES];
    int choice_count;
    bool one_shot;        // Only trigger once per game
    int cooldown;         // Turns before event can trigger again
    int priority;         // Higher = checked first
} Carbon_EventDef;

// Trigger context - game fills with current values
typedef struct Carbon_TriggerContext {
    const char *var_names[CARBON_EVENT_MAX_VARS];
    float var_values[CARBON_EVENT_MAX_VARS];
    int var_count;
} Carbon_TriggerContext;

// Active event awaiting player choice
typedef struct Carbon_ActiveEvent {
    const Carbon_EventDef *def;
    bool resolved;
    int choice_made;      // -1 if not yet chosen
} Carbon_ActiveEvent;

// Event manager
typedef struct Carbon_EventManager Carbon_EventManager;

Carbon_EventManager *carbon_event_create(void);
void carbon_event_destroy(Carbon_EventManager *em);

// Register event definitions (typically from data loader)
void carbon_event_register(Carbon_EventManager *em, const Carbon_EventDef *def);

// Set minimum turns between events (default: 0)
void carbon_event_set_cooldown_between(Carbon_EventManager *em, int turns);

// Check triggers and potentially activate an event
// Returns true if a new event was triggered
bool carbon_event_check_triggers(Carbon_EventManager *em, const Carbon_TriggerContext *ctx);

// Query active event
bool carbon_event_has_pending(const Carbon_EventManager *em);
const Carbon_ActiveEvent *carbon_event_get_pending(const Carbon_EventManager *em);

// Make a choice (returns true if valid choice)
bool carbon_event_choose(Carbon_EventManager *em, int choice_index);

// Get the chosen choice's effects (call after carbon_event_choose)
const Carbon_EventChoice *carbon_event_get_chosen(const Carbon_EventManager *em);

// Clear resolved event (call after applying effects)
void carbon_event_clear_pending(Carbon_EventManager *em);

// Reset all event state (one-shots, cooldowns)
void carbon_event_reset(Carbon_EventManager *em);

// Expression evaluation (public for game use)
// Evaluates expressions like "health < 0.2", "turn >= 10 && score > 100"
bool carbon_event_evaluate(const char *expr, const Carbon_TriggerContext *ctx);

// Helper to add variable to context
void carbon_trigger_context_add(Carbon_TriggerContext *ctx, const char *name, float value);

// Helper to clear context
void carbon_trigger_context_clear(Carbon_TriggerContext *ctx);

#endif // CARBON_GAME_EVENT_H
