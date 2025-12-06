/*
 * Carbon Command Queue Tests
 *
 * Tests for the command queue system including registration,
 * parameter handling, validation, execution, and history.
 */

#include "catch_amalgamated.hpp"
#include "carbon/command.h"
#include "carbon/error.h"
#include <cstring>

/* ============================================================================
 * Test Command Types
 * ============================================================================ */

enum TestCommandType {
    CMD_MOVE = 1,
    CMD_ATTACK = 2,
    CMD_BUILD = 3,
    CMD_ALWAYS_FAILS = 4,
    CMD_ALWAYS_INVALID = 5,
};

/* ============================================================================
 * Test Validators and Executors
 * ============================================================================ */

static int g_execute_count = 0;
static int g_last_x = 0;
static int g_last_y = 0;

static bool validate_move(const Carbon_Command *cmd, void *game_state,
                          char *error_buf, size_t error_size) {
    (void)game_state;

    int x = carbon_command_get_int(cmd, "x");
    int y = carbon_command_get_int(cmd, "y");

    if (x < 0 || y < 0) {
        snprintf(error_buf, error_size, "Invalid coordinates: %d, %d", x, y);
        return false;
    }

    if (x > 100 || y > 100) {
        snprintf(error_buf, error_size, "Coordinates out of bounds");
        return false;
    }

    return true;
}

static bool execute_move(const Carbon_Command *cmd, void *game_state) {
    (void)game_state;
    g_execute_count++;
    g_last_x = carbon_command_get_int(cmd, "x");
    g_last_y = carbon_command_get_int(cmd, "y");
    return true;
}

static bool execute_attack(const Carbon_Command *cmd, void *game_state) {
    (void)cmd;
    (void)game_state;
    g_execute_count++;
    return true;
}

static bool execute_always_fails(const Carbon_Command *cmd, void *game_state) {
    (void)cmd;
    (void)game_state;
    return false;
}

static bool validate_always_invalid(const Carbon_Command *cmd, void *game_state,
                                     char *error_buf, size_t error_size) {
    (void)cmd;
    (void)game_state;
    snprintf(error_buf, error_size, "Always invalid");
    return false;
}

static bool execute_always_invalid(const Carbon_Command *cmd, void *game_state) {
    (void)cmd;
    (void)game_state;
    return true;
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_CASE("Command system creation and destruction", "[command][lifecycle]") {
    Carbon_CommandSystem *sys = carbon_command_create();
    REQUIRE(sys != nullptr);
    carbon_command_destroy(sys);
}

TEST_CASE("Command creation and destruction", "[command][lifecycle]") {
    Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->type == CMD_MOVE);
    carbon_command_free(cmd);
}

TEST_CASE("Command creation with faction", "[command][lifecycle]") {
    Carbon_Command *cmd = carbon_command_new_ex(CMD_MOVE, 3);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->type == CMD_MOVE);
    REQUIRE(cmd->source_faction == 3);
    carbon_command_free(cmd);
}

TEST_CASE("Destroy NULL command system", "[command][lifecycle]") {
    // Should not crash
    carbon_command_destroy(nullptr);
}

TEST_CASE("Free NULL command", "[command][lifecycle]") {
    // Should not crash
    carbon_command_free(nullptr);
}

/* ============================================================================
 * Command Registration Tests
 * ============================================================================ */

