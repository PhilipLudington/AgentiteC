/*
 * Agentite Replay System Tests
 *
 * Tests for the replay system including recording, playback,
 * file I/O, seeking, and state management.
 */

#include "catch_amalgamated.hpp"
#include "agentite/replay.h"
#include "agentite/command.h"
#include "agentite/error.h"
#include <cstring>
#include <cstdio>

/* ============================================================================
 * Test Command Types
 * ============================================================================ */

enum TestCommandType {
    CMD_MOVE = 1,
    CMD_ATTACK = 2,
    CMD_BUILD = 3,
};

/* ============================================================================
 * Test Game State
 * ============================================================================ */

struct TestGameState {
    int player_x;
    int player_y;
    int health;
    int move_count;
};

/* ============================================================================
 * Test Callbacks
 * ============================================================================ */

static bool test_serialize(void *game_state, void **out_data, size_t *out_size) {
    if (!game_state || !out_data || !out_size) return false;

    TestGameState *state = static_cast<TestGameState *>(game_state);
    TestGameState *copy = static_cast<TestGameState *>(malloc(sizeof(TestGameState)));
    if (!copy) return false;

    *copy = *state;
    *out_data = copy;
    *out_size = sizeof(TestGameState);
    return true;
}

static bool test_deserialize(void *game_state, const void *data, size_t size) {
    if (!game_state || !data || size != sizeof(TestGameState)) return false;

    TestGameState *state = static_cast<TestGameState *>(game_state);
    const TestGameState *saved = static_cast<const TestGameState *>(data);
    *state = *saved;
    return true;
}

static bool test_reset(void *game_state, const Agentite_ReplayMetadata *metadata) {
    if (!game_state) return false;
    (void)metadata;

    TestGameState *state = static_cast<TestGameState *>(game_state);
    state->player_x = 0;
    state->player_y = 0;
    state->health = 100;
    state->move_count = 0;
    return true;
}

/* ============================================================================
 * Test Validators and Executors
 * ============================================================================ */

static bool validate_move(const Agentite_Command *cmd, void *game_state,
                          char *error_buf, size_t error_size) {
    (void)game_state;

    int x = agentite_command_get_int(cmd, "x");
    int y = agentite_command_get_int(cmd, "y");

    if (x < 0 || y < 0) {
        snprintf(error_buf, error_size, "Invalid coordinates");
        return false;
    }
    return true;
}

static bool execute_move(const Agentite_Command *cmd, void *game_state) {
    TestGameState *state = static_cast<TestGameState *>(game_state);
    if (state) {
        state->player_x = agentite_command_get_int(cmd, "x");
        state->player_y = agentite_command_get_int(cmd, "y");
        state->move_count++;
    }
    return true;
}

