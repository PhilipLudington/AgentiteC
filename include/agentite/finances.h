#ifndef AGENTITE_FINANCES_H
#define AGENTITE_FINANCES_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Carbon Financial Period Tracking
 *
 * Track revenue and expenses over rolling time periods for economy games.
 * Maintains current period, last period, all-time totals, and rolling history.
 *
 * Usage:
 *   // Create tracker (period duration in seconds)
 *   Agentite_FinancialTracker *finances = agentite_finances_create(30.0f);  // 30-second periods
 *
 *   // Record transactions
 *   agentite_finances_record_revenue(finances, 1000);
 *   agentite_finances_record_expense(finances, 500);
 *
 *   // Update each frame (handles period rollovers)
 *   agentite_finances_update(finances, delta_time);
 *
 *   // Query finances
 *   int32_t profit = agentite_finances_get_current_profit(finances);
 *   int32_t last_profit = agentite_finances_get_profit(&finances->last_period);
 *
 *   // Get rolling average
 *   Agentite_FinancialPeriod sum = agentite_finances_sum_periods(finances, 5);  // Last 5 periods
 *
 *   agentite_finances_destroy(finances);
 */

/** Maximum number of historical periods to track */
#define AGENTITE_FINANCES_MAX_HISTORY 12

/**
 * Financial data for a single period
 */
typedef struct {
    int32_t revenue;   /**< Total income during period */
    int32_t expenses;  /**< Total costs during period */
} Agentite_FinancialPeriod;

/**
 * Callback for period completion
 */
typedef void (*Agentite_FinancePeriodCallback)(const Agentite_FinancialPeriod *completed, void *userdata);

/**
 * Financial tracker structure
 */
typedef struct Agentite_FinancialTracker {
    /* Current accumulator */
    Agentite_FinancialPeriod current;

    /* Historical periods */
    Agentite_FinancialPeriod last_period;
    Agentite_FinancialPeriod all_time;

    /* Rolling history (circular buffer) */
    Agentite_FinancialPeriod history[AGENTITE_FINANCES_MAX_HISTORY];
    int history_index;   /**< Next write position */
    int history_count;   /**< Number of valid entries */

    /* Timing */
    float period_duration;  /**< Seconds per period */
    float time_in_period;   /**< Current progress */
    int periods_elapsed;    /**< Total periods completed */

    /* Callback */
    Agentite_FinancePeriodCallback period_callback;
    void *callback_userdata;
} Agentite_FinancialTracker;

/**
 * Create a financial tracker.
 *
 * @param period_duration Duration of each period in seconds
 * @return New tracker, or NULL on failure
 */
Agentite_FinancialTracker *agentite_finances_create(float period_duration);

/**
 * Destroy a financial tracker.
 *
 * @param tracker Tracker to destroy
 */
void agentite_finances_destroy(Agentite_FinancialTracker *tracker);

/**
 * Initialize a tracker (for stack-allocated structures).
 *
 * @param tracker Tracker to initialize
 * @param period_duration Duration of each period in seconds
 */
void agentite_finances_init(Agentite_FinancialTracker *tracker, float period_duration);

/**
 * Record revenue (income).
 *
 * @param tracker Financial tracker
 * @param amount Revenue amount (positive)
 */
void agentite_finances_record_revenue(Agentite_FinancialTracker *tracker, int32_t amount);

/**
 * Record an expense (cost).
 *
 * @param tracker Financial tracker
 * @param amount Expense amount (positive)
 */
void agentite_finances_record_expense(Agentite_FinancialTracker *tracker, int32_t amount);

/**
 * Update the tracker (call each frame).
 * Handles period rollovers automatically.
 *
 * @param tracker Financial tracker
 * @param dt Delta time in seconds
 */
void agentite_finances_update(Agentite_FinancialTracker *tracker, float dt);

/**
 * Force a period rollover (e.g., for turn-based games).
 *
 * @param tracker Financial tracker
 */
void agentite_finances_end_period(Agentite_FinancialTracker *tracker);

/**
 * Reset all financial data.
 *
 * @param tracker Financial tracker
 */
void agentite_finances_reset(Agentite_FinancialTracker *tracker);

/*============================================================================
 * Queries
 *============================================================================*/

/**
 * Get profit (revenue - expenses) for a period.
 *
 * @param period Financial period
 * @return Profit (can be negative)
 */
int32_t agentite_finances_get_profit(const Agentite_FinancialPeriod *period);

/**
 * Get current period revenue.
 *
 * @param tracker Financial tracker
 * @return Current period revenue
 */
int32_t agentite_finances_get_current_revenue(const Agentite_FinancialTracker *tracker);

/**
 * Get current period expenses.
 *
 * @param tracker Financial tracker
 * @return Current period expenses
 */
int32_t agentite_finances_get_current_expenses(const Agentite_FinancialTracker *tracker);

/**
 * Get current period profit.
 *
 * @param tracker Financial tracker
 * @return Current period profit (revenue - expenses)
 */
int32_t agentite_finances_get_current_profit(const Agentite_FinancialTracker *tracker);

/**
 * Get last completed period profit.
 *
 * @param tracker Financial tracker
 * @return Last period profit
 */
int32_t agentite_finances_get_last_profit(const Agentite_FinancialTracker *tracker);

/**
 * Get all-time profit.
 *
 * @param tracker Financial tracker
 * @return All-time profit
 */
int32_t agentite_finances_get_all_time_profit(const Agentite_FinancialTracker *tracker);

/**
 * Get sum of the last N periods.
 *
 * @param tracker Financial tracker
 * @param count Number of periods to sum (capped at history_count)
 * @return Summed financial period
 */
Agentite_FinancialPeriod agentite_finances_sum_periods(const Agentite_FinancialTracker *tracker, int count);

/**
 * Get average of the last N periods.
 *
 * @param tracker Financial tracker
 * @param count Number of periods to average
 * @return Averaged financial period (integer division)
 */
Agentite_FinancialPeriod agentite_finances_avg_periods(const Agentite_FinancialTracker *tracker, int count);

/**
 * Get a historical period by index (0 = most recent completed).
 *
 * @param tracker Financial tracker
 * @param index Period index (0 = most recent)
 * @return Pointer to period, or NULL if invalid index
 */
const Agentite_FinancialPeriod *agentite_finances_get_history(const Agentite_FinancialTracker *tracker, int index);

/**
 * Get the number of historical periods available.
 *
 * @param tracker Financial tracker
 * @return Number of periods in history
 */
int agentite_finances_get_history_count(const Agentite_FinancialTracker *tracker);

/**
 * Get progress through current period (0.0 to 1.0).
 *
 * @param tracker Financial tracker
 * @return Period progress
 */
float agentite_finances_get_period_progress(const Agentite_FinancialTracker *tracker);

/**
 * Get total number of periods elapsed.
 *
 * @param tracker Financial tracker
 * @return Periods elapsed
 */
int agentite_finances_get_periods_elapsed(const Agentite_FinancialTracker *tracker);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * Set callback for period completion.
 *
 * @param tracker Financial tracker
 * @param callback Function to call when period ends
 * @param userdata User data passed to callback
 */
void agentite_finances_set_period_callback(Agentite_FinancialTracker *tracker,
                                          Agentite_FinancePeriodCallback callback,
                                          void *userdata);

#endif /* AGENTITE_FINANCES_H */