TEST_CASE("Command type registration", "[command][registration]") {
    Carbon_CommandSystem *sys = carbon_command_create();

    SECTION("Register with validator") {
        REQUIRE(carbon_command_register(sys, CMD_MOVE, validate_move, execute_move));
        REQUIRE(carbon_command_is_registered(sys, CMD_MOVE));
    }

    SECTION("Register without validator") {
        REQUIRE(carbon_command_register(sys, CMD_ATTACK, nullptr, execute_attack));
        REQUIRE(carbon_command_is_registered(sys, CMD_ATTACK));
    }

    SECTION("Register named command") {
        REQUIRE(carbon_command_register_named(sys, CMD_BUILD, "Build Structure",
                                               nullptr, execute_attack));
        REQUIRE(carbon_command_is_registered(sys, CMD_BUILD));
        REQUIRE(strcmp(carbon_command_get_type_name(sys, CMD_BUILD), "Build Structure") == 0);
    }

    SECTION("Auto-generated name") {
        REQUIRE(carbon_command_register(sys, CMD_MOVE, nullptr, execute_move));
        const char *name = carbon_command_get_type_name(sys, CMD_MOVE);
        REQUIRE(name != nullptr);
        REQUIRE(strstr(name, "Command_") != nullptr);
    }

    SECTION("Cannot register same type twice") {
        REQUIRE(carbon_command_register(sys, CMD_MOVE, nullptr, execute_move));
        REQUIRE_FALSE(carbon_command_register(sys, CMD_MOVE, nullptr, execute_attack));
    }

    SECTION("Unregistered type") {
        REQUIRE_FALSE(carbon_command_is_registered(sys, 999));
        REQUIRE(carbon_command_get_type_name(sys, 999) == nullptr);
    }

    SECTION("Executor is required") {
        REQUIRE_FALSE(carbon_command_register(sys, CMD_MOVE, validate_move, nullptr));
    }

    carbon_command_destroy(sys);
}

/* ============================================================================
 * Parameter Tests
 * ============================================================================ */

TEST_CASE("Command parameters - integers", "[command][params]") {
    Carbon_Command *cmd = carbon_command_new(CMD_MOVE);

    SECTION("Set and get int") {
        carbon_command_set_int(cmd, "x", 42);
        REQUIRE(carbon_command_get_int(cmd, "x") == 42);
        REQUIRE(carbon_command_has_param(cmd, "x"));
        REQUIRE(carbon_command_get_param_type(cmd, "x") == CARBON_CMD_PARAM_INT);
    }

    SECTION("Get with default") {
        REQUIRE(carbon_command_get_int_or(cmd, "missing", -1) == -1);
        carbon_command_set_int(cmd, "present", 100);
        REQUIRE(carbon_command_get_int_or(cmd, "present", -1) == 100);
    }

    SECTION("Set and get int64") {
        carbon_command_set_int64(cmd, "big", 0x123456789ABCDEFLL);
        REQUIRE(carbon_command_get_int64(cmd, "big") == 0x123456789ABCDEFLL);
        REQUIRE(carbon_command_get_param_type(cmd, "big") == CARBON_CMD_PARAM_INT64);
    }

    SECTION("Update existing parameter") {
        carbon_command_set_int(cmd, "x", 10);
        carbon_command_set_int(cmd, "x", 20);
        REQUIRE(carbon_command_get_int(cmd, "x") == 20);
    }

    carbon_command_free(cmd);
}

TEST_CASE("Command parameters - floats", "[command][params]") {
    Carbon_Command *cmd = carbon_command_new(CMD_MOVE);

    SECTION("Set and get float") {
        carbon_command_set_float(cmd, "speed", 3.14f);
        REQUIRE(carbon_command_get_float(cmd, "speed") == Catch::Approx(3.14f));
        REQUIRE(carbon_command_get_param_type(cmd, "speed") == CARBON_CMD_PARAM_FLOAT);
    }

    SECTION("Get float with default") {
        REQUIRE(carbon_command_get_float_or(cmd, "missing", 1.0f) == 1.0f);
    }

    SECTION("Set and get double") {
        carbon_command_set_double(cmd, "precision", 3.141592653589793);
        REQUIRE(carbon_command_get_double(cmd, "precision") == Catch::Approx(3.141592653589793));
        REQUIRE(carbon_command_get_param_type(cmd, "precision") == CARBON_CMD_PARAM_DOUBLE);
    }

    carbon_command_free(cmd);
}

