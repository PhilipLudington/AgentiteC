/*
 * Carbon Path Validation Tests
 *
 * Tests for path validation functions that prevent directory traversal attacks.
 */

#include "catch_amalgamated.hpp"
#include "agentite/path.h"
#include <cstring>

/* ============================================================================
 * Component Validation Tests
 * ============================================================================ */

TEST_CASE("Path component validation", "[path][component]") {
    SECTION("Valid components") {
        REQUIRE(agentite_path_component_is_safe("file.txt", 0));
        REQUIRE(agentite_path_component_is_safe("image.png", 0));
        REQUIRE(agentite_path_component_is_safe("data", 0));
        REQUIRE(agentite_path_component_is_safe("a", 0));
        REQUIRE(agentite_path_component_is_safe("123", 0));
        REQUIRE(agentite_path_component_is_safe("file_name", 0));
        REQUIRE(agentite_path_component_is_safe("file-name", 0));
        REQUIRE(agentite_path_component_is_safe(".hidden", 0));
    }

    SECTION("Invalid components - NULL and empty") {
        REQUIRE_FALSE(agentite_path_component_is_safe(nullptr, 0));
        REQUIRE_FALSE(agentite_path_component_is_safe("", 0));
    }

    SECTION("Invalid components - parent reference") {
        REQUIRE_FALSE(agentite_path_component_is_safe("..", 0));
    }

    SECTION("Invalid components - path separators") {
        REQUIRE_FALSE(agentite_path_component_is_safe("dir/file", 0));
        REQUIRE_FALSE(agentite_path_component_is_safe("dir\\file", 0));
        REQUIRE_FALSE(agentite_path_component_is_safe("/file", 0));
        REQUIRE_FALSE(agentite_path_component_is_safe("file/", 0));
    }

    SECTION("Length limits") {
        REQUIRE(agentite_path_component_is_safe("abc", 5));
        REQUIRE(agentite_path_component_is_safe("abcde", 5));
        REQUIRE_FALSE(agentite_path_component_is_safe("abcdef", 5));
    }
}

/* ============================================================================
 * Path Safety Tests
 * ============================================================================ */

TEST_CASE("Path safety validation", "[path][safety]") {
    SECTION("Valid relative paths") {
        REQUIRE(agentite_path_is_safe("file.txt"));
        REQUIRE(agentite_path_is_safe("assets/textures/player.png"));
        REQUIRE(agentite_path_is_safe("data/sounds/sfx.wav"));
        REQUIRE(agentite_path_is_safe("fonts/default.ttf"));
        REQUIRE(agentite_path_is_safe("./file.txt"));
        REQUIRE(agentite_path_is_safe("a/b/c/d/e/f.txt"));
    }

    SECTION("Invalid paths - NULL and empty") {
        REQUIRE_FALSE(agentite_path_is_safe(nullptr));
        REQUIRE_FALSE(agentite_path_is_safe(""));
    }

    SECTION("Invalid paths - parent directory traversal") {
        REQUIRE_FALSE(agentite_path_is_safe(".."));
        REQUIRE_FALSE(agentite_path_is_safe("../file.txt"));
        REQUIRE_FALSE(agentite_path_is_safe("assets/../file.txt"));
        REQUIRE_FALSE(agentite_path_is_safe("assets/textures/../../file.txt"));
        REQUIRE_FALSE(agentite_path_is_safe("a/b/../../../c.txt"));
        REQUIRE_FALSE(agentite_path_is_safe("..\\file.txt"));
    }

    SECTION("Invalid paths - absolute paths") {
        REQUIRE_FALSE(agentite_path_is_safe("/etc/passwd"));
        REQUIRE_FALSE(agentite_path_is_safe("/home/user/file.txt"));
#ifdef _WIN32
        REQUIRE_FALSE(agentite_path_is_safe("C:\\Windows\\System32"));
        REQUIRE_FALSE(agentite_path_is_safe("D:\\data\\file.txt"));
        REQUIRE_FALSE(agentite_path_is_safe("\\\\server\\share"));
#endif
    }

    SECTION("Edge cases - similar to but not parent references") {
        REQUIRE(agentite_path_is_safe("..."));
        REQUIRE(agentite_path_is_safe("...."));
        REQUIRE(agentite_path_is_safe("..a"));
        REQUIRE(agentite_path_is_safe("a.."));
        REQUIRE(agentite_path_is_safe("a..b"));
    }
}

