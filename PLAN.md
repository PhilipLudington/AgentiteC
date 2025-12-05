# Carbon Engine Feature Plan

Features identified from the Railroad project that would be valuable additions to the Carbon game engine.

**STATUS: ALL FEATURES IMPLEMENTED** (December 2024)

---

## 1. Structured Logging System ✅

**Priority:** High | **Complexity:** Low | **Status:** Implemented

A file-based logging system with subsystem tags and log levels for debugging and post-mortem analysis.

### API Design

```c
// Log levels
typedef enum {
    CARBON_LOG_LEVEL_ERROR = 0,   // Always logged, auto-flush
    CARBON_LOG_LEVEL_WARNING = 1,
    CARBON_LOG_LEVEL_INFO = 2,
    CARBON_LOG_LEVEL_DEBUG = 3
} Carbon_LogLevel;

// Subsystem identifiers
#define CARBON_LOG_CORE       "Core"
#define CARBON_LOG_ECS        "ECS"
#define CARBON_LOG_GRAPHICS   "Graphics"
#define CARBON_LOG_AUDIO      "Audio"
#define CARBON_LOG_INPUT      "Input"
#define CARBON_LOG_AI         "AI"
#define CARBON_LOG_UI         "UI"

// API
bool carbon_log_init(void);
bool carbon_log_init_with_path(const char *path);
void carbon_log_shutdown(void);
void carbon_log_set_level(Carbon_LogLevel level);
Carbon_LogLevel carbon_log_get_level(void);

void carbon_log_error(const char *subsystem, const char *fmt, ...);
void carbon_log_warning(const char *subsystem, const char *fmt, ...);
void carbon_log_info(const char *subsystem, const char *fmt, ...);
void carbon_log_debug(const char *subsystem, const char *fmt, ...);
void carbon_log_flush(void);
```

### Features

- Timestamp + level + subsystem tagged output: `[2024-01-15 14:30:22] [ERROR] [Graphics  ] Failed to load texture`
- Automatic flush on errors (for crash debugging)
- Session start/end markers
- Configurable log level filtering
- File output to configurable path (default: `/tmp/carbon.log`)

### Files

- `include/carbon/log.h`
- `src/core/log.c`

---

## 2. Safe Arithmetic Library ✅

**Priority:** High | **Complexity:** Low | **Status:** Implemented

Overflow-protected integer arithmetic for financial calculations, scores, and resource systems.

### API Design

```c
// Overflow detection predicates (no side effects)
bool carbon_would_multiply_overflow(int32_t a, int32_t b);
bool carbon_would_add_overflow(int32_t a, int32_t b);
bool carbon_would_subtract_overflow(int32_t a, int32_t b);

// Safe operations (clamp to INT32_MAX/MIN on overflow, log warning)
int32_t carbon_safe_multiply(int32_t a, int32_t b);
int32_t carbon_safe_add(int32_t a, int32_t b);
int32_t carbon_safe_subtract(int32_t a, int32_t b);

// 64-bit variants
bool carbon_would_multiply_overflow_i64(int64_t a, int64_t b);
int64_t carbon_safe_multiply_i64(int64_t a, int64_t b);
int64_t carbon_safe_add_i64(int64_t a, int64_t b);
int64_t carbon_safe_subtract_i64(int64_t a, int64_t b);
```

### Use Cases

- Resource economy systems
- Score tracking
- Financial calculations
- Any game with potentially large numbers

### Files

- `include/carbon/math_safe.h`
- `src/core/math_safe.c`

---

## 3. Notification/Toast System ✅

**Priority:** High | **Complexity:** Low | **Status:** Implemented

Timed notification messages with color coding for player feedback.

### API Design

