# Utility Systems

## Container Utilities (`carbon/containers.h`)

Dynamic arrays, random selection, and shuffle algorithms.

```c
// Dynamic array
Carbon_Array(int) numbers;
carbon_array_init(&numbers);
carbon_array_push(&numbers, 10);
int last = carbon_array_pop(&numbers);
carbon_array_free(&numbers);

// Random utilities
carbon_random_seed(0);  // 0 = time-based
int roll = carbon_rand_int(1, 6);
float pct = carbon_rand_float(0.0f, 1.0f);
int chosen = carbon_random_choice(items, count);
carbon_shuffle_array(deck, 52);
```

## Validation Framework (`carbon/validate.h`)

Macro-based validation utilities.

```c
CARBON_VALIDATE_PTR(data);              // Return if NULL
CARBON_VALIDATE_PTR_RET(path, false);   // Return value if NULL
CARBON_VALIDATE_STRING_RET(path, false);// Check non-empty string
CARBON_VALIDATE_PTRS3(r, s, t);         // Check multiple pointers
CARBON_VALIDATE_RANGE_F(vol, 0.0f, 1.0f);
CARBON_VALIDATE_INDEX_RET(idx, count, -1);
CARBON_ASSERT(count > 0);               // Debug-only
```

## Formula Engine (`carbon/formula.h`)

Runtime expression evaluation.

```c
Carbon_FormulaContext *ctx = carbon_formula_create();
carbon_formula_set_var(ctx, "base", 10.0);
carbon_formula_set_var(ctx, "mult", 1.5);
double result = carbon_formula_eval(ctx, "base * mult + 5");

// Compiled for repeated evaluation
Carbon_Formula *f = carbon_formula_compile(ctx, "base * level");
double value = carbon_formula_exec(f, ctx);
```

Built-in functions: `min`, `max`, `clamp`, `abs`, `floor`, `ceil`, `sqrt`, `pow`, `sin`, `cos`, `lerp`, `if`

## Event Dispatcher (`carbon/event.h`)

Publish-subscribe event system.

```c
Carbon_EventDispatcher *events = carbon_event_dispatcher_create();

void on_turn(const Carbon_Event *e, void *ud) {
    printf("Turn %u\n", e->turn.turn);
}
carbon_event_subscribe(events, CARBON_EVENT_TURN_STARTED, on_turn, NULL);
carbon_event_emit_turn_started(events, 1);
```

## View Model (`carbon/viewmodel.h`)

Observable values with change detection for UI.

```c
Carbon_ViewModel *vm = carbon_vm_create();
uint32_t health = carbon_vm_define_int(vm, "health", 100);

void on_change(Carbon_ViewModel *vm, const Carbon_VMChangeEvent *e, void *ud) {
    update_ui(e->new_value.i32);
}
carbon_vm_subscribe(vm, health, on_change, NULL);
carbon_vm_set_int(vm, health, 75);  // Triggers callback
```

## Safe Arithmetic (`carbon/math_safe.h`)

Overflow-protected integer arithmetic.

```c
if (carbon_would_multiply_overflow(a, b)) { }
int32_t result = carbon_safe_multiply(a, b);  // Clamps on overflow
int32_t sum = carbon_safe_add(a, b);
int32_t diff = carbon_safe_subtract(a, b);
```

## Line Cell Iterator (`carbon/line.h`)

Bresenham line iteration for grids.

```c
bool check_clear(int32_t x, int32_t y, void *userdata) {
    return is_walkable(x, y);
}
bool clear = carbon_iterate_line_cells(x1, y1, x2, y2, check_clear, map);
```

## Notification/Toast System (`carbon/notification.h`)

Timed notification messages.

```c
Carbon_NotificationManager *notify = carbon_notify_create();
carbon_notify_add(notify, "Game saved!", CARBON_NOTIFY_SUCCESS);
carbon_notify_update(notify, delta_time);
carbon_notify_render(notify, text, font, 10.0f, 10.0f, 24.0f);
```

## Condition/Degradation (`carbon/condition.h`)

Track object condition with decay and repair.

```c
Carbon_Condition cond;
carbon_condition_init(&cond, CARBON_QUALITY_STANDARD);
carbon_condition_decay_usage(&cond, 1.0f);
float eff = carbon_condition_get_efficiency(&cond, 0.5f);
carbon_condition_repair(&cond, 25.0f);
```

## Financial Tracking (`carbon/finances.h`)

Revenue/expenses over rolling periods.

```c
Carbon_FinancialTracker *fin = carbon_finances_create(30.0f);
carbon_finances_record_revenue(fin, 1000);
carbon_finances_record_expense(fin, 500);
carbon_finances_update(fin, delta_time);
int32_t profit = carbon_finances_get_current_profit(fin);
```

## Loan System (`carbon/loan.h`)

Tiered loans with interest.

```c
Carbon_LoanSystem *loans = carbon_loan_create();
carbon_loan_add_tier(loans, "Small", 10000, 0.01f);

Carbon_LoanState state;
carbon_loan_state_init(&state);
carbon_loan_take(&state, loans, 0, &money);
carbon_loan_charge_interest(&state, loans);
```

## Demand System (`carbon/demand.h`)

Dynamic demand responding to service levels.

```c
Carbon_Demand demand;
carbon_demand_init(&demand, 50, 50);
carbon_demand_record_service(&demand);
carbon_demand_update(&demand, delta_time);
float mult = carbon_demand_get_multiplier(&demand);  // 0.5 to 2.0
```

## Incident System (`carbon/incident.h`)

Probabilistic events based on condition.

```c
Carbon_IncidentConfig config = {
    .base_probability = 0.1f,
    .minor_threshold = 0.70f,
    .major_threshold = 0.90f
};
Carbon_IncidentType result = carbon_incident_check_condition(&cond, &config);
```

## Logging (`carbon/log.h`)

File-based logging with subsystems.

```c
carbon_log_init();
carbon_log_set_level(CARBON_LOG_LEVEL_DEBUG);
carbon_log_info(CARBON_LOG_CORE, "Engine initialized");
carbon_log_warning(CARBON_LOG_GRAPHICS, "Texture not found: %s", path);
```
