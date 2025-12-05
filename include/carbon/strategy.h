/**
 * @file strategy.h
 * @brief Strategic Coordinator - Game phase detection and utility-based decision making
 *
 * Provides phase detection (early/mid/late game) and utility curve evaluation
 * for budget allocation and strategic decision making.
 */

#ifndef CARBON_STRATEGY_H
#define CARBON_STRATEGY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*                               Constants                                     */
/* -------------------------------------------------------------------------- */

#define CARBON_STRATEGY_MAX_OPTIONS     32
#define CARBON_STRATEGY_MAX_NAME_LEN    32
#define CARBON_STRATEGY_MAX_PHASES      8

/* -------------------------------------------------------------------------- */
/*                                  Types                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief Game phase enumeration
 */
typedef enum Carbon_GamePhase {
    CARBON_PHASE_EARLY_EXPANSION = 0,   /**< Early game - expansion focus */
    CARBON_PHASE_MID_CONSOLIDATION,     /**< Mid game - consolidation */
    CARBON_PHASE_LATE_COMPETITION,      /**< Late game - competition */
    CARBON_PHASE_ENDGAME,               /**< End game - final push */
    CARBON_PHASE_COUNT
} Carbon_GamePhase;

/**
 * @brief Utility curve types for option evaluation
 */
typedef enum Carbon_UtilityCurveType {
    CARBON_CURVE_LINEAR = 0,        /**< Linear: y = x */
    CARBON_CURVE_QUADRATIC,         /**< Quadratic: y = x^2 */
    CARBON_CURVE_SQRT,              /**< Square root: y = sqrt(x) */
    CARBON_CURVE_SIGMOID,           /**< S-curve: smooth transition */
    CARBON_CURVE_INVERSE,           /**< Inverse: y = 1 - x */
    CARBON_CURVE_STEP,              /**< Step function at threshold */
    CARBON_CURVE_EXPONENTIAL,       /**< Exponential: y = e^(ax) - 1 */
    CARBON_CURVE_LOGARITHMIC,       /**< Logarithmic: y = log(1 + ax) */
    CARBON_CURVE_CUSTOM             /**< Custom curve via callback */
} Carbon_UtilityCurveType;

/**
 * @brief Utility curve definition
 */
typedef struct Carbon_UtilityCurve {
    Carbon_UtilityCurveType type;
    float param_a;              /**< Curve parameter A (meaning varies by type) */
    float param_b;              /**< Curve parameter B (meaning varies by type) */
    float min_output;           /**< Minimum output value */
    float max_output;           /**< Maximum output value */
    float (*custom_fn)(float input, void *userdata);  /**< Custom curve function */
    void *custom_userdata;      /**< Userdata for custom curve */
} Carbon_UtilityCurve;

/**
 * @brief Strategic option definition
 */
typedef struct Carbon_StrategyOption {
    char name[CARBON_STRATEGY_MAX_NAME_LEN];
    Carbon_UtilityCurve curve;
    float base_weight;          /**< Base weight before modifiers */
    float current_input;        /**< Current input value (0-1) */
    float current_utility;      /**< Computed utility (cached) */
    float phase_modifiers[CARBON_PHASE_COUNT];  /**< Per-phase multipliers */
    bool active;                /**< Whether option is active */
} Carbon_StrategyOption;

/**
 * @brief Budget allocation result
 */
typedef struct Carbon_BudgetAllocation {
    char option_name[CARBON_STRATEGY_MAX_NAME_LEN];
    int32_t allocated;          /**< Amount allocated */
    float proportion;           /**< Proportion of total (0-1) */
} Carbon_BudgetAllocation;

/**
 * @brief Phase analysis result
 */
typedef struct Carbon_PhaseAnalysis {
    Carbon_GamePhase phase;
    float confidence;           /**< Confidence in phase detection (0-1) */
    float progress;             /**< Progress through current phase (0-1) */
    float metrics[8];           /**< Game metrics used for analysis */
    int metric_count;
} Carbon_PhaseAnalysis;

/**
 * @brief Callback for phase analysis
 *
 * @param game_state Game state pointer
 * @param out_metrics Output array for metrics (0-1 normalized)
 * @param max_metrics Maximum metrics to output
 * @param userdata User data pointer
 * @return Number of metrics output
 */
typedef int (*Carbon_PhaseAnalyzer)(void *game_state, float *out_metrics,
                                    int max_metrics, void *userdata);

/**
 * @brief Callback for input value calculation
 *
 * @param game_state Game state pointer
 * @param option_name Name of the option
 * @param userdata User data pointer
 * @return Input value (0-1)
 */