```c
#define CARBON_MAX_NOTIFICATIONS 8
#define CARBON_NOTIFICATION_MAX_LEN 128

typedef enum {
    CARBON_NOTIFY_INFO,
    CARBON_NOTIFY_SUCCESS,
    CARBON_NOTIFY_WARNING,
    CARBON_NOTIFY_ERROR
} Carbon_NotifyType;

typedef struct {
    char message[CARBON_NOTIFICATION_MAX_LEN];
    float time_remaining;
    float r, g, b, a;
    Carbon_NotifyType type;
} Carbon_Notification;

typedef struct Carbon_NotificationManager Carbon_NotificationManager;

Carbon_NotificationManager *carbon_notify_create(void);
void carbon_notify_destroy(Carbon_NotificationManager *mgr);

void carbon_notify_add(Carbon_NotificationManager *mgr, const char *message, Carbon_NotifyType type);
void carbon_notify_add_colored(Carbon_NotificationManager *mgr, const char *message,
                                float r, float g, float b);
void carbon_notify_printf(Carbon_NotificationManager *mgr, Carbon_NotifyType type,
                          const char *fmt, ...);

void carbon_notify_update(Carbon_NotificationManager *mgr, float dt);
void carbon_notify_clear(Carbon_NotificationManager *mgr);

int carbon_notify_count(Carbon_NotificationManager *mgr);
const Carbon_Notification *carbon_notify_get(Carbon_NotificationManager *mgr, int index);

// Optional: built-in rendering
void carbon_notify_render(Carbon_NotificationManager *mgr, Carbon_TextRenderer *text,
                          Carbon_Font *font, float x, float y, float spacing);
```

### Features

- Auto-expire after configurable duration (default 5 seconds)
- FIFO queue with max limit
- Color-coded by type (info=white, success=green, warning=yellow, error=red)
- Stack rendering (newest on top or bottom)
- Printf-style formatting

### Files

- `include/carbon/notification.h`
- `src/ui/notification.c`

---

## 4. Bresenham Line Cell Iterator ✅

**Priority:** Medium | **Complexity:** Low | **Status:** Implemented

Iterate over grid cells along a line for pathfinding, line-of-sight, and construction.

### API Design

```c
// Callback returns false to stop iteration early
typedef bool (*Carbon_LineCellCallback)(int32_t x, int32_t y, void *userdata);

// Iterate over all cells along a line
// Returns true if completed, false if callback stopped early
bool carbon_iterate_line_cells(
    int32_t from_x, int32_t from_y,
    int32_t to_x, int32_t to_y,
    Carbon_LineCellCallback callback,
    void *userdata
);

// Variant that skips endpoints (useful when endpoints are special, e.g., cities)
bool carbon_iterate_line_cells_ex(
    int32_t from_x, int32_t from_y,
    int32_t to_x, int32_t to_y,
    Carbon_LineCellCallback callback,
    void *userdata,
    bool skip_start,
    bool skip_end
);

// Count cells along a line (excluding endpoints)
int carbon_count_line_cells(int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y);
```

### Use Cases

- Line-of-sight checking
- Ray casting on tile grids
- Construction cost calculation along paths
- Drawing lines on tilemaps
- Checking terrain along routes

### Files

- `include/carbon/line.h` (or add to `include/carbon/helpers.h`)
- `src/core/line.c` (or add to `src/core/containers.c`)

---

## 5. Condition/Degradation System ✅

**Priority:** Medium | **Complexity:** Medium | **Status:** Implemented

Track object condition with time-based and usage-based decay for equipment, buildings, vehicles.

### API Design

```c
// Condition status thresholds
typedef enum {
    CARBON_CONDITION_GOOD,      // >= 75%
    CARBON_CONDITION_FAIR,      // >= 50%
    CARBON_CONDITION_POOR,      // >= 25%
    CARBON_CONDITION_CRITICAL   // < 25%
} Carbon_ConditionStatus;

// Quality tiers affect decay rates
typedef enum {
    CARBON_QUALITY_LOW,
    CARBON_QUALITY_STANDARD,
    CARBON_QUALITY_HIGH
} Carbon_QualityTier;

typedef struct {
    float condition;           // 0.0 - 100.0
    float max_condition;       // Usually 100.0
    Carbon_QualityTier quality;
    bool is_damaged;           // Requires repair before use
    uint32_t usage_count;      // Total uses
} Carbon_Condition;

// Decay rate multipliers by quality
#define CARBON_DECAY_MULT_LOW      1.5f
#define CARBON_DECAY_MULT_STANDARD 1.0f
#define CARBON_DECAY_MULT_HIGH     0.5f

void carbon_condition_init(Carbon_Condition *cond, Carbon_QualityTier quality);
void carbon_condition_decay_time(Carbon_Condition *cond, float amount);
void carbon_condition_decay_usage(Carbon_Condition *cond, float amount);
void carbon_condition_repair(Carbon_Condition *cond, float amount);
void carbon_condition_repair_full(Carbon_Condition *cond);
void carbon_condition_damage(Carbon_Condition *cond);

Carbon_ConditionStatus carbon_condition_get_status(const Carbon_Condition *cond);
float carbon_condition_get_percent(const Carbon_Condition *cond);
float carbon_condition_get_failure_probability(const Carbon_Condition *cond, float base_rate);

// Cost calculation for repairs
int32_t carbon_condition_get_repair_cost(const Carbon_Condition *cond, int32_t base_cost);
```

