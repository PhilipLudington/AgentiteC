/*
 * Carbon Command Queue Tests
 *
 * Tests for the command queue system including registration,
 * parameter handling, validation, execution, and history.
 */

#include "catch_amalgamated.hpp"
#include "agentite/command.h"
#include "agentite/error.h"
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

static bool validate_move(const Agentite_Command *cmd, void *game_state,
                          char *error_buf, size_t error_size) {
    (void)game_state;

    int x = agentite_command_get_int(cmd, "x");
    int y = agentite_command_get_int(cmd, "y");

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

static bool execute_move(const Agentite_Command *cmd, void *game_state) {
    (void)game_state;
    g_execute_count++;
    g_last_x = agentite_command_get_int(cmd, "x");
    g_last_y = agentite_command_get_int(cmd, "y");
    return true;
}

static bool execute_attack(const Agentite_Command *cmd, void *game_state) {
    (void)cmd;
    (void)game_state;
    g_execute_count++;
    return true;
}

static bool execute_always_fails(const Agentite_Command *cmd, void *game_state) {
    (void)cmd;
    (void)game_state;
    return false;
}

static bool validate_always_invalid(const Agentite_Command *cmd, void *game_state,
                                     char *error_buf, size_t error_size) {
    (void)cmd;
    (void)game_state;
    snprintf(error_buf, error_size, "Always invalid");
    return false;
}

static bool execute_always_invalid(const Agentite_Command *cmd, void *game_state) {
    (void)cmd;
    (void)game_state;
    return true;
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_CASE("Command system creation and destruction", "[command][lifecycle]") {
    Agentite_CommandSystem *sys = agentite_command_create();
    REQUIRE(sys != nullptr);
    agentite_command_destroy(sys);
}

TEST_CASE("Command creation and destruction", "[command][lifecycle]") {
    Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->type == CMD_MOVE);
    agentite_command_free(cmd);
}

TEST_CASE("Command creation with faction", "[command][lifecycle]") {
    Agentite_Command *cmd = agentite_command_new_ex(CMD_MOVE, 3);
    REQUIRE(cmd != nullptr);
    REQUIRE(cmd->type == CMD_MOVE);
    REQUIRE(cmd->source_faction == 3);
    agentite_command_free(cmd);
}

TEST_CASE("Destroy NULL command system", "[command][lifecycle]") {
    // Should not crash
    agentite_command_destroy(nullptr);
}

TEST_CASE("Free NULL command", "[command][lifecycle]") {
    // Should not crash
    agentite_command_free(nullptr);
}

/* ============================================================================
 * Command Registration Tests
 * ============================================================================ */

TEST_CASE("Command type registration", "[command][registration]") {
    Agentite_CommandSystem *sys = agentite_command_create();

    SECTION("Register with validator") {
        REQUIRE(agentite_command_register(sys, CMD_MOVE, validate_move, execute_move));
        REQUIRE(agentite_command_is_registered(sys, CMD_MOVE));
    }

    SECTION("Register without validator") {
        REQUIRE(agentite_command_register(sys, CMD_ATTACK, nullptr, execute_attack));
        REQUIRE(agentite_command_is_registered(sys, CMD_ATTACK));
    }

    SECTION("Register named command") {
        REQUIRE(agentite_command_register_named(sys, CMD_BUILD, "Build Structure",
                                               nullptr, execute_attack));
        REQUIRE(agentite_command_is_registered(sys, CMD_BUILD));
        REQUIRE(strcmp(agentite_command_get_type_name(sys, CMD_BUILD), "Build Structure") == 0);
    }

    SECTION("Auto-generated name") {
        REQUIRE(agentite_command_register(sys, CMD_MOVE, nullptr, execute_move));
        const char *name = agentite_command_get_type_name(sys, CMD_MOVE);
        REQUIRE(name != nullptr);
        REQUIRE(strstr(name, "Command_") != nullptr);
    }

    SECTION("Cannot register same type twice") {
        REQUIRE(agentite_command_register(sys, CMD_MOVE, nullptr, execute_move));
        REQUIRE_FALSE(agentite_command_register(sys, CMD_MOVE, nullptr, execute_attack));
    }

    SECTION("Unregistered type") {
        REQUIRE_FALSE(agentite_command_is_registered(sys, 999));
        REQUIRE(agentite_command_get_type_name(sys, 999) == nullptr);
    }

    SECTION("Executor is required") {
        REQUIRE_FALSE(agentite_command_register(sys, CMD_MOVE, validate_move, nullptr));
    }

    agentite_command_destroy(sys);
}

/* ============================================================================
 * Parameter Tests
 * ============================================================================ */

TEST_CASE("Command parameters - integers", "[command][params]") {
    Agentite_Command *cmd = agentite_command_new(CMD_MOVE);

    SECTION("Set and get int") {
        agentite_command_set_int(cmd, "x", 42);
        REQUIRE(agentite_command_get_int(cmd, "x") == 42);
        REQUIRE(agentite_command_has_param(cmd, "x"));
        REQUIRE(agentite_command_get_param_type(cmd, "x") == AGENTITE_CMD_PARAM_INT);
    }

    SECTION("Get with default") {
        REQUIRE(agentite_command_get_int_or(cmd, "missing", -1) == -1);
        agentite_command_set_int(cmd, "present", 100);
        REQUIRE(agentite_command_get_int_or(cmd, "present", -1) == 100);
    }

    SECTION("Set and get int64") {
        agentite_command_set_int64(cmd, "big", 0x123456789ABCDEFLL);
        REQUIRE(agentite_command_get_int64(cmd, "big") == 0x123456789ABCDEFLL);
        REQUIRE(agentite_command_get_param_type(cmd, "big") == AGENTITE_CMD_PARAM_INT64);
    }

    SECTION("Update existing parameter") {
        agentite_command_set_int(cmd, "x", 10);
        agentite_command_set_int(cmd, "x", 20);
        REQUIRE(agentite_command_get_int(cmd, "x") == 20);
    }

    agentite_command_free(cmd);
}

TEST_CASE("Command parameters - floats", "[command][params]") {
    Agentite_Command *cmd = agentite_command_new(CMD_MOVE);

    SECTION("Set and get float") {
        agentite_command_set_float(cmd, "speed", 3.14f);
        REQUIRE(agentite_command_get_float(cmd, "speed") == Catch::Approx(3.14f));
        REQUIRE(agentite_command_get_param_type(cmd, "speed") == AGENTITE_CMD_PARAM_FLOAT);
    }

    SECTION("Get float with default") {
        REQUIRE(agentite_command_get_float_or(cmd, "missing", 1.0f) == 1.0f);
    }

    SECTION("Set and get double") {
        agentite_command_set_double(cmd, "precision", 3.141592653589793);
        REQUIRE(agentite_command_get_double(cmd, "precision") == Catch::Approx(3.141592653589793));
        REQUIRE(agentite_command_get_param_type(cmd, "precision") == AGENTITE_CMD_PARAM_DOUBLE);
    }

    agentite_command_free(cmd);
}

TEST_CASE("Command parameters - other types", "[command][params]") {
    Agentite_Command *cmd = agentite_command_new(CMD_MOVE);

    SECTION("Boolean") {
        agentite_command_set_bool(cmd, "active", true);
        REQUIRE(agentite_command_get_bool(cmd, "active") == true);
        REQUIRE(agentite_command_get_param_type(cmd, "active") == AGENTITE_CMD_PARAM_BOOL);

        agentite_command_set_bool(cmd, "inactive", false);
        REQUIRE(agentite_command_get_bool(cmd, "inactive") == false);
    }

    SECTION("Entity") {
        agentite_command_set_entity(cmd, "target", 12345);
        REQUIRE(agentite_command_get_entity(cmd, "target") == 12345);
        REQUIRE(agentite_command_get_param_type(cmd, "target") == AGENTITE_CMD_PARAM_ENTITY);
    }

    SECTION("String") {
        agentite_command_set_string(cmd, "name", "Test Unit");
        REQUIRE(strcmp(agentite_command_get_string(cmd, "name"), "Test Unit") == 0);
        REQUIRE(agentite_command_get_param_type(cmd, "name") == AGENTITE_CMD_PARAM_STRING);
    }

    SECTION("String NULL") {
        agentite_command_set_string(cmd, "empty", nullptr);
        REQUIRE(agentite_command_get_string(cmd, "empty") != nullptr);
        REQUIRE(strlen(agentite_command_get_string(cmd, "empty")) == 0);
    }

    SECTION("Pointer") {
        int value = 42;
        agentite_command_set_ptr(cmd, "data", &value);
        REQUIRE(agentite_command_get_ptr(cmd, "data") == &value);
        REQUIRE(agentite_command_get_param_type(cmd, "data") == AGENTITE_CMD_PARAM_PTR);
    }

    SECTION("Missing parameter returns default") {
        REQUIRE(agentite_command_get_int(cmd, "missing") == 0);
        REQUIRE(agentite_command_get_int64(cmd, "missing") == 0);
        REQUIRE(agentite_command_get_float(cmd, "missing") == 0.0f);
        REQUIRE(agentite_command_get_double(cmd, "missing") == 0.0);
        REQUIRE(agentite_command_get_bool(cmd, "missing") == false);
        REQUIRE(agentite_command_get_entity(cmd, "missing") == 0);
        REQUIRE(agentite_command_get_string(cmd, "missing") == nullptr);
        REQUIRE(agentite_command_get_ptr(cmd, "missing") == nullptr);
    }

    SECTION("Has param and type") {
        REQUIRE_FALSE(agentite_command_has_param(cmd, "nonexistent"));
        REQUIRE(agentite_command_get_param_type(cmd, "nonexistent") == AGENTITE_CMD_PARAM_NONE);
    }

    agentite_command_free(cmd);
}

TEST_CASE("Command clone", "[command][params]") {
    Agentite_Command *cmd = agentite_command_new_ex(CMD_MOVE, 5);
    agentite_command_set_int(cmd, "x", 10);
    agentite_command_set_int(cmd, "y", 20);
    agentite_command_set_string(cmd, "name", "Unit1");

    Agentite_Command *clone = agentite_command_clone(cmd);
    REQUIRE(clone != nullptr);
    REQUIRE(clone->type == CMD_MOVE);
    REQUIRE(clone->source_faction == 5);
    REQUIRE(agentite_command_get_int(clone, "x") == 10);
    REQUIRE(agentite_command_get_int(clone, "y") == 20);
    REQUIRE(strcmp(agentite_command_get_string(clone, "name"), "Unit1") == 0);

    // Modifying clone doesn't affect original
    agentite_command_set_int(clone, "x", 999);
    REQUIRE(agentite_command_get_int(cmd, "x") == 10);
    REQUIRE(agentite_command_get_int(clone, "x") == 999);

    agentite_command_free(cmd);
    agentite_command_free(clone);
}

/* ============================================================================
 * Validation Tests
 * ============================================================================ */

TEST_CASE("Command validation", "[command][validation]") {
    Agentite_CommandSystem *sys = agentite_command_create();
    agentite_command_register(sys, CMD_MOVE, validate_move, execute_move);
    agentite_command_register(sys, CMD_ATTACK, nullptr, execute_attack);
    agentite_command_register(sys, CMD_ALWAYS_INVALID, validate_always_invalid, execute_always_invalid);

    SECTION("Valid command passes validation") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", 50);
        agentite_command_set_int(cmd, "y", 50);

        Agentite_CommandResult result = agentite_command_validate(sys, cmd, nullptr);
        REQUIRE(result.success);
        REQUIRE(result.command_type == CMD_MOVE);

        agentite_command_free(cmd);
    }

    SECTION("Invalid command fails validation") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", -10);
        agentite_command_set_int(cmd, "y", 50);

        Agentite_CommandResult result = agentite_command_validate(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error, "Invalid coordinates") != nullptr);

        agentite_command_free(cmd);
    }

    SECTION("Out of bounds fails validation") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", 150);
        agentite_command_set_int(cmd, "y", 50);

        Agentite_CommandResult result = agentite_command_validate(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error, "out of bounds") != nullptr);

        agentite_command_free(cmd);
    }

    SECTION("No validator means always valid") {
        Agentite_Command *cmd = agentite_command_new(CMD_ATTACK);
        Agentite_CommandResult result = agentite_command_validate(sys, cmd, nullptr);
        REQUIRE(result.success);
        agentite_command_free(cmd);
    }

    SECTION("Unregistered type fails") {
        Agentite_Command *cmd = agentite_command_new(999);
        Agentite_CommandResult result = agentite_command_validate(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error, "not registered") != nullptr);
        agentite_command_free(cmd);
    }

    agentite_command_destroy(sys);
}