TEST_CASE("Command parameters - other types", "[command][params]") {
    Carbon_Command *cmd = carbon_command_new(CMD_MOVE);

    SECTION("Boolean") {
        carbon_command_set_bool(cmd, "active", true);
        REQUIRE(carbon_command_get_bool(cmd, "active") == true);
        REQUIRE(carbon_command_get_param_type(cmd, "active") == CARBON_CMD_PARAM_BOOL);

        carbon_command_set_bool(cmd, "inactive", false);
        REQUIRE(carbon_command_get_bool(cmd, "inactive") == false);
    }

    SECTION("Entity") {
        carbon_command_set_entity(cmd, "target", 12345);
        REQUIRE(carbon_command_get_entity(cmd, "target") == 12345);
        REQUIRE(carbon_command_get_param_type(cmd, "target") == CARBON_CMD_PARAM_ENTITY);
    }

    SECTION("String") {
        carbon_command_set_string(cmd, "name", "Test Unit");
        REQUIRE(strcmp(carbon_command_get_string(cmd, "name"), "Test Unit") == 0);
        REQUIRE(carbon_command_get_param_type(cmd, "name") == CARBON_CMD_PARAM_STRING);
    }

    SECTION("String NULL") {
        carbon_command_set_string(cmd, "empty", nullptr);
        REQUIRE(carbon_command_get_string(cmd, "empty") != nullptr);
        REQUIRE(strlen(carbon_command_get_string(cmd, "empty")) == 0);
    }

    SECTION("Pointer") {
        int value = 42;
        carbon_command_set_ptr(cmd, "data", &value);
        REQUIRE(carbon_command_get_ptr(cmd, "data") == &value);
        REQUIRE(carbon_command_get_param_type(cmd, "data") == CARBON_CMD_PARAM_PTR);
    }

    SECTION("Missing parameter returns default") {
        REQUIRE(carbon_command_get_int(cmd, "missing") == 0);
        REQUIRE(carbon_command_get_int64(cmd, "missing") == 0);
        REQUIRE(carbon_command_get_float(cmd, "missing") == 0.0f);
        REQUIRE(carbon_command_get_double(cmd, "missing") == 0.0);
        REQUIRE(carbon_command_get_bool(cmd, "missing") == false);
        REQUIRE(carbon_command_get_entity(cmd, "missing") == 0);
        REQUIRE(carbon_command_get_string(cmd, "missing") == nullptr);
        REQUIRE(carbon_command_get_ptr(cmd, "missing") == nullptr);
    }

    SECTION("Has param and type") {
        REQUIRE_FALSE(carbon_command_has_param(cmd, "nonexistent"));
        REQUIRE(carbon_command_get_param_type(cmd, "nonexistent") == CARBON_CMD_PARAM_NONE);
    }

    carbon_command_free(cmd);
}

TEST_CASE("Command clone", "[command][params]") {
    Carbon_Command *cmd = carbon_command_new_ex(CMD_MOVE, 5);
    carbon_command_set_int(cmd, "x", 10);
    carbon_command_set_int(cmd, "y", 20);
    carbon_command_set_string(cmd, "name", "Unit1");

    Carbon_Command *clone = carbon_command_clone(cmd);
    REQUIRE(clone != nullptr);
    REQUIRE(clone->type == CMD_MOVE);
    REQUIRE(clone->source_faction == 5);
    REQUIRE(carbon_command_get_int(clone, "x") == 10);
    REQUIRE(carbon_command_get_int(clone, "y") == 20);
    REQUIRE(strcmp(carbon_command_get_string(clone, "name"), "Unit1") == 0);

    // Modifying clone doesn't affect original
    carbon_command_set_int(clone, "x", 999);
    REQUIRE(carbon_command_get_int(cmd, "x") == 10);
    REQUIRE(carbon_command_get_int(clone, "x") == 999);

    carbon_command_free(cmd);
    carbon_command_free(clone);
}

/* ============================================================================
 * Validation Tests
 * ============================================================================ */

