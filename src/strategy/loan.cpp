#include "agentite/agentite.h"
#include "agentite/loan.h"
#include "agentite/math_safe.h"
#include "agentite/validate.h"
#include <stdlib.h>
#include <string.h>

/* Loan system structure */
struct Agentite_LoanSystem {
    Agentite_LoanTier tiers[AGENTITE_LOAN_MAX_TIERS];
    int tier_count;
};

/*============================================================================
 * Loan System Management
 *============================================================================*/

Agentite_LoanSystem *agentite_loan_create(void) {
    Agentite_LoanSystem *loans = AGENTITE_ALLOC(Agentite_LoanSystem);
    return loans;
}

void agentite_loan_destroy(Agentite_LoanSystem *loans) {
    free(loans);
}

int agentite_loan_add_tier(Agentite_LoanSystem *loans, const char *name,
                         int32_t principal, float interest_rate) {
    AGENTITE_VALIDATE_PTR_RET(loans, -1);
    AGENTITE_VALIDATE_PTR_RET(name, -1);

    if (loans->tier_count >= AGENTITE_LOAN_MAX_TIERS) {
        return -1;
    }
    if (principal <= 0 || interest_rate < 0.0f) {
        return -1;
    }

    int index = loans->tier_count;
    Agentite_LoanTier *tier = &loans->tiers[index];

    strncpy(tier->name, name, AGENTITE_LOAN_NAME_LEN - 1);
    tier->name[AGENTITE_LOAN_NAME_LEN - 1] = '\0';
    tier->principal = principal;
    tier->interest_rate = interest_rate;

    loans->tier_count++;
    return index;
}

int agentite_loan_get_tier_count(const Agentite_LoanSystem *loans) {
    return loans ? loans->tier_count : 0;
}

const Agentite_LoanTier *agentite_loan_get_tier(const Agentite_LoanSystem *loans, int index) {
    if (!loans || index < 0 || index >= loans->tier_count) {
        return NULL;
    }
    return &loans->tiers[index];
}

/*============================================================================
 * Loan State Management
 *============================================================================*/

void agentite_loan_state_init(Agentite_LoanState *state) {
    AGENTITE_VALIDATE_PTR(state);

    state->active_tier = -1;
    state->principal = 0;
    state->amount_owed = 0;
    state->total_interest_paid = 0;
    state->periods_held = 0;
}

bool agentite_loan_can_take(const Agentite_LoanState *state) {
    return state && state->active_tier < 0;
}

bool agentite_loan_take(Agentite_LoanState *state, const Agentite_LoanSystem *loans,
                      int tier, int32_t *out_money) {
    AGENTITE_VALIDATE_PTR_RET(state, false);
    AGENTITE_VALIDATE_PTR_RET(loans, false);

    if (!agentite_loan_can_take(state)) {
        return false;
    }

    const Agentite_LoanTier *tier_info = agentite_loan_get_tier(loans, tier);
    if (!tier_info) {
        return false;
    }

    state->active_tier = tier;
    state->principal = tier_info->principal;
    state->amount_owed = tier_info->principal;
    state->periods_held = 0;

    if (out_money) {
        *out_money = tier_info->principal;
    }

    return true;
}

bool agentite_loan_can_repay(const Agentite_LoanState *state, int32_t available_money) {
    if (!state || state->active_tier < 0) {
        return false;
    }
    return available_money >= state->amount_owed;
}

bool agentite_loan_repay(Agentite_LoanState *state, int32_t *out_cost) {
    AGENTITE_VALIDATE_PTR_RET(state, false);

    if (state->active_tier < 0) {
        return false;
    }

    if (out_cost) {
        *out_cost = state->amount_owed;
    }

    /* Calculate interest portion of repayment */
    int32_t interest_portion = state->amount_owed - state->principal;
    if (interest_portion > 0) {
        state->total_interest_paid = agentite_safe_add(state->total_interest_paid, interest_portion);
    }

    /* Clear loan */
    state->active_tier = -1;
    state->principal = 0;
    state->amount_owed = 0;
    state->periods_held = 0;

    return true;
}

int32_t agentite_loan_pay(Agentite_LoanState *state, int32_t amount) {
    AGENTITE_VALIDATE_PTR_RET(state, 0);

    if (state->active_tier < 0 || amount <= 0) {
        return 0;
    }

    /* Cap payment at amount owed */
    int32_t actual_payment = amount;
    if (actual_payment > state->amount_owed) {
        actual_payment = state->amount_owed;
    }

    state->amount_owed -= actual_payment;

    /* If fully paid, clear the loan */
    if (state->amount_owed <= 0) {
        state->active_tier = -1;
        state->principal = 0;
        state->amount_owed = 0;
        state->periods_held = 0;
    }

    return actual_payment;
}

int32_t agentite_loan_charge_interest(Agentite_LoanState *state, const Agentite_LoanSystem *loans) {
    AGENTITE_VALIDATE_PTR_RET(state, 0);
    AGENTITE_VALIDATE_PTR_RET(loans, 0);

    if (state->active_tier < 0) {
        return 0;
    }

    const Agentite_LoanTier *tier = agentite_loan_get_tier(loans, state->active_tier);
    if (!tier) {
        return 0;
    }

    /* Calculate interest: amount_owed * interest_rate */
    float interest_f = (float)state->amount_owed * tier->interest_rate;
    int32_t interest = (int32_t)(interest_f + 0.5f);  /* Round to nearest */

    if (interest > 0) {
        state->amount_owed = agentite_safe_add(state->amount_owed, interest);
        state->total_interest_paid = agentite_safe_add(state->total_interest_paid, interest);
    }

    state->periods_held++;

    return interest;
}

/*============================================================================
 * Queries
 *============================================================================*/

bool agentite_loan_is_active(const Agentite_LoanState *state) {
    return state && state->active_tier >= 0;
}

int32_t agentite_loan_get_amount_owed(const Agentite_LoanState *state) {
    return (state && state->active_tier >= 0) ? state->amount_owed : 0;
}

int32_t agentite_loan_get_principal(const Agentite_LoanState *state) {
    return (state && state->active_tier >= 0) ? state->principal : 0;
}

int32_t agentite_loan_get_total_interest(const Agentite_LoanState *state) {
    return state ? state->total_interest_paid : 0;
}

int32_t agentite_loan_get_projected_interest(const Agentite_LoanState *state, const Agentite_LoanSystem *loans) {
    if (!state || !loans || state->active_tier < 0) {
        return 0;
    }

    const Agentite_LoanTier *tier = agentite_loan_get_tier(loans, state->active_tier);
    if (!tier) {
        return 0;
    }

    float interest_f = (float)state->amount_owed * tier->interest_rate;
    return (int32_t)(interest_f + 0.5f);
}

const char *agentite_loan_get_tier_name(const Agentite_LoanState *state, const Agentite_LoanSystem *loans) {
    if (!state || !loans || state->active_tier < 0) {
        return NULL;
    }

    const Agentite_LoanTier *tier = agentite_loan_get_tier(loans, state->active_tier);
    return tier ? tier->name : NULL;
}
