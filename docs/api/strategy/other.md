# Other Strategy Systems

## Rate Tracking (`carbon/rate.h`)

Rolling window metrics for production/consumption rates.

```c
Carbon_RateTracker *rates = carbon_rate_create(8, 0.5f, 120);  // 8 metrics, 0.5s interval, 120 samples
carbon_rate_record_production(rates, 0, 100);
carbon_rate_update(rates, delta_time);
float rate = carbon_rate_get_net_rate(rates, 0, 10.0f);  // Last 10 seconds
```

## Network/Graph System (`carbon/network.h`)

Union-find based network for power grids, water pipes, etc.

```c
Carbon_NetworkSystem *network = carbon_network_create();
uint32_t node = carbon_network_add_node(network, x, y, radius);
carbon_network_set_production(network, node, 100);
carbon_network_update(network);
if (carbon_network_cell_is_powered(network, x, y)) { }
```

## Blueprint System (`carbon/blueprint.h`)

Save and place building templates.

```c
Carbon_Blueprint *bp = carbon_blueprint_create("Factory Setup");
carbon_blueprint_add_entry(bp, 0, 0, BUILDING_TYPE, 0);
carbon_blueprint_rotate_cw(bp);
carbon_blueprint_place(bp, world_x, world_y, place_callback, game);
```

## Dialog System (`carbon/dialog.h`)

Event-driven dialog queue with speakers.

```c
Carbon_DialogSystem *dialog = carbon_dialog_create(32);
carbon_dialog_queue_message(dialog, CARBON_SPEAKER_SYSTEM, "Welcome!");
carbon_dialog_update(dialog, delta_time);
if (carbon_dialog_has_message(dialog)) {
    const Carbon_DialogMessage *msg = carbon_dialog_current(dialog);
}
```

## Game Speed (`carbon/game_speed.h`)

Variable simulation speed with pause.

```c
Carbon_GameSpeed *speed = carbon_game_speed_create();
carbon_game_speed_set(speed, 2.0f);  // 2x speed
carbon_game_speed_pause(speed);
float scaled = carbon_game_speed_scale_delta(speed, raw_delta);
```

## Crafting System (`carbon/crafting.h`)

Progress-based crafting with recipes.

```c
Carbon_RecipeRegistry *recipes = carbon_recipe_create();
// Register recipes...
Carbon_Crafter *crafter = carbon_crafter_create(recipes);
carbon_crafter_start(crafter, "iron_sword", 5);
carbon_crafter_update(crafter, delta_time);
```

## Biome System (`carbon/biome.h`)

Terrain types affecting movement and resources.

```c
Carbon_BiomeSystem *biomes = carbon_biome_create();
// Register biomes with movement costs, resource weights...
Carbon_BiomeMap *map = carbon_biome_map_create(biomes, 100, 100);
float cost = carbon_biome_map_get_movement_cost(map, x, y);
```

## Trade Routes (`carbon/trade.h`)

Economic connections between locations.

```c
Carbon_TradeSystem *trade = carbon_trade_create();
uint32_t route = carbon_trade_create_route(trade, city_a, city_b, CARBON_ROUTE_TRADE);
int32_t income = carbon_trade_calculate_income(trade, faction);
```

## Command Queue (`carbon/command.h`)

Validated player action execution.

```c
Carbon_CommandSystem *sys = carbon_command_create();
carbon_command_register(sys, CMD_MOVE, validate_move, execute_move);
Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
carbon_command_set_int(cmd, "x", 10);
carbon_command_queue_validated(sys, cmd, game);
carbon_command_execute_all(sys, game, NULL, 0);
```

## Game Query API (`carbon/query.h`)

Read-only cached state queries.

```c
Carbon_QuerySystem *queries = carbon_query_create();
carbon_query_register(queries, "faction_resources", query_fn, sizeof(Result));
carbon_query_enable_cache(queries, "faction_resources", 16);
Result result;
carbon_query_exec_int(queries, "faction_resources", game, faction_id, &result);
```
