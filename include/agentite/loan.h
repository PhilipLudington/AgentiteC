#ifndef AGENTITE_LOAN_H
#define AGENTITE_LOAN_H

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
 *   Agentite_LoanSystem *loans = agentite_loan_create();
 *   agentite_loan_add_tier(loans, "Small Loan", 10000, 0.01f);   // 1% per period
 *   agentite_loan_add_tier(loans, "Medium Loan", 50000, 0.015f); // 1.5% per period
 *   agentite_loan_add_tier(loans, "Large Loan", 100000, 0.02f);  // 2% per period
 *
 *   // Player loan state
 *   Agentite_LoanState state;
 *   agentite_loan_state_init(&state);
 *
 *   // Take a loan
 *   if (agentite_loan_can_take(&state)) {
 *       int32_t money;
 *       agentite_loan_take(&state, loans, 0, &money);  // Tier 0
 *       player_money += money;
 *   }
 *
 *   // Each period, charge interest
 *   agentite_loan_charge_interest(&state, loans);
 *
 *   // Repay loan
 *   if (agentite_loan_can_repay(&state, player_money)) {
 *       int32_t cost;
 *       agentite_loan_repay(&state, &cost);
 *       player_money -= cost;
 *   }
 *
 *   agentite_loan_destroy(loans);
 */

/** Maximum number of loan tiers */
#define AGENTITE_LOAN_MAX_TIERS 8

/** Maximum loan name length */
#define AGENTITE_LOAN_NAME_LEN 32

/**
 * Loan tier definition
 */
typedef struct {
    char name[AGENTITE_LOAN_NAME_LEN]; /**< Tier name (e.g., "Small Loan") */
    int32_t principal;               /**< Amount to borrow */
    float interest_rate;             /**< Interest per period (0.01 = 1%) */
} Agentite_LoanTier;

/**
 * Per-player loan state
 */
typedef struct {
    int active_tier;              /**< Current loan tier (-1 if no loan) */
    int32_t principal;            /**< Original borrowed amount */
    int32_t amount_owed;          /**< Current balance (principal + accrued interest) */
    int32_t total_interest_paid;  /**< Lifetime interest payments */
    int periods_held;             /**< Number of periods loan has been held */
} Agentite_LoanState;

/**
 * Loan system (manages tiers)
 */
typedef struct Agentite_LoanSystem Agentite_LoanSystem;

/*============================================================================
 * Loan System Management
 *============================================================================*/

/**
 * Create a loan system.
 *
 * @return New loan system, or NULL on failure
 */
Agentite_LoanSystem *agentite_loan_create(void);

/**
 * Destroy a loan system.
 *
 * @param loans Loan system to destroy
 */
void agentite_loan_destroy(Agentite_LoanSystem *loans);

/**
 * Add a loan tier to the system.
 *
 * @param loans Loan system
 * @param name Tier name (for display)
 * @param principal Amount to borrow
 * @param interest_rate Interest per period (0.01 = 1%)
 * @return Tier index, or -1 on failure
 */
int agentite_loan_add_tier(Agentite_LoanSystem *loans, const char *name,
                         int32_t principal, float interest_rate);

/**
 * Get the number of loan tiers.
 *
 * @param loans Loan system
 * @return Number of tiers
 */
int agentite_loan_get_tier_count(const Agentite_LoanSystem *loans);

/**
 * Get a loan tier by index.
 *
 * @param loans Loan system
 * @param index Tier index
 * @return Pointer to tier, or NULL if invalid
 */
const Agentite_LoanTier *agentite_loan_get_tier(const Agentite_LoanSystem *loans, int index);

/*============================================================================
 * Loan State Management
 *============================================================================*/

/**
 * Initialize a loan state (no active loan).
 *
 * @param state Loan state to initialize
 */
void agentite_loan_state_init(Agentite_LoanState *state);

/**
 * Check if a new loan can be taken.
 * Cannot take a new loan while one is active.
 *
 * @param state Loan state
 * @return true if can take a loan
 */
bool agentite_loan_can_take(const Agentite_LoanState *state);

/**
 * Take a loan.
 *
 * @param state Loan state
 * @param loans Loan system (for tier info)
 * @param tier Tier index to borrow
 * @param out_money Output: amount received
 * @return true on success, false if cannot take loan
 */
bool agentite_loan_take(Agentite_LoanState *state, const Agentite_LoanSystem *loans,
                      int tier, int32_t *out_money);

/**
 * Check if the current loan can be repaid.
 *
 * @param state Loan state
 * @param available_money Available money for repayment
 * @return true if can repay
 */
bool agentite_loan_can_repay(const Agentite_LoanState *state, int32_t available_money);

/**
 * Repay the current loan.
 *
 * @param state Loan state
 * @param out_cost Output: repayment cost
 * @return true on success, false if no loan active
 */
bool agentite_loan_repay(Agentite_LoanState *state, int32_t *out_cost);

/**
 * Make a partial payment on the loan.
 *
 * @param state Loan state
 * @param amount Payment amount
 * @return Actual amount paid (may be less if loan fully paid)
 */
int32_t agentite_loan_pay(Agentite_LoanState *state, int32_t amount);

/**
 * Charge interest for one period.
 * Call at the end of each financial period.
 *
 * @param state Loan state
 * @param loans Loan system (for interest rate)
 * @return Interest charged this period
 */
int32_t agentite_loan_charge_interest(Agentite_LoanState *state, const Agentite_LoanSystem *loans);

/*============================================================================
 * Queries
 *============================================================================*/

/**
 * Check if a loan is currently active.
 *
 * @param state Loan state
 * @return true if a loan is active
 */
bool agentite_loan_is_active(const Agentite_LoanState *state);

/**
 * Get the current amount owed.
 *
 * @param state Loan state
 * @return Amount owed (0 if no loan)
 */
int32_t agentite_loan_get_amount_owed(const Agentite_LoanState *state);

/**
 * Get the original principal borrowed.
 *
 * @param state Loan state
 * @return Principal (0 if no loan)
 */
int32_t agentite_loan_get_principal(const Agentite_LoanState *state);

/**
 * Get total interest paid over lifetime.
 *
 * @param state Loan state
 * @return Total interest paid
 */
int32_t agentite_loan_get_total_interest(const Agentite_LoanState *state);

/**
 * Get the interest that will be charged next period.
 *
 * @param state Loan state
 * @param loans Loan system
 * @return Projected interest charge
 */
int32_t agentite_loan_get_projected_interest(const Agentite_LoanState *state, const Agentite_LoanSystem *loans);

/**
 * Get the name of the current loan tier.
 *
 * @param state Loan state
 * @param loans Loan system
 * @return Tier name, or NULL if no loan
 */
const char *agentite_loan_get_tier_name(const Agentite_LoanState *state, const Agentite_LoanSystem *loans);

#endif /* AGENTITE_LOAN_H */
