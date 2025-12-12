/**
 * Strategy-Sim Example
 *
 * Demonstrates the Agentite engine's strategy game systems:
 * - Turn-based game loop with phases
 * - Resource management (money, research points)
 * - Modifier stacking for policy effects
 * - Event system with triggers and choices
 * - Tech tree with prerequisites
 * - History tracking for graphs
 * - Save/load game state
 *
 * Controls:
 * - SPACE: Advance turn
 * - S: Save game
 * - L: Load game
 * - 1-9: Select policy/event choice
 * - ESC: Quit
 */

#include "agentite/agentite.h"
#include "agentite/text.h"
#include "agentite/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Effect types
typedef enum {
    EFFECT_EMISSIONS = 0,
    EFFECT_INCOME = 1,
    EFFECT_APPROVAL = 2,
    EFFECT_RESEARCH_RATE = 3
} EffectType;

// Metrics for history tracking
typedef enum {
    METRIC_EMISSIONS = 0,
    METRIC_MONEY = 1,
    METRIC_APPROVAL = 2,
    METRIC_RESEARCH = 3,
    METRIC_COUNT
} MetricIndex;

// Game state
typedef struct {
    Agentite_TurnManager turns;
    Agentite_Resource money;
    Agentite_Resource research_points;

    float emissions;           // 0.0 to 1.0 (target: reduce to 0)
    float approval;            // 0.0 to 1.0 (public approval rating)

    Agentite_ModifierStack emissions_modifiers;
    Agentite_ModifierStack income_modifiers;

    Agentite_EventManager *events;
    Agentite_UnlockTree *tech_tree;
    Agentite_ResearchProgress research;

    Agentite_History *history;
    Agentite_SaveManager *saves;

    bool awaiting_choice;
    bool game_won;
    bool game_lost;
} GameState;

// Policy definition for loading
typedef struct {
    char id[64];
    char name[128];
    char description[256];
    int cost;
    char category[32];
    int effect_type;
    float effect_value;
} PolicyDef;

// Global game state
static GameState game;

// Parse policy from TOML (unused in demo, but shows pattern)
__attribute__((unused))
static bool parse_policy(const char *key, toml_table_t *table, void *out, void *userdata) {
    (void)key;
    (void)userdata;
    PolicyDef *p = (PolicyDef *)out;

    agentite_toml_get_string(table, "id", p->id, sizeof(p->id));
    agentite_toml_get_string(table, "name", p->name, sizeof(p->name));
    agentite_toml_get_string(table, "description", p->description, sizeof(p->description));
    agentite_toml_get_int(table, "cost", &p->cost);
    agentite_toml_get_string(table, "category", p->category, sizeof(p->category));
    agentite_toml_get_int(table, "effect_type", &p->effect_type);
    agentite_toml_get_float(table, "effect_value", &p->effect_value);

    return true;
}

// Parse tech from TOML
static bool parse_tech(const char *key, toml_table_t *table, void *out, void *userdata) {
    (void)key;
    (void)userdata;
    Agentite_UnlockDef *t = (Agentite_UnlockDef *)out;

    agentite_toml_get_string(table, "id", t->id, sizeof(t->id));
    agentite_toml_get_string(table, "name", t->name, sizeof(t->name));
    agentite_toml_get_string(table, "description", t->description, sizeof(t->description));
    agentite_toml_get_string(table, "category", t->category, sizeof(t->category));
    agentite_toml_get_int(table, "cost", &t->cost);
    agentite_toml_get_int(table, "effect_type", &t->effect_type);
    agentite_toml_get_float(table, "effect_value", &t->effect_value);

    // Parse prerequisites array
    char **prereqs = NULL;
    int prereq_count = 0;
    if (agentite_toml_get_string_array(table, "prerequisites", &prereqs, &prereq_count)) {
        t->prereq_count = prereq_count > AGENTITE_UNLOCK_MAX_PREREQS ?
                          AGENTITE_UNLOCK_MAX_PREREQS : prereq_count;
        for (int i = 0; i < t->prereq_count; i++) {
            strncpy(t->prerequisites[i], prereqs[i], sizeof(t->prerequisites[i]) - 1);
        }
        agentite_toml_free_strings(prereqs, prereq_count);
    }

    return true;
}

