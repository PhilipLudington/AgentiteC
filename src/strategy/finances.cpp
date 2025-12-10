#include "agentite/agentite.h"
#include "agentite/finances.h"
#include "agentite/math_safe.h"
#include "agentite/validate.h"
#include <stdlib.h>
#include <string.h>

Agentite_FinancialTracker *agentite_finances_create(float period_duration) {
    Agentite_FinancialTracker *tracker = AGENTITE_ALLOC(Agentite_FinancialTracker);
    if (!tracker) return NULL;

    agentite_finances_init(tracker, period_duration);
    return tracker;
}

void agentite_finances_destroy(Agentite_FinancialTracker *tracker) {
    free(tracker);
}

void agentite_finances_init(Agentite_FinancialTracker *tracker, float period_duration) {
    AGENTITE_VALIDATE_PTR(tracker);

    memset(tracker, 0, sizeof(Agentite_FinancialTracker));
    tracker->period_duration = period_duration > 0.0f ? period_duration : 1.0f;
}

void agentite_finances_record_revenue(Agentite_FinancialTracker *tracker, int32_t amount) {
    AGENTITE_VALIDATE_PTR(tracker);
    if (amount <= 0) return;

    tracker->current.revenue = agentite_safe_add(tracker->current.revenue, amount);
    tracker->all_time.revenue = agentite_safe_add(tracker->all_time.revenue, amount);
}

void agentite_finances_record_expense(Agentite_FinancialTracker *tracker, int32_t amount) {
    AGENTITE_VALIDATE_PTR(tracker);
    if (amount <= 0) return;

    tracker->current.expenses = agentite_safe_add(tracker->current.expenses, amount);
    tracker->all_time.expenses = agentite_safe_add(tracker->all_time.expenses, amount);
}

/* Internal: roll over to next period */
static void rollover_period(Agentite_FinancialTracker *tracker) {
    /* Store completed period in history */
    tracker->history[tracker->history_index] = tracker->current;
    tracker->history_index = (tracker->history_index + 1) % AGENTITE_FINANCES_MAX_HISTORY;
    if (tracker->history_count < AGENTITE_FINANCES_MAX_HISTORY) {
        tracker->history_count++;
    }

    /* Update last period */
    tracker->last_period = tracker->current;

    /* Callback */
    if (tracker->period_callback) {
        tracker->period_callback(&tracker->current, tracker->callback_userdata);
    }

    /* Reset current period */
    tracker->current.revenue = 0;
    tracker->current.expenses = 0;

    tracker->periods_elapsed++;
}

void agentite_finances_update(Agentite_FinancialTracker *tracker, float dt) {
    AGENTITE_VALIDATE_PTR(tracker);

    tracker->time_in_period += dt;

    /* Check for period rollover(s) */
    while (tracker->time_in_period >= tracker->period_duration) {
        tracker->time_in_period -= tracker->period_duration;
        rollover_period(tracker);
    }
}

void agentite_finances_end_period(Agentite_FinancialTracker *tracker) {
    AGENTITE_VALIDATE_PTR(tracker);

    rollover_period(tracker);
    tracker->time_in_period = 0.0f;
}

void agentite_finances_reset(Agentite_FinancialTracker *tracker) {
    AGENTITE_VALIDATE_PTR(tracker);

    float duration = tracker->period_duration;
    Agentite_FinancePeriodCallback callback = tracker->period_callback;
    void *userdata = tracker->callback_userdata;

    memset(tracker, 0, sizeof(Agentite_FinancialTracker));

    tracker->period_duration = duration;
    tracker->period_callback = callback;
    tracker->callback_userdata = userdata;
}

/*============================================================================
 * Queries
 *============================================================================*/

int32_t agentite_finances_get_profit(const Agentite_FinancialPeriod *period) {
    if (!period) return 0;
    return agentite_safe_subtract(period->revenue, period->expenses);
}

int32_t agentite_finances_get_current_revenue(const Agentite_FinancialTracker *tracker) {
    return tracker ? tracker->current.revenue : 0;
}

int32_t agentite_finances_get_current_expenses(const Agentite_FinancialTracker *tracker) {
    return tracker ? tracker->current.expenses : 0;
}

int32_t agentite_finances_get_current_profit(const Agentite_FinancialTracker *tracker) {
    return tracker ? agentite_finances_get_profit(&tracker->current) : 0;
}

int32_t agentite_finances_get_last_profit(const Agentite_FinancialTracker *tracker) {
    return tracker ? agentite_finances_get_profit(&tracker->last_period) : 0;
}

int32_t agentite_finances_get_all_time_profit(const Agentite_FinancialTracker *tracker) {
    return tracker ? agentite_finances_get_profit(&tracker->all_time) : 0;
}

Agentite_FinancialPeriod agentite_finances_sum_periods(const Agentite_FinancialTracker *tracker, int count) {
    Agentite_FinancialPeriod sum = {0, 0};
    if (!tracker || count <= 0) return sum;

    /* Cap count at available history */
    if (count > tracker->history_count) {
        count = tracker->history_count;
    }

    /* Sum from most recent backwards */
    for (int i = 0; i < count; i++) {
        const Agentite_FinancialPeriod *period = agentite_finances_get_history(tracker, i);
        if (period) {
            sum.revenue = agentite_safe_add(sum.revenue, period->revenue);
            sum.expenses = agentite_safe_add(sum.expenses, period->expenses);
        }
    }

    return sum;
}

Agentite_FinancialPeriod agentite_finances_avg_periods(const Agentite_FinancialTracker *tracker, int count) {
    Agentite_FinancialPeriod avg = {0, 0};
    if (!tracker || count <= 0) return avg;

    /* Cap count at available history */
    if (count > tracker->history_count) {
        count = tracker->history_count;
    }
    if (count == 0) return avg;

    Agentite_FinancialPeriod sum = agentite_finances_sum_periods(tracker, count);
    avg.revenue = sum.revenue / count;
    avg.expenses = sum.expenses / count;

    return avg;
}

const Agentite_FinancialPeriod *agentite_finances_get_history(const Agentite_FinancialTracker *tracker, int index) {
    if (!tracker || index < 0 || index >= tracker->history_count) {
        return NULL;
    }

    /* Index 0 = most recent (history_index - 1), wrapping around */
    int actual_index = (tracker->history_index - 1 - index + AGENTITE_FINANCES_MAX_HISTORY) % AGENTITE_FINANCES_MAX_HISTORY;
    return &tracker->history[actual_index];
}

int agentite_finances_get_history_count(const Agentite_FinancialTracker *tracker) {
    return tracker ? tracker->history_count : 0;
}

float agentite_finances_get_period_progress(const Agentite_FinancialTracker *tracker) {
    if (!tracker || tracker->period_duration <= 0.0f) return 0.0f;
    return tracker->time_in_period / tracker->period_duration;
}

int agentite_finances_get_periods_elapsed(const Agentite_FinancialTracker *tracker) {
    return tracker ? tracker->periods_elapsed : 0;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void agentite_finances_set_period_callback(Agentite_FinancialTracker *tracker,
                                          Agentite_FinancePeriodCallback callback,
                                          void *userdata) {
    AGENTITE_VALIDATE_PTR(tracker);
    tracker->period_callback = callback;
    tracker->callback_userdata = userdata;
}
