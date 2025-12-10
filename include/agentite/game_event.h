#ifndef AGENTITE_GAME_EVENT_H
#define AGENTITE_GAME_EVENT_H

#include <stdbool.h>

// Maximum values
#define AGENTITE_EVENT_MAX_CHOICES 4
#define AGENTITE_EVENT_MAX_EFFECTS 16
#define AGENTITE_EVENT_MAX_VARS 16

// Effect types are game-defined via indices
typedef struct Agentite_EventEffect {
    int type;             // Game-defined effect type enum
    float value;          // Effect magnitude
} Agentite_EventEffect;

// Player choice
typedef struct Agentite_EventChoice {
    char label[64];
    char description[256];
    Agentite_EventEffect effects[AGENTITE_EVENT_MAX_EFFECTS];
    int effect_count;
} Agentite_EventChoice;

// Event definition (loadable from config)
typedef struct Agentite_EventDef {
    char id[64];
    char name[128];
    char description[512];
    char trigger[256];    // Expression: "health < 0.2" or "turn > 10"
    Agentite_EventChoice choices[AGENTITE_EVENT_MAX_CHOICES];
    int choice_count;
    bool one_shot;        // Only trigger once per game
    int cooldown;         // Turns before event can trigger again
    int priority;         // Higher = checked first
} Agentite_EventDef;

// Trigger context - game fills with current values
typedef struct Agentite_TriggerContext {
    const char *var_names[AGENTITE_EVENT_MAX_VARS];
    float var_values[AGENTITE_EVENT_MAX_VARS];
    int var_count;
} Agentite_TriggerContext;

// Active event awaiting player choice
typedef struct Agentite_ActiveEvent {
    const Agentite_EventDef *def;
    bool resolved;
    int choice_made;      // -1 if not yet chosen
} Agentite_ActiveEvent;

// Event manager
typedef struct Agentite_EventManager Agentite_EventManager;

Agentite_EventManager *agentite_event_create(void);
void agentite_event_destroy(Agentite_EventManager *em);

// Register event definitions (typically from data loader)
void agentite_event_register(Agentite_EventManager *em, const Agentite_EventDef *def);

// Set minimum turns between events (default: 0)
void agentite_event_set_cooldown_between(Agentite_EventManager *em, int turns);

// Check triggers and potentially activate an event
// Returns true if a new event was triggered
bool agentite_event_check_triggers(Agentite_EventManager *em, const Agentite_TriggerContext *ctx);

// Query active event
bool agentite_event_has_pending(const Agentite_EventManager *em);
const Agentite_ActiveEvent *agentite_event_get_pending(const Agentite_EventManager *em);

// Make a choice (returns true if valid choice)
bool agentite_event_choose(Agentite_EventManager *em, int choice_index);

// Get the chosen choice's effects (call after agentite_event_choose)
const Agentite_EventChoice *agentite_event_get_chosen(const Agentite_EventManager *em);

// Clear resolved event (call after applying effects)
void agentite_event_clear_pending(Agentite_EventManager *em);

// Reset all event state (one-shots, cooldowns)
void agentite_event_reset(Agentite_EventManager *em);

// Expression evaluation (public for game use)
// Evaluates expressions like "health < 0.2", "turn >= 10 && score > 100"
bool agentite_event_evaluate(const char *expr, const Agentite_TriggerContext *ctx);

// Helper to add variable to context
void agentite_trigger_context_add(Agentite_TriggerContext *ctx, const char *name, float value);

// Helper to clear context
void agentite_trigger_context_clear(Agentite_TriggerContext *ctx);

#endif // AGENTITE_GAME_EVENT_H