// Initialize game state
static void game_init(void) {
    // Turn manager
    agentite_turn_init(&game.turns);

    // Resources
    agentite_resource_init(&game.money, 100, 0, 20);        // Start with 100, gain 20/turn
    agentite_resource_init(&game.research_points, 0, 0, 5); // Gain 5/turn

    // Starting values
    game.emissions = 0.8f;  // High emissions to start
    game.approval = 0.5f;   // Neutral approval

    // Modifier stacks
    agentite_modifier_init(&game.emissions_modifiers);
    agentite_modifier_init(&game.income_modifiers);

    // Event manager
    game.events = agentite_event_create();
    agentite_event_set_cooldown_between(game.events, 2);  // 2 turns between events

    // Tech tree
    game.tech_tree = agentite_unlock_create();

    // Load tech definitions
    Agentite_DataLoader *tech_loader = agentite_data_create();
    if (agentite_data_load(tech_loader, "examples/strategy-sim/data/techs.toml",
                         "tech", sizeof(Agentite_UnlockDef), parse_tech, NULL)) {
        for (size_t i = 0; i < agentite_data_count(tech_loader); i++) {
            Agentite_UnlockDef *def = (Agentite_UnlockDef *)agentite_data_get_by_index(tech_loader, i);
            agentite_unlock_register(game.tech_tree, def);
        }
        SDL_Log("Loaded %zu technologies", agentite_data_count(tech_loader));
    }
    agentite_data_destroy(tech_loader);

    // History
    game.history = agentite_history_create();
    agentite_history_set_metric_name(game.history, METRIC_EMISSIONS, "Emissions");
    agentite_history_set_metric_name(game.history, METRIC_MONEY, "Money");
    agentite_history_set_metric_name(game.history, METRIC_APPROVAL, "Approval");
    agentite_history_set_metric_name(game.history, METRIC_RESEARCH, "Research");

    // Save manager
    game.saves = agentite_save_create("saves");
    agentite_save_set_version(game.saves, 1, 1);

    game.awaiting_choice = false;
    game.game_won = false;
    game.game_lost = false;
}

// Record current metrics to history
static void record_history_snapshot(void) {
    Agentite_MetricSnapshot snap = {0};
    snap.turn = game.turns.turn_number;
    snap.values[METRIC_EMISSIONS] = game.emissions;
    snap.values[METRIC_MONEY] = (float)game.money.current;
    snap.values[METRIC_APPROVAL] = game.approval;
    snap.values[METRIC_RESEARCH] = (float)game.research_points.current;

    agentite_history_add_snapshot(game.history, &snap);
}

// Apply modifier effects
static void apply_modifiers(void) {
    // Apply emissions modifiers
    float emissions_change = agentite_modifier_total(&game.emissions_modifiers);
    game.emissions += game.emissions * emissions_change * 0.1f;  // 10% of modifier per turn
    if (game.emissions < 0.0f) game.emissions = 0.0f;
    if (game.emissions > 1.0f) game.emissions = 1.0f;

    // Apply income modifiers
    float income_mod = 1.0f + agentite_modifier_total(&game.income_modifiers);
    agentite_resource_set_modifier(&game.money, income_mod);
}

// Check win/lose conditions
static void check_end_conditions(void) {
    if (game.emissions <= 0.05f && game.approval > 0.3f) {
        game.game_won = true;
        agentite_history_add_event_ex(game.history, game.turns.turn_number, 0,
                                     "Victory!", "Emissions reduced to near zero!",
                                     0.8f, game.emissions);
    }

    if (game.approval <= 0.0f) {
        game.game_lost = true;
        agentite_history_add_event_ex(game.history, game.turns.turn_number, 1,
                                     "Defeat", "Lost public support entirely.",
                                     0.5f, game.approval);
    }
}

