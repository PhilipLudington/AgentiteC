/**
 * @file strategy.c
 * @brief Strategic Coordinator implementation
 */

#include "agentite/agentite.h"
#include "agentite/strategy.h"
#include "agentite/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -------------------------------------------------------------------------- */
/*                              Internal Types                                 */
/* -------------------------------------------------------------------------- */

typedef struct Agentite_StrategyCoordinator {
    /* Options */
    Agentite_StrategyOption options[AGENTITE_STRATEGY_MAX_OPTIONS];
    int option_count;

    /* Min/max allocation constraints */
    float min_allocations[AGENTITE_STRATEGY_MAX_OPTIONS];
    float max_allocations[AGENTITE_STRATEGY_MAX_OPTIONS];

    /* Phase detection */
    Agentite_GamePhase current_phase;
    float phase_thresholds[3];  /* early->mid, mid->late, late->end */
    Agentite_PhaseAnalyzer phase_analyzer;
    void *analyzer_userdata;
    bool phase_override;        /* True if phase was manually set */

    /* Input provider */
    Agentite_InputProvider input_provider;
    void *input_userdata;

    /* Statistics */
    int evaluations;
    int phase_changes;
} Agentite_StrategyCoordinator;

/* -------------------------------------------------------------------------- */
/*                              Phase Names                                    */
/* -------------------------------------------------------------------------- */

static const char *s_phase_names[AGENTITE_GAME_PHASE_COUNT] = {
    "Early Expansion",
    "Mid Consolidation",
    "Late Competition",
    "Endgame"
};

/* -------------------------------------------------------------------------- */
/*                           Lifecycle Functions                               */
/* -------------------------------------------------------------------------- */

Agentite_StrategyCoordinator *agentite_strategy_create(void) {
    Agentite_StrategyCoordinator *coord = AGENTITE_ALLOC(Agentite_StrategyCoordinator);
    if (!coord) {
        agentite_set_error("Failed to allocate strategy coordinator");
        return NULL;
    }

    /* Set default phase thresholds */
    coord->phase_thresholds[0] = 0.33f;  /* early -> mid */
    coord->phase_thresholds[1] = 0.66f;  /* mid -> late */
    coord->phase_thresholds[2] = 0.90f;  /* late -> endgame */

    /* Initialize allocations to no constraints */
    for (int i = 0; i < AGENTITE_STRATEGY_MAX_OPTIONS; i++) {
        coord->min_allocations[i] = 0.0f;
        coord->max_allocations[i] = 1.0f;
    }

    return coord;
}

void agentite_strategy_destroy(Agentite_StrategyCoordinator *coord) {
    if (coord) {
        free(coord);
    }
}

void agentite_strategy_reset(Agentite_StrategyCoordinator *coord) {
    if (!coord) return;

    coord->option_count = 0;
    coord->current_phase = AGENTITE_GAME_PHASE_EARLY_EXPANSION;
    coord->phase_override = false;
    coord->evaluations = 0;
    coord->phase_changes = 0;

    for (int i = 0; i < AGENTITE_STRATEGY_MAX_OPTIONS; i++) {
        coord->min_allocations[i] = 0.0f;
        coord->max_allocations[i] = 1.0f;
    }
}

/* -------------------------------------------------------------------------- */
/*                           Phase Detection                                   */
/* -------------------------------------------------------------------------- */

void agentite_strategy_set_phase_thresholds(Agentite_StrategyCoordinator *coord,
                                          float early_to_mid,
                                          float mid_to_late,
                                          float late_to_end) {
    if (!coord) return;

    coord->phase_thresholds[0] = early_to_mid;
    coord->phase_thresholds[1] = mid_to_late;
    coord->phase_thresholds[2] = late_to_end;
}

void agentite_strategy_set_phase_analyzer(Agentite_StrategyCoordinator *coord,
                                        Agentite_PhaseAnalyzer analyzer,
                                        void *userdata) {
    if (!coord) return;

    coord->phase_analyzer = analyzer;
    coord->analyzer_userdata = userdata;
}