static bool execute_attack(const Agentite_Command *cmd, void *game_state) {
    (void)cmd;
    TestGameState *state = static_cast<TestGameState *>(game_state);
    if (state) {
        state->health -= 10;
    }
    return true;
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_CASE("Replay system creation and destruction", "[replay][lifecycle]") {
    Agentite_ReplaySystem *replay = agentite_replay_create(nullptr);
    REQUIRE(replay != nullptr);
    agentite_replay_destroy(replay);
}

TEST_CASE("Replay system creation with config", "[replay][lifecycle]") {
    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    config.snapshot_interval = 100;
    config.compress = false;

    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    REQUIRE(replay != nullptr);
    agentite_replay_destroy(replay);
}

TEST_CASE("Destroy NULL replay system", "[replay][lifecycle]") {
    agentite_replay_destroy(nullptr);
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

TEST_CASE("Initial replay state", "[replay][state]") {
    Agentite_ReplaySystem *replay = agentite_replay_create(nullptr);
    REQUIRE(replay != nullptr);

    REQUIRE(agentite_replay_get_state(replay) == AGENTITE_REPLAY_IDLE);
    REQUIRE_FALSE(agentite_replay_is_recording(replay));
    REQUIRE_FALSE(agentite_replay_is_playing(replay));
    REQUIRE_FALSE(agentite_replay_is_paused(replay));
    REQUIRE(agentite_replay_get_current_frame(replay) == 0);
    REQUIRE(agentite_replay_get_total_frames(replay) == 0);
    REQUIRE_FALSE(agentite_replay_has_data(replay));

    agentite_replay_destroy(replay);
}

TEST_CASE("Speed control", "[replay][speed]") {
    Agentite_ReplaySystem *replay = agentite_replay_create(nullptr);
    REQUIRE(replay != nullptr);

    REQUIRE(agentite_replay_get_speed(replay) == 1.0f);

    agentite_replay_set_speed(replay, 2.0f);
    REQUIRE(agentite_replay_get_speed(replay) == 2.0f);

    agentite_replay_set_speed(replay, 0.5f);
    REQUIRE(agentite_replay_get_speed(replay) == 0.5f);

    /* Clamp to min */
    agentite_replay_set_speed(replay, 0.01f);
    REQUIRE(agentite_replay_get_speed(replay) >= 0.1f);

    /* Clamp to max */
    agentite_replay_set_speed(replay, 100.0f);
    REQUIRE(agentite_replay_get_speed(replay) <= 16.0f);

    agentite_replay_destroy(replay);
}

/* ============================================================================
 * Recording Tests
 * ============================================================================ */

TEST_CASE("Basic recording", "[replay][recording]") {
    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    config.serialize = test_serialize;
    config.deserialize = test_deserialize;
    config.reset = test_reset;

    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    REQUIRE(replay != nullptr);

    Agentite_CommandSystem *cmd_sys = agentite_command_create();
    REQUIRE(cmd_sys != nullptr);
    agentite_command_register(cmd_sys, CMD_MOVE, validate_move, execute_move);

    TestGameState game_state = {0, 0, 100, 0};

    /* Start recording */
    Agentite_ReplayMetadata meta = {};
    strncpy(meta.map_name, "TestMap", sizeof(meta.map_name) - 1);
    strncpy(meta.game_version, "1.0.0", sizeof(meta.game_version) - 1);

    REQUIRE(agentite_replay_start_recording(replay, cmd_sys, &game_state, &meta));
    REQUIRE(agentite_replay_is_recording(replay));
    REQUIRE(agentite_replay_get_state(replay) == AGENTITE_REPLAY_RECORDING);

    /* Record some frames */
    for (int i = 0; i < 10; i++) {
        agentite_replay_record_frame(replay, 0.016f);
    }

    /* Stop recording */
    agentite_replay_stop_recording(replay);
    REQUIRE_FALSE(agentite_replay_is_recording(replay));
    REQUIRE(agentite_replay_get_state(replay) == AGENTITE_REPLAY_IDLE);
    REQUIRE(agentite_replay_get_total_frames(replay) == 10);

    /* Check metadata */
    const Agentite_ReplayMetadata *stored_meta = agentite_replay_get_metadata(replay);
    REQUIRE(stored_meta != nullptr);
    REQUIRE(strcmp(stored_meta->map_name, "TestMap") == 0);
    REQUIRE(stored_meta->total_frames == 10);

    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

TEST_CASE("Recording with commands", "[replay][recording]") {
    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    config.serialize = test_serialize;
    config.deserialize = test_deserialize;
    config.reset = test_reset;

    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    Agentite_CommandSystem *cmd_sys = agentite_command_create();
    agentite_command_register(cmd_sys, CMD_MOVE, validate_move, execute_move);

    TestGameState game_state = {0, 0, 100, 0};

    Agentite_ReplayMetadata meta = {};
    REQUIRE(agentite_replay_start_recording(replay, cmd_sys, &game_state, &meta));

    /* Execute some commands while recording */
    for (int i = 0; i < 5; i++) {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", i * 10);
        agentite_command_set_int(cmd, "y", i * 5);
        agentite_command_execute(cmd_sys, cmd, &game_state);
        agentite_command_free(cmd);

        agentite_replay_record_frame(replay, 0.016f);
    }

    agentite_replay_stop_recording(replay);

    REQUIRE(agentite_replay_get_total_frames(replay) == 5);
    REQUIRE(agentite_replay_has_data(replay));

    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

TEST_CASE("Cannot start recording while already recording", "[replay][recording]") {
    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    Agentite_CommandSystem *cmd_sys = agentite_command_create();
    agentite_command_register(cmd_sys, CMD_MOVE, nullptr, execute_move);

    Agentite_ReplayMetadata meta = {};
    REQUIRE(agentite_replay_start_recording(replay, cmd_sys, nullptr, &meta));
    REQUIRE_FALSE(agentite_replay_start_recording(replay, cmd_sys, nullptr, &meta));

    agentite_replay_stop_recording(replay);
    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

/* ============================================================================
 * File I/O Tests
 * ============================================================================ */

TEST_CASE("Save and load replay", "[replay][file]") {
    const char *test_file = "/tmp/test_replay.replay";

    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    config.serialize = test_serialize;
    config.deserialize = test_deserialize;
    config.reset = test_reset;
    config.compress = false;

    /* Record a replay */
    {
        Agentite_ReplaySystem *replay = agentite_replay_create(&config);
        Agentite_CommandSystem *cmd_sys = agentite_command_create();
        agentite_command_register(cmd_sys, CMD_MOVE, validate_move, execute_move);

        TestGameState game_state = {10, 20, 100, 0};

        Agentite_ReplayMetadata meta = {};
        strncpy(meta.map_name, "SaveTest", sizeof(meta.map_name) - 1);
        meta.random_seed = 12345;

        REQUIRE(agentite_replay_start_recording(replay, cmd_sys, &game_state, &meta));

        for (int i = 0; i < 5; i++) {
            Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
            agentite_command_set_int(cmd, "x", i);
            agentite_command_set_int(cmd, "y", i * 2);
            agentite_command_execute(cmd_sys, cmd, &game_state);
            agentite_command_free(cmd);
            agentite_replay_record_frame(replay, 0.016f);
        }

        agentite_replay_stop_recording(replay);

        REQUIRE(agentite_replay_save(replay, test_file));

        agentite_command_destroy(cmd_sys);
        agentite_replay_destroy(replay);
    }

    /* Load the replay */
    {
        Agentite_ReplaySystem *replay = agentite_replay_create(&config);

        REQUIRE(agentite_replay_load(replay, test_file));
        REQUIRE(agentite_replay_has_data(replay));
        REQUIRE(agentite_replay_get_total_frames(replay) == 5);

        const Agentite_ReplayMetadata *meta = agentite_replay_get_metadata(replay);
        REQUIRE(meta != nullptr);
        REQUIRE(strcmp(meta->map_name, "SaveTest") == 0);
        REQUIRE(meta->random_seed == 12345);

        agentite_replay_destroy(replay);
    }

    /* Clean up */
    remove(test_file);
}

TEST_CASE("Get file info without loading", "[replay][file]") {
    const char *test_file = "/tmp/test_replay_info.replay";

    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    config.compress = false;

    /* Create a replay file */
    {
        Agentite_ReplaySystem *replay = agentite_replay_create(&config);
        Agentite_CommandSystem *cmd_sys = agentite_command_create();
        agentite_command_register(cmd_sys, CMD_MOVE, nullptr, execute_move);

        Agentite_ReplayMetadata meta = {};
        strncpy(meta.map_name, "InfoTest", sizeof(meta.map_name) - 1);
        strncpy(meta.game_version, "2.0.0", sizeof(meta.game_version) - 1);
        meta.player_count = 4;

        REQUIRE(agentite_replay_start_recording(replay, cmd_sys, nullptr, &meta));
        agentite_replay_record_frame(replay, 0.016f);
        agentite_replay_stop_recording(replay);
        REQUIRE(agentite_replay_save(replay, test_file));

        agentite_command_destroy(cmd_sys);
        agentite_replay_destroy(replay);
    }

    /* Get file info */
    Agentite_ReplayMetadata info;
    REQUIRE(agentite_replay_get_file_info(test_file, &info));
    REQUIRE(info.magic == AGENTITE_REPLAY_MAGIC);
    REQUIRE(strcmp(info.map_name, "InfoTest") == 0);
    REQUIRE(strcmp(info.game_version, "2.0.0") == 0);
    REQUIRE(info.player_count == 4);

    /* Validate file check */
    REQUIRE(agentite_replay_is_valid_file(test_file));
    REQUIRE_FALSE(agentite_replay_is_valid_file("/tmp/nonexistent.replay"));

    remove(test_file);
}

TEST_CASE("Load non-existent file fails", "[replay][file]") {
    Agentite_ReplaySystem *replay = agentite_replay_create(nullptr);
    REQUIRE_FALSE(agentite_replay_load(replay, "/tmp/nonexistent_replay_file.replay"));
    agentite_replay_destroy(replay);
}

TEST_CASE("Cannot save empty replay", "[replay][file]") {
    Agentite_ReplaySystem *replay = agentite_replay_create(nullptr);
    REQUIRE_FALSE(agentite_replay_save(replay, "/tmp/empty.replay"));
    agentite_replay_destroy(replay);
}

/* ============================================================================
 * Playback Tests
 * ============================================================================ */

TEST_CASE("Basic playback", "[replay][playback]") {
    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    config.serialize = test_serialize;
    config.deserialize = test_deserialize;
    config.reset = test_reset;

    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    Agentite_CommandSystem *cmd_sys = agentite_command_create();
    agentite_command_register(cmd_sys, CMD_MOVE, validate_move, execute_move);

    TestGameState game_state = {0, 0, 100, 0};

    /* Record */
    Agentite_ReplayMetadata meta = {};
    agentite_replay_start_recording(replay, cmd_sys, &game_state, &meta);

    for (int i = 0; i < 3; i++) {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", (i + 1) * 10);
        agentite_command_set_int(cmd, "y", (i + 1) * 5);
        agentite_command_execute(cmd_sys, cmd, &game_state);
        agentite_command_free(cmd);
        agentite_replay_record_frame(replay, 0.016f);
    }

    agentite_replay_stop_recording(replay);

    /* Reset state for playback */
    game_state.player_x = 0;
    game_state.player_y = 0;
    game_state.move_count = 0;

    /* Start playback */
    REQUIRE(agentite_replay_start_playback(replay, cmd_sys, &game_state));
    REQUIRE(agentite_replay_is_playing(replay));
    REQUIRE(agentite_replay_get_state(replay) == AGENTITE_REPLAY_PLAYING);

    /* Play through frames */
    int total_commands = 0;
    while (agentite_replay_is_playing(replay)) {
        int cmds = agentite_replay_playback_frame(replay, &game_state, 0.016f);
        if (cmds > 0) total_commands += cmds;
    }

    /* Verify playback completed */
    REQUIRE(agentite_replay_get_state(replay) == AGENTITE_REPLAY_IDLE);
    REQUIRE(total_commands == 3);
    REQUIRE(game_state.move_count == 3);
    REQUIRE(game_state.player_x == 30);
    REQUIRE(game_state.player_y == 15);

    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

TEST_CASE("Playback pause and resume", "[replay][playback]") {
    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    Agentite_CommandSystem *cmd_sys = agentite_command_create();
    agentite_command_register(cmd_sys, CMD_MOVE, nullptr, execute_move);

    Agentite_ReplayMetadata meta = {};
    agentite_replay_start_recording(replay, cmd_sys, nullptr, &meta);
    for (int i = 0; i < 5; i++) {
        agentite_replay_record_frame(replay, 0.016f);
    }
    agentite_replay_stop_recording(replay);

    agentite_replay_start_playback(replay, cmd_sys, nullptr);
    REQUIRE(agentite_replay_is_playing(replay));

    /* Pause */
    agentite_replay_pause(replay);
    REQUIRE(agentite_replay_is_paused(replay));
    REQUIRE_FALSE(agentite_replay_is_playing(replay));
    REQUIRE(agentite_replay_get_state(replay) == AGENTITE_REPLAY_PAUSED);

    /* Resume */
    agentite_replay_resume(replay);
    REQUIRE(agentite_replay_is_playing(replay));
    REQUIRE_FALSE(agentite_replay_is_paused(replay));

    /* Toggle */
    agentite_replay_toggle_pause(replay);
    REQUIRE(agentite_replay_is_paused(replay));

    agentite_replay_toggle_pause(replay);
    REQUIRE(agentite_replay_is_playing(replay));

    agentite_replay_stop_playback(replay);
    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

TEST_CASE("Cannot start playback without data", "[replay][playback]") {
    Agentite_ReplaySystem *replay = agentite_replay_create(nullptr);
    Agentite_CommandSystem *cmd_sys = agentite_command_create();

    REQUIRE_FALSE(agentite_replay_start_playback(replay, cmd_sys, nullptr));

    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

/* ============================================================================
 * Progress and Time Tests
 * ============================================================================ */

TEST_CASE("Progress tracking", "[replay][progress]") {
    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    Agentite_CommandSystem *cmd_sys = agentite_command_create();
    agentite_command_register(cmd_sys, CMD_MOVE, nullptr, execute_move);

    Agentite_ReplayMetadata meta = {};
    agentite_replay_start_recording(replay, cmd_sys, nullptr, &meta);
    for (int i = 0; i < 10; i++) {
        agentite_replay_record_frame(replay, 0.1f); /* 100ms per frame */
    }
    agentite_replay_stop_recording(replay);

    REQUIRE(agentite_replay_get_total_frames(replay) == 10);
    REQUIRE(agentite_replay_get_total_duration(replay) == Catch::Approx(1.0f).margin(0.01f));

    agentite_replay_start_playback(replay, cmd_sys, nullptr);

    /* Play half the frames */
    for (int i = 0; i < 5; i++) {
        agentite_replay_playback_frame(replay, nullptr, 0.1f);
    }

    REQUIRE(agentite_replay_get_current_frame(replay) == 5);
    REQUIRE(agentite_replay_get_progress(replay) == Catch::Approx(0.5f).margin(0.01f));
    REQUIRE(agentite_replay_get_current_time(replay) == Catch::Approx(0.5f).margin(0.01f));

    agentite_replay_stop_playback(replay);
    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

/* ============================================================================
 * Seek Tests
 * ============================================================================ */

TEST_CASE("Seek by frame", "[replay][seek]") {
    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    config.serialize = test_serialize;
    config.deserialize = test_deserialize;
    config.reset = test_reset;
    config.snapshot_interval = 5; /* Snapshot every 5 frames */

    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    Agentite_CommandSystem *cmd_sys = agentite_command_create();
    agentite_command_register(cmd_sys, CMD_MOVE, validate_move, execute_move);

    TestGameState game_state = {0, 0, 100, 0};

    /* Record with snapshots */
    Agentite_ReplayMetadata meta = {};
    agentite_replay_start_recording(replay, cmd_sys, &game_state, &meta);

    for (int i = 0; i < 20; i++) {
        Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
        agentite_command_set_int(cmd, "x", i);
        agentite_command_set_int(cmd, "y", i);
        agentite_command_execute(cmd_sys, cmd, &game_state);
        agentite_command_free(cmd);
        agentite_replay_record_frame(replay, 0.016f);

        /* Create snapshots periodically */
        if ((i + 1) % 5 == 0) {
            agentite_replay_create_snapshot(replay, &game_state);
        }
    }

    agentite_replay_stop_recording(replay);
    REQUIRE(agentite_replay_get_snapshot_count(replay) == 4);

    /* Start playback and seek */
    game_state = {0, 0, 100, 0};
    agentite_replay_start_playback(replay, cmd_sys, &game_state);

    /* Seek to frame 15 */
    REQUIRE(agentite_replay_seek(replay, &game_state, 15));
    REQUIRE(agentite_replay_get_current_frame(replay) == 15);

    /* Seek backward to frame 5 */
    REQUIRE(agentite_replay_seek(replay, &game_state, 5));
    REQUIRE(agentite_replay_get_current_frame(replay) == 5);

    agentite_replay_stop_playback(replay);
    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

TEST_CASE("Seek by percent", "[replay][seek]") {
    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    config.serialize = test_serialize;
    config.deserialize = test_deserialize;
    config.reset = test_reset;

    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    Agentite_CommandSystem *cmd_sys = agentite_command_create();
    agentite_command_register(cmd_sys, CMD_MOVE, nullptr, execute_move);

    TestGameState game_state = {0, 0, 100, 0};

    Agentite_ReplayMetadata meta = {};
    agentite_replay_start_recording(replay, cmd_sys, &game_state, &meta);
    for (int i = 0; i < 100; i++) {
        agentite_replay_record_frame(replay, 0.016f);
    }
    agentite_replay_stop_recording(replay);

    agentite_replay_start_playback(replay, cmd_sys, &game_state);

    /* Seek to 50% */
    REQUIRE(agentite_replay_seek_percent(replay, &game_state, 0.5f));
    REQUIRE(agentite_replay_get_current_frame(replay) == 50);

    /* Seek to 25% */
    REQUIRE(agentite_replay_seek_percent(replay, &game_state, 0.25f));
    REQUIRE(agentite_replay_get_current_frame(replay) == 25);

    /* Clamp to valid range */
    REQUIRE(agentite_replay_seek_percent(replay, &game_state, -0.5f));
    REQUIRE(agentite_replay_get_current_frame(replay) == 0);

    REQUIRE(agentite_replay_seek_percent(replay, &game_state, 1.5f));
    /* Should be at or near the end */

    agentite_replay_stop_playback(replay);
    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

/* ============================================================================
 * Step Tests
 * ============================================================================ */

TEST_CASE("Step forward while paused", "[replay][step]") {
    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    Agentite_CommandSystem *cmd_sys = agentite_command_create();
    agentite_command_register(cmd_sys, CMD_MOVE, nullptr, execute_move);

    Agentite_ReplayMetadata meta = {};
    agentite_replay_start_recording(replay, cmd_sys, nullptr, &meta);
    for (int i = 0; i < 5; i++) {
        agentite_replay_record_frame(replay, 0.016f);
    }
    agentite_replay_stop_recording(replay);

    agentite_replay_start_playback(replay, cmd_sys, nullptr);
    agentite_replay_pause(replay);

    REQUIRE(agentite_replay_get_current_frame(replay) == 0);

    /* Step forward */
    agentite_replay_step_forward(replay, nullptr);
    REQUIRE(agentite_replay_get_current_frame(replay) == 1);

    agentite_replay_step_forward(replay, nullptr);
    REQUIRE(agentite_replay_get_current_frame(replay) == 2);

    agentite_replay_stop_playback(replay);
    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

static bool g_on_end_called = false;
static bool g_on_seek_called = false;

static void test_on_end(Agentite_ReplaySystem *replay, void *userdata) {
    (void)replay;
    (void)userdata;
    g_on_end_called = true;
}

static void test_on_seek(Agentite_ReplaySystem *replay, void *userdata) {
    (void)replay;
    (void)userdata;
    g_on_seek_called = true;
}

TEST_CASE("On end callback", "[replay][callback]") {
    g_on_end_called = false;

    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    Agentite_CommandSystem *cmd_sys = agentite_command_create();
    agentite_command_register(cmd_sys, CMD_MOVE, nullptr, execute_move);

    agentite_replay_set_on_end(replay, test_on_end, nullptr);

    Agentite_ReplayMetadata meta = {};
    agentite_replay_start_recording(replay, cmd_sys, nullptr, &meta);
    agentite_replay_record_frame(replay, 0.016f);
    agentite_replay_record_frame(replay, 0.016f);
    agentite_replay_stop_recording(replay);

    agentite_replay_start_playback(replay, cmd_sys, nullptr);

    /* Play until end */
    while (agentite_replay_is_playing(replay)) {
        agentite_replay_playback_frame(replay, nullptr, 0.016f);
    }

    REQUIRE(g_on_end_called);

    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

TEST_CASE("On seek callback", "[replay][callback]") {
    g_on_seek_called = false;

    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    config.serialize = test_serialize;
    config.deserialize = test_deserialize;
    config.reset = test_reset;

    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    Agentite_CommandSystem *cmd_sys = agentite_command_create();
    agentite_command_register(cmd_sys, CMD_MOVE, nullptr, execute_move);

    agentite_replay_set_on_seek(replay, test_on_seek, nullptr);

    TestGameState game_state = {0, 0, 100, 0};

    Agentite_ReplayMetadata meta = {};
    agentite_replay_start_recording(replay, cmd_sys, &game_state, &meta);
    for (int i = 0; i < 10; i++) {
        agentite_replay_record_frame(replay, 0.016f);
    }
    agentite_replay_stop_recording(replay);

    agentite_replay_start_playback(replay, cmd_sys, &game_state);
    agentite_replay_seek(replay, &game_state, 5);

    REQUIRE(g_on_seek_called);

    agentite_replay_stop_playback(replay);
    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST_CASE("Format time", "[replay][utility]") {
    char buffer[32];

    agentite_replay_format_time(0, buffer, sizeof(buffer));
    REQUIRE(strcmp(buffer, "0:00") == 0);

    agentite_replay_format_time(65.5f, buffer, sizeof(buffer));
    REQUIRE(strcmp(buffer, "1:05") == 0);

    agentite_replay_format_time(3661.0f, buffer, sizeof(buffer));
    REQUIRE(strcmp(buffer, "1:01:01") == 0);
}

TEST_CASE("Clear replay data", "[replay][utility]") {
    Agentite_ReplayConfig config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    Agentite_ReplaySystem *replay = agentite_replay_create(&config);
    Agentite_CommandSystem *cmd_sys = agentite_command_create();
    agentite_command_register(cmd_sys, CMD_MOVE, nullptr, execute_move);

    Agentite_ReplayMetadata meta = {};
    agentite_replay_start_recording(replay, cmd_sys, nullptr, &meta);
    for (int i = 0; i < 10; i++) {
        agentite_replay_record_frame(replay, 0.016f);
    }
    agentite_replay_stop_recording(replay);

    REQUIRE(agentite_replay_has_data(replay));
    REQUIRE(agentite_replay_get_total_frames(replay) == 10);

    agentite_replay_clear(replay);

    REQUIRE_FALSE(agentite_replay_has_data(replay));
    REQUIRE(agentite_replay_get_total_frames(replay) == 0);

    agentite_command_destroy(cmd_sys);
    agentite_replay_destroy(replay);
}

/* ============================================================================
 * NULL Safety Tests
 * ============================================================================ */

TEST_CASE("NULL safety", "[replay][null]") {
    /* These should not crash */
    agentite_replay_destroy(nullptr);
    agentite_replay_stop_recording(nullptr);
    agentite_replay_stop_playback(nullptr);
    agentite_replay_record_frame(nullptr, 0.016f);
    agentite_replay_pause(nullptr);
    agentite_replay_resume(nullptr);
    agentite_replay_toggle_pause(nullptr);
    agentite_replay_set_speed(nullptr, 2.0f);
    agentite_replay_clear(nullptr);

    REQUIRE(agentite_replay_get_state(nullptr) == AGENTITE_REPLAY_IDLE);
    REQUIRE_FALSE(agentite_replay_is_recording(nullptr));
    REQUIRE_FALSE(agentite_replay_is_playing(nullptr));
    REQUIRE_FALSE(agentite_replay_is_paused(nullptr));
    REQUIRE(agentite_replay_get_current_frame(nullptr) == 0);
    REQUIRE(agentite_replay_get_total_frames(nullptr) == 0);
    REQUIRE(agentite_replay_get_speed(nullptr) == 1.0f);
    REQUIRE(agentite_replay_get_metadata(nullptr) == nullptr);
    REQUIRE_FALSE(agentite_replay_has_data(nullptr));
}
