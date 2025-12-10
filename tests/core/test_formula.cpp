/*
 * Carbon Formula Engine Tests
 *
 * Tests for the expression evaluation system.
 */

#include "catch_amalgamated.hpp"
#include "agentite/formula.h"
#include <cmath>

/* ============================================================================
 * Context Lifecycle Tests
 * ============================================================================ */

TEST_CASE("Formula context creation and destruction", "[formula][lifecycle]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();
    REQUIRE(ctx != nullptr);
    agentite_formula_destroy(ctx);
}

TEST_CASE("Formula context clone", "[formula][lifecycle]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();
    REQUIRE(ctx != nullptr);

    agentite_formula_set_var(ctx, "x", 42.0);
    agentite_formula_set_var(ctx, "y", 100.0);

    Agentite_FormulaContext *clone = agentite_formula_clone(ctx);
    REQUIRE(clone != nullptr);

    REQUIRE(agentite_formula_get_var(clone, "x") == 42.0);
    REQUIRE(agentite_formula_get_var(clone, "y") == 100.0);

    // Modifying clone shouldn't affect original
    agentite_formula_set_var(clone, "x", 999.0);
    REQUIRE(agentite_formula_get_var(ctx, "x") == 42.0);
    REQUIRE(agentite_formula_get_var(clone, "x") == 999.0);

    agentite_formula_destroy(clone);
    agentite_formula_destroy(ctx);
}

/* ============================================================================
 * Variable Management Tests
 * ============================================================================ */

TEST_CASE("Variable set and get", "[formula][variables]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();

    SECTION("Basic set/get") {
        REQUIRE(agentite_formula_set_var(ctx, "damage", 25.5));
        REQUIRE(agentite_formula_get_var(ctx, "damage") == 25.5);
    }

    SECTION("Update existing variable") {
        agentite_formula_set_var(ctx, "health", 100.0);
        REQUIRE(agentite_formula_get_var(ctx, "health") == 100.0);

        agentite_formula_set_var(ctx, "health", 75.0);
        REQUIRE(agentite_formula_get_var(ctx, "health") == 75.0);
    }

    SECTION("Get non-existent variable returns 0") {
        REQUIRE(agentite_formula_get_var(ctx, "nonexistent") == 0.0);
    }

    SECTION("Get with default") {
        REQUIRE(agentite_formula_get_var_or(ctx, "missing", -1.0) == -1.0);
        agentite_formula_set_var(ctx, "present", 42.0);
        REQUIRE(agentite_formula_get_var_or(ctx, "present", -1.0) == 42.0);
    }

    SECTION("Has variable") {
        REQUIRE_FALSE(agentite_formula_has_var(ctx, "x"));
        agentite_formula_set_var(ctx, "x", 1.0);
        REQUIRE(agentite_formula_has_var(ctx, "x"));
    }

    SECTION("Remove variable") {
        agentite_formula_set_var(ctx, "temp", 10.0);
        REQUIRE(agentite_formula_has_var(ctx, "temp"));
        REQUIRE(agentite_formula_remove_var(ctx, "temp"));
        REQUIRE_FALSE(agentite_formula_has_var(ctx, "temp"));
        REQUIRE_FALSE(agentite_formula_remove_var(ctx, "temp")); // Already removed
    }

    SECTION("Clear all variables") {
        agentite_formula_set_var(ctx, "a", 1.0);
        agentite_formula_set_var(ctx, "b", 2.0);
        agentite_formula_set_var(ctx, "c", 3.0);
        REQUIRE(agentite_formula_var_count(ctx) == 3);

        agentite_formula_clear_vars(ctx);
        REQUIRE(agentite_formula_var_count(ctx) == 0);
    }

    SECTION("Variable iteration") {
        agentite_formula_set_var(ctx, "alpha", 1.0);
        agentite_formula_set_var(ctx, "beta", 2.0);

        REQUIRE(agentite_formula_var_count(ctx) == 2);

        // Note: Order may not be guaranteed, so just check they exist
        bool found_alpha = false, found_beta = false;
        for (int i = 0; i < agentite_formula_var_count(ctx); i++) {
            const char *name = agentite_formula_var_name(ctx, i);
            double value = agentite_formula_var_value(ctx, i);
            if (strcmp(name, "alpha") == 0 && value == 1.0) found_alpha = true;
            if (strcmp(name, "beta") == 0 && value == 2.0) found_beta = true;
        }
        REQUIRE(found_alpha);
        REQUIRE(found_beta);
    }

    agentite_formula_destroy(ctx);
}

/* ============================================================================
 * Basic Arithmetic Tests
 * ============================================================================ */