Agentite_GamePhase agentite_strategy_detect_phase(Agentite_StrategyCoordinator *coord,
                                              void *game_state) {
    if (!coord) return AGENTITE_GAME_PHASE_EARLY_EXPANSION;

    /* If phase was manually overridden, return that */
    if (coord->phase_override) {
        return coord->current_phase;
    }

    /* If no analyzer, use current phase */
    if (!coord->phase_analyzer) {
        return coord->current_phase;
    }

    /* Get metrics from analyzer */
    float metrics[8];
    int metric_count = coord->phase_analyzer(game_state, metrics, 8,
                                              coord->analyzer_userdata);

    if (metric_count == 0) {
        return coord->current_phase;
    }

    /* Calculate average metric */
    float avg = 0.0f;
    for (int i = 0; i < metric_count; i++) {
        avg += metrics[i];
    }
    avg /= (float)metric_count;

    /* Determine phase based on thresholds */
    Agentite_GamePhase old_phase = coord->current_phase;
    Agentite_GamePhase new_phase;

    if (avg < coord->phase_thresholds[0]) {
        new_phase = AGENTITE_GAME_PHASE_EARLY_EXPANSION;
    } else if (avg < coord->phase_thresholds[1]) {
        new_phase = AGENTITE_GAME_PHASE_MID_CONSOLIDATION;
    } else if (avg < coord->phase_thresholds[2]) {
        new_phase = AGENTITE_GAME_PHASE_LATE_COMPETITION;
    } else {
        new_phase = AGENTITE_GAME_PHASE_ENDGAME;
    }

    /* Track phase changes */
    if (new_phase != old_phase) {
        coord->phase_changes++;
    }

    coord->current_phase = new_phase;
    return new_phase;
}

bool agentite_strategy_analyze_phase(Agentite_StrategyCoordinator *coord,
                                   void *game_state,
                                   Agentite_PhaseAnalysis *out_analysis) {
    if (!coord || !out_analysis) return false;

    memset(out_analysis, 0, sizeof(Agentite_PhaseAnalysis));

    /* Detect phase (updates coord->current_phase) */
    out_analysis->phase = agentite_strategy_detect_phase(coord, game_state);

    /* If we have an analyzer, get detailed metrics */
    if (coord->phase_analyzer) {
        out_analysis->metric_count = coord->phase_analyzer(
            game_state, out_analysis->metrics, 8, coord->analyzer_userdata);

        /* Calculate progress within phase */
        float avg = 0.0f;
        for (int i = 0; i < out_analysis->metric_count; i++) {
            avg += out_analysis->metrics[i];
        }
        if (out_analysis->metric_count > 0) {
            avg /= (float)out_analysis->metric_count;
        }

        /* Calculate progress within current phase */
        float phase_start = 0.0f;
        float phase_end = 1.0f;

        switch (out_analysis->phase) {
            case AGENTITE_GAME_PHASE_EARLY_EXPANSION:
                phase_start = 0.0f;
                phase_end = coord->phase_thresholds[0];
                break;
            case AGENTITE_GAME_PHASE_MID_CONSOLIDATION:
                phase_start = coord->phase_thresholds[0];
                phase_end = coord->phase_thresholds[1];
                break;
            case AGENTITE_GAME_PHASE_LATE_COMPETITION:
                phase_start = coord->phase_thresholds[1];
                phase_end = coord->phase_thresholds[2];
                break;
            case AGENTITE_GAME_PHASE_ENDGAME:
                phase_start = coord->phase_thresholds[2];
                phase_end = 1.0f;
                break;
            default:
                break;
        }

        float phase_range = phase_end - phase_start;
        if (phase_range > 0.0f) {
            out_analysis->progress = (avg - phase_start) / phase_range;
            out_analysis->progress = fmaxf(0.0f, fminf(1.0f, out_analysis->progress));
        }

        /* Confidence based on how far from threshold we are */
        float min_dist = 1.0f;
        for (int i = 0; i < 3; i++) {
            float dist = fabsf(avg - coord->phase_thresholds[i]);
            if (dist < min_dist) min_dist = dist;
        }
        out_analysis->confidence = fminf(1.0f, min_dist * 5.0f);
    } else {
        out_analysis->confidence = 1.0f;  /* Manual override = full confidence */
    }

    return true;
}