TEST_CASE("Command validation", "[command][validation]") {
    Carbon_CommandSystem *sys = carbon_command_create();
    carbon_command_register(sys, CMD_MOVE, validate_move, execute_move);
    carbon_command_register(sys, CMD_ATTACK, nullptr, execute_attack);
    carbon_command_register(sys, CMD_ALWAYS_INVALID, validate_always_invalid, execute_always_invalid);

    SECTION("Valid command passes validation") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", 50);
        carbon_command_set_int(cmd, "y", 50);

        Carbon_CommandResult result = carbon_command_validate(sys, cmd, nullptr);
        REQUIRE(result.success);
        REQUIRE(result.command_type == CMD_MOVE);

        carbon_command_free(cmd);
    }

    SECTION("Invalid command fails validation") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", -10);
        carbon_command_set_int(cmd, "y", 50);

        Carbon_CommandResult result = carbon_command_validate(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error, "Invalid coordinates") != nullptr);

        carbon_command_free(cmd);
    }

    SECTION("Out of bounds fails validation") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", 150);
        carbon_command_set_int(cmd, "y", 50);

        Carbon_CommandResult result = carbon_command_validate(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error, "out of bounds") != nullptr);

        carbon_command_free(cmd);
    }

    SECTION("No validator means always valid") {
        Carbon_Command *cmd = carbon_command_new(CMD_ATTACK);
        Carbon_CommandResult result = carbon_command_validate(sys, cmd, nullptr);
        REQUIRE(result.success);
        carbon_command_free(cmd);
    }

    SECTION("Unregistered type fails") {
        Carbon_Command *cmd = carbon_command_new(999);
        Carbon_CommandResult result = carbon_command_validate(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error, "not registered") != nullptr);
        carbon_command_free(cmd);
    }

    carbon_command_destroy(sys);
}

/* ============================================================================
 * Queue Tests
 * ============================================================================ */

TEST_CASE("Command queue operations", "[command][queue]") {
    Carbon_CommandSystem *sys = carbon_command_create();
    carbon_command_register(sys, CMD_MOVE, validate_move, execute_move);

    SECTION("Queue and count") {
        REQUIRE(carbon_command_queue_count(sys) == 0);

        Carbon_Command *cmd1 = carbon_command_new(CMD_MOVE);
        Carbon_Command *cmd2 = carbon_command_new(CMD_MOVE);

        REQUIRE(carbon_command_queue(sys, cmd1));
        REQUIRE(carbon_command_queue_count(sys) == 1);

        REQUIRE(carbon_command_queue(sys, cmd2));
        REQUIRE(carbon_command_queue_count(sys) == 2);

        carbon_command_free(cmd1);
        carbon_command_free(cmd2);
    }

    SECTION("Queue get by index") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", 42);
        carbon_command_queue(sys, cmd);

        const Carbon_Command *queued = carbon_command_queue_get(sys, 0);
        REQUIRE(queued != nullptr);
        REQUIRE(carbon_command_get_int(queued, "x") == 42);

        REQUIRE(carbon_command_queue_get(sys, 1) == nullptr);
        REQUIRE(carbon_command_queue_get(sys, -1) == nullptr);

        carbon_command_free(cmd);
    }

    SECTION("Queue clear") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_queue(sys, cmd);
        carbon_command_queue(sys, cmd);
        carbon_command_free(cmd);

        REQUIRE(carbon_command_queue_count(sys) == 2);
        carbon_command_queue_clear(sys);
        REQUIRE(carbon_command_queue_count(sys) == 0);
    }

    SECTION("Queue remove") {
        Carbon_Command *cmd1 = carbon_command_new(CMD_MOVE);
        Carbon_Command *cmd2 = carbon_command_new(CMD_MOVE);
        Carbon_Command *cmd3 = carbon_command_new(CMD_MOVE);

        carbon_command_set_int(cmd1, "id", 1);
        carbon_command_set_int(cmd2, "id", 2);
        carbon_command_set_int(cmd3, "id", 3);

        carbon_command_queue(sys, cmd1);
        carbon_command_queue(sys, cmd2);
        carbon_command_queue(sys, cmd3);

        // Remove middle
        REQUIRE(carbon_command_queue_remove(sys, 1));
        REQUIRE(carbon_command_queue_count(sys) == 2);

        // Verify order: 1, 3
        REQUIRE(carbon_command_get_int(carbon_command_queue_get(sys, 0), "id") == 1);
        REQUIRE(carbon_command_get_int(carbon_command_queue_get(sys, 1), "id") == 3);

        // Invalid index
        REQUIRE_FALSE(carbon_command_queue_remove(sys, 5));
        REQUIRE_FALSE(carbon_command_queue_remove(sys, -1));

        carbon_command_free(cmd1);
        carbon_command_free(cmd2);
        carbon_command_free(cmd3);
    }

    SECTION("Queue validated - success") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", 50);
        carbon_command_set_int(cmd, "y", 50);

        Carbon_CommandResult result = carbon_command_queue_validated(sys, cmd, nullptr);
        REQUIRE(result.success);
        REQUIRE(carbon_command_queue_count(sys) == 1);

        carbon_command_free(cmd);
    }

    SECTION("Queue validated - failure") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", -10);  // Invalid
        carbon_command_set_int(cmd, "y", 50);

        Carbon_CommandResult result = carbon_command_queue_validated(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(carbon_command_queue_count(sys) == 0);

        carbon_command_free(cmd);
    }

    SECTION("Queue assigns sequence numbers") {
        Carbon_Command *cmd1 = carbon_command_new(CMD_MOVE);
        Carbon_Command *cmd2 = carbon_command_new(CMD_MOVE);

        carbon_command_queue(sys, cmd1);
        carbon_command_queue(sys, cmd2);

        const Carbon_Command *q1 = carbon_command_queue_get(sys, 0);
        const Carbon_Command *q2 = carbon_command_queue_get(sys, 1);

        REQUIRE(q1->sequence > 0);
        REQUIRE(q2->sequence > q1->sequence);

        carbon_command_free(cmd1);
        carbon_command_free(cmd2);
    }

    carbon_command_destroy(sys);
}

