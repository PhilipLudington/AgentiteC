/*
 * Agentite Turn Manager Tests
 *
 * Tests for the turn-based phase management system.
 */

#include "catch_amalgamated.hpp"
#include "agentite/turn.h"
#include <cstring>

/* ============================================================================
 * Callback Test Helpers
 * ============================================================================ */

static int g_callback_count = 0;
static int g_last_turn = -1;
static Agentite_TurnPhase g_phases_called[AGENTITE_PHASE_COUNT];

static void reset_callback_state() {
    g_callback_count = 0;
    g_last_turn = -1;
    memset(g_phases_called, 0, sizeof(g_phases_called));
}

static void test_callback(void *userdata, int turn_number) {
    (void)userdata;
    if (g_callback_count < AGENTITE_PHASE_COUNT) {
        g_phases_called[g_callback_count] = (Agentite_TurnPhase)g_callback_count;
    }
    g_callback_count++;
    g_last_turn = turn_number;
}

static void counting_callback(void *userdata, int turn_number) {
    (void)turn_number;
    int *counter = (int *)userdata;
    (*counter)++;
}

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

TEST_CASE("Turn manager initialization", "[turn][init]") {
    SECTION("Basic initialization") {
        Agentite_TurnManager tm;
        agentite_turn_init(&tm);

        // Turn-based games start at turn 1, not turn 0
        REQUIRE(tm.turn_number == 1);
        REQUIRE(tm.current_phase == AGENTITE_PHASE_WORLD_UPDATE);
        REQUIRE(tm.turn_in_progress == false);
    }

    SECTION("Initialize with NULL is safe") {
        agentite_turn_init(nullptr);
        // Should not crash
    }

    SECTION("All callbacks are NULL after init") {
        Agentite_TurnManager tm;
        agentite_turn_init(&tm);

        for (int i = 0; i < AGENTITE_PHASE_COUNT; i++) {
            REQUIRE(tm.phase_callbacks[i] == nullptr);
            REQUIRE(tm.phase_userdata[i] == nullptr);
        }
    }
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

TEST_CASE("Turn manager callbacks", "[turn][callback]") {
    Agentite_TurnManager tm;
    agentite_turn_init(&tm);

    SECTION("Set callback for a phase") {
        int counter = 0;
        agentite_turn_set_callback(&tm, AGENTITE_PHASE_PLAYER_INPUT,
                                   counting_callback, &counter);

        REQUIRE(tm.phase_callbacks[AGENTITE_PHASE_PLAYER_INPUT] == counting_callback);
        REQUIRE(tm.phase_userdata[AGENTITE_PHASE_PLAYER_INPUT] == &counter);
    }

    SECTION("Set callback with NULL turn manager is safe") {
        agentite_turn_set_callback(nullptr, AGENTITE_PHASE_PLAYER_INPUT,
                                   counting_callback, nullptr);
        // Should not crash
    }

    SECTION("Set NULL callback to clear") {
        int counter = 0;
        agentite_turn_set_callback(&tm, AGENTITE_PHASE_PLAYER_INPUT,
                                   counting_callback, &counter);
        agentite_turn_set_callback(&tm, AGENTITE_PHASE_PLAYER_INPUT,
                                   nullptr, nullptr);

        REQUIRE(tm.phase_callbacks[AGENTITE_PHASE_PLAYER_INPUT] == nullptr);
    }

    SECTION("Callback receives turn number") {
        reset_callback_state();
        agentite_turn_set_callback(&tm, AGENTITE_PHASE_WORLD_UPDATE,
                                   test_callback, nullptr);

        agentite_turn_advance(&tm);

        REQUIRE(g_last_turn == 1);  // Turn 1 initially
    }

    SECTION("Callback receives userdata") {
        int counter = 0;
        agentite_turn_set_callback(&tm, AGENTITE_PHASE_WORLD_UPDATE,
                                   counting_callback, &counter);

        agentite_turn_advance(&tm);

        REQUIRE(counter == 1);
    }
}

/* ============================================================================
 * Phase Advance Tests
 * ============================================================================ */

TEST_CASE("Turn advance through phases", "[turn][advance]") {
    Agentite_TurnManager tm;
    agentite_turn_init(&tm);

    SECTION("Advance through all phases completes turn") {
        REQUIRE(tm.current_phase == AGENTITE_PHASE_WORLD_UPDATE);

        // Advance through all phases
        for (int i = 0; i < AGENTITE_PHASE_COUNT - 1; i++) {
            bool turn_complete = agentite_turn_advance(&tm);
            REQUIRE_FALSE(turn_complete);
        }

        // Last advance should complete the turn
        bool turn_complete = agentite_turn_advance(&tm);
        REQUIRE(turn_complete);
        REQUIRE(tm.turn_number == 2);  // Started at 1, now at 2
        REQUIRE(tm.current_phase == AGENTITE_PHASE_WORLD_UPDATE);
    }

    SECTION("Phase order is correct") {
        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_WORLD_UPDATE);

        agentite_turn_advance(&tm);
        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_EVENTS);

        agentite_turn_advance(&tm);
        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_PLAYER_INPUT);

        agentite_turn_advance(&tm);
        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_RESOLUTION);

        agentite_turn_advance(&tm);
        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_END_CHECK);

        agentite_turn_advance(&tm);
        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_WORLD_UPDATE);
    }

    SECTION("Multiple complete turns") {
        // Complete 5 turns (starting from turn 1)
        for (int turn = 0; turn < 5; turn++) {
            for (int phase = 0; phase < AGENTITE_PHASE_COUNT; phase++) {
                agentite_turn_advance(&tm);
            }
        }

        REQUIRE(tm.turn_number == 6);  // Started at 1, completed 5 turns = 6
    }

    SECTION("Callbacks called in order during advance") {
        reset_callback_state();

        for (int i = 0; i < AGENTITE_PHASE_COUNT; i++) {
            agentite_turn_set_callback(&tm, (Agentite_TurnPhase)i, test_callback, nullptr);
        }

        // Advance through all phases
        for (int i = 0; i < AGENTITE_PHASE_COUNT; i++) {
            agentite_turn_advance(&tm);
        }

        REQUIRE(g_callback_count == AGENTITE_PHASE_COUNT);
    }
}