/* ============================================================================
 * Queue Tests
 * ============================================================================ */

TEST_CASE("Command queue operations", "[command][queue]") {
    Agentite_CommandSystem *sys = agentite_command_create();
    agentite_command_register(sys, CMD_MOVE, validate_move, execute_move);

    SECTION("Queue and count") {
        REQUIRE(agentite_command_queue_count(sys) == 0);

        Agentite_Command *cmd1 = agentite_command_new(CMD_MOVE);
        Agentite_Command *cmd2 = agentite_command_new(CMD_MOVE);

        REQUIRE(agentite_command_queue(sys, cmd1));
        REQUIRE(agentite_command_queue_count(sys) == 1);

        REQUIRE(agentite_command_queue(sys, cmd2));
        REQUIRE(agentite_command_queue_count(sys) == 2);

        agentite_command_free(cmd1);
        agentite_command_free(cmd2);
    }

    SECTION("Queue get by index") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", 42);
        agentite_command_queue(sys, cmd);

        const Agentite_Command *queued = agentite_command_queue_get(sys, 0);
        REQUIRE(queued != nullptr);
        REQUIRE(agentite_command_get_int(queued, "x") == 42);

        REQUIRE(agentite_command_queue_get(sys, 1) == nullptr);
        REQUIRE(agentite_command_queue_get(sys, -1) == nullptr);

        agentite_command_free(cmd);
    }

    SECTION("Queue clear") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_queue(sys, cmd);
        agentite_command_queue(sys, cmd);
        agentite_command_free(cmd);

        REQUIRE(agentite_command_queue_count(sys) == 2);
        agentite_command_queue_clear(sys);
        REQUIRE(agentite_command_queue_count(sys) == 0);
    }

    SECTION("Queue remove") {
        Agentite_Command *cmd1 = agentite_command_new(CMD_MOVE);
        Agentite_Command *cmd2 = agentite_command_new(CMD_MOVE);
        Agentite_Command *cmd3 = agentite_command_new(CMD_MOVE);

        agentite_command_set_int(cmd1, "id", 1);
        agentite_command_set_int(cmd2, "id", 2);
        agentite_command_set_int(cmd3, "id", 3);

        agentite_command_queue(sys, cmd1);
        agentite_command_queue(sys, cmd2);
        agentite_command_queue(sys, cmd3);

        // Remove middle
        REQUIRE(agentite_command_queue_remove(sys, 1));
        REQUIRE(agentite_command_queue_count(sys) == 2);

        // Verify order: 1, 3
        REQUIRE(agentite_command_get_int(agentite_command_queue_get(sys, 0), "id") == 1);
        REQUIRE(agentite_command_get_int(agentite_command_queue_get(sys, 1), "id") == 3);

        // Invalid index
        REQUIRE_FALSE(agentite_command_queue_remove(sys, 5));
        REQUIRE_FALSE(agentite_command_queue_remove(sys, -1));

        agentite_command_free(cmd1);
        agentite_command_free(cmd2);
        agentite_command_free(cmd3);
    }

    SECTION("Queue validated - success") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", 50);
        agentite_command_set_int(cmd, "y", 50);

        Agentite_CommandResult result = agentite_command_queue_validated(sys, cmd, nullptr);
        REQUIRE(result.success);
        REQUIRE(agentite_command_queue_count(sys) == 1);

        agentite_command_free(cmd);
    }

    SECTION("Queue validated - failure") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", -10);  // Invalid
        agentite_command_set_int(cmd, "y", 50);

        Agentite_CommandResult result = agentite_command_queue_validated(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(agentite_command_queue_count(sys) == 0);

        agentite_command_free(cmd);
    }

    SECTION("Queue assigns sequence numbers") {
        Agentite_Command *cmd1 = agentite_command_new(CMD_MOVE);
        Agentite_Command *cmd2 = agentite_command_new(CMD_MOVE);

        agentite_command_queue(sys, cmd1);
        agentite_command_queue(sys, cmd2);

        const Agentite_Command *q1 = agentite_command_queue_get(sys, 0);
        const Agentite_Command *q2 = agentite_command_queue_get(sys, 1);

        REQUIRE(q1->sequence > 0);
        REQUIRE(q2->sequence > q1->sequence);

        agentite_command_free(cmd1);
        agentite_command_free(cmd2);
    }

    agentite_command_destroy(sys);
}