/* ============================================================================
 * Execution Tests
 * ============================================================================ */

TEST_CASE("Command execution", "[command][execution]") {
    Carbon_CommandSystem *sys = carbon_command_create();
    carbon_command_register(sys, CMD_MOVE, validate_move, execute_move);
    carbon_command_register(sys, CMD_ATTACK, nullptr, execute_attack);
    carbon_command_register(sys, CMD_ALWAYS_FAILS, nullptr, execute_always_fails);

    g_execute_count = 0;

    SECTION("Execute single command") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", 25);
        carbon_command_set_int(cmd, "y", 75);

        Carbon_CommandResult result = carbon_command_execute(sys, cmd, nullptr);
        REQUIRE(result.success);
        REQUIRE(g_execute_count == 1);
        REQUIRE(g_last_x == 25);
        REQUIRE(g_last_y == 75);

        carbon_command_free(cmd);
    }

    SECTION("Execute next from queue") {
        Carbon_Command *cmd1 = carbon_command_new(CMD_MOVE);
        Carbon_Command *cmd2 = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd1, "x", 10);
        carbon_command_set_int(cmd1, "y", 10);
        carbon_command_set_int(cmd2, "x", 20);
        carbon_command_set_int(cmd2, "y", 20);

        carbon_command_queue(sys, cmd1);
        carbon_command_queue(sys, cmd2);

        Carbon_CommandResult r1 = carbon_command_execute_next(sys, nullptr);
        REQUIRE(r1.success);
        REQUIRE(g_last_x == 10);
        REQUIRE(carbon_command_queue_count(sys) == 1);

        Carbon_CommandResult r2 = carbon_command_execute_next(sys, nullptr);
        REQUIRE(r2.success);
        REQUIRE(g_last_x == 20);
        REQUIRE(carbon_command_queue_count(sys) == 0);

        carbon_command_free(cmd1);
        carbon_command_free(cmd2);
    }

    SECTION("Execute next on empty queue") {
        Carbon_CommandResult result = carbon_command_execute_next(sys, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error, "empty") != nullptr);
    }

    SECTION("Execute all") {
        Carbon_Command *cmd = carbon_command_new(CMD_ATTACK);
        carbon_command_queue(sys, cmd);
        carbon_command_queue(sys, cmd);
        carbon_command_queue(sys, cmd);
        carbon_command_free(cmd);

        g_execute_count = 0;
        Carbon_CommandResult results[10];
        int count = carbon_command_execute_all(sys, nullptr, results, 10);

        REQUIRE(count == 3);
        REQUIRE(g_execute_count == 3);
        REQUIRE(carbon_command_queue_count(sys) == 0);

        for (int i = 0; i < count; i++) {
            REQUIRE(results[i].success);
        }
    }

    SECTION("Execute all with max limit") {
        Carbon_Command *cmd = carbon_command_new(CMD_ATTACK);
        for (int i = 0; i < 5; i++) {
            carbon_command_queue(sys, cmd);
        }
        carbon_command_free(cmd);

        g_execute_count = 0;
        Carbon_CommandResult results[10];
        int count = carbon_command_execute_all(sys, nullptr, results, 2);

        REQUIRE(count == 2);
        REQUIRE(g_execute_count == 2);
        REQUIRE(carbon_command_queue_count(sys) == 3);  // 3 remaining
    }

    SECTION("Execution validates first") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", -10);  // Invalid
        carbon_command_set_int(cmd, "y", 50);

        g_execute_count = 0;
        Carbon_CommandResult result = carbon_command_execute(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(g_execute_count == 0);  // Not executed

        carbon_command_free(cmd);
    }

    SECTION("Executor failure") {
        Carbon_Command *cmd = carbon_command_new(CMD_ALWAYS_FAILS);
        Carbon_CommandResult result = carbon_command_execute(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        carbon_command_free(cmd);
    }

    SECTION("Unregistered type fails") {
        Carbon_Command *cmd = carbon_command_new(999);
        Carbon_CommandResult result = carbon_command_execute(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        carbon_command_free(cmd);
    }

    carbon_command_destroy(sys);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

static int g_callback_count = 0;
static bool g_callback_last_success = false;

static void test_callback(Carbon_CommandSystem *sys, const Carbon_Command *cmd,
                          const Carbon_CommandResult *result, void *userdata) {
    (void)sys;
    (void)cmd;
    g_callback_count++;
    g_callback_last_success = result->success;

    int *counter = (int*)userdata;
    if (counter) {
        (*counter)++;
    }
}

TEST_CASE("Command execution callbacks", "[command][callback]") {
    Carbon_CommandSystem *sys = carbon_command_create();
    carbon_command_register(sys, CMD_MOVE, validate_move, execute_move);
    carbon_command_register(sys, CMD_ALWAYS_FAILS, nullptr, execute_always_fails);

    g_callback_count = 0;
    int userdata_counter = 0;

    carbon_command_set_callback(sys, test_callback, &userdata_counter);

    SECTION("Callback on success") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", 50);
        carbon_command_set_int(cmd, "y", 50);

        carbon_command_execute(sys, cmd, nullptr);

        REQUIRE(g_callback_count == 1);
        REQUIRE(g_callback_last_success == true);
        REQUIRE(userdata_counter == 1);

        carbon_command_free(cmd);
    }

    SECTION("Callback on failure") {
        Carbon_Command *cmd = carbon_command_new(CMD_ALWAYS_FAILS);
        carbon_command_execute(sys, cmd, nullptr);

        REQUIRE(g_callback_count == 1);
        REQUIRE(g_callback_last_success == false);

        carbon_command_free(cmd);
    }

    SECTION("Callback on validation failure") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", -10);  // Invalid
        carbon_command_set_int(cmd, "y", 50);

        carbon_command_execute(sys, cmd, nullptr);

        REQUIRE(g_callback_count == 1);
        REQUIRE(g_callback_last_success == false);

        carbon_command_free(cmd);
    }

    SECTION("Clear callback") {
        carbon_command_set_callback(sys, nullptr, nullptr);

        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", 50);
        carbon_command_set_int(cmd, "y", 50);

        g_callback_count = 0;
        carbon_command_execute(sys, cmd, nullptr);

        REQUIRE(g_callback_count == 0);

        carbon_command_free(cmd);
    }

    carbon_command_destroy(sys);
}