TEST_CASE("Basic arithmetic operations", "[formula][arithmetic]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();

    SECTION("Addition") {
        REQUIRE(agentite_formula_eval(ctx, "2 + 3") == 5.0);
        REQUIRE(agentite_formula_eval(ctx, "10 + 20 + 30") == 60.0);
    }

    SECTION("Subtraction") {
        REQUIRE(agentite_formula_eval(ctx, "10 - 3") == 7.0);
        REQUIRE(agentite_formula_eval(ctx, "100 - 50 - 25") == 25.0);
    }

    SECTION("Multiplication") {
        REQUIRE(agentite_formula_eval(ctx, "4 * 5") == 20.0);
        REQUIRE(agentite_formula_eval(ctx, "2 * 3 * 4") == 24.0);
    }

    SECTION("Division") {
        REQUIRE(agentite_formula_eval(ctx, "20 / 4") == 5.0);
        REQUIRE(agentite_formula_eval(ctx, "100 / 10 / 2") == 5.0);
    }

    SECTION("Modulo") {
        REQUIRE(agentite_formula_eval(ctx, "17 % 5") == 2.0);
        REQUIRE(agentite_formula_eval(ctx, "10 % 3") == 1.0);
    }

    SECTION("Power") {
        REQUIRE(agentite_formula_eval(ctx, "2 ^ 3") == 8.0);
        REQUIRE(agentite_formula_eval(ctx, "10 ^ 2") == 100.0);
    }

    SECTION("Negative numbers") {
        REQUIRE(agentite_formula_eval(ctx, "-5") == -5.0);
        REQUIRE(agentite_formula_eval(ctx, "10 + -3") == 7.0);
        REQUIRE(agentite_formula_eval(ctx, "-2 * -3") == 6.0);
    }

    SECTION("Decimal numbers") {
        REQUIRE(agentite_formula_eval(ctx, "3.14 + 2.86") == 6.0);
        REQUIRE(agentite_formula_eval(ctx, "0.5 * 0.5") == 0.25);
    }

    agentite_formula_destroy(ctx);
}

/* ============================================================================
 * Operator Precedence Tests
 * ============================================================================ */

TEST_CASE("Operator precedence", "[formula][precedence]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();

    SECTION("Multiplication before addition") {
        REQUIRE(agentite_formula_eval(ctx, "2 + 3 * 4") == 14.0);
        REQUIRE(agentite_formula_eval(ctx, "3 * 4 + 2") == 14.0);
    }

    SECTION("Division before subtraction") {
        REQUIRE(agentite_formula_eval(ctx, "10 - 6 / 2") == 7.0);
    }

    SECTION("Power before multiplication") {
        REQUIRE(agentite_formula_eval(ctx, "2 * 3 ^ 2") == 18.0);
    }

    SECTION("Parentheses override precedence") {
        REQUIRE(agentite_formula_eval(ctx, "(2 + 3) * 4") == 20.0);
        REQUIRE(agentite_formula_eval(ctx, "2 * (3 + 4)") == 14.0);
        REQUIRE(agentite_formula_eval(ctx, "((2 + 3) * (4 + 5))") == 45.0);
    }

    agentite_formula_destroy(ctx);
}

/* ============================================================================
 * Comparison and Logical Operator Tests
 * ============================================================================ */

TEST_CASE("Comparison operators", "[formula][comparison]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();

    SECTION("Equal") {
        REQUIRE(agentite_formula_eval(ctx, "5 == 5") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "5 == 6") == 0.0);
    }

    SECTION("Not equal") {
        REQUIRE(agentite_formula_eval(ctx, "5 != 6") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "5 != 5") == 0.0);
    }

    SECTION("Less than") {
        REQUIRE(agentite_formula_eval(ctx, "3 < 5") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "5 < 3") == 0.0);
        REQUIRE(agentite_formula_eval(ctx, "5 < 5") == 0.0);
    }

    SECTION("Less than or equal") {
        REQUIRE(agentite_formula_eval(ctx, "3 <= 5") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "5 <= 5") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "6 <= 5") == 0.0);
    }

    SECTION("Greater than") {
        REQUIRE(agentite_formula_eval(ctx, "5 > 3") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "3 > 5") == 0.0);
    }

    SECTION("Greater than or equal") {
        REQUIRE(agentite_formula_eval(ctx, "5 >= 3") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "5 >= 5") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "3 >= 5") == 0.0);
    }

    agentite_formula_destroy(ctx);
}