/* ============================================================================
 * Path Normalization Tests
 * ============================================================================ */

TEST_CASE("Path normalization", "[path][normalize]") {
    char out[256];

    SECTION("Simple paths unchanged") {
        REQUIRE(agentite_path_normalize("file.txt", out, sizeof(out)));
        REQUIRE(strcmp(out, "file.txt") == 0);

        REQUIRE(agentite_path_normalize("a/b/c", out, sizeof(out)));
        REQUIRE(strcmp(out, "a/b/c") == 0);
    }

    SECTION("Collapse multiple separators") {
        REQUIRE(agentite_path_normalize("a//b", out, sizeof(out)));
        REQUIRE(strcmp(out, "a/b") == 0);

        REQUIRE(agentite_path_normalize("a///b////c", out, sizeof(out)));
        REQUIRE(strcmp(out, "a/b/c") == 0);
    }

    SECTION("Skip current directory components") {
        REQUIRE(agentite_path_normalize("./file.txt", out, sizeof(out)));
        REQUIRE(strcmp(out, "file.txt") == 0);

        REQUIRE(agentite_path_normalize("a/./b/./c", out, sizeof(out)));
        REQUIRE(strcmp(out, "a/b/c") == 0);
    }

    SECTION("Normalize backslashes to forward slashes") {
        REQUIRE(agentite_path_normalize("a\\b\\c", out, sizeof(out)));
        REQUIRE(strcmp(out, "a/b/c") == 0);
    }

    SECTION("Buffer too small") {
        char small[4];
        REQUIRE_FALSE(agentite_path_normalize("a/b/c/d/e/f", small, sizeof(small)));
    }

    SECTION("Invalid inputs") {
        REQUIRE_FALSE(agentite_path_normalize(nullptr, out, sizeof(out)));
        REQUIRE_FALSE(agentite_path_normalize("test", nullptr, sizeof(out)));
        REQUIRE_FALSE(agentite_path_normalize("test", out, 0));
    }
}

/* ============================================================================
 * Path Join Tests
 * ============================================================================ */

TEST_CASE("Path join", "[path][join]") {
    char out[256];

    SECTION("Join two components") {
        REQUIRE(agentite_path_join("assets", "file.txt", out, sizeof(out)));
        REQUIRE(strcmp(out, "assets/file.txt") == 0);
    }

    SECTION("Join with nested path") {
        REQUIRE(agentite_path_join("assets/textures", "player.png", out, sizeof(out)));
        REQUIRE(strcmp(out, "assets/textures/player.png") == 0);
    }

    SECTION("Base with trailing separator") {
        REQUIRE(agentite_path_join("assets/", "file.txt", out, sizeof(out)));
        REQUIRE(strcmp(out, "assets/file.txt") == 0);
    }

    SECTION("Empty base") {
        REQUIRE(agentite_path_join("", "file.txt", out, sizeof(out)));
        REQUIRE(strcmp(out, "file.txt") == 0);

        REQUIRE(agentite_path_join(nullptr, "file.txt", out, sizeof(out)));
        REQUIRE(strcmp(out, "file.txt") == 0);
    }

    SECTION("Reject unsafe name") {
        REQUIRE_FALSE(agentite_path_join("assets", "../etc/passwd", out, sizeof(out)));
        REQUIRE_FALSE(agentite_path_join("assets", "..", out, sizeof(out)));
    }

    SECTION("Buffer too small") {
        char small[10];
        REQUIRE_FALSE(agentite_path_join("very/long/base/path", "file.txt", small, sizeof(small)));
    }
}

