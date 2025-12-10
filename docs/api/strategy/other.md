# Other Strategy Systems

## Rate Tracking (`agentite/rate.h`)

Rolling window metrics for production/consumption rates.

```c
Agentite_RateTracker *rates = agentite_rate_create(8, 0.5f, 120);  // 8 metrics, 0.5s interval, 120 samples
agentite_rate_record_production(rates, 0, 100);
agentite_rate_update(rates, delta_time);
float rate = agentite_rate_get_net_rate(rates, 0, 10.0f);  // Last 10 seconds
```

## Network/Graph System (`agentite/network.h`)

Union-find based network for power grids, water pipes, etc.

```c
Agentite_NetworkSystem *network = agentite_network_create();
uint32_t node = agentite_network_add_node(network, x, y, radius);
agentite_network_set_production(network, node, 100);
agentite_network_update(network);
if (agentite_network_cell_is_powered(network, x, y)) { }
```

## Blueprint System (`agentite/blueprint.h`)

Save and place building templates.

```c
Agentite_Blueprint *bp = agentite_blueprint_create("Factory Setup");
agentite_blueprint_add_entry(bp, 0, 0, BUILDING_TYPE, 0);
agentite_blueprint_rotate_cw(bp);
agentite_blueprint_place(bp, world_x, world_y, place_callback, game);
```

## Dialog System (`agentite/dialog.h`)

Event-driven dialog queue with speakers.

```c
Agentite_DialogSystem *dialog = agentite_dialog_create(32);
agentite_dialog_queue_message(dialog, AGENTITE_SPEAKER_SYSTEM, "Welcome!");
agentite_dialog_update(dialog, delta_time);
if (agentite_dialog_has_message(dialog)) {
    const Agentite_DialogMessage *msg = agentite_dialog_current(dialog);
}
```

## Game Speed (`agentite/game_speed.h`)

Variable simulation speed with pause.

```c
Agentite_GameSpeed *speed = agentite_game_speed_create();
agentite_game_speed_set(speed, 2.0f);  // 2x speed
agentite_game_speed_pause(speed);
float scaled = agentite_game_speed_scale_delta(speed, raw_delta);
```

## Crafting System (`agentite/crafting.h`)

Progress-based crafting with recipes.

```c
Agentite_RecipeRegistry *recipes = agentite_recipe_create();
// Register recipes...
Agentite_Crafter *crafter = agentite_crafter_create(recipes);
agentite_crafter_start(crafter, "iron_sword", 5);
agentite_crafter_update(crafter, delta_time);
```

## Biome System (`agentite/biome.h`)

Terrain types affecting movement and resources.

```c
Agentite_BiomeSystem *biomes = agentite_biome_create();
// Register biomes with movement costs, resource weights...
Agentite_BiomeMap *map = agentite_biome_map_create(biomes, 100, 100);
float cost = agentite_biome_map_get_movement_cost(map, x, y);
```

## Trade Routes (`agentite/trade.h`)

Economic connections between locations.

```c
Agentite_TradeSystem *trade = agentite_trade_create();
uint32_t route = agentite_trade_create_route(trade, city_a, city_b, AGENTITE_ROUTE_TRADE);
int32_t income = agentite_trade_calculate_income(trade, faction);
```

## Command Queue (`agentite/command.h`)

Validated player action execution.

```c
Agentite_CommandSystem *sys = agentite_command_create();
agentite_command_register(sys, CMD_MOVE, validate_move, execute_move);
Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
agentite_command_set_int(cmd, "x", 10);
agentite_command_queue_validated(sys, cmd, game);
agentite_command_execute_all(sys, game, NULL, 0);
```

## Game Query API (`agentite/query.h`)

Read-only cached state queries.

```c
Agentite_QuerySystem *queries = agentite_query_create();
agentite_query_register(queries, "faction_resources", query_fn, sizeof(Result));
agentite_query_enable_cache(queries, "faction_resources", 16);
Result result;
agentite_query_exec_int(queries, "faction_resources", game, faction_id, &result);
```