// Serialize game state for saving
static bool serialize_game(void *gs, Agentite_SaveWriter *writer) {
    GameState *g = (GameState *)gs;

    agentite_save_write_int(writer, "turn", g->turns.turn_number);
    agentite_save_write_int(writer, "money", g->money.current);
    agentite_save_write_int(writer, "research", g->research_points.current);
    agentite_save_write_float(writer, "emissions", g->emissions);
    agentite_save_write_float(writer, "approval", g->approval);

    return true;
}

// Deserialize game state when loading
static bool deserialize_game(void *gs, Agentite_SaveReader *reader) {
    GameState *g = (GameState *)gs;

    agentite_save_read_int(reader, "turn", &g->turns.turn_number);
    agentite_save_read_int(reader, "money", &g->money.current);
    agentite_save_read_int(reader, "research", &g->research_points.current);
    agentite_save_read_float(reader, "emissions", &g->emissions);
    agentite_save_read_float(reader, "approval", &g->approval);

    return true;
}

// Process one game turn
static void process_turn(void) {
    SDL_Log("Processing turn %d...", game.turns.turn_number);

    // Record state before turn
    record_history_snapshot();

    // World update phase - apply modifiers and tick resources
    apply_modifiers();
    agentite_resource_tick(&game.money);
    agentite_resource_tick(&game.research_points);

    // Process ongoing research
    if (agentite_unlock_is_researching(&game.research)) {
        if (agentite_unlock_add_points(game.tech_tree, &game.research, 5)) {
            SDL_Log("Research completed: %s", game.research.current_id);

            // Apply tech effect
            const Agentite_UnlockDef *tech = agentite_unlock_find(game.tech_tree,
                                                               game.research.current_id);
            if (tech && tech->effect_type == EFFECT_EMISSIONS) {
                char source[64];
                snprintf(source, sizeof(source), "tech_%s", tech->id);
                agentite_modifier_add(&game.emissions_modifiers, source, tech->effect_value);
            }
        }
    }

    // Event phase - check triggers
    Agentite_TriggerContext ctx = {0};
    agentite_trigger_context_add(&ctx, "turn", (float)game.turns.turn_number);
    agentite_trigger_context_add(&ctx, "emissions", game.emissions);
    agentite_trigger_context_add(&ctx, "approval", game.approval);
    agentite_trigger_context_add(&ctx, "research_points", (float)game.research_points.current);

    if (agentite_event_check_triggers(game.events, &ctx)) {
        const Agentite_ActiveEvent *event = agentite_event_get_pending(game.events);
        SDL_Log("EVENT: %s - %s", event->def->name, event->def->description);
        for (int i = 0; i < event->def->choice_count; i++) {
            SDL_Log("  [%d] %s - %s", i + 1,
                   event->def->choices[i].label,
                   event->def->choices[i].description);
        }
        game.awaiting_choice = true;
    }

    // End check phase
    check_end_conditions();

    // Advance turn
    game.turns.turn_number++;
    SDL_Log("Turn %d started. Money: %d, Research: %d, Emissions: %.0f%%, Approval: %.0f%%",
            game.turns.turn_number, game.money.current, game.research_points.current,
            game.emissions * 100.0f, game.approval * 100.0f);
}