/* ============================================================================
 * Execution Tests
 * ============================================================================ */

TEST_CASE("Command execution", "[command][execution]") {
    Agentite_CommandSystem *sys = agentite_command_create();
    agentite_command_register(sys, CMD_MOVE, validate_move, execute_move);
    agentite_command_register(sys, CMD_ATTACK, nullptr, execute_attack);
    agentite_command_register(sys, CMD_ALWAYS_FAILS, nullptr, execute_always_fails);

    g_execute_count = 0;

    SECTION("Execute single command") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", 25);
        agentite_command_set_int(cmd, "y", 75);

        Agentite_CommandResult result = agentite_command_execute(sys, cmd, nullptr);
        REQUIRE(result.success);
        REQUIRE(g_execute_count == 1);
        REQUIRE(g_last_x == 25);
        REQUIRE(g_last_y == 75);

        agentite_command_free(cmd);
    }

    SECTION("Execute next from queue") {
        Agentite_Command *cmd1 = agentite_command_new(CMD_MOVE);
        Agentite_Command *cmd2 = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd1, "x", 10);
        agentite_command_set_int(cmd1, "y", 10);
        agentite_command_set_int(cmd2, "x", 20);
        agentite_command_set_int(cmd2, "y", 20);

        agentite_command_queue(sys, cmd1);
        agentite_command_queue(sys, cmd2);

        Agentite_CommandResult r1 = agentite_command_execute_next(sys, nullptr);
        REQUIRE(r1.success);
        REQUIRE(g_last_x == 10);
        REQUIRE(agentite_command_queue_count(sys) == 1);

        Agentite_CommandResult r2 = agentite_command_execute_next(sys, nullptr);
        REQUIRE(r2.success);
        REQUIRE(g_last_x == 20);
        REQUIRE(agentite_command_queue_count(sys) == 0);

        agentite_command_free(cmd1);
        agentite_command_free(cmd2);
    }

    SECTION("Execute next on empty queue") {
        Agentite_CommandResult result = agentite_command_execute_next(sys, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(strstr(result.error, "empty") != nullptr);
    }

    SECTION("Execute all") {
        Agentite_Command *cmd = agentite_command_new(CMD_ATTACK);
        agentite_command_queue(sys, cmd);
        agentite_command_queue(sys, cmd);
        agentite_command_queue(sys, cmd);
        agentite_command_free(cmd);

        g_execute_count = 0;
        Agentite_CommandResult results[10];
        int count = agentite_command_execute_all(sys, nullptr, results, 10);

        REQUIRE(count == 3);
        REQUIRE(g_execute_count == 3);
        REQUIRE(agentite_command_queue_count(sys) == 0);

        for (int i = 0; i < count; i++) {
            REQUIRE(results[i].success);
        }
    }

    SECTION("Execute all with max limit") {
        Agentite_Command *cmd = agentite_command_new(CMD_ATTACK);
        for (int i = 0; i < 5; i++) {
            agentite_command_queue(sys, cmd);
        }
        agentite_command_free(cmd);

        g_execute_count = 0;
        Agentite_CommandResult results[10];
        int count = agentite_command_execute_all(sys, nullptr, results, 2);

        REQUIRE(count == 2);
        REQUIRE(g_execute_count == 2);
        REQUIRE(agentite_command_queue_count(sys) == 3);  // 3 remaining
    }

    SECTION("Execution validates first") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", -10);  // Invalid
        agentite_command_set_int(cmd, "y", 50);

        g_execute_count = 0;
        Agentite_CommandResult result = agentite_command_execute(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(g_execute_count == 0);  // Not executed

        agentite_command_free(cmd);
    }

    SECTION("Executor failure") {
        Agentite_Command *cmd = agentite_command_new(CMD_ALWAYS_FAILS);
        Agentite_CommandResult result = agentite_command_execute(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        agentite_command_free(cmd);
    }

    SECTION("Unregistered type fails") {
        Agentite_Command *cmd = agentite_command_new(999);
        Agentite_CommandResult result = agentite_command_execute(sys, cmd, nullptr);
        REQUIRE_FALSE(result.success);
        agentite_command_free(cmd);
    }

    agentite_command_destroy(sys);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

static int g_callback_count = 0;
static bool g_callback_last_success = false;

static void test_callback(Agentite_CommandSystem *sys, const Agentite_Command *cmd,
                          const Agentite_CommandResult *result, void *userdata) {
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
    Agentite_CommandSystem *sys = agentite_command_create();
    agentite_command_register(sys, CMD_MOVE, validate_move, execute_move);
    agentite_command_register(sys, CMD_ALWAYS_FAILS, nullptr, execute_always_fails);

    g_callback_count = 0;
    int userdata_counter = 0;

    agentite_command_set_callback(sys, test_callback, &userdata_counter);

    SECTION("Callback on success") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", 50);
        agentite_command_set_int(cmd, "y", 50);

        agentite_command_execute(sys, cmd, nullptr);

        REQUIRE(g_callback_count == 1);
        REQUIRE(g_callback_last_success == true);
        REQUIRE(userdata_counter == 1);

        agentite_command_free(cmd);
    }

    SECTION("Callback on failure") {
        Agentite_Command *cmd = agentite_command_new(CMD_ALWAYS_FAILS);
        agentite_command_execute(sys, cmd, nullptr);

        REQUIRE(g_callback_count == 1);
        REQUIRE(g_callback_last_success == false);

        agentite_command_free(cmd);
    }

    SECTION("Callback on validation failure") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", -10);  // Invalid
        agentite_command_set_int(cmd, "y", 50);

        agentite_command_execute(sys, cmd, nullptr);

        REQUIRE(g_callback_count == 1);
        REQUIRE(g_callback_last_success == false);

        agentite_command_free(cmd);
    }

    SECTION("Clear callback") {
        agentite_command_set_callback(sys, nullptr, nullptr);

        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", 50);
        agentite_command_set_int(cmd, "y", 50);

        g_callback_count = 0;
        agentite_command_execute(sys, cmd, nullptr);

        REQUIRE(g_callback_count == 0);

        agentite_command_free(cmd);
    }

    agentite_command_destroy(sys);
}