/* ============================================================================
 * Skip To Phase Tests
 * ============================================================================ */

TEST_CASE("Turn skip to phase", "[turn][skip]") {
    Agentite_TurnManager tm;
    agentite_turn_init(&tm);

    SECTION("Skip to specific phase") {
        agentite_turn_skip_to(&tm, AGENTITE_PHASE_RESOLUTION);
        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_RESOLUTION);
    }

    SECTION("Skip to same phase is no-op") {
        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_WORLD_UPDATE);
        agentite_turn_skip_to(&tm, AGENTITE_PHASE_WORLD_UPDATE);
        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_WORLD_UPDATE);
    }

    SECTION("Skip does not increment turn number") {
        REQUIRE(tm.turn_number == 1);  // Starts at 1
        agentite_turn_skip_to(&tm, AGENTITE_PHASE_END_CHECK);
        REQUIRE(tm.turn_number == 1);  // Still 1
    }

    SECTION("Skip to NULL turn manager is safe") {
        agentite_turn_skip_to(nullptr, AGENTITE_PHASE_PLAYER_INPUT);
        // Should not crash
    }

    SECTION("Continue advancing after skip") {
        agentite_turn_skip_to(&tm, AGENTITE_PHASE_RESOLUTION);
        agentite_turn_advance(&tm);
        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_END_CHECK);
    }
}

/* ============================================================================
 * Query Function Tests
 * ============================================================================ */

TEST_CASE("Turn query functions", "[turn][query]") {
    Agentite_TurnManager tm;
    agentite_turn_init(&tm);

    SECTION("Get current phase") {
        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_WORLD_UPDATE);

        agentite_turn_advance(&tm);
        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_EVENTS);
    }

    SECTION("Get turn number") {
        REQUIRE(agentite_turn_number(&tm) == 1);  // Starts at 1

        // Complete one turn
        for (int i = 0; i < AGENTITE_PHASE_COUNT; i++) {
            agentite_turn_advance(&tm);
        }

        REQUIRE(agentite_turn_number(&tm) == 2);  // Now turn 2
    }

    SECTION("Query NULL turn manager") {
        Agentite_TurnPhase phase = agentite_turn_current_phase(nullptr);
        int turn = agentite_turn_number(nullptr);

        // Should return reasonable defaults without crashing
        REQUIRE(phase == AGENTITE_PHASE_WORLD_UPDATE);
        REQUIRE(turn == 0);
    }
}

/* ============================================================================
 * Phase Name Tests
 * ============================================================================ */

TEST_CASE("Turn phase names", "[turn][names]") {
    SECTION("All phases have names") {
        for (int i = 0; i < AGENTITE_PHASE_COUNT; i++) {
            const char *name = agentite_turn_phase_name((Agentite_TurnPhase)i);
            REQUIRE(name != nullptr);
            REQUIRE(strlen(name) > 0);
        }
    }

    SECTION("Phase name for WORLD_UPDATE") {
        const char *name = agentite_turn_phase_name(AGENTITE_PHASE_WORLD_UPDATE);
        REQUIRE(name != nullptr);
        // Should contain relevant text
    }

    SECTION("Phase name for EVENTS") {
        const char *name = agentite_turn_phase_name(AGENTITE_PHASE_EVENTS);
        REQUIRE(name != nullptr);
    }

    SECTION("Phase name for PLAYER_INPUT") {
        const char *name = agentite_turn_phase_name(AGENTITE_PHASE_PLAYER_INPUT);
        REQUIRE(name != nullptr);
    }

    SECTION("Phase name for RESOLUTION") {
        const char *name = agentite_turn_phase_name(AGENTITE_PHASE_RESOLUTION);
        REQUIRE(name != nullptr);
    }

    SECTION("Phase name for END_CHECK") {
        const char *name = agentite_turn_phase_name(AGENTITE_PHASE_END_CHECK);
        REQUIRE(name != nullptr);
    }

    SECTION("Invalid phase returns fallback") {
        const char *name = agentite_turn_phase_name((Agentite_TurnPhase)999);
        // Should return something, even if it's "Unknown"
        REQUIRE(name != nullptr);
    }
}

