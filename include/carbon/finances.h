#ifndef CARBON_FINANCES_H
#define CARBON_FINANCES_H

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
 *   Carbon_FinancialTracker *finances = carbon_finances_create(30.0f);  // 30-second periods
 *
 *   // Record transactions
 *   carbon_finances_record_revenue(finances, 1000);
 *   carbon_finances_record_expense(finances, 500);
 *
 *   // Update each frame (handles period rollovers)
 *   carbon_finances_update(finances, delta_time);
 *
 *   // Query finances
 *   int32_t profit = carbon_finances_get_current_profit(finances);
 *   int32_t last_profit = carbon_finances_get_profit(&finances->last_period);
 *
 *   // Get rolling average
 *   Carbon_FinancialPeriod sum = carbon_finances_sum_periods(finances, 5);  // Last 5 periods
 *
 *   carbon_finances_destroy(finances);
 */

/** Maximum number of historical periods to track */
#define CARBON_FINANCES_MAX_HISTORY 12

/**
 * Financial data for a single period
 */
typedef struct {
    int32_t revenue;   /**< Total income during period */
    int32_t expenses;  /**< Total costs during period */
} Carbon_FinancialPeriod;

/**
 * Callback for period completion
 */
typedef void (*Carbon_FinancePeriodCallback)(const Carbon_FinancialPeriod *completed, void *userdata);

/**
 * Financial tracker structure
 */
typedef struct Carbon_FinancialTracker {
    /* Current accumulator */
    Carbon_FinancialPeriod current;

    /* Historical periods */
    Carbon_FinancialPeriod last_period;
    Carbon_FinancialPeriod all_time;

    /* Rolling history (circular buffer) */
    Carbon_FinancialPeriod history[CARBON_FINANCES_MAX_HISTORY];
    int history_index;   /**< Next write position */
    int history_count;   /**< Number of valid entries */

    /* Timing */
    float period_duration;  /**< Seconds per period */
    float time_in_period;   /**< Current progress */
    int periods_elapsed;    /**< Total periods completed */

    /* Callback */
    Carbon_FinancePeriodCallback period_callback;
    void *callback_userdata;
} Carbon_FinancialTracker;

/**
 * Create a financial tracker.
 *
 * @param period_duration Duration of each period in seconds
 * @return New tracker, or NULL on failure
 */
Carbon_FinancialTracker *carbon_finances_create(float period_duration);

/**
 * Destroy a financial tracker.
 *
 * @param tracker Tracker to destroy
 */
void carbon_finances_destroy(Carbon_FinancialTracker *tracker);

/**
 * Initialize a tracker (for stack-allocated structures).
 *
 * @param tracker Tracker to initialize
 * @param period_duration Duration of each period in seconds
 */
void carbon_finances_init(Carbon_FinancialTracker *tracker, float period_duration);

/**
 * Record revenue (income).
 *
 * @param tracker Financial tracker
 * @param amount Revenue amount (positive)
 */
void carbon_finances_record_revenue(Carbon_FinancialTracker *tracker, int32_t amount);

/**
 * Record an expense (cost).
 *
 * @param tracker Financial tracker
 * @param amount Expense amount (positive)
 */
void carbon_finances_record_expense(Carbon_FinancialTracker *tracker, int32_t amount);

/**
 * Update the tracker (call each frame).
 * Handles period rollovers automatically.
 *
 * @param tracker Financial tracker
 * @param dt Delta time in seconds
 */
void carbon_finances_update(Carbon_FinancialTracker *tracker, float dt);

/**
 * Force a period rollover (e.g., for turn-based games).
 *
 * @param tracker Financial tracker
 */
void carbon_finances_end_period(Carbon_FinancialTracker *tracker);

/**
 * Reset all financial data.
 *
 * @param tracker Financial tracker
 */
void carbon_finances_reset(Carbon_FinancialTracker *tracker);

/*============================================================================
 * Queries
 *============================================================================*/

/**
 * Get profit (revenue - expenses) for a period.
 *
 * @param period Financial period
 * @return Profit (can be negative)
 */
int32_t carbon_finances_get_profit(const Carbon_FinancialPeriod *period);

/**
 * Get current period revenue.
 *
 * @param tracker Financial tracker
 * @return Current period revenue
 */
int32_t carbon_finances_get_current_revenue(const Carbon_FinancialTracker *tracker);

/**
 * Get current period expenses.
 *
 * @param tracker Financial tracker
 * @return Current period expenses
 */
int32_t carbon_finances_get_current_expenses(const Carbon_FinancialTracker *tracker);

/**
 * Get current period profit.
 *
 * @param tracker Financial tracker
 * @return Current period profit (revenue - expenses)
 */
int32_t carbon_finances_get_current_profit(const Carbon_FinancialTracker *tracker);

/**
 * Get last completed period profit.
 *
 * @param tracker Financial tracker
 * @return Last period profit
 */
int32_t carbon_finances_get_last_profit(const Carbon_FinancialTracker *tracker);

/**
 * Get all-time profit.
 *
 * @param tracker Financial tracker
 * @return All-time profit
 */
int32_t carbon_finances_get_all_time_profit(const Carbon_FinancialTracker *tracker);

/**
 * Get sum of the last N periods.
 *
 * @param tracker Financial tracker
 * @param count Number of periods to sum (capped at history_count)
 * @return Summed financial period
 */
Carbon_FinancialPeriod carbon_finances_sum_periods(const Carbon_FinancialTracker *tracker, int count);

/**
 * Get average of the last N periods.
 *
 * @param tracker Financial tracker
 * @param count Number of periods to average
 * @return Averaged financial period (integer division)
 */
Carbon_FinancialPeriod carbon_finances_avg_periods(const Carbon_FinancialTracker *tracker, int count);

/**
 * Get a historical period by index (0 = most recent completed).
 *
 * @param tracker Financial tracker
 * @param index Period index (0 = most recent)
 * @return Pointer to period, or NULL if invalid index
 */
const Carbon_FinancialPeriod *carbon_finances_get_history(const Carbon_FinancialTracker *tracker, int index);

/**
 * Get the number of historical periods available.
 *
 * @param tracker Financial tracker
 * @return Number of periods in history
 */
int carbon_finances_get_history_count(const Carbon_FinancialTracker *tracker);

/**
 * Get progress through current period (0.0 to 1.0).
 *
 * @param tracker Financial tracker
 * @return Period progress
 */
float carbon_finances_get_period_progress(const Carbon_FinancialTracker *tracker);

/**
 * Get total number of periods elapsed.
 *
 * @param tracker Financial tracker
 * @return Periods elapsed
 */
int carbon_finances_get_periods_elapsed(const Carbon_FinancialTracker *tracker);

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
void carbon_finances_set_period_callback(Carbon_FinancialTracker *tracker,
                                          Carbon_FinancePeriodCallback callback,
                                          void *userdata);

#endif /* CARBON_FINANCES_H */
