#include "carbon/carbon.h"
#include "carbon/finances.h"
#include "carbon/math_safe.h"
#include "carbon/validate.h"
#include <stdlib.h>
#include <string.h>

Carbon_FinancialTracker *carbon_finances_create(float period_duration) {
    Carbon_FinancialTracker *tracker = CARBON_ALLOC(Carbon_FinancialTracker);
    if (!tracker) return NULL;

    carbon_finances_init(tracker, period_duration);
    return tracker;
}

void carbon_finances_destroy(Carbon_FinancialTracker *tracker) {
    free(tracker);
}

void carbon_finances_init(Carbon_FinancialTracker *tracker, float period_duration) {
    CARBON_VALIDATE_PTR(tracker);

    memset(tracker, 0, sizeof(Carbon_FinancialTracker));
    tracker->period_duration = period_duration > 0.0f ? period_duration : 1.0f;
}

void carbon_finances_record_revenue(Carbon_FinancialTracker *tracker, int32_t amount) {
    CARBON_VALIDATE_PTR(tracker);
    if (amount <= 0) return;

    tracker->current.revenue = carbon_safe_add(tracker->current.revenue, amount);
    tracker->all_time.revenue = carbon_safe_add(tracker->all_time.revenue, amount);
}

void carbon_finances_record_expense(Carbon_FinancialTracker *tracker, int32_t amount) {
    CARBON_VALIDATE_PTR(tracker);
    if (amount <= 0) return;

    tracker->current.expenses = carbon_safe_add(tracker->current.expenses, amount);
    tracker->all_time.expenses = carbon_safe_add(tracker->all_time.expenses, amount);
}

/* Internal: roll over to next period */
static void rollover_period(Carbon_FinancialTracker *tracker) {
    /* Store completed period in history */
    tracker->history[tracker->history_index] = tracker->current;
    tracker->history_index = (tracker->history_index + 1) % CARBON_FINANCES_MAX_HISTORY;
    if (tracker->history_count < CARBON_FINANCES_MAX_HISTORY) {
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

void carbon_finances_update(Carbon_FinancialTracker *tracker, float dt) {
    CARBON_VALIDATE_PTR(tracker);

    tracker->time_in_period += dt;

    /* Check for period rollover(s) */
    while (tracker->time_in_period >= tracker->period_duration) {
        tracker->time_in_period -= tracker->period_duration;
        rollover_period(tracker);
    }
}

void carbon_finances_end_period(Carbon_FinancialTracker *tracker) {
    CARBON_VALIDATE_PTR(tracker);

    rollover_period(tracker);
    tracker->time_in_period = 0.0f;
}

void carbon_finances_reset(Carbon_FinancialTracker *tracker) {
    CARBON_VALIDATE_PTR(tracker);

    float duration = tracker->period_duration;
    Carbon_FinancePeriodCallback callback = tracker->period_callback;
    void *userdata = tracker->callback_userdata;

    memset(tracker, 0, sizeof(Carbon_FinancialTracker));

    tracker->period_duration = duration;
    tracker->period_callback = callback;
    tracker->callback_userdata = userdata;
}

/*============================================================================
 * Queries
 *============================================================================*/

int32_t carbon_finances_get_profit(const Carbon_FinancialPeriod *period) {
    if (!period) return 0;
    return carbon_safe_subtract(period->revenue, period->expenses);
}

int32_t carbon_finances_get_current_revenue(const Carbon_FinancialTracker *tracker) {
    return tracker ? tracker->current.revenue : 0;
}

int32_t carbon_finances_get_current_expenses(const Carbon_FinancialTracker *tracker) {
    return tracker ? tracker->current.expenses : 0;
}

int32_t carbon_finances_get_current_profit(const Carbon_FinancialTracker *tracker) {
    return tracker ? carbon_finances_get_profit(&tracker->current) : 0;
}

int32_t carbon_finances_get_last_profit(const Carbon_FinancialTracker *tracker) {
    return tracker ? carbon_finances_get_profit(&tracker->last_period) : 0;
}

int32_t carbon_finances_get_all_time_profit(const Carbon_FinancialTracker *tracker) {
    return tracker ? carbon_finances_get_profit(&tracker->all_time) : 0;
}

Carbon_FinancialPeriod carbon_finances_sum_periods(const Carbon_FinancialTracker *tracker, int count) {
    Carbon_FinancialPeriod sum = {0, 0};
    if (!tracker || count <= 0) return sum;

    /* Cap count at available history */
    if (count > tracker->history_count) {
        count = tracker->history_count;
    }

    /* Sum from most recent backwards */
    for (int i = 0; i < count; i++) {
        const Carbon_FinancialPeriod *period = carbon_finances_get_history(tracker, i);
        if (period) {
            sum.revenue = carbon_safe_add(sum.revenue, period->revenue);
            sum.expenses = carbon_safe_add(sum.expenses, period->expenses);
        }
    }

    return sum;
}

Carbon_FinancialPeriod carbon_finances_avg_periods(const Carbon_FinancialTracker *tracker, int count) {
    Carbon_FinancialPeriod avg = {0, 0};
    if (!tracker || count <= 0) return avg;

    /* Cap count at available history */
    if (count > tracker->history_count) {
        count = tracker->history_count;
    }
    if (count == 0) return avg;

    Carbon_FinancialPeriod sum = carbon_finances_sum_periods(tracker, count);
    avg.revenue = sum.revenue / count;
    avg.expenses = sum.expenses / count;

    return avg;
}

const Carbon_FinancialPeriod *carbon_finances_get_history(const Carbon_FinancialTracker *tracker, int index) {
    if (!tracker || index < 0 || index >= tracker->history_count) {
        return NULL;
    }

    /* Index 0 = most recent (history_index - 1), wrapping around */
    int actual_index = (tracker->history_index - 1 - index + CARBON_FINANCES_MAX_HISTORY) % CARBON_FINANCES_MAX_HISTORY;
    return &tracker->history[actual_index];
}

int carbon_finances_get_history_count(const Carbon_FinancialTracker *tracker) {
    return tracker ? tracker->history_count : 0;
}

float carbon_finances_get_period_progress(const Carbon_FinancialTracker *tracker) {
    if (!tracker || tracker->period_duration <= 0.0f) return 0.0f;
    return tracker->time_in_period / tracker->period_duration;
}

int carbon_finances_get_periods_elapsed(const Carbon_FinancialTracker *tracker) {
    return tracker ? tracker->periods_elapsed : 0;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void carbon_finances_set_period_callback(Carbon_FinancialTracker *tracker,
                                          Carbon_FinancePeriodCallback callback,
                                          void *userdata) {
    CARBON_VALIDATE_PTR(tracker);
    tracker->period_callback = callback;
    tracker->callback_userdata = userdata;
}