/* ============================================================================
 * History Tests
 * ============================================================================ */

TEST_CASE("Command history", "[command][history]") {
    Carbon_CommandSystem *sys = carbon_command_create();
    carbon_command_register(sys, CMD_MOVE, nullptr, execute_move);

    SECTION("History disabled by default") {
        REQUIRE(carbon_command_get_history_count(sys) == 0);

        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", 10);
        carbon_command_set_int(cmd, "y", 20);
        carbon_command_execute(sys, cmd, nullptr);
        carbon_command_free(cmd);

        REQUIRE(carbon_command_get_history_count(sys) == 0);
    }

    SECTION("Enable history") {
        carbon_command_enable_history(sys, 10);

        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", 10);
        carbon_command_set_int(cmd, "y", 20);
        carbon_command_execute(sys, cmd, nullptr);
        carbon_command_free(cmd);

        REQUIRE(carbon_command_get_history_count(sys) == 1);
    }

    SECTION("History order (newest first)") {
        carbon_command_enable_history(sys, 10);

        for (int i = 1; i <= 3; i++) {
            Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
            carbon_command_set_int(cmd, "id", i);
            carbon_command_execute(sys, cmd, nullptr);
            carbon_command_free(cmd);
        }

        REQUIRE(carbon_command_get_history_count(sys) == 3);

        const Carbon_Command *history[10];
        int count = carbon_command_get_history(sys, history, 10);
        REQUIRE(count == 3);

        // Newest first
        REQUIRE(carbon_command_get_int(history[0], "id") == 3);
        REQUIRE(carbon_command_get_int(history[1], "id") == 2);
        REQUIRE(carbon_command_get_int(history[2], "id") == 1);
    }

    SECTION("History wraps when full") {
        carbon_command_enable_history(sys, 3);

        for (int i = 1; i <= 5; i++) {
            Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
            carbon_command_set_int(cmd, "id", i);
            carbon_command_execute(sys, cmd, nullptr);
            carbon_command_free(cmd);
        }

        REQUIRE(carbon_command_get_history_count(sys) == 3);

        const Carbon_Command *history[10];
        int count = carbon_command_get_history(sys, history, 10);
        REQUIRE(count == 3);

        // Should have 3, 4, 5 (oldest 1, 2 were overwritten)
        // Newest first: 5, 4, 3
        REQUIRE(carbon_command_get_int(history[0], "id") == 5);
        REQUIRE(carbon_command_get_int(history[1], "id") == 4);
        REQUIRE(carbon_command_get_int(history[2], "id") == 3);
    }

    SECTION("Clear history") {
        carbon_command_enable_history(sys, 10);

        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_execute(sys, cmd, nullptr);
        carbon_command_execute(sys, cmd, nullptr);
        carbon_command_free(cmd);

        REQUIRE(carbon_command_get_history_count(sys) == 2);

        carbon_command_clear_history(sys);
        REQUIRE(carbon_command_get_history_count(sys) == 0);
    }

    SECTION("Replay command") {
        carbon_command_enable_history(sys, 10);
        g_execute_count = 0;

        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", 42);
        carbon_command_set_int(cmd, "y", 24);
        carbon_command_execute(sys, cmd, nullptr);
        carbon_command_free(cmd);

        REQUIRE(g_execute_count == 1);
        REQUIRE(g_last_x == 42);

        // Change values to verify replay works
        g_last_x = 0;
        g_last_y = 0;

        Carbon_CommandResult result = carbon_command_replay(sys, 0, nullptr);
        REQUIRE(result.success);
        REQUIRE(g_execute_count == 2);
        REQUIRE(g_last_x == 42);
        REQUIRE(g_last_y == 24);
    }

    SECTION("Replay invalid index") {
        carbon_command_enable_history(sys, 10);

        Carbon_CommandResult result = carbon_command_replay(sys, 0, nullptr);
        REQUIRE_FALSE(result.success);

        result = carbon_command_replay(sys, -1, nullptr);
        REQUIRE_FALSE(result.success);
    }

    carbon_command_destroy(sys);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_CASE("Command statistics", "[command][stats]") {
    Carbon_CommandSystem *sys = carbon_command_create();
    carbon_command_register(sys, CMD_MOVE, validate_move, execute_move);
    carbon_command_register(sys, CMD_ATTACK, nullptr, execute_attack);
    carbon_command_register(sys, CMD_ALWAYS_FAILS, nullptr, execute_always_fails);
    carbon_command_register(sys, CMD_ALWAYS_INVALID, validate_always_invalid, execute_always_invalid);

    SECTION("Stats track executions") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);
        carbon_command_set_int(cmd, "x", 50);
        carbon_command_set_int(cmd, "y", 50);

        carbon_command_execute(sys, cmd, nullptr);
        carbon_command_execute(sys, cmd, nullptr);

        carbon_command_free(cmd);

        Carbon_CommandStats stats;
        carbon_command_get_stats(sys, &stats);

        REQUIRE(stats.total_executed == 2);
        REQUIRE(stats.total_succeeded == 2);
        REQUIRE(stats.total_failed == 0);
        REQUIRE(stats.commands_by_type[CMD_MOVE] == 2);
    }

    SECTION("Stats track failures") {
        Carbon_Command *cmd = carbon_command_new(CMD_ALWAYS_FAILS);
        carbon_command_execute(sys, cmd, nullptr);
        carbon_command_free(cmd);

        Carbon_CommandStats stats;
        carbon_command_get_stats(sys, &stats);

        REQUIRE(stats.total_executed == 1);
        REQUIRE(stats.total_succeeded == 0);
        REQUIRE(stats.total_failed == 1);
    }

    SECTION("Stats track validation failures") {
        Carbon_Command *cmd = carbon_command_new(CMD_ALWAYS_INVALID);
        carbon_command_execute(sys, cmd, nullptr);
        carbon_command_free(cmd);

        Carbon_CommandStats stats;
        carbon_command_get_stats(sys, &stats);

        REQUIRE(stats.total_invalid == 1);
    }

    SECTION("Reset stats") {
        Carbon_Command *cmd = carbon_command_new(CMD_ATTACK);
        carbon_command_execute(sys, cmd, nullptr);
        carbon_command_free(cmd);

        carbon_command_reset_stats(sys);

        Carbon_CommandStats stats;
        carbon_command_get_stats(sys, &stats);

        REQUIRE(stats.total_executed == 0);
        REQUIRE(stats.total_succeeded == 0);
    }

    carbon_command_destroy(sys);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_CASE("Command utility functions", "[command][utility]") {
    SECTION("Result OK check") {
        Carbon_CommandResult success = carbon_command_result_success(CMD_MOVE);
        Carbon_CommandResult failure = carbon_command_result_failure(CMD_MOVE, "Error");

        REQUIRE(carbon_command_result_ok(&success));
        REQUIRE_FALSE(carbon_command_result_ok(&failure));
        REQUIRE_FALSE(carbon_command_result_ok(nullptr));
    }

    SECTION("Result success") {
        Carbon_CommandResult result = carbon_command_result_success(CMD_ATTACK);
        REQUIRE(result.success);
        REQUIRE(result.command_type == CMD_ATTACK);
    }

    SECTION("Result failure") {
        Carbon_CommandResult result = carbon_command_result_failure(CMD_BUILD, "Not enough resources");
        REQUIRE_FALSE(result.success);
        REQUIRE(result.command_type == CMD_BUILD);
        REQUIRE(strcmp(result.error, "Not enough resources") == 0);
    }

    SECTION("Result failure with NULL error") {
        Carbon_CommandResult result = carbon_command_result_failure(CMD_BUILD, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(result.error[0] == '\0');
    }
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_CASE("Command edge cases", "[command][edge]") {
    Carbon_CommandSystem *sys = carbon_command_create();
    carbon_command_register(sys, CMD_MOVE, nullptr, execute_move);

    SECTION("Maximum parameters") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);

        // Fill all parameter slots
        for (int i = 0; i < CARBON_COMMAND_MAX_PARAMS; i++) {
            char key[32];
            snprintf(key, sizeof(key), "param_%d", i);
            carbon_command_set_int(cmd, key, i);
        }

        REQUIRE(cmd->param_count == CARBON_COMMAND_MAX_PARAMS);

        // Verify first and last
        REQUIRE(carbon_command_get_int(cmd, "param_0") == 0);
        char last_key[32];
        snprintf(last_key, sizeof(last_key), "param_%d", CARBON_COMMAND_MAX_PARAMS - 1);
        REQUIRE(carbon_command_get_int(cmd, last_key) == CARBON_COMMAND_MAX_PARAMS - 1);

        carbon_command_free(cmd);
    }

    SECTION("Long string truncation") {
        Carbon_Command *cmd = carbon_command_new(CMD_MOVE);

        // Create a string longer than max key length
        char long_string[256];
        memset(long_string, 'A', sizeof(long_string) - 1);
        long_string[sizeof(long_string) - 1] = '\0';

        carbon_command_set_string(cmd, "test", long_string);

        const char *result = carbon_command_get_string(cmd, "test");
        REQUIRE(strlen(result) < sizeof(long_string));
        REQUIRE(strlen(result) == CARBON_COMMAND_MAX_PARAM_KEY - 1);

        carbon_command_free(cmd);
    }

    carbon_command_destroy(sys);
}