typedef float (*Carbon_InputProvider)(void *game_state, const char *option_name,
                                      void *userdata);

/* Forward declaration */
typedef struct Carbon_StrategyCoordinator Carbon_StrategyCoordinator;

/* -------------------------------------------------------------------------- */
/*                            Lifecycle Functions                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create a new strategy coordinator
 * @return New coordinator or NULL on failure
 */
Carbon_StrategyCoordinator *carbon_strategy_create(void);

/**
 * @brief Destroy a strategy coordinator
 * @param coord Coordinator to destroy
 */
void carbon_strategy_destroy(Carbon_StrategyCoordinator *coord);

/**
 * @brief Reset coordinator to default state
 * @param coord Coordinator to reset
 */
void carbon_strategy_reset(Carbon_StrategyCoordinator *coord);

/* -------------------------------------------------------------------------- */
/*                          Phase Detection                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set phase transition thresholds
 *
 * Thresholds define when phase transitions occur based on the analysis
 * metric average (0-1 range).
 *
 * @param coord Coordinator
 * @param early_to_mid Threshold for early -> mid transition
 * @param mid_to_late Threshold for mid -> late transition
 * @param late_to_end Threshold for late -> endgame transition
 */
void carbon_strategy_set_phase_thresholds(Carbon_StrategyCoordinator *coord,
                                          float early_to_mid,
                                          float mid_to_late,
                                          float late_to_end);

/**
 * @brief Set phase analyzer callback
 *
 * The analyzer should return normalized metrics (0-1) that indicate
 * game progression. Higher values = later game.
 *
 * @param coord Coordinator
 * @param analyzer Analyzer callback
 * @param userdata User data for callback
 */
void carbon_strategy_set_phase_analyzer(Carbon_StrategyCoordinator *coord,
                                        Carbon_PhaseAnalyzer analyzer,
                                        void *userdata);

/**
 * @brief Detect current game phase
 *
 * Uses the configured analyzer to determine current phase.
 *
 * @param coord Coordinator
 * @param game_state Game state pointer
 * @return Current game phase
 */
Carbon_GamePhase carbon_strategy_detect_phase(Carbon_StrategyCoordinator *coord,
                                              void *game_state);

/**
 * @brief Get detailed phase analysis
 *
 * @param coord Coordinator
 * @param game_state Game state pointer
 * @param out_analysis Output for analysis results
 * @return true if analysis succeeded
 */
bool carbon_strategy_analyze_phase(Carbon_StrategyCoordinator *coord,
                                   void *game_state,
                                   Carbon_PhaseAnalysis *out_analysis);

/**
 * @brief Get current phase (cached from last detect call)
 * @param coord Coordinator
 * @return Current phase
 */
Carbon_GamePhase carbon_strategy_get_current_phase(const Carbon_StrategyCoordinator *coord);

/**
 * @brief Get phase name as string
 * @param phase Phase to get name for
 * @return Phase name string
 */
const char *carbon_strategy_phase_name(Carbon_GamePhase phase);

/**
 * @brief Manually set phase (override detection)
 * @param coord Coordinator
 * @param phase Phase to set
 */
void carbon_strategy_set_phase(Carbon_StrategyCoordinator *coord, Carbon_GamePhase phase);

/* -------------------------------------------------------------------------- */
/*                          Option Management                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Add a strategic option
 *
 * @param coord Coordinator
 * @param name Option name (unique identifier)
 * @param curve Utility curve for evaluation
 * @param base_weight Base weight for budget allocation
 * @return Option index or -1 on failure
 */
int carbon_strategy_add_option(Carbon_StrategyCoordinator *coord,
                               const char *name,
                               const Carbon_UtilityCurve *curve,
                               float base_weight);

/**
 * @brief Remove an option
 * @param coord Coordinator
 * @param name Option name
 * @return true if removed
 */
bool carbon_strategy_remove_option(Carbon_StrategyCoordinator *coord, const char *name);

/**
 * @brief Find option index by name
 * @param coord Coordinator
 * @param name Option name
 * @return Option index or -1 if not found
 */
int carbon_strategy_find_option(const Carbon_StrategyCoordinator *coord, const char *name);

/**
 * @brief Get option count
 * @param coord Coordinator
 * @return Number of active options
 */
int carbon_strategy_get_option_count(const Carbon_StrategyCoordinator *coord);

/**
 * @brief Get option by index
 * @param coord Coordinator
 * @param index Option index
 * @return Option pointer or NULL
 */
const Carbon_StrategyOption *carbon_strategy_get_option(const Carbon_StrategyCoordinator *coord,
                                                        int index);