/* ============================================================================
 * History Tests
 * ============================================================================ */

TEST_CASE("Command history", "[command][history]") {
    Agentite_CommandSystem *sys = agentite_command_create();
    agentite_command_register(sys, CMD_MOVE, nullptr, execute_move);

    SECTION("History disabled by default") {
        REQUIRE(agentite_command_get_history_count(sys) == 0);

        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", 10);
        agentite_command_set_int(cmd, "y", 20);
        agentite_command_execute(sys, cmd, nullptr);
        agentite_command_free(cmd);

        REQUIRE(agentite_command_get_history_count(sys) == 0);
    }

    SECTION("Enable history") {
        agentite_command_enable_history(sys, 10);

        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", 10);
        agentite_command_set_int(cmd, "y", 20);
        agentite_command_execute(sys, cmd, nullptr);
        agentite_command_free(cmd);

        REQUIRE(agentite_command_get_history_count(sys) == 1);
    }

    SECTION("History order (newest first)") {
        agentite_command_enable_history(sys, 10);

        for (int i = 1; i <= 3; i++) {
            Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
            agentite_command_set_int(cmd, "id", i);
            agentite_command_execute(sys, cmd, nullptr);
            agentite_command_free(cmd);
        }

        REQUIRE(agentite_command_get_history_count(sys) == 3);

        const Agentite_Command *history[10];
        int count = agentite_command_get_history(sys, history, 10);
        REQUIRE(count == 3);

        // Newest first
        REQUIRE(agentite_command_get_int(history[0], "id") == 3);
        REQUIRE(agentite_command_get_int(history[1], "id") == 2);
        REQUIRE(agentite_command_get_int(history[2], "id") == 1);
    }

    SECTION("History wraps when full") {
        agentite_command_enable_history(sys, 3);

        for (int i = 1; i <= 5; i++) {
            Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
            agentite_command_set_int(cmd, "id", i);
            agentite_command_execute(sys, cmd, nullptr);
            agentite_command_free(cmd);
        }

        REQUIRE(agentite_command_get_history_count(sys) == 3);

        const Agentite_Command *history[10];
        int count = agentite_command_get_history(sys, history, 10);
        REQUIRE(count == 3);

        // Should have 3, 4, 5 (oldest 1, 2 were overwritten)
        // Newest first: 5, 4, 3
        REQUIRE(agentite_command_get_int(history[0], "id") == 5);
        REQUIRE(agentite_command_get_int(history[1], "id") == 4);
        REQUIRE(agentite_command_get_int(history[2], "id") == 3);
    }

    SECTION("Clear history") {
        agentite_command_enable_history(sys, 10);

        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_execute(sys, cmd, nullptr);
        agentite_command_execute(sys, cmd, nullptr);
        agentite_command_free(cmd);

        REQUIRE(agentite_command_get_history_count(sys) == 2);

        agentite_command_clear_history(sys);
        REQUIRE(agentite_command_get_history_count(sys) == 0);
    }

    SECTION("Replay command") {
        agentite_command_enable_history(sys, 10);
        g_execute_count = 0;

        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", 42);
        agentite_command_set_int(cmd, "y", 24);
        agentite_command_execute(sys, cmd, nullptr);
        agentite_command_free(cmd);

        REQUIRE(g_execute_count == 1);
        REQUIRE(g_last_x == 42);

        // Change values to verify replay works
        g_last_x = 0;
        g_last_y = 0;

        Agentite_CommandResult result = agentite_command_replay(sys, 0, nullptr);
        REQUIRE(result.success);
        REQUIRE(g_execute_count == 2);
        REQUIRE(g_last_x == 42);
        REQUIRE(g_last_y == 24);
    }

    SECTION("Replay invalid index") {
        agentite_command_enable_history(sys, 10);

        Agentite_CommandResult result = agentite_command_replay(sys, 0, nullptr);
        REQUIRE_FALSE(result.success);

        result = agentite_command_replay(sys, -1, nullptr);
        REQUIRE_FALSE(result.success);
    }

    agentite_command_destroy(sys);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_CASE("Command statistics", "[command][stats]") {
    Agentite_CommandSystem *sys = agentite_command_create();
    agentite_command_register(sys, CMD_MOVE, validate_move, execute_move);
    agentite_command_register(sys, CMD_ATTACK, nullptr, execute_attack);
    agentite_command_register(sys, CMD_ALWAYS_FAILS, nullptr, execute_always_fails);
    agentite_command_register(sys, CMD_ALWAYS_INVALID, validate_always_invalid, execute_always_invalid);

    SECTION("Stats track executions") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", 50);
        agentite_command_set_int(cmd, "y", 50);

        agentite_command_execute(sys, cmd, nullptr);
        agentite_command_execute(sys, cmd, nullptr);

        agentite_command_free(cmd);

        Agentite_CommandStats stats;
        agentite_command_get_stats(sys, &stats);

        REQUIRE(stats.total_executed == 2);
        REQUIRE(stats.total_succeeded == 2);
        REQUIRE(stats.total_failed == 0);
        REQUIRE(stats.commands_by_type[CMD_MOVE] == 2);
    }

    SECTION("Stats track failures") {
        Agentite_Command *cmd = agentite_command_new(CMD_ALWAYS_FAILS);
        agentite_command_execute(sys, cmd, nullptr);
        agentite_command_free(cmd);

        Agentite_CommandStats stats;
        agentite_command_get_stats(sys, &stats);

        REQUIRE(stats.total_executed == 1);
        REQUIRE(stats.total_succeeded == 0);
        REQUIRE(stats.total_failed == 1);
    }

    SECTION("Stats track validation failures") {
        Agentite_Command *cmd = agentite_command_new(CMD_ALWAYS_INVALID);
        agentite_command_execute(sys, cmd, nullptr);
        agentite_command_free(cmd);

        Agentite_CommandStats stats;
        agentite_command_get_stats(sys, &stats);

        REQUIRE(stats.total_invalid == 1);
    }

    SECTION("Reset stats") {
        Agentite_Command *cmd = agentite_command_new(CMD_ATTACK);
        agentite_command_execute(sys, cmd, nullptr);
        agentite_command_free(cmd);

        agentite_command_reset_stats(sys);

        Agentite_CommandStats stats;
        agentite_command_get_stats(sys, &stats);

        REQUIRE(stats.total_executed == 0);
        REQUIRE(stats.total_succeeded == 0);
    }

    agentite_command_destroy(sys);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_CASE("Command utility functions", "[command][utility]") {
    SECTION("Result OK check") {
        Agentite_CommandResult success = agentite_command_result_success(CMD_MOVE);
        Agentite_CommandResult failure = agentite_command_result_failure(CMD_MOVE, "Error");

        REQUIRE(agentite_command_result_ok(&success));
        REQUIRE_FALSE(agentite_command_result_ok(&failure));
        REQUIRE_FALSE(agentite_command_result_ok(nullptr));
    }

    SECTION("Result success") {
        Agentite_CommandResult result = agentite_command_result_success(CMD_ATTACK);
        REQUIRE(result.success);
        REQUIRE(result.command_type == CMD_ATTACK);
    }

    SECTION("Result failure") {
        Agentite_CommandResult result = agentite_command_result_failure(CMD_BUILD, "Not enough resources");
        REQUIRE_FALSE(result.success);
        REQUIRE(result.command_type == CMD_BUILD);
        REQUIRE(strcmp(result.error, "Not enough resources") == 0);
    }

    SECTION("Result failure with NULL error") {
        Agentite_CommandResult result = agentite_command_result_failure(CMD_BUILD, nullptr);
        REQUIRE_FALSE(result.success);
        REQUIRE(result.error[0] == '\0');
    }
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_CASE("Command edge cases", "[command][edge]") {
    Agentite_CommandSystem *sys = agentite_command_create();
    agentite_command_register(sys, CMD_MOVE, nullptr, execute_move);

    SECTION("Maximum parameters") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);

        // Fill all parameter slots
        for (int i = 0; i < AGENTITE_COMMAND_MAX_PARAMS; i++) {
            char key[32];
            snprintf(key, sizeof(key), "param_%d", i);
            agentite_command_set_int(cmd, key, i);
        }

        REQUIRE(cmd->param_count == AGENTITE_COMMAND_MAX_PARAMS);

        // Verify first and last
        REQUIRE(agentite_command_get_int(cmd, "param_0") == 0);
        char last_key[32];
        snprintf(last_key, sizeof(last_key), "param_%d", AGENTITE_COMMAND_MAX_PARAMS - 1);
        REQUIRE(agentite_command_get_int(cmd, last_key) == AGENTITE_COMMAND_MAX_PARAMS - 1);

        agentite_command_free(cmd);
    }

    SECTION("Long string truncation") {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);

        // Create a string longer than max key length
        char long_string[256];
        memset(long_string, 'A', sizeof(long_string) - 1);
        long_string[sizeof(long_string) - 1] = '\0';

        agentite_command_set_string(cmd, "test", long_string);

        const char *result = agentite_command_get_string(cmd, "test");
        REQUIRE(strlen(result) < sizeof(long_string));
        REQUIRE(strlen(result) == AGENTITE_COMMAND_MAX_PARAM_KEY - 1);

        agentite_command_free(cmd);
    }

    agentite_command_destroy(sys);
}