// Handle event choice
static void handle_event_choice(int choice) {
    if (!game.awaiting_choice) return;

    if (agentite_event_choose(game.events, choice)) {
        const Agentite_EventChoice *chosen = agentite_event_get_chosen(game.events);
        if (chosen) {
            // Apply effects
            for (int i = 0; i < chosen->effect_count; i++) {
                switch (chosen->effects[i].type) {
                    case EFFECT_EMISSIONS:
                        game.emissions += chosen->effects[i].value;
                        break;
                    case EFFECT_INCOME:
                        agentite_resource_add(&game.money, (int)chosen->effects[i].value);
                        break;
                    case EFFECT_APPROVAL:
                        game.approval += chosen->effects[i].value;
                        break;
                    default:
                        break;
                }
            }
        }
        agentite_event_clear_pending(game.events);
        game.awaiting_choice = false;
    }
}

// Start researching a tech
static void start_research(const char *tech_id) {
    if (agentite_unlock_can_research(game.tech_tree, tech_id)) {
        agentite_unlock_start_research(game.tech_tree, &game.research, tech_id);
        SDL_Log("Started researching: %s", tech_id);
    } else {
        SDL_Log("Cannot research: %s (already completed or missing prereqs)", tech_id);
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // Initialize engine
    Agentite_Config config = AGENTITE_DEFAULT_CONFIG;
    config.window_title = "Strategy Sim - Agentite Engine Demo";
    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    // Initialize text rendering
    Agentite_TextRenderer *text = agentite_text_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine)
    );
    Agentite_Font *font = agentite_font_load(text, "assets/fonts/Roboto-Regular.ttf", 18.0f);
    if (!font) {
        fprintf(stderr, "Failed to load font\n");
        agentite_shutdown(engine);
        return 1;
    }

    // Initialize input
    Agentite_Input *input = agentite_input_init();

    // Initialize game
    game_init();

    SDL_Log("=== Strategy Sim Demo ===");
    SDL_Log("SPACE: Advance turn | S: Save | L: Load | ESC: Quit");
    SDL_Log("1-9: Event choices | R: Start research");

    // Debug: Check available techs at start
    const Agentite_UnlockDef *initial_techs[10];
    int initial_count = agentite_unlock_get_available(game.tech_tree, initial_techs, 10);
    SDL_Log("Available techs at start: %d", initial_count);
    for (int i = 0; i < initial_count; i++) {
        SDL_Log("  - %s: %s", initial_techs[i]->id, initial_techs[i]->name);
    }

    // Main loop
    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);

        // Input handling
        agentite_input_begin_frame(input);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }
        agentite_input_update(input);

        // Handle key presses
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE)) {
            agentite_quit(engine);
        }

        if (!game.game_won && !game.game_lost) {
            if (game.awaiting_choice) {
                // Handle event choices 1-9
                for (int i = 0; i < 9; i++) {
                    if (agentite_input_key_just_pressed(input, (SDL_Scancode)(SDL_SCANCODE_1 + i))) {
                        handle_event_choice(i);
                    }
                }
            } else {
                // Normal game controls
                if (agentite_input_key_just_pressed(input, SDL_SCANCODE_SPACE)) {
                    process_turn();
                }

                if (agentite_input_key_just_pressed(input, SDL_SCANCODE_R)) {
                    // Find first available tech
                    const Agentite_UnlockDef *available[10];
                    int count = agentite_unlock_get_available(game.tech_tree, available, 10);
                    SDL_Log("R pressed - available techs: %d", count);
                    if (count > 0) {
                        SDL_Log("Starting research: %s", available[0]->id);
                        start_research(available[0]->id);
                    } else {
                        SDL_Log("No techs available to research");
                    }
                }
            }
        }

        // Save/Load
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_S)) {
            Agentite_SaveResult result = agentite_save_quick(game.saves, serialize_game, &game);
            if (result.success) {
                SDL_Log("Game saved: %s", result.filepath);
            } else {
                SDL_Log("Save failed: %s", result.error_message);
            }
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_L)) {
            Agentite_SaveResult result = agentite_load_quick(game.saves, deserialize_game, &game);
            if (result.success) {
                SDL_Log("Game loaded from: %s", result.filepath);
            } else {
                SDL_Log("Load failed: %s", result.error_message);
            }
        }

        // Rendering
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);

        // Begin text batch
        agentite_text_begin(text);

        // Draw game state
        float y = 20.0f;

        if (game.game_won) {
            agentite_text_draw_colored(text, font, "VICTORY! Emissions eliminated!",
                                      20.0f, y, 0.2f, 1.0f, 0.2f, 1.0f);
        } else if (game.game_lost) {
            agentite_text_draw_colored(text, font, "DEFEAT! Lost public support.",
                                      20.0f, y, 1.0f, 0.2f, 0.2f, 1.0f);
        } else {
            agentite_text_printf(text, font, 20.0f, y, "Turn: %d", game.turns.turn_number);
        }
        y += 25.0f;

        agentite_text_printf(text, font, 20.0f, y, "Money: $%d (+%d/turn)",
                           game.money.current, agentite_resource_preview_tick(&game.money));
        y += 25.0f;

        agentite_text_printf(text, font, 20.0f, y, "Research: %d (+%d/turn)",
                           game.research_points.current,
                           agentite_resource_preview_tick(&game.research_points));
        y += 25.0f;

        // Color-coded emissions
        float er = game.emissions;
        float eg = 1.0f - game.emissions;
        agentite_text_printf_colored(text, font, 20.0f, y, er, eg, 0.0f, 1.0f,
                                    "Emissions: %.0f%%", game.emissions * 100.0f);
        y += 25.0f;

        // Color-coded approval
        float ar = 1.0f - game.approval;
        float ag = game.approval;
        agentite_text_printf_colored(text, font, 20.0f, y, ar, ag, 0.2f, 1.0f,
                                    "Approval: %.0f%%", game.approval * 100.0f);
        y += 35.0f;

        // Show active research
        if (agentite_unlock_is_researching(&game.research)) {
            const Agentite_UnlockDef *tech = agentite_unlock_find(game.tech_tree,
                                                               game.research.current_id);
            if (tech) {
                float progress = agentite_unlock_get_progress_percent(&game.research);
                agentite_text_printf(text, font, 20.0f, y, "Researching: %s (%.0f%%)",
                                   tech->name, progress * 100.0f);
                y += 25.0f;
            }
        }

        // Show pending event
        if (game.awaiting_choice) {
            const Agentite_ActiveEvent *evt = agentite_event_get_pending(game.events);
            if (evt && evt->def) {
                y += 10.0f;
                agentite_text_draw_colored(text, font, "=== EVENT ===",
                                          20.0f, y, 1.0f, 1.0f, 0.0f, 1.0f);
                y += 25.0f;
                agentite_text_draw(text, font, evt->def->name, 20.0f, y);
                y += 25.0f;

                for (int i = 0; i < evt->def->choice_count; i++) {
                    agentite_text_printf(text, font, 30.0f, y, "[%d] %s",
                                       i + 1, evt->def->choices[i].label);
                    y += 22.0f;
                }
            }
        }

        // Controls help
        y = 650.0f;
        agentite_text_draw_colored(text, font,
                                  "SPACE: Next Turn | S: Save | L: Load | R: Research | ESC: Quit",
                                  20.0f, y, 0.6f, 0.6f, 0.6f, 1.0f);

        agentite_text_end(text);
        agentite_text_upload(text, cmd);

        // Render
        if (agentite_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
            SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
            agentite_text_render(text, cmd, pass);
            agentite_end_render_pass(engine);
        }

        agentite_end_frame(engine);
    }

    // Cleanup
    agentite_history_destroy(game.history);
    agentite_save_destroy(game.saves);
    agentite_unlock_destroy(game.tech_tree);
    agentite_event_destroy(game.events);

    agentite_font_destroy(text, font);
    agentite_text_shutdown(text);
    agentite_input_shutdown(input);
    agentite_shutdown(engine);

    return 0;
}