/**
 * @brief Set option's base weight
 * @param coord Coordinator
 * @param name Option name
 * @param weight New base weight
 */
void carbon_strategy_set_option_weight(Carbon_StrategyCoordinator *coord,
                                       const char *name, float weight);

/**
 * @brief Enable/disable an option
 * @param coord Coordinator
 * @param name Option name
 * @param active Whether option is active
 */
void carbon_strategy_set_option_active(Carbon_StrategyCoordinator *coord,
                                       const char *name, bool active);

/* -------------------------------------------------------------------------- */
/*                          Phase Modifiers                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set phase modifier for an option
 *
 * Modifiers multiply the option's utility during specific phases.
 * Default is 1.0 (no modification).
 *
 * @param coord Coordinator
 * @param option_name Option name
 * @param phase Game phase
 * @param modifier Modifier value (1.0 = no change)
 */
void carbon_strategy_set_phase_modifier(Carbon_StrategyCoordinator *coord,
                                        const char *option_name,
                                        Carbon_GamePhase phase,
                                        float modifier);

/**
 * @brief Get phase modifier for an option
 * @param coord Coordinator
 * @param option_name Option name
 * @param phase Game phase
 * @return Modifier value
 */
float carbon_strategy_get_phase_modifier(const Carbon_StrategyCoordinator *coord,
                                         const char *option_name,
                                         Carbon_GamePhase phase);

/**
 * @brief Set all phase modifiers for an option at once
 * @param coord Coordinator
 * @param option_name Option name
 * @param modifiers Array of CARBON_PHASE_COUNT modifiers
 */
void carbon_strategy_set_all_phase_modifiers(Carbon_StrategyCoordinator *coord,
                                             const char *option_name,
                                             const float *modifiers);

/* -------------------------------------------------------------------------- */
/*                          Utility Evaluation                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set input provider callback
 *
 * The input provider calculates the input value (0-1) for each option
 * based on current game state.
 *
 * @param coord Coordinator
 * @param provider Provider callback
 * @param userdata User data for callback
 */
void carbon_strategy_set_input_provider(Carbon_StrategyCoordinator *coord,
                                        Carbon_InputProvider provider,
                                        void *userdata);

/**
 * @brief Set input value for an option manually
 * @param coord Coordinator
 * @param option_name Option name
 * @param input Input value (0-1)
 */
void carbon_strategy_set_input(Carbon_StrategyCoordinator *coord,
                               const char *option_name, float input);

/**
 * @brief Evaluate all options
 *
 * Updates utility values based on current inputs and phase.
 * If input provider is set, calls it for each option.
 *
 * @param coord Coordinator
 * @param game_state Game state pointer (passed to input provider)
 */
void carbon_strategy_evaluate_options(Carbon_StrategyCoordinator *coord,
                                      void *game_state);

/**
 * @brief Get utility value for an option
 *
 * Returns the most recently computed utility. Call evaluate_options first.
 *
 * @param coord Coordinator
 * @param option_name Option name
 * @return Utility value (0-1), or -1 if not found
 */
float carbon_strategy_get_utility(const Carbon_StrategyCoordinator *coord,
                                  const char *option_name);

/**
 * @brief Get highest utility option name
 * @param coord Coordinator
 * @return Name of highest utility option, or NULL if none
 */
const char *carbon_strategy_get_best_option(const Carbon_StrategyCoordinator *coord);

/**
 * @brief Get options sorted by utility (descending)
 *
 * @param coord Coordinator
 * @param out_names Output array for option names
 * @param out_utilities Output array for utilities (optional, can be NULL)
 * @param max Maximum options to output
 * @return Number of options output
 */
int carbon_strategy_get_options_by_utility(const Carbon_StrategyCoordinator *coord,
                                           const char **out_names,
                                           float *out_utilities,
                                           int max);

/* -------------------------------------------------------------------------- */
/*                          Budget Allocation                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Allocate budget proportionally based on utilities
 *
 * Distributes total_budget among options based on their utility values.
 * Options with higher utility get larger allocations.
 *
 * @param coord Coordinator
 * @param total_budget Total budget to allocate
 * @param out_allocations Output array for allocations
 * @param max_allocations Maximum allocations to output
 * @return Number of allocations made
 */
int carbon_strategy_allocate_budget(Carbon_StrategyCoordinator *coord,
                                    int32_t total_budget,
                                    Carbon_BudgetAllocation *out_allocations,
                                    int max_allocations);

/**
 * @brief Set minimum allocation for an option
 *
 * Ensures option gets at least this proportion of budget.
 *
 * @param coord Coordinator
 * @param option_name Option name
 * @param min_proportion Minimum proportion (0-1)
 */