TEST_CASE("Logical operators", "[formula][logical]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();

    SECTION("And") {
        REQUIRE(agentite_formula_eval(ctx, "1 && 1") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "1 && 0") == 0.0);
        REQUIRE(agentite_formula_eval(ctx, "0 && 1") == 0.0);
        REQUIRE(agentite_formula_eval(ctx, "0 && 0") == 0.0);
    }

    SECTION("Or") {
        REQUIRE(agentite_formula_eval(ctx, "1 || 1") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "1 || 0") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "0 || 1") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "0 || 0") == 0.0);
    }

    SECTION("Not") {
        REQUIRE(agentite_formula_eval(ctx, "!0") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "!1") == 0.0);
        REQUIRE(agentite_formula_eval(ctx, "!5") == 0.0);  // Non-zero is truthy
    }

    SECTION("Combined logical") {
        REQUIRE(agentite_formula_eval(ctx, "(5 > 3) && (2 < 4)") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "(5 > 3) && (2 > 4)") == 0.0);
        REQUIRE(agentite_formula_eval(ctx, "(5 < 3) || (2 < 4)") == 1.0);
    }

    agentite_formula_destroy(ctx);
}

/* ============================================================================
 * Ternary Operator Tests
 * ============================================================================ */

TEST_CASE("Ternary operator", "[formula][ternary]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();

    SECTION("Basic ternary") {
        REQUIRE(agentite_formula_eval(ctx, "1 ? 10 : 20") == 10.0);
        REQUIRE(agentite_formula_eval(ctx, "0 ? 10 : 20") == 20.0);
    }

    SECTION("Ternary with comparison") {
        agentite_formula_set_var(ctx, "health", 30.0);
        REQUIRE(agentite_formula_eval(ctx, "health < 50 ? 1 : 0") == 1.0);

        agentite_formula_set_var(ctx, "health", 80.0);
        REQUIRE(agentite_formula_eval(ctx, "health < 50 ? 1 : 0") == 0.0);
    }

    SECTION("Nested ternary") {
        agentite_formula_set_var(ctx, "x", 5.0);
        // x < 3 ? 1 : (x < 7 ? 2 : 3)  => x=5 should give 2
        REQUIRE(agentite_formula_eval(ctx, "x < 3 ? 1 : (x < 7 ? 2 : 3)") == 2.0);
    }

    agentite_formula_destroy(ctx);
}

/* ============================================================================
 * Built-in Function Tests
 * ============================================================================ */

TEST_CASE("Built-in math functions", "[formula][functions]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();

    SECTION("min/max") {
        REQUIRE(agentite_formula_eval(ctx, "min(5, 3)") == 3.0);
        REQUIRE(agentite_formula_eval(ctx, "max(5, 3)") == 5.0);
        REQUIRE(agentite_formula_eval(ctx, "min(1, 2, 3, 4, 5)") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "max(1, 2, 3, 4, 5)") == 5.0);
    }

    SECTION("clamp") {
        REQUIRE(agentite_formula_eval(ctx, "clamp(5, 0, 10)") == 5.0);
        REQUIRE(agentite_formula_eval(ctx, "clamp(-5, 0, 10)") == 0.0);
        REQUIRE(agentite_formula_eval(ctx, "clamp(15, 0, 10)") == 10.0);
    }

    SECTION("floor/ceil/round") {
        REQUIRE(agentite_formula_eval(ctx, "floor(3.7)") == 3.0);
        REQUIRE(agentite_formula_eval(ctx, "ceil(3.2)") == 4.0);
        REQUIRE(agentite_formula_eval(ctx, "round(3.5)") == 4.0);
        REQUIRE(agentite_formula_eval(ctx, "round(3.4)") == 3.0);
    }

    SECTION("abs") {
        REQUIRE(agentite_formula_eval(ctx, "abs(-5)") == 5.0);
        REQUIRE(agentite_formula_eval(ctx, "abs(5)") == 5.0);
    }

    SECTION("sqrt") {
        REQUIRE(agentite_formula_eval(ctx, "sqrt(16)") == 4.0);
        REQUIRE(agentite_formula_eval(ctx, "sqrt(2)") == Catch::Approx(1.41421356).margin(0.0001));
    }

    SECTION("pow") {
        REQUIRE(agentite_formula_eval(ctx, "pow(2, 8)") == 256.0);
        REQUIRE(agentite_formula_eval(ctx, "pow(10, 3)") == 1000.0);
    }

    SECTION("log/exp") {
        REQUIRE(agentite_formula_eval(ctx, "log(1)") == 0.0);
        REQUIRE(agentite_formula_eval(ctx, "exp(0)") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "log(exp(5))") == Catch::Approx(5.0).margin(0.0001));
    }

    SECTION("lerp") {
        REQUIRE(agentite_formula_eval(ctx, "lerp(0, 100, 0.5)") == 50.0);
        REQUIRE(agentite_formula_eval(ctx, "lerp(0, 100, 0)") == 0.0);
        REQUIRE(agentite_formula_eval(ctx, "lerp(0, 100, 1)") == 100.0);
        REQUIRE(agentite_formula_eval(ctx, "lerp(10, 20, 0.25)") == 12.5);
    }

    SECTION("trigonometry") {
        REQUIRE(agentite_formula_eval(ctx, "sin(0)") == 0.0);
        REQUIRE(agentite_formula_eval(ctx, "cos(0)") == 1.0);
        REQUIRE(agentite_formula_eval(ctx, "tan(0)") == 0.0);
    }

    agentite_formula_destroy(ctx);
}

