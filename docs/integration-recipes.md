# Integration Recipes

Multi-system patterns for common game features. Each recipe shows which systems to combine and how they interact.

## Table of Contents

1. [Clickable Unit with Health Bar](#clickable-unit-with-health-bar)
2. [RTS Unit Selection and Movement](#rts-unit-selection-and-movement)
3. [Turn-Based Combat with AI](#turn-based-combat-with-ai)
4. [4X Strategy Layer](#4x-strategy-layer)
5. [Factory Game with Power Grid](#factory-game-with-power-grid)
6. [AI Agent with Goals](#ai-agent-with-goals)
7. [Fog of War with Exploration](#fog-of-war-with-exploration)
8. [Tech Tree with Unlocks](#tech-tree-with-unlocks)

---

## Clickable Unit with Health Bar

**Systems:** ECS + Sprite + UI + Input + Spatial

Shows a unit you can click to select, with a health bar rendered above it.

```c
// === COMPONENTS ===
typedef struct C_Position { float x, y; } C_Position;
typedef struct C_Health { int current, max; } C_Health;
typedef struct C_Selectable { bool selected; } C_Selectable;
typedef struct C_Sprite { Agentite_Texture *tex; } C_Sprite;

ECS_COMPONENT_DECLARE(C_Position);
ECS_COMPONENT_DECLARE(C_Health);
ECS_COMPONENT_DECLARE(C_Selectable);
ECS_COMPONENT_DECLARE(C_Sprite);

// === SETUP ===
ecs_world_t *world = ecs_init();
Agentite_SpatialIndex *spatial = agentite_spatial_create(1024, 1024, 64);
Agentite_Input *input = agentite_input_create();
AUI_Context *ui = aui_create(engine);

// Register components
ECS_COMPONENT_DEFINE(world, C_Position);
ECS_COMPONENT_DEFINE(world, C_Health);
ECS_COMPONENT_DEFINE(world, C_Selectable);
ECS_COMPONENT_DEFINE(world, C_Sprite);

// Create a unit
ecs_entity_t unit = ecs_new(world);
ecs_set(world, unit, C_Position, { .x = 100, .y = 100 });
ecs_set(world, unit, C_Health, { .current = 80, .max = 100 });
ecs_set(world, unit, C_Selectable, { .selected = false });
ecs_set(world, unit, C_Sprite, { .tex = unit_texture });

// Add to spatial index for click detection
agentite_spatial_insert(spatial, (int32_t)unit, 100, 100);

// === UPDATE LOOP ===
void update(void) {
    // Handle click to select
    if (agentite_input_mouse_pressed(input, SDL_BUTTON_LEFT)) {
        float mx, my;
        agentite_input_mouse_position(input, &mx, &my);

        // Convert screen to world coordinates
        vec2 world_pos;
        agentite_camera_screen_to_world(camera, mx, my, world_pos);

        // Query spatial index for entities at click position
        int32_t found[16];
        int count = agentite_spatial_query_point(
            spatial, world_pos[0], world_pos[1], found, 16
        );

        // Deselect all, then select clicked
        ecs_query_t *q = ecs_query(world, { .terms = {{ ecs_id(C_Selectable) }} });
        ecs_iter_t it = ecs_query_iter(world, q);
        while (ecs_query_next(&it)) {
            C_Selectable *sel = ecs_field(&it, C_Selectable, 0);
            for (int i = 0; i < it.count; i++) {
                sel[i].selected = false;
                for (int j = 0; j < count; j++) {
                    if (it.entities[i] == (ecs_entity_t)found[j]) {
                        sel[i].selected = true;
                    }
                }
            }
        }
    }
}

// === RENDER ===
void render(void) {
    // Draw sprites
    agentite_sprite_begin(sprite_renderer, camera);

    ecs_query_t *q = ecs_query(world, {
        .terms = {{ ecs_id(C_Position) }, { ecs_id(C_Sprite) }}
    });
    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_query_next(&it)) {
        C_Position *pos = ecs_field(&it, C_Position, 0);
        C_Sprite *spr = ecs_field(&it, C_Sprite, 1);
        for (int i = 0; i < it.count; i++) {
            Agentite_Sprite sprite = { .texture = spr[i].tex };
            agentite_sprite_draw(sprite_renderer, &sprite, pos[i].x, pos[i].y);
        }
    }

    agentite_sprite_upload(sprite_renderer, cmd);

    // Begin render pass
    agentite_begin_render_pass(engine, cmd, &pass);
    agentite_sprite_render(sprite_renderer, cmd, pass);

    // Draw health bars with UI (during render pass)
    q = ecs_query(world, {
        .terms = {{ ecs_id(C_Position) }, { ecs_id(C_Health) }, { ecs_id(C_Selectable) }}
    });
    it = ecs_query_iter(world, q);
    while (ecs_query_next(&it)) {
        C_Position *pos = ecs_field(&it, C_Position, 0);
        C_Health *hp = ecs_field(&it, C_Health, 1);
        C_Selectable *sel = ecs_field(&it, C_Selectable, 2);

        for (int i = 0; i < it.count; i++) {
            // Convert world to screen for UI
            vec2 screen;
            agentite_camera_world_to_screen(camera, pos[i].x, pos[i].y - 20, screen);

            // Draw health bar
            float pct = (float)hp[i].current / hp[i].max;
            aui_progress_bar(ui, screen[0] - 25, screen[1], 50, 6, pct);

            // Draw selection indicator
            if (sel[i].selected) {
                aui_rect(ui, screen[0] - 30, screen[1] + 10, 60, 60,
                         (AUI_Color){0, 255, 0, 128});
            }
        }
    }

    agentite_end_render_pass(engine, cmd, pass);
}
```

---

## RTS Unit Selection and Movement

**Systems:** ECS + Input + Pathfinding + Spatial + Sprite + Camera

Box selection, right-click-to-move with pathfinding.

```c
// === ADDITIONAL COMPONENTS ===
typedef struct C_Velocity { float x, y; } C_Velocity;
typedef struct C_Path {
    vec2 waypoints[64];
    int count, current;
} C_Path;

// === STATE ===
static bool is_box_selecting = false;
static float box_start_x, box_start_y;

// === SETUP ===
Agentite_Pathfinder *pathfinder = agentite_pathfinder_create(map_width, map_height);
// Mark obstacles
for (int y = 0; y < map_height; y++) {
    for (int x = 0; x < map_width; x++) {
        if (is_obstacle(x, y)) {
            agentite_pathfinder_set_blocked(pathfinder, x, y, true);
        }
    }
}

// === UPDATE ===
void update(float dt) {
    float mx, my;
    agentite_input_mouse_position(input, &mx, &my);
    vec2 world_mouse;
    agentite_camera_screen_to_world(camera, mx, my, world_mouse);

    // Box selection start
    if (agentite_input_mouse_pressed(input, SDL_BUTTON_LEFT)) {
        is_box_selecting = true;
        box_start_x = world_mouse[0];
        box_start_y = world_mouse[1];
    }

    // Box selection end
    if (agentite_input_mouse_released(input, SDL_BUTTON_LEFT) && is_box_selecting) {
        is_box_selecting = false;

        float min_x = fminf(box_start_x, world_mouse[0]);
        float min_y = fminf(box_start_y, world_mouse[1]);
        float max_x = fmaxf(box_start_x, world_mouse[0]);
        float max_y = fmaxf(box_start_y, world_mouse[1]);

        // Query spatial index for units in box
        int32_t found[128];
        int count = agentite_spatial_query_rect(
            spatial, min_x, min_y, max_x - min_x, max_y - min_y, found, 128
        );

        // Select found units
        for (int i = 0; i < count; i++) {
            ecs_entity_t e = (ecs_entity_t)found[i];
            if (ecs_has(world, e, C_Selectable)) {
                C_Selectable *sel = ecs_get_mut(world, e, C_Selectable);
                sel->selected = true;
            }
        }
    }

    // Right-click to move
    if (agentite_input_mouse_pressed(input, SDL_BUTTON_RIGHT)) {
        int target_x = (int)(world_mouse[0] / TILE_SIZE);
        int target_y = (int)(world_mouse[1] / TILE_SIZE);

        // Find path for each selected unit
        ecs_query_t *q = ecs_query(world, {
            .terms = {{ ecs_id(C_Position) }, { ecs_id(C_Selectable) }, { ecs_id(C_Path) }}
        });
        ecs_iter_t it = ecs_query_iter(world, q);
        while (ecs_query_next(&it)) {
            C_Position *pos = ecs_field(&it, C_Position, 0);
            C_Selectable *sel = ecs_field(&it, C_Selectable, 1);
            C_Path *path = ecs_field(&it, C_Path, 2);

            for (int i = 0; i < it.count; i++) {
                if (!sel[i].selected) continue;

                int start_x = (int)(pos[i].x / TILE_SIZE);
                int start_y = (int)(pos[i].y / TILE_SIZE);

                // Find path
                Agentite_PathResult result;
                if (agentite_pathfinder_find(pathfinder,
                        start_x, start_y, target_x, target_y, &result)) {
                    // Copy waypoints
                    path[i].count = result.length;
                    path[i].current = 0;
                    for (int j = 0; j < result.length && j < 64; j++) {
                        path[i].waypoints[j][0] = result.path[j].x * TILE_SIZE + TILE_SIZE/2;
                        path[i].waypoints[j][1] = result.path[j].y * TILE_SIZE + TILE_SIZE/2;
                    }
                }
            }
        }
    }

    // Move units along paths
    ecs_query_t *q = ecs_query(world, {
        .terms = {{ ecs_id(C_Position) }, { ecs_id(C_Path) }}
    });
    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_query_next(&it)) {
        C_Position *pos = ecs_field(&it, C_Position, 0);
        C_Path *path = ecs_field(&it, C_Path, 1);

        for (int i = 0; i < it.count; i++) {
            if (path[i].current >= path[i].count) continue;

            vec2 *target = &path[i].waypoints[path[i].current];
            float dx = (*target)[0] - pos[i].x;
            float dy = (*target)[1] - pos[i].y;
            float dist = sqrtf(dx*dx + dy*dy);

            if (dist < 5.0f) {
                path[i].current++;  // Next waypoint
            } else {
                float speed = 100.0f * dt;
                pos[i].x += (dx / dist) * speed;
                pos[i].y += (dy / dist) * speed;

                // Update spatial index
                agentite_spatial_update(spatial, (int32_t)it.entities[i],
                                        pos[i].x, pos[i].y);
            }
        }
    }
}
```

---

## Turn-Based Combat with AI

**Systems:** Combat + HTN + ECS + UI

Tactical combat where enemies telegraph actions and AI plans responses.

```c
// === SETUP ===
Agentite_CombatSystem *combat = agentite_combat_create(16, 16);
Agentite_HTNPlanner *ai_planner = agentite_htn_create();

// Define AI combat domain
agentite_htn_add_task(ai_planner, "fight", true);  // Compound task
agentite_htn_add_method(ai_planner, "fight", "attack_weakest",
    can_attack, do_attack_weakest);
agentite_htn_add_method(ai_planner, "fight", "heal_self",
    is_low_health, do_heal_self);
agentite_htn_add_method(ai_planner, "fight", "defend",
    NULL, do_defend);  // Fallback

// Add combatants
Agentite_Combatant player = {
    .name = "Hero",
    .hp = 100, .hp_max = 100,
    .initiative = 15,
    .armor = 5,
    .position = {2, 8}
};
int player_id = agentite_combat_add_combatant(combat, &player, true);

Agentite_Combatant enemy = {
    .name = "Goblin",
    .hp = 40, .hp_max = 40,
    .initiative = 8,
    .armor = 2,
    .position = {12, 8}
};
int enemy_id = agentite_combat_add_combatant(combat, &enemy, false);

agentite_combat_start(combat);

// === COMBAT LOOP ===
void combat_update(void) {
    if (agentite_combat_is_over(combat)) {
        Agentite_CombatResult result = agentite_combat_get_result(combat);
        // Handle victory/defeat
        return;
    }

    int current = agentite_combat_get_current_combatant(combat);
    const Agentite_Combatant *combatant = agentite_combat_get_combatant_const(combat, current);

    if (combatant->is_player_team) {
        // Show telegraphs (enemy intent)
        Agentite_Telegraph telegraphs[16];
        int count = agentite_combat_get_telegraphs(combat, telegraphs, 16);

        // Display telegraphs in UI
        for (int i = 0; i < count; i++) {
            // Draw attack indicator on target tile
            draw_telegraph_indicator(telegraphs[i].target_pos,
                                     telegraphs[i].predicted_damage);
        }

        // Handle player input for action selection
        if (player_selected_action) {
            Agentite_CombatAction action = {
                .type = selected_action_type,
                .actor_id = current,
                .target_id = selected_target,
            };

            if (agentite_combat_is_action_valid(combat, &action)) {
                agentite_combat_queue_action(combat, &action);
                agentite_combat_execute_turn(combat);
            }
        }
    } else {
        // AI turn - use HTN to decide action
        Agentite_HTNWorldState state = build_combat_state(combat, current);
        Agentite_HTNPlan plan;

        if (agentite_htn_plan(ai_planner, "fight", &state, &plan)) {
            // Execute first action in plan
            Agentite_CombatAction action = plan_to_action(&plan, current);
            agentite_combat_queue_action(combat, &action);
        } else {
            // Fallback: defend
            Agentite_CombatAction action = {
                .type = AGENTITE_ACTION_DEFEND,
                .actor_id = current
            };
            agentite_combat_queue_action(combat, &action);
        }

        agentite_combat_execute_turn(combat);
    }
}

// === UI RENDERING ===
void combat_render_ui(void) {
    // Turn order display
    int order[16];
    int count = agentite_combat_get_turn_order(combat, order, 16);

    for (int i = 0; i < count; i++) {
        const Agentite_Combatant *c = agentite_combat_get_combatant_const(combat, order[i]);
        bool is_current = (order[i] == agentite_combat_get_current_combatant(combat));

        aui_label(ui, 10, 10 + i * 30, c->name);
        aui_progress_bar(ui, 80, 10 + i * 30, 100, 20,
                         (float)c->hp / c->hp_max);

        if (is_current) {
            aui_rect(ui, 5, 8 + i * 30, 200, 26, (AUI_Color){255, 255, 0, 64});
        }
    }

    // Action buttons for player
    int current = agentite_combat_get_current_combatant(combat);
    const Agentite_Combatant *c = agentite_combat_get_combatant_const(combat, current);

    if (c->is_player_team) {
        if (aui_button(ui, 10, 500, 80, 30, "Attack")) {
            selected_action_type = AGENTITE_ACTION_ATTACK;
            show_target_selection = true;
        }
        if (aui_button(ui, 100, 500, 80, 30, "Defend")) {
            Agentite_CombatAction action = { .type = AGENTITE_ACTION_DEFEND, .actor_id = current };
            agentite_combat_queue_action(combat, &action);
            agentite_combat_execute_turn(combat);
        }
        if (aui_button(ui, 190, 500, 80, 30, "Move")) {
            selected_action_type = AGENTITE_ACTION_MOVE;
            show_move_tiles = true;
        }
    }
}
```

---

## 4X Strategy Layer

**Systems:** Turn + Resources + Tech + Victory + Fleet + Fog

Complete 4X game loop with exploration, expansion, exploitation, extermination.

```c
// === SETUP ===
Agentite_TurnManager *turns = agentite_turn_create(4);  // 4 players
Agentite_ResourceManager *resources = agentite_resource_create();
Agentite_TechTree *tech = agentite_tech_create();
Agentite_VictoryManager *victory = agentite_victory_create();
Agentite_FleetManager *fleets = agentite_fleet_create();
Agentite_FogOfWar *fog = agentite_fog_create(100, 100);  // Map size

// Setup resources
agentite_resource_register(resources, "credits", 1000, 10000);
agentite_resource_register(resources, "minerals", 500, 5000);
agentite_resource_register(resources, "research", 0, -1);  // No cap

// Setup tech tree
int tech_lasers = agentite_tech_add(tech, "Laser Weapons", 100, NULL, 0);
int tech_shields = agentite_tech_add(tech, "Shield Tech", 150, NULL, 0);
int prereqs[] = { tech_lasers, tech_shields };
int tech_plasma = agentite_tech_add(tech, "Plasma Weapons", 300, prereqs, 2);

// Setup victory conditions
agentite_victory_add_condition(victory, "conquest",
    check_conquest, 0.0f);  // Eliminate all enemies
agentite_victory_add_condition(victory, "tech",
    check_tech_victory, 0.0f);  // Research final tech
agentite_victory_add_condition(victory, "economic",
    check_economic, 0.0f);  // Accumulate 100,000 credits

// === TURN STRUCTURE ===
void process_turn(void) {
    int current_player = agentite_turn_get_current(turns);
    Agentite_TurnPhase phase = agentite_turn_get_phase(turns);

    switch (phase) {
        case AGENTITE_PHASE_BEGIN:
            // Collect income
            int income = calculate_income(current_player);
            agentite_resource_add(resources, current_player, "credits", income);

            // Update fog (reveal around owned sectors)
            update_player_visibility(fog, current_player);

            agentite_turn_next_phase(turns);
            break;

        case AGENTITE_PHASE_MAIN:
            // Player actions (handled by input)
            // - Move fleets
            // - Research tech
            // - Build units
            // - Colonize
            break;

        case AGENTITE_PHASE_COMBAT:
            // Resolve fleet battles
            resolve_all_battles(fleets, current_player);
            agentite_turn_next_phase(turns);
            break;

        case AGENTITE_PHASE_END:
            // Apply tech research
            int research = agentite_resource_get(resources, current_player, "research");
            int active_tech = get_researching_tech(current_player);
            if (active_tech >= 0) {
                if (agentite_tech_add_progress(tech, current_player, active_tech, research)) {
                    // Tech completed!
                    apply_tech_bonuses(current_player, active_tech);
                }
            }

            // Check victory
            int winner;
            const char *condition;
            if (agentite_victory_check(victory, &winner, &condition)) {
                handle_victory(winner, condition);
            }

            agentite_turn_end(turns);
            break;
    }
}

// === FLEET BATTLE RESOLUTION ===
void resolve_all_battles(Agentite_FleetManager *fm, int player) {
    // Find player fleets in same sector as enemy fleets
    int player_fleets[32];
    int count = agentite_fleet_get_by_owner(fm, player, player_fleets, 32);

    for (int i = 0; i < count; i++) {
        const Agentite_Fleet *fleet = agentite_fleet_get_const(fm, player_fleets[i]);

        // Check for enemy fleets in same sector
        for (int enemy = 0; enemy < 4; enemy++) {
            if (enemy == player) continue;

            int enemy_fleets[32];
            int enemy_count = agentite_fleet_get_by_owner(fm, enemy, enemy_fleets, 32);

            for (int j = 0; j < enemy_count; j++) {
                const Agentite_Fleet *enemy_fleet = agentite_fleet_get_const(fm, enemy_fleets[j]);

                if (fleet->sector_id == enemy_fleet->sector_id) {
                    // Battle!
                    Agentite_BattlePreview preview;
                    agentite_fleet_preview_battle(fm, player_fleets[i], enemy_fleets[j], &preview);

                    // Show preview UI, let player confirm or retreat
                    if (player_confirms_battle || preview.attacker_win_chance > 0.6f) {
                        Agentite_BattleResult result;
                        agentite_fleet_battle(fm, player_fleets[i], enemy_fleets[j], &result);
                        show_battle_report(&result);
                    }
                }
            }
        }
    }
}

// === FOG OF WAR RENDERING ===
void render_map_with_fog(int viewing_player) {
    for (int y = 0; y < map_height; y++) {
        for (int x = 0; x < map_width; x++) {
            Agentite_FogState state = agentite_fog_get_state(fog, viewing_player, x, y);

            switch (state) {
                case AGENTITE_FOG_UNEXPLORED:
                    draw_tile_black(x, y);
                    break;
                case AGENTITE_FOG_EXPLORED:
                    draw_tile_dimmed(x, y);  // Show terrain, hide units
                    break;
                case AGENTITE_FOG_VISIBLE:
                    draw_tile_full(x, y);    // Show everything
                    draw_units_at(x, y);
                    break;
            }
        }
    }
}
```

---

## Factory Game with Power Grid

**Systems:** Power + Construction + ECS + Spatial + UI

Buildings that require power, with visual power grid.

```c
// === SETUP ===
Agentite_PowerSystem *power = agentite_power_create(200, 200);
Agentite_ConstructionManager *construction = agentite_construction_create();
Agentite_SpatialIndex *spatial = agentite_spatial_create(200 * 32, 200 * 32, 32);

// Register building types
agentite_construction_register(construction, "power_pole",
    (Agentite_BuildingDef){ .cost = 10, .size_x = 1, .size_y = 1 });
agentite_construction_register(construction, "generator",
    (Agentite_BuildingDef){ .cost = 100, .size_x = 2, .size_y = 2 });
agentite_construction_register(construction, "factory",
    (Agentite_BuildingDef){ .cost = 200, .size_x = 3, .size_y = 3 });

// Power change callback
void on_power_change(Agentite_PowerSystem *ps, int network_id,
                     Agentite_PowerStatus old_status,
                     Agentite_PowerStatus new_status, void *userdata) {
    if (new_status == AGENTITE_POWER_BROWNOUT) {
        show_notification("Power shortage in network %d!", network_id);
    }
}
agentite_power_set_callback(power, on_power_change, NULL);

// === BUILDING PLACEMENT ===
void handle_building_placement(void) {
    if (current_tool == TOOL_BUILD) {
        float mx, my;
        agentite_input_mouse_position(input, &mx, &my);
        int tile_x = (int)(mx / TILE_SIZE);
        int tile_y = (int)(my / TILE_SIZE);

        // Show ghost preview
        bool can_place = agentite_construction_can_place(
            construction, selected_building, tile_x, tile_y
        );

        // Check power coverage for non-power buildings
        if (strcmp(selected_building, "power_pole") != 0 &&
            strcmp(selected_building, "generator") != 0) {
            can_place = can_place && agentite_power_is_covered(power, tile_x, tile_y);
        }

        draw_building_ghost(selected_building, tile_x, tile_y, can_place);

        if (agentite_input_mouse_pressed(input, SDL_BUTTON_LEFT) && can_place) {
            // Place building
            ecs_entity_t building = ecs_new(world);
            ecs_set(world, building, C_Position, { tile_x * TILE_SIZE, tile_y * TILE_SIZE });
            ecs_set(world, building, C_Building, { .type = selected_building });

            if (strcmp(selected_building, "power_pole") == 0) {
                int pole_id = agentite_power_add_pole(power, tile_x, tile_y, 5);
                ecs_set(world, building, C_PowerPole, { .pole_id = pole_id });
            }
            else if (strcmp(selected_building, "generator") == 0) {
                int gen_id = agentite_power_add_producer(power, tile_x, tile_y, 100);
                ecs_set(world, building, C_PowerProducer, { .producer_id = gen_id });
            }
            else {
                // Consumer building
                int consumer_id = agentite_power_add_consumer(power, tile_x, tile_y, 25);
                ecs_set(world, building, C_PowerConsumer, { .consumer_id = consumer_id });
            }

            agentite_spatial_insert(spatial, (int32_t)building, tile_x, tile_y);
            agentite_construction_mark_placed(construction, tile_x, tile_y,
                get_building_size(selected_building));
        }
    }
}

// === FACTORY UPDATE ===
void update_factories(float dt) {
    ecs_query_t *q = ecs_query(world, {
        .terms = {{ ecs_id(C_Factory) }, { ecs_id(C_PowerConsumer) }}
    });
    ecs_iter_t it = ecs_query_iter(world, q);

    while (ecs_query_next(&it)) {
        C_Factory *factory = ecs_field(&it, C_Factory, 0);
        C_PowerConsumer *pc = ecs_field(&it, C_PowerConsumer, 1);

        for (int i = 0; i < it.count; i++) {
            bool powered = agentite_power_is_consumer_powered(power, pc[i].consumer_id);

            if (powered) {
                factory[i].progress += dt * factory[i].speed;
                if (factory[i].progress >= 1.0f) {
                    produce_item(factory[i].recipe);
                    factory[i].progress = 0.0f;
                }
            } else {
                // Show unpowered indicator
                factory[i].unpowered_time += dt;
            }
        }
    }
}

// === POWER GRID VISUALIZATION ===
void render_power_overlay(void) {
    if (!show_power_overlay) return;

    // Draw coverage areas
    int networks[64];
    int net_count = agentite_power_get_networks(power, networks, 64);

    for (int n = 0; n < net_count; n++) {
        Agentite_NetworkStats stats;
        agentite_power_get_network_stats(power, networks[n], &stats);

        // Color based on status
        AUI_Color color;
        switch (stats.status) {
            case AGENTITE_POWER_POWERED:  color = (AUI_Color){0, 255, 0, 64}; break;
            case AGENTITE_POWER_BROWNOUT: color = (AUI_Color){255, 255, 0, 64}; break;
            case AGENTITE_POWER_UNPOWERED: color = (AUI_Color){255, 0, 0, 64}; break;
        }

        // Get coverage cells
        int cells[1024];
        int cell_count = agentite_power_get_network_coverage(power, networks[n], cells, 1024);

        for (int c = 0; c < cell_count; c += 2) {
            int x = cells[c], y = cells[c + 1];
            aui_rect(ui, x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, color);
        }

        // Draw power lines between poles
        int poles[64];
        int pole_count = agentite_power_get_network_poles(power, networks[n], poles, 64);

        for (int p = 0; p < pole_count; p++) {
            const Agentite_PowerPole *pole = agentite_power_get_pole(power, poles[p]);
            // Draw lines to nearby poles (simplified)
            for (int p2 = p + 1; p2 < pole_count; p2++) {
                const Agentite_PowerPole *pole2 = agentite_power_get_pole(power, poles[p2]);
                int dist = abs(pole->x - pole2->x) + abs(pole->y - pole2->y);
                if (dist <= pole->radius * 2) {
                    draw_power_line(pole->x, pole->y, pole2->x, pole2->y);
                }
            }
        }
    }

    // Power stats UI
    aui_label(ui, 10, 10, "Power:");
    char buf[64];
    snprintf(buf, sizeof(buf), "%d / %d MW",
             agentite_power_get_total_consumption(power),
             agentite_power_get_total_production(power));
    aui_label(ui, 70, 10, buf);
}
```

---

## AI Agent with Goals

**Systems:** HTN + Blackboard + AI Tracks + Task Queue + ECS

Autonomous AI that plans and executes complex behaviors.

```c
// === SETUP ===
Agentite_HTNPlanner *planner = agentite_htn_create();
Agentite_Blackboard *blackboard = agentite_blackboard_create();
Agentite_AITracks *tracks = agentite_ai_tracks_create(3);  // 3 parallel tracks
Agentite_TaskQueue *tasks = agentite_task_queue_create();

// Define HTN domain for worker AI
// Compound task: "work"
agentite_htn_add_task(planner, "work", true);
agentite_htn_add_method(planner, "work", "gather_resources",
    need_resources, decompose_gather);
agentite_htn_add_method(planner, "work", "build_structure",
    has_resources_and_blueprint, decompose_build);
agentite_htn_add_method(planner, "work", "idle",
    NULL, decompose_idle);  // Fallback

// Primitive tasks
agentite_htn_add_task(planner, "move_to", false);
agentite_htn_add_task(planner, "harvest", false);
agentite_htn_add_task(planner, "deposit", false);
agentite_htn_add_task(planner, "construct", false);

// Setup AI tracks
agentite_ai_tracks_set_name(tracks, 0, "survival");   // Highest priority
agentite_ai_tracks_set_name(tracks, 1, "work");       // Normal priority
agentite_ai_tracks_set_name(tracks, 2, "social");     // Lowest priority

// === AI COMPONENT ===
typedef struct C_AIAgent {
    Agentite_HTNPlan current_plan;
    int plan_step;
    bool has_plan;
    float replan_timer;
} C_AIAgent;

// === AI UPDATE SYSTEM ===
void AIAgentSystem(ecs_iter_t *it) {
    C_AIAgent *ai = ecs_field(it, C_AIAgent, 0);
    C_Position *pos = ecs_field(it, C_Position, 1);

    for (int i = 0; i < it->count; i++) {
        ecs_entity_t entity = it->entities[i];

        // Replan periodically or when plan completes
        ai[i].replan_timer -= it->delta_time;

        if (!ai[i].has_plan || ai[i].replan_timer <= 0) {
            // Build world state from blackboard and entity state
            Agentite_HTNWorldState state;
            build_agent_world_state(&state, entity, blackboard);

            // Plan
            if (agentite_htn_plan(planner, "work", &state, &ai[i].current_plan)) {
                ai[i].has_plan = true;
                ai[i].plan_step = 0;
                ai[i].replan_timer = 5.0f;  // Replan every 5 seconds
            }
        }

        // Execute current plan step
        if (ai[i].has_plan && ai[i].plan_step < ai[i].current_plan.length) {
            const char *task = ai[i].current_plan.tasks[ai[i].plan_step];
            bool completed = execute_primitive_task(entity, task, it->delta_time);

            if (completed) {
                ai[i].plan_step++;

                // Broadcast completion to blackboard
                char key[64];
                snprintf(key, sizeof(key), "agent_%llu_completed", entity);
                agentite_blackboard_set_int(blackboard, key, ai[i].plan_step);
            }
        } else {
            ai[i].has_plan = false;  // Plan complete, replan next frame
        }

        // Check parallel tracks for interrupts
        Agentite_AITrackResult track_result;
        agentite_ai_tracks_evaluate(tracks, entity, &track_result);

        if (track_result.track_id == 0) {  // Survival track triggered
            // Interrupt current plan for survival behavior
            ai[i].has_plan = false;
            ai[i].replan_timer = 0;  // Replan immediately

            // Queue survival task
            agentite_task_queue_push(tasks, entity, "flee_danger", PRIORITY_HIGH);
        }
    }
}

// === BLACKBOARD COORDINATION ===
void update_shared_knowledge(void) {
    // Count resources at known locations
    int known_resources = 0;
    ecs_query_t *q = ecs_query(world, { .terms = {{ ecs_id(C_Resource) }} });
    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_query_next(&it)) {
        known_resources += it.count;
    }
    agentite_blackboard_set_int(blackboard, "known_resource_count", known_resources);

    // Publish nearest resource location
    vec2 nearest;
    if (find_nearest_resource(&nearest)) {
        agentite_blackboard_set_float(blackboard, "nearest_resource_x", nearest[0]);
        agentite_blackboard_set_float(blackboard, "nearest_resource_y", nearest[1]);
    }

    // Track threats
    int threat_level = calculate_threat_level();
    agentite_blackboard_set_int(blackboard, "threat_level", threat_level);
}

// === TASK EXECUTION ===
bool execute_primitive_task(ecs_entity_t entity, const char *task, float dt) {
    C_Position *pos = ecs_get_mut(world, entity, C_Position);

    if (strcmp(task, "move_to") == 0) {
        // Get target from blackboard
        char key[64];
        snprintf(key, sizeof(key), "agent_%llu_target_x", entity);
        float tx = agentite_blackboard_get_float(blackboard, key);
        snprintf(key, sizeof(key), "agent_%llu_target_y", entity);
        float ty = agentite_blackboard_get_float(blackboard, key);

        float dx = tx - pos->x;
        float dy = ty - pos->y;
        float dist = sqrtf(dx*dx + dy*dy);

        if (dist < 5.0f) return true;  // Arrived

        float speed = 50.0f * dt;
        pos->x += (dx / dist) * speed;
        pos->y += (dy / dist) * speed;
        return false;
    }
    else if (strcmp(task, "harvest") == 0) {
        // Find resource at location and gather
        // ...
        return true;  // Instant for simplicity
    }
    else if (strcmp(task, "deposit") == 0) {
        // Add to stockpile
        // ...
        return true;
    }

    return true;  // Unknown task, skip
}
```

---

## Fog of War with Exploration

**Systems:** Fog + ECS + Tilemap + Camera

Reveal map as units explore, with explored/unexplored states.

```c
// === SETUP ===
Agentite_FogOfWar *fog = agentite_fog_create(map_width, map_height);

// Unit component for vision
typedef struct C_Vision { int range; } C_Vision;

// === UPDATE VISIBILITY ===
void VisionSystem(ecs_iter_t *it) {
    C_Position *pos = ecs_field(it, C_Position, 0);
    C_Vision *vision = ecs_field(it, C_Vision, 1);
    C_Owner *owner = ecs_field(it, C_Owner, 2);

    // Clear visible state (keep explored)
    for (int p = 0; p < player_count; p++) {
        agentite_fog_clear_visible(fog, p);
    }

    // Reveal around each unit
    for (int i = 0; i < it->count; i++) {
        int tile_x = (int)(pos[i].x / TILE_SIZE);
        int tile_y = (int)(pos[i].y / TILE_SIZE);

        agentite_fog_reveal_radius(fog, owner[i].player_id,
                                   tile_x, tile_y, vision[i].range);
    }
}

// === TILEMAP RENDERING WITH FOG ===
void render_tilemap_with_fog(int viewing_player) {
    // First pass: render full tilemap
    agentite_tilemap_render(tilemap, cmd, pass);

    // Second pass: overlay fog
    agentite_sprite_begin(fog_renderer, camera);

    // Get visible region from camera
    int start_x, start_y, end_x, end_y;
    agentite_camera_get_visible_tiles(camera, TILE_SIZE,
                                      &start_x, &start_y, &end_x, &end_y);

    for (int y = start_y; y <= end_y; y++) {
        for (int x = start_x; x <= end_x; x++) {
            Agentite_FogState state = agentite_fog_get_state(fog, viewing_player, x, y);

            Agentite_Sprite fog_sprite = { .texture = fog_texture };

            switch (state) {
                case AGENTITE_FOG_UNEXPLORED:
                    // Full black
                    fog_sprite.color = (SDL_Color){0, 0, 0, 255};
                    agentite_sprite_draw(fog_renderer, &fog_sprite,
                                         x * TILE_SIZE, y * TILE_SIZE);
                    break;

                case AGENTITE_FOG_EXPLORED:
                    // Semi-transparent (show terrain, hide units)
                    fog_sprite.color = (SDL_Color){0, 0, 0, 128};
                    agentite_sprite_draw(fog_renderer, &fog_sprite,
                                         x * TILE_SIZE, y * TILE_SIZE);
                    break;

                case AGENTITE_FOG_VISIBLE:
                    // No fog overlay
                    break;
            }
        }
    }

    agentite_sprite_upload(fog_renderer, cmd);
    agentite_sprite_render(fog_renderer, cmd, pass);
}

// === ENTITY VISIBILITY CHECK ===
bool is_entity_visible(ecs_entity_t entity, int viewing_player) {
    const C_Position *pos = ecs_get(world, entity, C_Position);
    if (!pos) return false;

    int tile_x = (int)(pos->x / TILE_SIZE);
    int tile_y = (int)(pos->y / TILE_SIZE);

    return agentite_fog_get_state(fog, viewing_player, tile_x, tile_y)
           == AGENTITE_FOG_VISIBLE;
}

// Only render entities in visible tiles
void render_entities(int viewing_player) {
    agentite_sprite_begin(sprite_renderer, camera);

    ecs_query_t *q = ecs_query(world, {
        .terms = {{ ecs_id(C_Position) }, { ecs_id(C_Sprite) }}
    });
    ecs_iter_t it = ecs_query_iter(world, q);

    while (ecs_query_next(&it)) {
        C_Position *pos = ecs_field(&it, C_Position, 0);
        C_Sprite *spr = ecs_field(&it, C_Sprite, 1);

        for (int i = 0; i < it.count; i++) {
            if (is_entity_visible(it.entities[i], viewing_player)) {
                Agentite_Sprite sprite = { .texture = spr[i].texture };
                agentite_sprite_draw(sprite_renderer, &sprite, pos[i].x, pos[i].y);
            }
        }
    }

    agentite_sprite_upload(sprite_renderer, cmd);
}
```

---

## Tech Tree with Unlocks

**Systems:** Tech + Resources + Modifier + Event

Research system that unlocks buildings, units, and applies bonuses.

```c
// === SETUP ===
Agentite_TechTree *tech = agentite_tech_create();
Agentite_ModifierStack *modifiers = agentite_modifier_create();
Agentite_EventDispatcher *events = agentite_event_create();

// Define technologies with unlock effects
typedef struct TechUnlock {
    const char *unlock_type;  // "building", "unit", "modifier"
    const char *unlock_id;
    float modifier_value;
} TechUnlock;

// Tech definitions
int tech_mining = agentite_tech_add(tech, "Advanced Mining", 100, NULL, 0);
int tech_armor = agentite_tech_add(tech, "Composite Armor", 150, NULL, 0);
int tech_weapons = agentite_tech_add(tech, "Laser Weapons", 200, NULL, 0);

int prereqs_plasma[] = { tech_weapons };
int tech_plasma = agentite_tech_add(tech, "Plasma Weapons", 400, prereqs_plasma, 1);

// Store unlock data
TechUnlock unlocks[] = {
    [tech_mining] = { "modifier", "mining_speed", 1.25f },
    [tech_armor] = { "modifier", "armor_bonus", 20.0f },
    [tech_weapons] = { "unit", "laser_tank", 0 },
    [tech_plasma] = { "unit", "plasma_tank", 0 },
};

// Tech completion callback
void on_tech_complete(int player_id, int tech_id, void *userdata) {
    const char *tech_name = agentite_tech_get_name(tech, tech_id);
    SDL_Log("Player %d researched: %s", player_id, tech_name);

    TechUnlock *unlock = &unlocks[tech_id];

    if (strcmp(unlock->unlock_type, "modifier") == 0) {
        // Apply global modifier
        agentite_modifier_add(modifiers, player_id, unlock->unlock_id,
                              AGENTITE_MOD_MULTIPLY, unlock->modifier_value,
                              -1);  // Permanent
    }
    else if (strcmp(unlock->unlock_type, "unit") == 0) {
        // Unlock unit for production
        unlock_unit_for_player(player_id, unlock->unlock_id);
    }
    else if (strcmp(unlock->unlock_type, "building") == 0) {
        // Unlock building
        unlock_building_for_player(player_id, unlock->unlock_id);
    }

    // Dispatch event for UI/achievements
    Agentite_Event evt = {
        .type = EVENT_TECH_COMPLETE,
        .data.tech = { .player_id = player_id, .tech_id = tech_id }
    };
    agentite_event_dispatch(events, &evt);
}

agentite_tech_set_callback(tech, on_tech_complete, NULL);

// === RESEARCH UI ===
void render_tech_tree_ui(int player_id) {
    aui_begin_window(ui, "Research", 100, 100, 600, 400);

    int tech_count = agentite_tech_count(tech);

    for (int t = 0; t < tech_count; t++) {
        const char *name = agentite_tech_get_name(tech, t);
        int cost = agentite_tech_get_cost(tech, t);
        int progress = agentite_tech_get_progress(tech, player_id, t);

        Agentite_TechState state = agentite_tech_get_state(tech, player_id, t);

        float x = 50 + (t % 3) * 180;
        float y = 50 + (t / 3) * 100;

        // Draw tech node
        AUI_Color bg_color;
        switch (state) {
            case AGENTITE_TECH_LOCKED:
                bg_color = (AUI_Color){100, 100, 100, 255};
                break;
            case AGENTITE_TECH_AVAILABLE:
                bg_color = (AUI_Color){50, 100, 200, 255};
                break;
            case AGENTITE_TECH_RESEARCHING:
                bg_color = (AUI_Color){200, 200, 50, 255};
                break;
            case AGENTITE_TECH_COMPLETE:
                bg_color = (AUI_Color){50, 200, 50, 255};
                break;
        }

        aui_rect(ui, x, y, 160, 80, bg_color);
        aui_label(ui, x + 10, y + 10, name);

        if (state == AGENTITE_TECH_RESEARCHING) {
            // Show progress bar
            float pct = (float)progress / cost;
            aui_progress_bar(ui, x + 10, y + 50, 140, 15, pct);
        }
        else if (state == AGENTITE_TECH_AVAILABLE) {
            // Research button
            char label[32];
            snprintf(label, sizeof(label), "Research (%d)", cost);
            if (aui_button(ui, x + 10, y + 45, 140, 25, label)) {
                agentite_tech_start_research(tech, player_id, t);
            }
        }

        // Draw prerequisite lines
        int prereqs[8];
        int prereq_count = agentite_tech_get_prerequisites(tech, t, prereqs, 8);
        for (int p = 0; p < prereq_count; p++) {
            float px = 50 + (prereqs[p] % 3) * 180 + 80;
            float py = 50 + (prereqs[p] / 3) * 100 + 80;
            draw_line(px, py, x + 80, y);
        }
    }

    aui_end_window(ui);
}

// === APPLY RESEARCH POINTS ===
void apply_research(int player_id, int research_points) {
    int active = agentite_tech_get_researching(tech, player_id);
    if (active >= 0) {
        bool completed = agentite_tech_add_progress(tech, player_id, active, research_points);
        // Callback fires automatically on completion
    }
}

// === MODIFIER USAGE ===
float get_mining_speed(int player_id) {
    float base_speed = 1.0f;
    return agentite_modifier_apply(modifiers, player_id, "mining_speed", base_speed);
}

float get_unit_armor(int player_id, int base_armor) {
    float bonus = agentite_modifier_apply(modifiers, player_id, "armor_bonus", 0);
    return base_armor + bonus;
}
```

---

## Error Recovery Patterns

When systems fail, use these fallback patterns:

```c
// Texture loading with fallback
Agentite_Texture *load_texture_safe(Agentite_SpriteRenderer *sr, const char *path) {
    Agentite_Texture *tex = agentite_texture_load(sr, path);
    if (!tex) {
        SDL_Log("Warning: Failed to load %s: %s", path, agentite_get_last_error());
        tex = agentite_texture_load(sr, "assets/placeholder.png");
        if (!tex) {
            // Create 1x1 magenta texture as ultimate fallback
            uint8_t pixels[] = {255, 0, 255, 255};
            tex = agentite_texture_create_from_data(sr, pixels, 1, 1);
        }
    }
    return tex;
}

// Pathfinding with fallback to direct movement
bool move_to_target(ecs_entity_t entity, float target_x, float target_y) {
    C_Position *pos = ecs_get_mut(world, entity, C_Position);

    Agentite_PathResult path;
    int sx = (int)(pos->x / TILE_SIZE);
    int sy = (int)(pos->y / TILE_SIZE);
    int tx = (int)(target_x / TILE_SIZE);
    int ty = (int)(target_y / TILE_SIZE);

    if (agentite_pathfinder_find(pathfinder, sx, sy, tx, ty, &path)) {
        // Use pathfinding result
        set_unit_path(entity, &path);
        return true;
    } else {
        // Fallback: try direct movement (may get stuck on obstacles)
        SDL_Log("Pathfinding failed, using direct movement");

        float dx = target_x - pos->x;
        float dy = target_y - pos->y;
        float dist = sqrtf(dx*dx + dy*dy);

        if (dist > 0) {
            // Store simple waypoint
            C_Path *p = ecs_get_mut(world, entity, C_Path);
            p->waypoints[0][0] = target_x;
            p->waypoints[0][1] = target_y;
            p->count = 1;
            p->current = 0;
        }
        return false;
    }
}

// Combat system with validation
bool safe_combat_action(Agentite_CombatSystem *combat, Agentite_CombatAction *action) {
    // Validate before queueing
    if (!agentite_combat_is_action_valid(combat, action)) {
        const char *error = agentite_get_last_error();
        SDL_Log("Invalid combat action: %s", error);

        // Fallback to defend
        action->type = AGENTITE_ACTION_DEFEND;
        action->target_id = -1;
    }

    return agentite_combat_queue_action(combat, action);
}
```