void carbon_strategy_set_min_allocation(Carbon_StrategyCoordinator *coord,
                                        const char *option_name,
                                        float min_proportion);

/**
 * @brief Set maximum allocation for an option
 *
 * Caps option's budget allocation at this proportion.
 *
 * @param coord Coordinator
 * @param option_name Option name
 * @param max_proportion Maximum proportion (0-1)
 */
void carbon_strategy_set_max_allocation(Carbon_StrategyCoordinator *coord,
                                        const char *option_name,
                                        float max_proportion);

/**
 * @brief Get allocation for a specific option
 *
 * Returns the allocation that would be made for this option given
 * the specified total budget.
 *
 * @param coord Coordinator
 * @param option_name Option name
 * @param total_budget Total budget
 * @return Allocated amount
 */
int32_t carbon_strategy_get_allocation(const Carbon_StrategyCoordinator *coord,
                                       const char *option_name,
                                       int32_t total_budget);

/* -------------------------------------------------------------------------- */
/*                          Utility Curve Helpers                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create a linear utility curve
 * @param min_output Minimum output value
 * @param max_output Maximum output value
 * @return Curve definition
 */
Carbon_UtilityCurve carbon_curve_linear(float min_output, float max_output);

/**
 * @brief Create a quadratic utility curve (x^2)
 * @param min_output Minimum output value
 * @param max_output Maximum output value
 * @return Curve definition
 */
Carbon_UtilityCurve carbon_curve_quadratic(float min_output, float max_output);

/**
 * @brief Create a square root utility curve (sqrt(x))
 * @param min_output Minimum output value
 * @param max_output Maximum output value
 * @return Curve definition
 */
Carbon_UtilityCurve carbon_curve_sqrt(float min_output, float max_output);

/**
 * @brief Create a sigmoid (S-curve) utility curve
 * @param steepness How steep the transition is (default: 10)
 * @param midpoint Input value at midpoint of transition (default: 0.5)
 * @return Curve definition
 */
Carbon_UtilityCurve carbon_curve_sigmoid(float steepness, float midpoint);

/**
 * @brief Create an inverse utility curve (1 - x)
 * @param min_output Minimum output value
 * @param max_output Maximum output value
 * @return Curve definition
 */
Carbon_UtilityCurve carbon_curve_inverse(float min_output, float max_output);

/**
 * @brief Create a step function curve
 * @param threshold Input threshold for step
 * @param low_value Output below threshold
 * @param high_value Output at/above threshold
 * @return Curve definition
 */
Carbon_UtilityCurve carbon_curve_step(float threshold, float low_value, float high_value);

/**
 * @brief Create an exponential curve
 * @param rate Exponential rate (higher = steeper)
 * @param min_output Minimum output value
 * @param max_output Maximum output value
 * @return Curve definition
 */
Carbon_UtilityCurve carbon_curve_exponential(float rate, float min_output, float max_output);

/**
 * @brief Create a logarithmic curve
 * @param scale Logarithm scale factor
 * @param min_output Minimum output value
 * @param max_output Maximum output value
 * @return Curve definition
 */
Carbon_UtilityCurve carbon_curve_logarithmic(float scale, float min_output, float max_output);

/**
 * @brief Create a custom curve with callback
 * @param fn Custom curve function
 * @param userdata User data for callback
 * @return Curve definition
 */
Carbon_UtilityCurve carbon_curve_custom(float (*fn)(float input, void *userdata),
                                        void *userdata);

/**
 * @brief Evaluate a utility curve
 * @param curve Curve to evaluate
 * @param input Input value (0-1)
 * @return Output value
 */
float carbon_curve_evaluate(const Carbon_UtilityCurve *curve, float input);

/* -------------------------------------------------------------------------- */
/*                          Statistics                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Strategy coordinator statistics
 */
typedef struct Carbon_StrategyStats {
    int evaluations;            /**< Total evaluate_options calls */
    int phase_changes;          /**< Number of phase transitions */
    Carbon_GamePhase last_phase;/**< Last detected phase */
    float total_utility;        /**< Sum of all utilities */
    float highest_utility;      /**< Highest single utility */
    const char *highest_option; /**< Option with highest utility */
} Carbon_StrategyStats;

/**
 * @brief Get coordinator statistics
 * @param coord Coordinator
 * @param out_stats Output for statistics
 */
void carbon_strategy_get_stats(const Carbon_StrategyCoordinator *coord,
                               Carbon_StrategyStats *out_stats);

/**
 * @brief Reset statistics
 * @param coord Coordinator
 */
void carbon_strategy_reset_stats(Carbon_StrategyCoordinator *coord);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_STRATEGY_H */