/* ============================================================================
 * Phase Enum Tests
 * ============================================================================ */

TEST_CASE("Turn phase enum values", "[turn][enum]") {
    SECTION("Phase count is correct") {
        REQUIRE(AGENTITE_PHASE_COUNT == 5);
    }

    SECTION("Phases have sequential values") {
        REQUIRE(AGENTITE_PHASE_WORLD_UPDATE == 0);
        REQUIRE(AGENTITE_PHASE_EVENTS == 1);
        REQUIRE(AGENTITE_PHASE_PLAYER_INPUT == 2);
        REQUIRE(AGENTITE_PHASE_RESOLUTION == 3);
        REQUIRE(AGENTITE_PHASE_END_CHECK == 4);
    }

    SECTION("PHASE_COUNT matches actual count") {
        int count = 0;
        count++;  // WORLD_UPDATE
        count++;  // EVENTS
        count++;  // PLAYER_INPUT
        count++;  // RESOLUTION
        count++;  // END_CHECK
        REQUIRE(count == AGENTITE_PHASE_COUNT);
    }
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_CASE("Turn manager integration", "[turn][integration]") {
    Agentite_TurnManager tm;
    agentite_turn_init(&tm);

    SECTION("Full game turn simulation") {
        int world_updates = 0;
        int event_triggers = 0;
        int player_inputs = 0;
        int resolutions = 0;
        int end_checks = 0;

        agentite_turn_set_callback(&tm, AGENTITE_PHASE_WORLD_UPDATE,
                                   counting_callback, &world_updates);
        agentite_turn_set_callback(&tm, AGENTITE_PHASE_EVENTS,
                                   counting_callback, &event_triggers);
        agentite_turn_set_callback(&tm, AGENTITE_PHASE_PLAYER_INPUT,
                                   counting_callback, &player_inputs);
        agentite_turn_set_callback(&tm, AGENTITE_PHASE_RESOLUTION,
                                   counting_callback, &resolutions);
        agentite_turn_set_callback(&tm, AGENTITE_PHASE_END_CHECK,
                                   counting_callback, &end_checks);

        // Simulate 3 complete turns
        for (int turn = 0; turn < 3; turn++) {
            for (int phase = 0; phase < AGENTITE_PHASE_COUNT; phase++) {
                agentite_turn_advance(&tm);
            }
        }

        REQUIRE(world_updates == 3);
        REQUIRE(event_triggers == 3);
        REQUIRE(player_inputs == 3);
        REQUIRE(resolutions == 3);
        REQUIRE(end_checks == 3);
        REQUIRE(tm.turn_number == 4);  // Started at 1, completed 3 turns = 4
    }

    SECTION("Partial turn state") {
        // Advance to player input phase
        agentite_turn_advance(&tm);  // WORLD_UPDATE -> EVENTS
        agentite_turn_advance(&tm);  // EVENTS -> PLAYER_INPUT

        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_PLAYER_INPUT);
        REQUIRE(tm.turn_number == 1);  // Still turn 1

        // Complete the turn
        agentite_turn_advance(&tm);  // PLAYER_INPUT -> RESOLUTION
        agentite_turn_advance(&tm);  // RESOLUTION -> END_CHECK
        agentite_turn_advance(&tm);  // END_CHECK -> WORLD_UPDATE (turn 2)

        REQUIRE(agentite_turn_current_phase(&tm) == AGENTITE_PHASE_WORLD_UPDATE);
        REQUIRE(tm.turn_number == 2);
    }
}

/* ============================================================================
 * TurnManager Struct Tests
 * ============================================================================ */

TEST_CASE("TurnManager struct", "[turn][struct]") {
    SECTION("Struct is stack-allocatable") {
        // The struct should be lightweight and stack-allocatable
        Agentite_TurnManager tm;
        agentite_turn_init(&tm);

        // Can use it directly
        REQUIRE(agentite_turn_number(&tm) == 1);  // Starts at turn 1
    }

    SECTION("Struct size is reasonable") {
        // Should be relatively small (callbacks + state)
        size_t size = sizeof(Agentite_TurnManager);
        // Account for: int turn_number, TurnPhase current_phase,
        // callbacks[COUNT], userdata[COUNT], bool turn_in_progress
        REQUIRE(size < 512);  // Reasonable upper bound
    }

    SECTION("Multiple independent turn managers") {
        Agentite_TurnManager tm1, tm2;
        agentite_turn_init(&tm1);
        agentite_turn_init(&tm2);

        // Advance tm1 only
        agentite_turn_advance(&tm1);
        agentite_turn_advance(&tm1);

        // tm1 should be at PLAYER_INPUT, tm2 still at WORLD_UPDATE
        REQUIRE(agentite_turn_current_phase(&tm1) == AGENTITE_PHASE_PLAYER_INPUT);
        REQUIRE(agentite_turn_current_phase(&tm2) == AGENTITE_PHASE_WORLD_UPDATE);
    }
}
