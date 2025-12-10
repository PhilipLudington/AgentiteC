# Utility Systems

## Container Utilities (`agentite/containers.h`)

Dynamic arrays, random selection, and shuffle algorithms.

```c
// Dynamic array
Agentite_Array(int) numbers;
agentite_array_init(&numbers);
agentite_array_push(&numbers, 10);
int last = agentite_array_pop(&numbers);
agentite_array_free(&numbers);

// Random utilities
agentite_random_seed(0);  // 0 = time-based
int roll = agentite_rand_int(1, 6);
float pct = agentite_rand_float(0.0f, 1.0f);
int chosen = agentite_random_choice(items, count);
agentite_shuffle_array(deck, 52);
```

## Validation Framework (`agentite/validate.h`)

Macro-based validation utilities.

```c
AGENTITE_VALIDATE_PTR(data);              // Return if NULL
AGENTITE_VALIDATE_PTR_RET(path, false);   // Return value if NULL
AGENTITE_VALIDATE_STRING_RET(path, false);// Check non-empty string
AGENTITE_VALIDATE_PTRS3(r, s, t);         // Check multiple pointers
AGENTITE_VALIDATE_RANGE_F(vol, 0.0f, 1.0f);
AGENTITE_VALIDATE_INDEX_RET(idx, count, -1);
AGENTITE_ASSERT(count > 0);               // Debug-only
```

## Formula Engine (`agentite/formula.h`)

Runtime expression evaluation.

```c
Agentite_FormulaContext *ctx = agentite_formula_create();
agentite_formula_set_var(ctx, "base", 10.0);
agentite_formula_set_var(ctx, "mult", 1.5);
double result = agentite_formula_eval(ctx, "base * mult + 5");

// Compiled for repeated evaluation
Agentite_Formula *f = agentite_formula_compile(ctx, "base * level");
double value = agentite_formula_exec(f, ctx);
```

Built-in functions: `min`, `max`, `clamp`, `abs`, `floor`, `ceil`, `sqrt`, `pow`, `sin`, `cos`, `lerp`, `if`

## Event Dispatcher (`agentite/event.h`)

Publish-subscribe event system.

```c
Agentite_EventDispatcher *events = agentite_event_dispatcher_create();

void on_turn(const Agentite_Event *e, void *ud) {
    printf("Turn %u\n", e->turn.turn);
}
agentite_event_subscribe(events, AGENTITE_EVENT_TURN_STARTED, on_turn, NULL);
agentite_event_emit_turn_started(events, 1);
```

## View Model (`agentite/viewmodel.h`)

Observable values with change detection for UI.

```c
Agentite_ViewModel *vm = agentite_vm_create();
uint32_t health = agentite_vm_define_int(vm, "health", 100);

void on_change(Agentite_ViewModel *vm, const Agentite_VMChangeEvent *e, void *ud) {
    update_ui(e->new_value.i32);
}
agentite_vm_subscribe(vm, health, on_change, NULL);
agentite_vm_set_int(vm, health, 75);  // Triggers callback
```

## Safe Arithmetic (`agentite/math_safe.h`)

Overflow-protected integer arithmetic.

```c
if (agentite_would_multiply_overflow(a, b)) { }
int32_t result = agentite_safe_multiply(a, b);  // Clamps on overflow
int32_t sum = agentite_safe_add(a, b);
int32_t diff = agentite_safe_subtract(a, b);
```

## Line Cell Iterator (`agentite/line.h`)

Bresenham line iteration for grids.

```c
bool check_clear(int32_t x, int32_t y, void *userdata) {
    return is_walkable(x, y);
}
bool clear = agentite_iterate_line_cells(x1, y1, x2, y2, check_clear, map);
```

## Notification/Toast System (`agentite/notification.h`)

Timed notification messages.

```c
Agentite_NotificationManager *notify = agentite_notify_create();
agentite_notify_add(notify, "Game saved!", AGENTITE_NOTIFY_SUCCESS);
agentite_notify_update(notify, delta_time);
agentite_notify_render(notify, text, font, 10.0f, 10.0f, 24.0f);
```

## Condition/Degradation (`agentite/condition.h`)

Track object condition with decay and repair.

```c
Agentite_Condition cond;
agentite_condition_init(&cond, AGENTITE_QUALITY_STANDARD);
agentite_condition_decay_usage(&cond, 1.0f);
float eff = agentite_condition_get_efficiency(&cond, 0.5f);
agentite_condition_repair(&cond, 25.0f);
```

## Financial Tracking (`agentite/finances.h`)

Revenue/expenses over rolling periods.

```c
Agentite_FinancialTracker *fin = agentite_finances_create(30.0f);
agentite_finances_record_revenue(fin, 1000);
agentite_finances_record_expense(fin, 500);
agentite_finances_update(fin, delta_time);
int32_t profit = agentite_finances_get_current_profit(fin);
```

## Loan System (`agentite/loan.h`)

Tiered loans with interest.

```c
Agentite_LoanSystem *loans = agentite_loan_create();
agentite_loan_add_tier(loans, "Small", 10000, 0.01f);

Agentite_LoanState state;
agentite_loan_state_init(&state);
agentite_loan_take(&state, loans, 0, &money);
agentite_loan_charge_interest(&state, loans);
```

## Demand System (`agentite/demand.h`)

Dynamic demand responding to service levels.

```c
Agentite_Demand demand;
agentite_demand_init(&demand, 50, 50);
agentite_demand_record_service(&demand);
agentite_demand_update(&demand, delta_time);
float mult = agentite_demand_get_multiplier(&demand);  // 0.5 to 2.0
```

## Incident System (`agentite/incident.h`)

Probabilistic events based on condition.

```c
Agentite_IncidentConfig config = {
    .base_probability = 0.1f,
    .minor_threshold = 0.70f,
    .major_threshold = 0.90f
};
Agentite_IncidentType result = agentite_incident_check_condition(&cond, &config);
```

## Logging (`agentite/log.h`)

File-based logging with subsystems.

```c
agentite_log_init();
agentite_log_set_level(AGENTITE_LOG_LEVEL_DEBUG);
agentite_log_info(AGENTITE_LOG_CORE, "Engine initialized");
agentite_log_warning(AGENTITE_LOG_GRAPHICS, "Texture not found: %s", path);
```
