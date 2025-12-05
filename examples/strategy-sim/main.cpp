/**
 * Strategy-Sim Example
 *
 * Demonstrates the Carbon engine's strategy game systems:
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

#include "carbon/carbon.h"
#include "carbon/text.h"
#include "carbon/input.h"
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
    Carbon_TurnManager turns;
    Carbon_Resource money;
    Carbon_Resource research_points;

    float emissions;           // 0.0 to 1.0 (target: reduce to 0)
    float approval;            // 0.0 to 1.0 (public approval rating)

    Carbon_ModifierStack emissions_modifiers;
    Carbon_ModifierStack income_modifiers;

    Carbon_EventManager *events;
    Carbon_UnlockTree *tech_tree;
    Carbon_ResearchProgress research;

    Carbon_History *history;
    Carbon_SaveManager *saves;

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
    PolicyDef *p = out;

    carbon_toml_get_string(table, "id", p->id, sizeof(p->id));
    carbon_toml_get_string(table, "name", p->name, sizeof(p->name));
    carbon_toml_get_string(table, "description", p->description, sizeof(p->description));
    carbon_toml_get_int(table, "cost", &p->cost);
    carbon_toml_get_string(table, "category", p->category, sizeof(p->category));
    carbon_toml_get_int(table, "effect_type", &p->effect_type);
    carbon_toml_get_float(table, "effect_value", &p->effect_value);

    return true;
}

// Parse tech from TOML
static bool parse_tech(const char *key, toml_table_t *table, void *out, void *userdata) {
    (void)key;
    (void)userdata;
    Carbon_UnlockDef *t = out;

    carbon_toml_get_string(table, "id", t->id, sizeof(t->id));
    carbon_toml_get_string(table, "name", t->name, sizeof(t->name));
    carbon_toml_get_string(table, "description", t->description, sizeof(t->description));
    carbon_toml_get_string(table, "category", t->category, sizeof(t->category));
    carbon_toml_get_int(table, "cost", &t->cost);
    carbon_toml_get_int(table, "effect_type", &t->effect_type);
    carbon_toml_get_float(table, "effect_value", &t->effect_value);

    // Parse prerequisites array
    char **prereqs = NULL;
    int prereq_count = 0;
    if (carbon_toml_get_string_array(table, "prerequisites", &prereqs, &prereq_count)) {
        t->prereq_count = prereq_count > CARBON_UNLOCK_MAX_PREREQS ?
                          CARBON_UNLOCK_MAX_PREREQS : prereq_count;
        for (int i = 0; i < t->prereq_count; i++) {
            strncpy(t->prerequisites[i], prereqs[i], sizeof(t->prerequisites[i]) - 1);
        }
        carbon_toml_free_strings(prereqs, prereq_count);
    }

    return true;
}

// Initialize game state
static void game_init(void) {
    // Turn manager
    carbon_turn_init(&game.turns);

    // Resources
    carbon_resource_init(&game.money, 100, 0, 20);        // Start with 100, gain 20/turn
    carbon_resource_init(&game.research_points, 0, 0, 5); // Gain 5/turn

    // Starting values
    game.emissions = 0.8f;  // High emissions to start
    game.approval = 0.5f;   // Neutral approval

    // Modifier stacks
    carbon_modifier_init(&game.emissions_modifiers);
    carbon_modifier_init(&game.income_modifiers);

    // Event manager
    game.events = carbon_event_create();
    carbon_event_set_cooldown_between(game.events, 2);  // 2 turns between events

    // Tech tree
    game.tech_tree = carbon_unlock_create();

    // Load tech definitions
    Carbon_DataLoader *tech_loader = carbon_data_create();
    if (carbon_data_load(tech_loader, "examples/strategy-sim/data/techs.toml",
                         "tech", sizeof(Carbon_UnlockDef), parse_tech, NULL)) {
        for (size_t i = 0; i < carbon_data_count(tech_loader); i++) {
            Carbon_UnlockDef *def = carbon_data_get_by_index(tech_loader, i);
            carbon_unlock_register(game.tech_tree, def);
        }
        printf("Loaded %zu technologies\n", carbon_data_count(tech_loader));
    }
    carbon_data_destroy(tech_loader);

    // History
    game.history = carbon_history_create();
    carbon_history_set_metric_name(game.history, METRIC_EMISSIONS, "Emissions");
    carbon_history_set_metric_name(game.history, METRIC_MONEY, "Money");
    carbon_history_set_metric_name(game.history, METRIC_APPROVAL, "Approval");
    carbon_history_set_metric_name(game.history, METRIC_RESEARCH, "Research");

    // Save manager
    game.saves = carbon_save_create("saves");
    carbon_save_set_version(game.saves, 1, 1);

    game.awaiting_choice = false;
    game.game_won = false;
    game.game_lost = false;
}

// Record current metrics to history
static void record_history_snapshot(void) {
    Carbon_MetricSnapshot snap = {0};
    snap.turn = game.turns.turn_number;
    snap.values[METRIC_EMISSIONS] = game.emissions;
    snap.values[METRIC_MONEY] = (float)game.money.current;
    snap.values[METRIC_APPROVAL] = game.approval;
    snap.values[METRIC_RESEARCH] = (float)game.research_points.current;

    carbon_history_add_snapshot(game.history, &snap);
}

// Apply modifier effects
static void apply_modifiers(void) {
    // Apply emissions modifiers
    float emissions_change = carbon_modifier_total(&game.emissions_modifiers);
    game.emissions += game.emissions * emissions_change * 0.1f;  // 10% of modifier per turn
    if (game.emissions < 0.0f) game.emissions = 0.0f;
    if (game.emissions > 1.0f) game.emissions = 1.0f;

    // Apply income modifiers
    float income_mod = 1.0f + carbon_modifier_total(&game.income_modifiers);
    carbon_resource_set_modifier(&game.money, income_mod);
}

// Check win/lose conditions
static void check_end_conditions(void) {
    if (game.emissions <= 0.05f && game.approval > 0.3f) {
        game.game_won = true;
        carbon_history_add_event_ex(game.history, game.turns.turn_number, 0,
                                     "Victory!", "Emissions reduced to near zero!",
                                     0.8f, game.emissions);
    }

    if (game.approval <= 0.0f) {
        game.game_lost = true;
        carbon_history_add_event_ex(game.history, game.turns.turn_number, 1,
                                     "Defeat", "Lost public support entirely.",
                                     0.5f, game.approval);
    }
}

// Serialize game state for saving
static bool serialize_game(void *gs, Carbon_SaveWriter *writer) {
    GameState *g = gs;

    carbon_save_write_int(writer, "turn", g->turns.turn_number);
    carbon_save_write_int(writer, "money", g->money.current);
    carbon_save_write_int(writer, "research", g->research_points.current);
    carbon_save_write_float(writer, "emissions", g->emissions);
    carbon_save_write_float(writer, "approval", g->approval);

    return true;
}

// Deserialize game state when loading
static bool deserialize_game(void *gs, Carbon_SaveReader *reader) {
    GameState *g = gs;

    carbon_save_read_int(reader, "turn", &g->turns.turn_number);
    carbon_save_read_int(reader, "money", &g->money.current);
    carbon_save_read_int(reader, "research", &g->research_points.current);
    carbon_save_read_float(reader, "emissions", &g->emissions);
    carbon_save_read_float(reader, "approval", &g->approval);

    return true;
}

// Process one game turn
static void process_turn(void) {
    // Record state before turn
    record_history_snapshot();

    // World update phase - apply modifiers and tick resources
    apply_modifiers();
    carbon_resource_tick(&game.money);
    carbon_resource_tick(&game.research_points);

    // Process ongoing research
    if (carbon_unlock_is_researching(&game.research)) {
        if (carbon_unlock_add_points(game.tech_tree, &game.research, 5)) {
            printf("Research completed!\n");

            // Apply tech effect
            const Carbon_UnlockDef *tech = carbon_unlock_find(game.tech_tree,
                                                               game.research.current_id);
            if (tech && tech->effect_type == EFFECT_EMISSIONS) {
                char source[64];
                snprintf(source, sizeof(source), "tech_%s", tech->id);
                carbon_modifier_add(&game.emissions_modifiers, source, tech->effect_value);
            }
        }
    }

    // Event phase - check triggers
    Carbon_TriggerContext ctx = {0};
    carbon_trigger_context_add(&ctx, "turn", (float)game.turns.turn_number);
    carbon_trigger_context_add(&ctx, "emissions", game.emissions);
    carbon_trigger_context_add(&ctx, "approval", game.approval);
    carbon_trigger_context_add(&ctx, "research_points", (float)game.research_points.current);

    if (carbon_event_check_triggers(game.events, &ctx)) {
        const Carbon_ActiveEvent *event = carbon_event_get_pending(game.events);
        printf("\n=== EVENT: %s ===\n%s\n", event->def->name, event->def->description);
        for (int i = 0; i < event->def->choice_count; i++) {
            printf("[%d] %s - %s\n", i + 1,
                   event->def->choices[i].label,
                   event->def->choices[i].description);
        }
        game.awaiting_choice = true;
    }

    // End check phase
    check_end_conditions();

    // Advance turn
    game.turns.turn_number++;
}

// Handle event choice
static void handle_event_choice(int choice) {
    if (!game.awaiting_choice) return;

    if (carbon_event_choose(game.events, choice)) {
        const Carbon_EventChoice *chosen = carbon_event_get_chosen(game.events);
        if (chosen) {
            // Apply effects
            for (int i = 0; i < chosen->effect_count; i++) {
                switch (chosen->effects[i].type) {
                    case EFFECT_EMISSIONS:
                        game.emissions += chosen->effects[i].value;
                        break;
                    case EFFECT_INCOME:
                        carbon_resource_add(&game.money, (int)chosen->effects[i].value);
                        break;
                    case EFFECT_APPROVAL:
                        game.approval += chosen->effects[i].value;
                        break;
                    default:
                        break;
                }
            }
        }
        carbon_event_clear_pending(game.events);
        game.awaiting_choice = false;
    }
}

// Start researching a tech
static void start_research(const char *tech_id) {
    if (carbon_unlock_can_research(game.tech_tree, tech_id)) {
        carbon_unlock_start_research(game.tech_tree, &game.research, tech_id);
        printf("Started researching: %s\n", tech_id);
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // Initialize engine
    Carbon_Config config = CARBON_DEFAULT_CONFIG;
    config.window_title = "Strategy Sim - Carbon Engine Demo";
    Carbon_Engine *engine = carbon_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    // Initialize text rendering
    Carbon_TextRenderer *text = carbon_text_init(
        carbon_get_gpu_device(engine),
        carbon_get_window(engine)
    );
    Carbon_Font *font = carbon_font_load(text, "assets/fonts/Roboto-Regular.ttf", 18.0f);
    if (!font) {
        fprintf(stderr, "Failed to load font\n");
        carbon_shutdown(engine);
        return 1;
    }

    // Initialize input
    Carbon_Input *input = carbon_input_init();

    // Initialize game
    game_init();

    printf("\n=== Strategy Sim Demo ===\n");
    printf("SPACE: Advance turn | S: Save | L: Load | ESC: Quit\n");
    printf("1-9: Event choices | R: Start research\n\n");

    // Main loop
    while (carbon_is_running(engine)) {
        carbon_begin_frame(engine);

        // Input handling
        carbon_input_begin_frame(input);
        carbon_poll_events(engine);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            carbon_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                carbon_quit(engine);
            }
        }
        carbon_input_update(input);

        // Handle key presses
        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE)) {
            carbon_quit(engine);
        }

        if (!game.game_won && !game.game_lost) {
            if (game.awaiting_choice) {
                // Handle event choices 1-9
                for (int i = 0; i < 9; i++) {
                    if (carbon_input_key_just_pressed(input, SDL_SCANCODE_1 + i)) {
                        handle_event_choice(i);
                    }
                }
            } else {
                // Normal game controls
                if (carbon_input_key_just_pressed(input, SDL_SCANCODE_SPACE)) {
                    process_turn();
                }

                if (carbon_input_key_just_pressed(input, SDL_SCANCODE_R)) {
                    // Find first available tech
                    const Carbon_UnlockDef *available[10];
                    int count = carbon_unlock_get_available(game.tech_tree, available, 10);
                    if (count > 0) {
                        start_research(available[0]->id);
                    }
                }
            }
        }

        // Save/Load
        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_S)) {
            Carbon_SaveResult result = carbon_save_quick(game.saves, serialize_game, &game);
            if (result.success) {
                printf("Game saved: %s\n", result.filepath);
            } else {
                printf("Save failed: %s\n", result.error_message);
            }
        }

        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_L)) {
            Carbon_SaveResult result = carbon_load_quick(game.saves, deserialize_game, &game);
            if (result.success) {
                printf("Game loaded from: %s\n", result.filepath);
            } else {
                printf("Load failed: %s\n", result.error_message);
            }
        }

        // Rendering
        SDL_GPUCommandBuffer *cmd = carbon_acquire_command_buffer(engine);

        // Begin text batch
        carbon_text_begin(text);

        // Draw game state
        float y = 20.0f;

        if (game.game_won) {
            carbon_text_draw_colored(text, font, "VICTORY! Emissions eliminated!",
                                      20.0f, y, 0.2f, 1.0f, 0.2f, 1.0f);
        } else if (game.game_lost) {
            carbon_text_draw_colored(text, font, "DEFEAT! Lost public support.",
                                      20.0f, y, 1.0f, 0.2f, 0.2f, 1.0f);
        } else {
            carbon_text_printf(text, font, 20.0f, y, "Turn: %d", game.turns.turn_number);
        }
        y += 25.0f;

        carbon_text_printf(text, font, 20.0f, y, "Money: $%d (+%d/turn)",
                           game.money.current, carbon_resource_preview_tick(&game.money));
        y += 25.0f;

        carbon_text_printf(text, font, 20.0f, y, "Research: %d (+%d/turn)",
                           game.research_points.current,
                           carbon_resource_preview_tick(&game.research_points));
        y += 25.0f;

        // Color-coded emissions
        float er = game.emissions;
        float eg = 1.0f - game.emissions;
        carbon_text_printf_colored(text, font, 20.0f, y, er, eg, 0.0f, 1.0f,
                                    "Emissions: %.0f%%", game.emissions * 100.0f);
        y += 25.0f;

        // Color-coded approval
        float ar = 1.0f - game.approval;
        float ag = game.approval;
        carbon_text_printf_colored(text, font, 20.0f, y, ar, ag, 0.2f, 1.0f,
                                    "Approval: %.0f%%", game.approval * 100.0f);
        y += 35.0f;

        // Show active research
        if (carbon_unlock_is_researching(&game.research)) {
            const Carbon_UnlockDef *tech = carbon_unlock_find(game.tech_tree,
                                                               game.research.current_id);
            if (tech) {
                float progress = carbon_unlock_get_progress_percent(&game.research);
                carbon_text_printf(text, font, 20.0f, y, "Researching: %s (%.0f%%)",
                                   tech->name, progress * 100.0f);
                y += 25.0f;
            }
        }

        // Show pending event
        if (game.awaiting_choice) {
            const Carbon_ActiveEvent *evt = carbon_event_get_pending(game.events);
            if (evt && evt->def) {
                y += 10.0f;
                carbon_text_draw_colored(text, font, "=== EVENT ===",
                                          20.0f, y, 1.0f, 1.0f, 0.0f, 1.0f);
                y += 25.0f;
                carbon_text_draw(text, font, evt->def->name, 20.0f, y);
                y += 25.0f;

                for (int i = 0; i < evt->def->choice_count; i++) {
                    carbon_text_printf(text, font, 30.0f, y, "[%d] %s",
                                       i + 1, evt->def->choices[i].label);
                    y += 22.0f;
                }
            }
        }

        // Controls help
        y = 650.0f;
        carbon_text_draw_colored(text, font,
                                  "SPACE: Next Turn | S: Save | L: Load | R: Research | ESC: Quit",
                                  20.0f, y, 0.6f, 0.6f, 0.6f, 1.0f);

        carbon_text_end(text);
        carbon_text_upload(text, cmd);

        // Render
        if (carbon_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
            SDL_GPURenderPass *pass = carbon_get_render_pass(engine);
            carbon_text_render(text, cmd, pass);
            carbon_end_render_pass(engine);
        }

        carbon_end_frame(engine);
    }

    // Cleanup
    carbon_history_destroy(game.history);
    carbon_save_destroy(game.saves);
    carbon_unlock_destroy(game.tech_tree);
    carbon_event_destroy(game.events);

    carbon_font_destroy(text, font);
    carbon_text_shutdown(text);
    carbon_input_shutdown(input);
    carbon_shutdown(engine);

    return 0;
}