### Features

- Quality tiers affect decay rates
- Separate time-based and usage-based decay
- Damage flag for blocking usage until repaired
- Status thresholds (good/fair/poor/critical)
- Failure probability calculation based on condition
- Repair cost scaling

### Files

- `include/carbon/condition.h`
- `src/strategy/condition.c`

---

## 6. Financial Period Tracking ✅

**Priority:** Medium | **Complexity:** Medium | **Status:** Implemented

Track revenue and expenses over rolling time periods for economy games.

### API Design

```c
typedef struct {
    int32_t revenue;
    int32_t expenses;
} Carbon_FinancialPeriod;

typedef struct {
    // Current accumulator
    Carbon_FinancialPeriod current;

    // Historical periods
    Carbon_FinancialPeriod last_period;
    Carbon_FinancialPeriod all_time;

    // Rolling history (circular buffer)
    Carbon_FinancialPeriod history[12];
    int history_index;
    int history_count;

    // Timing
    float period_duration;     // Seconds per period
    float time_in_period;      // Current progress
    int periods_elapsed;       // Total periods completed
} Carbon_FinancialTracker;

Carbon_FinancialTracker *carbon_finances_create(float period_duration);
void carbon_finances_destroy(Carbon_FinancialTracker *tracker);

void carbon_finances_record_revenue(Carbon_FinancialTracker *tracker, int32_t amount);
void carbon_finances_record_expense(Carbon_FinancialTracker *tracker, int32_t amount);
void carbon_finances_update(Carbon_FinancialTracker *tracker, float dt);

// Queries
int32_t carbon_finances_get_profit(const Carbon_FinancialPeriod *period);
int32_t carbon_finances_get_current_revenue(const Carbon_FinancialTracker *tracker);
int32_t carbon_finances_get_current_expenses(const Carbon_FinancialTracker *tracker);
int32_t carbon_finances_get_current_profit(const Carbon_FinancialTracker *tracker);

// Get sum of last N periods
Carbon_FinancialPeriod carbon_finances_sum_periods(const Carbon_FinancialTracker *tracker, int count);

// Callbacks for period rollover
typedef void (*Carbon_FinancePeriodCallback)(const Carbon_FinancialPeriod *completed, void *userdata);
void carbon_finances_set_period_callback(Carbon_FinancialTracker *tracker,
                                          Carbon_FinancePeriodCallback callback, void *userdata);
```

### Files

- `include/carbon/finances.h`
- `src/strategy/finances.c`

---

## 7. Loan/Credit System ✅

**Priority:** Low | **Complexity:** Medium | **Status:** Implemented

Tiered loan system with interest for economy games.

### API Design

```c
typedef struct {
    const char *name;
    int32_t principal;        // Amount to borrow
    float interest_rate;      // Per period (e.g., 0.01 = 1%)
} Carbon_LoanTier;

typedef struct {
    int active_tier;              // -1 if no loan
    int32_t principal;            // Original amount
    int32_t amount_owed;          // Current balance
    int32_t total_interest_paid;  // Lifetime interest
} Carbon_LoanState;

typedef struct Carbon_LoanSystem Carbon_LoanSystem;

Carbon_LoanSystem *carbon_loan_create(void);
void carbon_loan_destroy(Carbon_LoanSystem *loans);

void carbon_loan_add_tier(Carbon_LoanSystem *loans, const char *name,
                          int32_t principal, float interest_rate);

bool carbon_loan_can_take(const Carbon_LoanState *state);
bool carbon_loan_take(Carbon_LoanState *state, const Carbon_LoanSystem *loans,
                      int tier, int32_t *out_money);

bool carbon_loan_can_repay(const Carbon_LoanState *state, int32_t available_money);
bool carbon_loan_repay(Carbon_LoanState *state, int32_t *out_cost);

void carbon_loan_charge_interest(Carbon_LoanState *state, const Carbon_LoanSystem *loans);

int carbon_loan_get_tier_count(const Carbon_LoanSystem *loans);
const Carbon_LoanTier *carbon_loan_get_tier(const Carbon_LoanSystem *loans, int index);
```