Agentite_GamePhase agentite_strategy_get_current_phase(const Agentite_StrategyCoordinator *coord) {
    if (!coord) return AGENTITE_GAME_PHASE_EARLY_EXPANSION;
    return coord->current_phase;
}

const char *agentite_strategy_phase_name(Agentite_GamePhase phase) {
    if (phase < 0 || phase >= AGENTITE_GAME_PHASE_COUNT) {
        return "Unknown";
    }
    return s_phase_names[phase];
}

void agentite_strategy_set_phase(Agentite_StrategyCoordinator *coord, Agentite_GamePhase phase) {
    if (!coord) return;

    if (phase != coord->current_phase) {
        coord->phase_changes++;
    }
    coord->current_phase = phase;
    coord->phase_override = true;
}

/* -------------------------------------------------------------------------- */
/*                           Option Management                                 */
/* -------------------------------------------------------------------------- */

int agentite_strategy_add_option(Agentite_StrategyCoordinator *coord,
                               const char *name,
                               const Agentite_UtilityCurve *curve,
                               float base_weight) {
    if (!coord || !name || !curve) return -1;

    if (coord->option_count >= AGENTITE_STRATEGY_MAX_OPTIONS) {
        agentite_set_error("Maximum strategy options reached");
        return -1;
    }

    /* Check for duplicate name */
    if (agentite_strategy_find_option(coord, name) >= 0) {
        agentite_set_error("Option '%s' already exists", name);
        return -1;
    }

    int idx = coord->option_count++;
    Agentite_StrategyOption *opt = &coord->options[idx];

    strncpy(opt->name, name, AGENTITE_STRATEGY_MAX_NAME_LEN - 1);
    opt->name[AGENTITE_STRATEGY_MAX_NAME_LEN - 1] = '\0';
    opt->curve = *curve;
    opt->base_weight = base_weight;
    opt->current_input = 0.0f;
    opt->current_utility = 0.0f;
    opt->active = true;

    /* Initialize phase modifiers to 1.0 (no modification) */
    for (int i = 0; i < AGENTITE_PHASE_COUNT; i++) {
        opt->phase_modifiers[i] = 1.0f;
    }

    return idx;
}

bool agentite_strategy_remove_option(Agentite_StrategyCoordinator *coord, const char *name) {
    if (!coord || !name) return false;

    int idx = agentite_strategy_find_option(coord, name);
    if (idx < 0) return false;

    /* Shift remaining options */
    for (int i = idx; i < coord->option_count - 1; i++) {
        coord->options[i] = coord->options[i + 1];
        coord->min_allocations[i] = coord->min_allocations[i + 1];
        coord->max_allocations[i] = coord->max_allocations[i + 1];
    }

    coord->option_count--;
    return true;
}

