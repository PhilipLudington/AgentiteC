#ifndef CARBON_LOAN_H
#define CARBON_LOAN_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Carbon Loan/Credit System
 *
 * Tiered loan system with interest for economy games.
 * Define loan tiers with different principal amounts and interest rates.
 *
 * Usage:
 *   // Create loan system with tiers
 *   Carbon_LoanSystem *loans = carbon_loan_create();
 *   carbon_loan_add_tier(loans, "Small Loan", 10000, 0.01f);   // 1% per period
 *   carbon_loan_add_tier(loans, "Medium Loan", 50000, 0.015f); // 1.5% per period
 *   carbon_loan_add_tier(loans, "Large Loan", 100000, 0.02f);  // 2% per period
 *
 *   // Player loan state
 *   Carbon_LoanState state;
 *   carbon_loan_state_init(&state);
 *
 *   // Take a loan
 *   if (carbon_loan_can_take(&state)) {
 *       int32_t money;
 *       carbon_loan_take(&state, loans, 0, &money);  // Tier 0
 *       player_money += money;
 *   }
 *
 *   // Each period, charge interest
 *   carbon_loan_charge_interest(&state, loans);
 *
 *   // Repay loan
 *   if (carbon_loan_can_repay(&state, player_money)) {
 *       int32_t cost;
 *       carbon_loan_repay(&state, &cost);
 *       player_money -= cost;
 *   }
 *
 *   carbon_loan_destroy(loans);
 */

/** Maximum number of loan tiers */
#define CARBON_LOAN_MAX_TIERS 8

/** Maximum loan name length */
#define CARBON_LOAN_NAME_LEN 32

/**
 * Loan tier definition
 */
typedef struct {
    char name[CARBON_LOAN_NAME_LEN]; /**< Tier name (e.g., "Small Loan") */
    int32_t principal;               /**< Amount to borrow */
    float interest_rate;             /**< Interest per period (0.01 = 1%) */
} Carbon_LoanTier;

/**
 * Per-player loan state
 */
typedef struct {
    int active_tier;              /**< Current loan tier (-1 if no loan) */
    int32_t principal;            /**< Original borrowed amount */
    int32_t amount_owed;          /**< Current balance (principal + accrued interest) */
    int32_t total_interest_paid;  /**< Lifetime interest payments */
    int periods_held;             /**< Number of periods loan has been held */
} Carbon_LoanState;

/**
 * Loan system (manages tiers)
 */
typedef struct Carbon_LoanSystem Carbon_LoanSystem;

/*============================================================================
 * Loan System Management
 *============================================================================*/

/**
 * Create a loan system.
 *
 * @return New loan system, or NULL on failure
 */
Carbon_LoanSystem *carbon_loan_create(void);

/**
 * Destroy a loan system.
 *
 * @param loans Loan system to destroy
 */
void carbon_loan_destroy(Carbon_LoanSystem *loans);

/**
 * Add a loan tier to the system.
 *
 * @param loans Loan system
 * @param name Tier name (for display)
 * @param principal Amount to borrow
 * @param interest_rate Interest per period (0.01 = 1%)
 * @return Tier index, or -1 on failure
 */
int carbon_loan_add_tier(Carbon_LoanSystem *loans, const char *name,
                         int32_t principal, float interest_rate);

/**
 * Get the number of loan tiers.
 *
 * @param loans Loan system
 * @return Number of tiers
 */
int carbon_loan_get_tier_count(const Carbon_LoanSystem *loans);

/**
 * Get a loan tier by index.
 *
 * @param loans Loan system
 * @param index Tier index
 * @return Pointer to tier, or NULL if invalid
 */
const Carbon_LoanTier *carbon_loan_get_tier(const Carbon_LoanSystem *loans, int index);

/*============================================================================
 * Loan State Management
 *============================================================================*/

/**
 * Initialize a loan state (no active loan).
 *
 * @param state Loan state to initialize
 */
void carbon_loan_state_init(Carbon_LoanState *state);

/**
 * Check if a new loan can be taken.
 * Cannot take a new loan while one is active.
 *
 * @param state Loan state
 * @return true if can take a loan
 */
bool carbon_loan_can_take(const Carbon_LoanState *state);

/**
 * Take a loan.
 *
 * @param state Loan state
 * @param loans Loan system (for tier info)
 * @param tier Tier index to borrow
 * @param out_money Output: amount received
 * @return true on success, false if cannot take loan
 */
bool carbon_loan_take(Carbon_LoanState *state, const Carbon_LoanSystem *loans,
                      int tier, int32_t *out_money);

/**
 * Check if the current loan can be repaid.
 *
 * @param state Loan state
 * @param available_money Available money for repayment
 * @return true if can repay
 */
bool carbon_loan_can_repay(const Carbon_LoanState *state, int32_t available_money);

/**
 * Repay the current loan.
 *
 * @param state Loan state
 * @param out_cost Output: repayment cost
 * @return true on success, false if no loan active
 */
bool carbon_loan_repay(Carbon_LoanState *state, int32_t *out_cost);

/**
 * Make a partial payment on the loan.
 *
 * @param state Loan state
 * @param amount Payment amount
 * @return Actual amount paid (may be less if loan fully paid)
 */
int32_t carbon_loan_pay(Carbon_LoanState *state, int32_t amount);

/**
 * Charge interest for one period.
 * Call at the end of each financial period.
 *
 * @param state Loan state
 * @param loans Loan system (for interest rate)
 * @return Interest charged this period
 */
int32_t carbon_loan_charge_interest(Carbon_LoanState *state, const Carbon_LoanSystem *loans);

/*============================================================================
 * Queries
 *============================================================================*/

/**
 * Check if a loan is currently active.
 *
 * @param state Loan state
 * @return true if a loan is active
 */
bool carbon_loan_is_active(const Carbon_LoanState *state);

/**
 * Get the current amount owed.
 *
 * @param state Loan state
 * @return Amount owed (0 if no loan)
 */
int32_t carbon_loan_get_amount_owed(const Carbon_LoanState *state);

/**
 * Get the original principal borrowed.
 *
 * @param state Loan state
 * @return Principal (0 if no loan)
 */
int32_t carbon_loan_get_principal(const Carbon_LoanState *state);

/**
 * Get total interest paid over lifetime.
 *
 * @param state Loan state
 * @return Total interest paid
 */
int32_t carbon_loan_get_total_interest(const Carbon_LoanState *state);

/**
 * Get the interest that will be charged next period.
 *
 * @param state Loan state
 * @param loans Loan system
 * @return Projected interest charge
 */
int32_t carbon_loan_get_projected_interest(const Carbon_LoanState *state, const Carbon_LoanSystem *loans);

/**
 * Get the name of the current loan tier.
 *
 * @param state Loan state
 * @param loans Loan system
 * @return Tier name, or NULL if no loan
 */
const char *carbon_loan_get_tier_name(const Carbon_LoanState *state, const Carbon_LoanSystem *loans);

#endif /* CARBON_LOAN_H */