/* ============================================================================
 * Variable Substitution Tests
 * ============================================================================ */

TEST_CASE("Variable substitution in expressions", "[formula][variables]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();

    agentite_formula_set_var(ctx, "base_damage", 10.0);
    agentite_formula_set_var(ctx, "strength", 15.0);
    agentite_formula_set_var(ctx, "level", 5.0);

    SECTION("Simple variable") {
        REQUIRE(agentite_formula_eval(ctx, "base_damage") == 10.0);
    }

    SECTION("Variable in expression") {
        REQUIRE(agentite_formula_eval(ctx, "base_damage + 5") == 15.0);
    }

    SECTION("Multiple variables") {
        REQUIRE(agentite_formula_eval(ctx, "base_damage + strength") == 25.0);
    }

    SECTION("Complex game formula") {
        double result = agentite_formula_eval(ctx, "base_damage + strength * 0.5 + level * 2");
        REQUIRE(result == Catch::Approx(27.5));  // 10 + 7.5 + 10
    }

    SECTION("Variable with function") {
        REQUIRE(agentite_formula_eval(ctx, "max(base_damage, strength)") == 15.0);
    }

    agentite_formula_destroy(ctx);
}

/* ============================================================================
 * Compiled Formula Tests
 * ============================================================================ */

TEST_CASE("Compiled formula execution", "[formula][compiled]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();

    SECTION("Basic compile and execute") {
        Agentite_Formula *f = agentite_formula_compile(ctx, "2 + 3 * 4");
        REQUIRE(f != nullptr);
        REQUIRE(agentite_formula_exec(f, ctx) == 14.0);
        agentite_formula_free(f);
    }

    SECTION("Compiled with variables") {
        agentite_formula_set_var(ctx, "x", 10.0);
        Agentite_Formula *f = agentite_formula_compile(ctx, "x * 2 + 5");
        REQUIRE(f != nullptr);

        REQUIRE(agentite_formula_exec(f, ctx) == 25.0);

        // Change variable and re-execute
        agentite_formula_set_var(ctx, "x", 20.0);
        REQUIRE(agentite_formula_exec(f, ctx) == 45.0);

        agentite_formula_free(f);
    }

    SECTION("Get expression from compiled") {
        Agentite_Formula *f = agentite_formula_compile(ctx, "a + b");
        REQUIRE(f != nullptr);
        REQUIRE(strcmp(agentite_formula_get_expr(f), "a + b") == 0);
        agentite_formula_free(f);
    }

    SECTION("Get variables from compiled") {
        agentite_formula_set_var(ctx, "health", 100.0);
        agentite_formula_set_var(ctx, "max_health", 100.0);

        Agentite_Formula *f = agentite_formula_compile(ctx, "health / max_health");
        REQUIRE(f != nullptr);

        const char *vars[10];
        int count = agentite_formula_get_vars(f, vars, 10);
        REQUIRE(count == 2);

        agentite_formula_free(f);
    }

    agentite_formula_destroy(ctx);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_CASE("Error handling", "[formula][errors]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();

    SECTION("Syntax error - double operator") {
        // Note: "2 + + 3" is actually VALID (unary + on 3), so use truly invalid syntax
        double result = agentite_formula_eval(ctx, "2 + * 3");
        REQUIRE(agentite_formula_is_nan(result));
        REQUIRE(agentite_formula_has_error(ctx));
    }

    SECTION("Unbalanced parentheses") {
        double result = agentite_formula_eval(ctx, "(2 + 3");
        REQUIRE(agentite_formula_is_nan(result));
        REQUIRE(agentite_formula_has_error(ctx));
    }

    SECTION("Unknown function") {
        double result = agentite_formula_eval(ctx, "unknown_func(5)");
        REQUIRE(agentite_formula_is_nan(result));
        REQUIRE(agentite_formula_has_error(ctx));
    }

    SECTION("Division by zero returns NaN with error") {
        double result = agentite_formula_eval(ctx, "10 / 0");
        // The formula parser explicitly catches division by zero and returns NaN
        REQUIRE(agentite_formula_is_nan(result));
        REQUIRE(agentite_formula_has_error(ctx));
    }

    SECTION("Clear error") {
        agentite_formula_eval(ctx, "2 + * 3");  // Invalid syntax
        REQUIRE(agentite_formula_has_error(ctx));

        agentite_formula_clear_error(ctx);
        REQUIRE_FALSE(agentite_formula_has_error(ctx));
    }

    SECTION("Valid expression check") {
        REQUIRE(agentite_formula_valid(ctx, "2 + 3"));
        REQUIRE(agentite_formula_valid(ctx, "2 + + 3"));  // Valid: unary + on 3
        REQUIRE_FALSE(agentite_formula_valid(ctx, "2 + * 3"));  // Invalid: * needs operand
        REQUIRE_FALSE(agentite_formula_valid(ctx, "(2 + 3"));   // Unbalanced
    }

    agentite_formula_destroy(ctx);
}

/* ============================================================================
 * Recursion Depth Tests
 * ============================================================================ */

TEST_CASE("Recursion depth limit", "[formula][security]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();

    SECTION("Deeply nested parentheses") {
        // Build expression with 100 nested parentheses
        std::string expr = "";
        for (int i = 0; i < 100; i++) expr += "(";
        expr += "1";
        for (int i = 0; i < 100; i++) expr += ")";

        double result = agentite_formula_eval(ctx, expr.c_str());
        REQUIRE(agentite_formula_is_nan(result));
        REQUIRE(agentite_formula_has_error(ctx));

        const char *error = agentite_formula_get_error(ctx);
        REQUIRE(strstr(error, "deeply nested") != nullptr);
    }

    agentite_formula_destroy(ctx);
}

/* ============================================================================
 * Custom Function Tests
 * ============================================================================ */

static double custom_double(const double *args, int argc, void *userdata) {
    (void)argc;
    (void)userdata;
    return args[0] * 2;
}

static double custom_sum(const double *args, int argc, void *userdata) {
    (void)userdata;
    double sum = 0;
    for (int i = 0; i < argc; i++) {
        sum += args[i];
    }
    return sum;
}

TEST_CASE("Custom functions", "[formula][custom]") {
    Agentite_FormulaContext *ctx = agentite_formula_create();

    SECTION("Register and call custom function") {
        REQUIRE(agentite_formula_register_func(ctx, "double", custom_double, 1, 1, nullptr));
        REQUIRE(agentite_formula_eval(ctx, "double(5)") == 10.0);
    }

    SECTION("Variadic custom function") {
        REQUIRE(agentite_formula_register_func(ctx, "sum", custom_sum, 1, -1, nullptr));
        REQUIRE(agentite_formula_eval(ctx, "sum(1, 2, 3, 4, 5)") == 15.0);
    }

    SECTION("Unregister custom function") {
        agentite_formula_register_func(ctx, "myfunc", custom_double, 1, 1, nullptr);
        REQUIRE(agentite_formula_eval(ctx, "myfunc(5)") == 10.0);

        REQUIRE(agentite_formula_unregister_func(ctx, "myfunc"));

        double result = agentite_formula_eval(ctx, "myfunc(5)");
        REQUIRE(agentite_formula_is_nan(result));
    }

    agentite_formula_destroy(ctx);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_CASE("Utility functions", "[formula][utility]") {
    SECTION("eval_simple convenience function") {
        double result = agentite_formula_eval_simple("x + y * 2", "x", 10.0, "y", 5.0, NULL);
        REQUIRE(result == 20.0);  // 10 + 5*2
    }

    SECTION("Format function") {
        char buf[32];
        agentite_formula_format(3.14159, buf, sizeof(buf), 2);
        REQUIRE(strcmp(buf, "3.14") == 0);

        agentite_formula_format(42.0, buf, sizeof(buf), 0);
        REQUIRE(strcmp(buf, "42") == 0);
    }

    SECTION("Constants") {
        Agentite_FormulaContext *ctx = agentite_formula_create();
        agentite_formula_set_constants(ctx);

        REQUIRE(agentite_formula_get_var(ctx, "pi") == Catch::Approx(3.14159265).margin(0.0001));
        REQUIRE(agentite_formula_get_var(ctx, "e") == Catch::Approx(2.71828182).margin(0.0001));
        REQUIRE(agentite_formula_has_var(ctx, "tau"));
        REQUIRE(agentite_formula_has_var(ctx, "phi"));

        agentite_formula_destroy(ctx);
    }
}