int agentite_strategy_find_option(const Agentite_StrategyCoordinator *coord, const char *name) {
    if (!coord || !name) return -1;

    for (int i = 0; i < coord->option_count; i++) {
        if (strcmp(coord->options[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int agentite_strategy_get_option_count(const Agentite_StrategyCoordinator *coord) {
    if (!coord) return 0;
    return coord->option_count;
}

const Agentite_StrategyOption *agentite_strategy_get_option(const Agentite_StrategyCoordinator *coord,
                                                        int index) {
    if (!coord || index < 0 || index >= coord->option_count) return NULL;
    return &coord->options[index];
}

void agentite_strategy_set_option_weight(Agentite_StrategyCoordinator *coord,
                                       const char *name, float weight) {
    if (!coord || !name) return;

    int idx = agentite_strategy_find_option(coord, name);
    if (idx >= 0) {
        coord->options[idx].base_weight = weight;
    }
}

void agentite_strategy_set_option_active(Agentite_StrategyCoordinator *coord,
                                       const char *name, bool active) {
    if (!coord || !name) return;

    int idx = agentite_strategy_find_option(coord, name);
    if (idx >= 0) {
        coord->options[idx].active = active;
    }
}

/* -------------------------------------------------------------------------- */
/*                           Phase Modifiers                                   */
/* -------------------------------------------------------------------------- */

void agentite_strategy_set_phase_modifier(Agentite_StrategyCoordinator *coord,
                                        const char *option_name,
                                        Agentite_GamePhase phase,
                                        float modifier) {
    if (!coord || !option_name) return;
    if (phase < 0 || phase >= AGENTITE_GAME_PHASE_COUNT) return;

    int idx = agentite_strategy_find_option(coord, option_name);
    if (idx >= 0) {
        coord->options[idx].phase_modifiers[phase] = modifier;
    }
}

float agentite_strategy_get_phase_modifier(const Agentite_StrategyCoordinator *coord,
                                         const char *option_name,
                                         Agentite_GamePhase phase) {
    if (!coord || !option_name) return 1.0f;
    if (phase < 0 || phase >= AGENTITE_GAME_PHASE_COUNT) return 1.0f;

    int idx = agentite_strategy_find_option(coord, option_name);
    if (idx >= 0) {
        return coord->options[idx].phase_modifiers[phase];
    }
    return 1.0f;
}

void agentite_strategy_set_all_phase_modifiers(Agentite_StrategyCoordinator *coord,
                                             const char *option_name,
                                             const float *modifiers) {
    if (!coord || !option_name || !modifiers) return;

    int idx = agentite_strategy_find_option(coord, option_name);
    if (idx >= 0) {
        memcpy(coord->options[idx].phase_modifiers, modifiers,
               sizeof(float) * AGENTITE_GAME_PHASE_COUNT);
    }
}

/* -------------------------------------------------------------------------- */
/*                           Utility Evaluation                                */
/* -------------------------------------------------------------------------- */

void agentite_strategy_set_input_provider(Agentite_StrategyCoordinator *coord,
                                        Agentite_InputProvider provider,
                                        void *userdata) {
    if (!coord) return;

    coord->input_provider = provider;
    coord->input_userdata = userdata;
}

void agentite_strategy_set_input(Agentite_StrategyCoordinator *coord,
                               const char *option_name, float input) {
    if (!coord || !option_name) return;

    int idx = agentite_strategy_find_option(coord, option_name);
    if (idx >= 0) {
        coord->options[idx].current_input = fmaxf(0.0f, fminf(1.0f, input));
    }
}

void agentite_strategy_evaluate_options(Agentite_StrategyCoordinator *coord,
                                      void *game_state) {
    if (!coord) return;

    coord->evaluations++;

    for (int i = 0; i < coord->option_count; i++) {
        Agentite_StrategyOption *opt = &coord->options[i];

        if (!opt->active) {
            opt->current_utility = 0.0f;
            continue;
        }

        /* Get input value */
        if (coord->input_provider) {
            opt->current_input = coord->input_provider(game_state, opt->name,
                                                        coord->input_userdata);
            opt->current_input = fmaxf(0.0f, fminf(1.0f, opt->current_input));
        }

        /* Evaluate curve */
        float raw_utility = agentite_curve_evaluate(&opt->curve, opt->current_input);

        /* Apply base weight */
        float weighted = raw_utility * opt->base_weight;

        /* Apply phase modifier */
        float phase_mod = opt->phase_modifiers[coord->current_phase];
        opt->current_utility = weighted * phase_mod;
    }
}

float agentite_strategy_get_utility(const Agentite_StrategyCoordinator *coord,
                                  const char *option_name) {
    if (!coord || !option_name) return -1.0f;

    int idx = agentite_strategy_find_option(coord, option_name);
    if (idx >= 0) {
        return coord->options[idx].current_utility;
    }
    return -1.0f;
}

const char *agentite_strategy_get_best_option(const Agentite_StrategyCoordinator *coord) {
    if (!coord || coord->option_count == 0) return NULL;

    int best_idx = -1;
    float best_utility = -1.0f;

    for (int i = 0; i < coord->option_count; i++) {
        if (coord->options[i].active &&
            coord->options[i].current_utility > best_utility) {
            best_utility = coord->options[i].current_utility;
            best_idx = i;
        }
    }

    return (best_idx >= 0) ? coord->options[best_idx].name : NULL;
}

int agentite_strategy_get_options_by_utility(const Agentite_StrategyCoordinator *coord,
                                           const char **out_names,
                                           float *out_utilities,
                                           int max) {
    if (!coord || !out_names || max <= 0) return 0;

    /* Build sorted list (simple insertion sort, options count is small) */
    int indices[AGENTITE_STRATEGY_MAX_OPTIONS];
    int count = 0;

    for (int i = 0; i < coord->option_count && count < max; i++) {
        if (!coord->options[i].active) continue;

        /* Find insertion position */
        int pos = count;
        for (int j = 0; j < count; j++) {
            if (coord->options[i].current_utility >
                coord->options[indices[j]].current_utility) {
                pos = j;
                break;
            }
        }

        /* Shift to make room */
        for (int j = count; j > pos; j--) {
            indices[j] = indices[j - 1];
        }
        indices[pos] = i;
        count++;
    }

    /* Copy to output */
    for (int i = 0; i < count; i++) {
        out_names[i] = coord->options[indices[i]].name;
        if (out_utilities) {
            out_utilities[i] = coord->options[indices[i]].current_utility;
        }
    }

    return count;
}

/* -------------------------------------------------------------------------- */
/*                           Budget Allocation                                 */
/* -------------------------------------------------------------------------- */

int agentite_strategy_allocate_budget(Agentite_StrategyCoordinator *coord,
                                    int32_t total_budget,
                                    Agentite_BudgetAllocation *out_allocations,
                                    int max_allocations) {
    if (!coord || !out_allocations || max_allocations <= 0) return 0;
    if (total_budget <= 0) return 0;

    /* Calculate total weighted utility */
    float total_utility = 0.0f;
    int active_count = 0;

    for (int i = 0; i < coord->option_count; i++) {
        if (coord->options[i].active) {
            total_utility += coord->options[i].current_utility;
            active_count++;
        }
    }

    if (total_utility <= 0.0f || active_count == 0) {
        return 0;
    }

    /* First pass: calculate proportions based on utility */
    float proportions[AGENTITE_STRATEGY_MAX_OPTIONS];
    int count = 0;

    for (int i = 0; i < coord->option_count && count < max_allocations; i++) {
        if (!coord->options[i].active) continue;

        float prop = coord->options[i].current_utility / total_utility;

        /* Apply min/max constraints */
        prop = fmaxf(prop, coord->min_allocations[i]);
        prop = fminf(prop, coord->max_allocations[i]);

        proportions[count] = prop;
        count++;
    }

    /* Second pass: normalize proportions to sum to 1.0 */
    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        sum += proportions[i];
    }

    if (sum > 0.0f) {
        for (int i = 0; i < count; i++) {
            proportions[i] /= sum;
        }
    }

    /* Generate allocations */
    int alloc_count = 0;
    int alloc_idx = 0;

    for (int i = 0; i < coord->option_count && alloc_idx < count; i++) {
        if (!coord->options[i].active) continue;

        Agentite_BudgetAllocation *alloc = &out_allocations[alloc_count++];
        strncpy(alloc->option_name, coord->options[i].name,
                AGENTITE_STRATEGY_MAX_NAME_LEN - 1);
        alloc->option_name[AGENTITE_STRATEGY_MAX_NAME_LEN - 1] = '\0';
        alloc->proportion = proportions[alloc_idx];
        alloc->allocated = (int32_t)(total_budget * proportions[alloc_idx]);

        alloc_idx++;
    }

    return alloc_count;
}

void agentite_strategy_set_min_allocation(Agentite_StrategyCoordinator *coord,
                                        const char *option_name,
                                        float min_proportion) {
    if (!coord || !option_name) return;

    int idx = agentite_strategy_find_option(coord, option_name);
    if (idx >= 0) {
        coord->min_allocations[idx] = fmaxf(0.0f, fminf(1.0f, min_proportion));
    }
}

void agentite_strategy_set_max_allocation(Agentite_StrategyCoordinator *coord,
                                        const char *option_name,
                                        float max_proportion) {
    if (!coord || !option_name) return;

    int idx = agentite_strategy_find_option(coord, option_name);
    if (idx >= 0) {
        coord->max_allocations[idx] = fmaxf(0.0f, fminf(1.0f, max_proportion));
    }
}

int32_t agentite_strategy_get_allocation(const Agentite_StrategyCoordinator *coord,
                                       const char *option_name,
                                       int32_t total_budget) {
    if (!coord || !option_name || total_budget <= 0) return 0;

    /* Find the option */
    int idx = agentite_strategy_find_option(coord, option_name);
    if (idx < 0 || !coord->options[idx].active) return 0;

    /* Calculate total utility of active options */
    float total_utility = 0.0f;
    for (int i = 0; i < coord->option_count; i++) {
        if (coord->options[i].active) {
            total_utility += coord->options[i].current_utility;
        }
    }

    if (total_utility <= 0.0f) return 0;

    /* Calculate this option's proportion */
    float prop = coord->options[idx].current_utility / total_utility;

    /* Apply constraints */
    prop = fmaxf(prop, coord->min_allocations[idx]);
    prop = fminf(prop, coord->max_allocations[idx]);

    return (int32_t)(total_budget * prop);
}

/* -------------------------------------------------------------------------- */
/*                          Utility Curve Helpers                              */
/* -------------------------------------------------------------------------- */

Agentite_UtilityCurve agentite_curve_linear(float min_output, float max_output) {
    Agentite_UtilityCurve curve = {};
    curve.type = AGENTITE_CURVE_LINEAR;
    curve.min_output = min_output;
    curve.max_output = max_output;
    return curve;
}

Agentite_UtilityCurve agentite_curve_quadratic(float min_output, float max_output) {
    Agentite_UtilityCurve curve = {};
    curve.type = AGENTITE_CURVE_QUADRATIC;
    curve.min_output = min_output;
    curve.max_output = max_output;
    return curve;
}

Agentite_UtilityCurve agentite_curve_sqrt(float min_output, float max_output) {
    Agentite_UtilityCurve curve = {};
    curve.type = AGENTITE_CURVE_SQRT;
    curve.min_output = min_output;
    curve.max_output = max_output;
    return curve;
}

Agentite_UtilityCurve agentite_curve_sigmoid(float steepness, float midpoint) {
    Agentite_UtilityCurve curve = {};
    curve.type = AGENTITE_CURVE_SIGMOID;
    curve.param_a = steepness;
    curve.param_b = midpoint;
    curve.min_output = 0.0f;
    curve.max_output = 1.0f;
    return curve;
}

Agentite_UtilityCurve agentite_curve_inverse(float min_output, float max_output) {
    Agentite_UtilityCurve curve = {};
    curve.type = AGENTITE_CURVE_INVERSE;
    curve.min_output = min_output;
    curve.max_output = max_output;
    return curve;
}

Agentite_UtilityCurve agentite_curve_step(float threshold, float low_value, float high_value) {
    Agentite_UtilityCurve curve = {};
    curve.type = AGENTITE_CURVE_STEP;
    curve.param_a = threshold;
    curve.min_output = low_value;
    curve.max_output = high_value;
    return curve;
}

Agentite_UtilityCurve agentite_curve_exponential(float rate, float min_output, float max_output) {
    Agentite_UtilityCurve curve = {};
    curve.type = AGENTITE_CURVE_EXPONENTIAL;
    curve.param_a = rate;
    curve.min_output = min_output;
    curve.max_output = max_output;
    return curve;
}

Agentite_UtilityCurve agentite_curve_logarithmic(float scale, float min_output, float max_output) {
    Agentite_UtilityCurve curve = {};
    curve.type = AGENTITE_CURVE_LOGARITHMIC;
    curve.param_a = scale;
    curve.min_output = min_output;
    curve.max_output = max_output;
    return curve;
}

Agentite_UtilityCurve agentite_curve_custom(float (*fn)(float input, void *userdata),
                                        void *userdata) {
    Agentite_UtilityCurve curve = {};
    curve.type = AGENTITE_CURVE_CUSTOM;
    curve.custom_fn = fn;
    curve.custom_userdata = userdata;
    curve.min_output = 0.0f;
    curve.max_output = 1.0f;
    return curve;
}

float agentite_curve_evaluate(const Agentite_UtilityCurve *curve, float input) {
    if (!curve) return 0.0f;

    /* Clamp input to 0-1 */
    input = fmaxf(0.0f, fminf(1.0f, input));

    float t = 0.0f;  /* Normalized result 0-1 */

    switch (curve->type) {
        case AGENTITE_CURVE_LINEAR:
            t = input;
            break;

        case AGENTITE_CURVE_QUADRATIC:
            t = input * input;
            break;

        case AGENTITE_CURVE_SQRT:
            t = sqrtf(input);
            break;

        case AGENTITE_CURVE_SIGMOID: {
            float steepness = curve->param_a > 0.0f ? curve->param_a : 10.0f;
            float midpoint = curve->param_b > 0.0f ? curve->param_b : 0.5f;
            t = 1.0f / (1.0f + expf(-steepness * (input - midpoint)));
            break;
        }

        case AGENTITE_CURVE_INVERSE:
            t = 1.0f - input;
            break;

        case AGENTITE_CURVE_STEP:
            t = (input >= curve->param_a) ? 1.0f : 0.0f;
            break;

        case AGENTITE_CURVE_EXPONENTIAL: {
            float rate = curve->param_a > 0.0f ? curve->param_a : 2.0f;
            /* Normalize so that e^(rate*1) - 1 maps to 1 */
            float max_exp = expf(rate) - 1.0f;
            if (max_exp > 0.0f) {
                t = (expf(rate * input) - 1.0f) / max_exp;
            }
            break;
        }

        case AGENTITE_CURVE_LOGARITHMIC: {
            float scale = curve->param_a > 0.0f ? curve->param_a : 10.0f;
            /* Normalize so that log(1 + scale*1) maps to 1 */
            float max_log = logf(1.0f + scale);
            if (max_log > 0.0f) {
                t = logf(1.0f + scale * input) / max_log;
            }
            break;
        }

        case AGENTITE_CURVE_CUSTOM:
            if (curve->custom_fn) {
                return curve->custom_fn(input, curve->custom_userdata);
            }
            t = input;
            break;

        default:
            t = input;
            break;
    }

    /* Map to output range */
    return curve->min_output + t * (curve->max_output - curve->min_output);
}

/* -------------------------------------------------------------------------- */
/*                              Statistics                                     */
/* -------------------------------------------------------------------------- */

void agentite_strategy_get_stats(const Agentite_StrategyCoordinator *coord,
                               Agentite_StrategyStats *out_stats) {
    if (!coord || !out_stats) return;

    memset(out_stats, 0, sizeof(Agentite_StrategyStats));

    out_stats->evaluations = coord->evaluations;
    out_stats->phase_changes = coord->phase_changes;
    out_stats->last_phase = coord->current_phase;

    /* Calculate total and highest utility */
    float total = 0.0f;
    float highest = -1.0f;
    const char *highest_name = NULL;

    for (int i = 0; i < coord->option_count; i++) {
        if (!coord->options[i].active) continue;

        total += coord->options[i].current_utility;
        if (coord->options[i].current_utility > highest) {
            highest = coord->options[i].current_utility;
            highest_name = coord->options[i].name;
        }
    }

    out_stats->total_utility = total;
    out_stats->highest_utility = highest;
    out_stats->highest_option = highest_name;
}

void agentite_strategy_reset_stats(Agentite_StrategyCoordinator *coord) {
    if (!coord) return;

    coord->evaluations = 0;
    coord->phase_changes = 0;
}