/* ============================================================================
 * Path Utility Tests
 * ============================================================================ */

TEST_CASE("Path utility functions", "[path][util]") {
    SECTION("Is absolute") {
        REQUIRE(agentite_path_is_absolute("/usr/bin"));
        REQUIRE(agentite_path_is_absolute("/"));
        REQUIRE_FALSE(agentite_path_is_absolute("relative/path"));
        REQUIRE_FALSE(agentite_path_is_absolute("./path"));
        REQUIRE_FALSE(agentite_path_is_absolute(nullptr));
        REQUIRE_FALSE(agentite_path_is_absolute(""));

#ifdef _WIN32
        REQUIRE(agentite_path_is_absolute("C:\\Windows"));
        REQUIRE(agentite_path_is_absolute("D:\\"));
        REQUIRE(agentite_path_is_absolute("\\\\server\\share"));
#endif
    }

    SECTION("Filename extraction") {
        REQUIRE(strcmp(agentite_path_filename("file.txt"), "file.txt") == 0);
        REQUIRE(strcmp(agentite_path_filename("a/b/file.txt"), "file.txt") == 0);
        REQUIRE(strcmp(agentite_path_filename("/abs/path/file.txt"), "file.txt") == 0);
        REQUIRE(strcmp(agentite_path_filename("a\\b\\file.txt"), "file.txt") == 0);
        REQUIRE(agentite_path_filename(nullptr) == nullptr);
    }

    SECTION("Directory extraction") {
        char out[256];

        REQUIRE(agentite_path_dirname("a/b/file.txt", out, sizeof(out)));
        REQUIRE(strcmp(out, "a/b") == 0);

        REQUIRE(agentite_path_dirname("/file.txt", out, sizeof(out)));
        REQUIRE(strcmp(out, "/") == 0);

        REQUIRE(agentite_path_dirname("file.txt", out, sizeof(out)));
        REQUIRE(strcmp(out, ".") == 0);

        REQUIRE_FALSE(agentite_path_dirname(nullptr, out, sizeof(out)));
    }
}

/* ============================================================================
 * Security Scenario Tests
 * ============================================================================ */

TEST_CASE("Path security scenarios", "[path][security]") {
    SECTION("Common traversal attack patterns") {
        REQUIRE_FALSE(agentite_path_is_safe("../../../etc/passwd"));
        REQUIRE_FALSE(agentite_path_is_safe("..\\..\\..\\windows\\system32\\config\\sam"));
        REQUIRE_FALSE(agentite_path_is_safe("assets/textures/../../../etc/passwd"));
        /* Note: "....//....//etc" is actually safe - "...." is a valid filename */
        REQUIRE(agentite_path_is_safe("....//....//....//etc/passwd"));
    }

    SECTION("URL-encoded attacks (paths should already be decoded)") {
        /* Note: If paths come URL-encoded, they should be decoded before validation.
         * These tests verify that if the encoded form somehow appears, it's still rejected
         * or at least doesn't enable traversal. */
        REQUIRE(agentite_path_is_safe("%2e%2e"));  /* This is a literal filename "%2e%2e" */
    }

    SECTION("Null byte injection (C strings terminate at null)") {
        /* In C, strings are null-terminated, so "file.txt\0../etc/passwd"
         * would just be seen as "file.txt" - safe by design */
        REQUIRE(agentite_path_is_safe("file.txt"));
    }

    SECTION("Mixed separator attacks") {
        REQUIRE_FALSE(agentite_path_is_safe("..\\..\\file.txt"));
        REQUIRE_FALSE(agentite_path_is_safe("assets\\..\\..\\file.txt"));
        REQUIRE_FALSE(agentite_path_is_safe("a/b\\../c/../../../d.txt"));
    }
}