### Files

- `include/carbon/loan.h`
- `src/strategy/loan.c`

---

## 8. Dynamic Demand System ✅

**Priority:** Low | **Complexity:** Medium | **Status:** Implemented

Demand values that respond to service levels for economy/logistics games.

### API Design

```c
#define CARBON_DEMAND_MIN 0
#define CARBON_DEMAND_MAX 100

typedef struct {
    uint8_t demand;           // Current demand (0-100)
    uint8_t equilibrium;      // Natural resting point
    uint8_t min_demand;       // Floor
    uint8_t max_demand;       // Ceiling

    float update_interval;    // Seconds between updates
    float time_since_update;
    uint32_t service_count;   // Services since last update

    float growth_per_service; // Demand increase per service
    float decay_rate;         // Demand decrease per update without service
} Carbon_Demand;

void carbon_demand_init(Carbon_Demand *demand, uint8_t initial, uint8_t equilibrium);
void carbon_demand_record_service(Carbon_Demand *demand);
void carbon_demand_update(Carbon_Demand *demand, float dt);

uint8_t carbon_demand_get(const Carbon_Demand *demand);
float carbon_demand_get_multiplier(const Carbon_Demand *demand);  // 0.5 - 2.0
```

### Files

- `include/carbon/demand.h`
- `src/strategy/demand.c`

---

## 9. Incident/Random Event System ✅

**Priority:** Low | **Complexity:** Medium | **Status:** Implemented

Probabilistic event system for random failures and events.

### API Design

```c
typedef enum {
    CARBON_INCIDENT_NONE = 0,
    CARBON_INCIDENT_MINOR = 1,     // Temporary effect
    CARBON_INCIDENT_MAJOR = 2,     // Lasting effect
    CARBON_INCIDENT_CRITICAL = 4   // Severe consequence
} Carbon_IncidentType;

typedef struct {
    float base_probability;        // Base chance (0.0 - 1.0)
    float minor_threshold;         // Roll below = minor (e.g., 0.70)
    float major_threshold;         // Roll below = major (e.g., 0.90)
    // Above major_threshold = critical
} Carbon_IncidentConfig;

// Calculate incident probability based on condition
float carbon_incident_calc_probability(float condition, float quality_mult);

// Roll for incident
Carbon_IncidentType carbon_incident_check(float probability, const Carbon_IncidentConfig *config);

// Convenience for condition-based incidents
Carbon_IncidentType carbon_incident_check_condition(const Carbon_Condition *cond,
                                                     const Carbon_IncidentConfig *config);
```

### Files

- `include/carbon/incident.h`
- `src/strategy/incident.c`

---

## Implementation Order

All phases completed in December 2024.

### Phase 1: Core Utilities ✅
1. **Logging System** - `include/carbon/log.h`, `src/core/log.c`
2. **Safe Arithmetic** - `include/carbon/math_safe.h`, `src/core/math_safe.c`
3. **Notification System** - `include/carbon/notification.h`, `src/ui/notification.c`

### Phase 2: Grid Utilities ✅
4. **Line Cell Iterator** - `include/carbon/line.h`, `src/core/line.c`

### Phase 3: Strategy Game Systems ✅
5. **Condition/Degradation** - `include/carbon/condition.h`, `src/strategy/condition.c`
6. **Financial Tracking** - `include/carbon/finances.h`, `src/strategy/finances.c`

### Phase 4: Advanced Economy ✅
7. **Loan System** - `include/carbon/loan.h`, `src/strategy/loan.c`
8. **Demand System** - `include/carbon/demand.h`, `src/strategy/demand.c`
9. **Incident System** - `include/carbon/incident.h`, `src/strategy/incident.c`

---

## Notes

- All systems should integrate with the existing Carbon event dispatcher where appropriate
- Safe arithmetic should log warnings via the logging system
- Financial and loan systems should use safe arithmetic internally
- Consider adding these to `carbon_game_context_create()` as optional systems
