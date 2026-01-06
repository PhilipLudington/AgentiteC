/**
 * @file replay.cpp
 * @brief Replay System Implementation
 *
 * Implements command-based replay recording and playback with seeking support
 * via periodic state snapshots.
 */

#include "agentite/replay.h"
#include "agentite/command.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <vector>
#include <algorithm>

/*============================================================================
 * Internal Constants
 *============================================================================*/

static const int k_initial_frame_capacity = 1024;

/*============================================================================
 * Internal Structures
 *============================================================================*/

/** Recorded command with frame info */
struct ReplayCommand {
    int type;
    Agentite_CommandParam params[AGENTITE_COMMAND_MAX_PARAMS];
    int param_count;
    uint32_t sequence;
    int32_t source_faction;
};

/** Single frame of recorded data */
struct ReplayFrame {
    uint64_t frame_number;
    float delta_time;
    std::vector<ReplayCommand> commands;
};

/** State snapshot for seeking */
struct ReplaySnapshot {
    uint64_t frame_number;
    void *data;
    size_t size;
};

/** Main replay system structure */
struct Agentite_ReplaySystem {
    /* Configuration */
    Agentite_ReplayConfig config;

    /* State */
    Agentite_ReplayState state;
    Agentite_ReplayMetadata metadata;

    /* Frame data */
    std::vector<ReplayFrame> frames;
    uint64_t current_frame;
    float current_time;
    float accumulated_time;

    /* Snapshots */
    std::vector<ReplaySnapshot> snapshots;
    uint64_t frames_since_snapshot;

    /* Initial state */
    void *initial_state_data;
    size_t initial_state_size;

    /* Recording */
    Agentite_CommandSystem *recording_cmd_sys;
    Agentite_CommandCallback original_callback;
    void *original_callback_userdata;
    std::vector<ReplayCommand> pending_commands;

    /* Playback */
    Agentite_CommandSystem *playback_cmd_sys;
    float playback_speed;

    /* Callbacks */
    Agentite_ReplayCallback on_end_callback;
    void *on_end_userdata;
    Agentite_ReplayCallback on_seek_callback;
    void *on_seek_userdata;
};

/*============================================================================
 * Internal Helper Functions
 *============================================================================*/

static void copy_command_to_replay(ReplayCommand *dst, const Agentite_Command *src) {
    dst->type = src->type;
    dst->param_count = src->param_count;
    dst->sequence = src->sequence;
    dst->source_faction = src->source_faction;

    for (int i = 0; i < src->param_count && i < AGENTITE_COMMAND_MAX_PARAMS; i++) {
        /* Skip pointer parameters - not serializable */
        if (src->params[i].type == AGENTITE_CMD_PARAM_PTR) {
            dst->params[i].type = AGENTITE_CMD_PARAM_NONE;
            continue;
        }
        dst->params[i] = src->params[i];
    }
}

static void copy_replay_to_command(Agentite_Command *dst, const ReplayCommand *src) {
    dst->type = src->type;
    dst->param_count = src->param_count;
    dst->sequence = src->sequence;
    dst->source_faction = src->source_faction;
    dst->userdata = nullptr;

    for (int i = 0; i < src->param_count && i < AGENTITE_COMMAND_MAX_PARAMS; i++) {
        dst->params[i] = src->params[i];
    }
}

static void free_snapshot(ReplaySnapshot *snapshot) {
    if (snapshot && snapshot->data) {
        free(snapshot->data);
        snapshot->data = nullptr;
        snapshot->size = 0;
    }
}

static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(nullptr);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%dT%H:%M:%S", tm_info);
}

/*============================================================================
 * Recording Callback
 *============================================================================*/

static void replay_command_callback(Agentite_CommandSystem *sys,
                                    const Agentite_Command *cmd,
                                    const Agentite_CommandResult *result,
                                    void *userdata) {
    Agentite_ReplaySystem *replay = static_cast<Agentite_ReplaySystem *>(userdata);

    if (!replay || replay->state != AGENTITE_REPLAY_RECORDING) {
        return;
    }

    /* Only record successful commands */
    if (result->success) {
        ReplayCommand rc;
        copy_command_to_replay(&rc, cmd);
        replay->pending_commands.push_back(rc);
    }

    /* Chain to original callback if set */
    if (replay->original_callback) {
        replay->original_callback(sys, cmd, result, replay->original_callback_userdata);
    }
}

/*============================================================================
 * Lifecycle
 *============================================================================*/

Agentite_ReplaySystem *agentite_replay_create(const Agentite_ReplayConfig *config) {
    Agentite_ReplaySystem *replay = new (std::nothrow) Agentite_ReplaySystem();
    if (!replay) {
        agentite_set_error("replay: failed to allocate replay system");
        return nullptr;
    }

    /* Initialize with defaults or provided config */
    if (config) {
        replay->config = *config;
    } else {
        Agentite_ReplayConfig def = AGENTITE_REPLAY_CONFIG_DEFAULT;
        replay->config = def;
    }

    /* Auto snapshot interval */
    if (replay->config.snapshot_interval <= 0) {
        replay->config.snapshot_interval = AGENTITE_REPLAY_DEFAULT_SNAPSHOT_INTERVAL;
    }

    /* Initialize state */
    replay->state = AGENTITE_REPLAY_IDLE;
    memset(&replay->metadata, 0, sizeof(replay->metadata));
    replay->metadata.magic = AGENTITE_REPLAY_MAGIC;
    replay->metadata.version = AGENTITE_REPLAY_VERSION;
    replay->metadata.min_compatible_version = AGENTITE_REPLAY_MIN_VERSION;

    replay->current_frame = 0;
    replay->current_time = 0.0f;
    replay->accumulated_time = 0.0f;
    replay->frames_since_snapshot = 0;

    replay->initial_state_data = nullptr;
    replay->initial_state_size = 0;

    replay->recording_cmd_sys = nullptr;
    replay->original_callback = nullptr;
    replay->original_callback_userdata = nullptr;

    replay->playback_cmd_sys = nullptr;
    replay->playback_speed = 1.0f;

    replay->on_end_callback = nullptr;
    replay->on_end_userdata = nullptr;
    replay->on_seek_callback = nullptr;
    replay->on_seek_userdata = nullptr;

    replay->frames.reserve(k_initial_frame_capacity);

    return replay;
}

void agentite_replay_destroy(Agentite_ReplaySystem *replay) {
    if (!replay) {
        return;
    }

    /* Stop any active recording/playback */
    if (replay->state == AGENTITE_REPLAY_RECORDING) {
        agentite_replay_stop_recording(replay);
    } else if (replay->state == AGENTITE_REPLAY_PLAYING ||
               replay->state == AGENTITE_REPLAY_PAUSED) {
        agentite_replay_stop_playback(replay);
    }

    /* Free initial state */
    if (replay->initial_state_data) {
        free(replay->initial_state_data);
    }

    /* Free snapshots */
    for (auto &snapshot : replay->snapshots) {
        free_snapshot(&snapshot);
    }

    delete replay;
}

/*============================================================================
 * Recording
 *============================================================================*/

bool agentite_replay_start_recording(Agentite_ReplaySystem *replay,
                                      Agentite_CommandSystem *cmd_sys,
                                      void *game_state,
                                      const Agentite_ReplayMetadata *metadata) {
    AGENTITE_VALIDATE_PTR_RET(replay, false);
    AGENTITE_VALIDATE_PTR_RET(cmd_sys, false);

    if (replay->state != AGENTITE_REPLAY_IDLE) {
        agentite_set_error("replay: cannot start recording, not in idle state");
        return false;
    }

    /* Clear any existing data */
    agentite_replay_clear(replay);

    /* Copy metadata */
    if (metadata) {
        replay->metadata = *metadata;
    }
    replay->metadata.magic = AGENTITE_REPLAY_MAGIC;
    replay->metadata.version = AGENTITE_REPLAY_VERSION;
    replay->metadata.min_compatible_version = AGENTITE_REPLAY_MIN_VERSION;

    /* Set timestamp */
    get_timestamp(replay->metadata.timestamp, sizeof(replay->metadata.timestamp));

    /* Capture initial state if serialize callback provided */
    if (replay->config.serialize && game_state) {
        void *data = nullptr;
        size_t size = 0;

        if (!replay->config.serialize(game_state, &data, &size)) {
            agentite_set_error("replay: failed to serialize initial state");
            return false;
        }

        replay->initial_state_data = data;
        replay->initial_state_size = size;
    }

    /* Hook into command system */
    replay->recording_cmd_sys = cmd_sys;
    /* Note: In a full implementation, we'd need to get the current callback
     * and chain to it. For now, we just set our callback. */
    agentite_command_set_callback(cmd_sys, replay_command_callback, replay);

    replay->state = AGENTITE_REPLAY_RECORDING;
    replay->current_frame = 0;
    replay->current_time = 0.0f;
    replay->frames_since_snapshot = 0;

    return true;
}

void agentite_replay_stop_recording(Agentite_ReplaySystem *replay) {
    if (!replay || replay->state != AGENTITE_REPLAY_RECORDING) {
        return;
    }

    /* Restore original callback */
    if (replay->recording_cmd_sys) {
        agentite_command_set_callback(replay->recording_cmd_sys,
                                      replay->original_callback,
                                      replay->original_callback_userdata);
        replay->recording_cmd_sys = nullptr;
    }

    /* Finalize metadata */
    replay->metadata.total_frames = replay->frames.size();
    replay->metadata.total_duration = replay->current_time;

    replay->state = AGENTITE_REPLAY_IDLE;
}

void agentite_replay_record_frame(Agentite_ReplaySystem *replay, float delta_time) {
    if (!replay || replay->state != AGENTITE_REPLAY_RECORDING) {
        return;
    }

    /* Create frame with pending commands */
    ReplayFrame frame;
    frame.frame_number = replay->current_frame;
    frame.delta_time = delta_time;
    frame.commands = std::move(replay->pending_commands);
    replay->pending_commands.clear();

    replay->frames.push_back(std::move(frame));

    replay->current_frame++;
    replay->current_time += delta_time;
    replay->frames_since_snapshot++;
}

bool agentite_replay_create_snapshot(Agentite_ReplaySystem *replay,
                                      void *game_state) {
    AGENTITE_VALIDATE_PTR_RET(replay, false);

    if (!replay->config.serialize) {
        agentite_set_error("replay: no serialize callback configured");
        return false;
    }

    /* Limit snapshots */
    if (replay->config.max_snapshots > 0 &&
        (int)replay->snapshots.size() >= replay->config.max_snapshots) {
        /* Remove oldest snapshot */
        free_snapshot(&replay->snapshots[0]);
        replay->snapshots.erase(replay->snapshots.begin());
    }

    void *data = nullptr;
    size_t size = 0;

    if (!replay->config.serialize(game_state, &data, &size)) {
        agentite_set_error("replay: failed to serialize snapshot");
        return false;
    }

    ReplaySnapshot snapshot;
    snapshot.frame_number = replay->current_frame;
    snapshot.data = data;
    snapshot.size = size;

    replay->snapshots.push_back(snapshot);
    replay->frames_since_snapshot = 0;

    return true;
}

/*============================================================================
 * File I/O
 *============================================================================*/

/* Binary format helpers */

static bool write_uint8(FILE *fp, uint8_t val) {
    return fwrite(&val, 1, 1, fp) == 1;
}

static bool write_uint16(FILE *fp, uint16_t val) {
    return fwrite(&val, sizeof(val), 1, fp) == 1;
}

static bool write_uint32(FILE *fp, uint32_t val) {
    return fwrite(&val, sizeof(val), 1, fp) == 1;
}

static bool write_uint64(FILE *fp, uint64_t val) {
    return fwrite(&val, sizeof(val), 1, fp) == 1;
}

static bool write_int32(FILE *fp, int32_t val) {
    return fwrite(&val, sizeof(val), 1, fp) == 1;
}

static bool write_float(FILE *fp, float val) {
    return fwrite(&val, sizeof(val), 1, fp) == 1;
}

static bool write_string(FILE *fp, const char *str, size_t max_len) {
    char buf[256] = {0};
    if (str) {
        strncpy(buf, str, max_len - 1);
    }
    return fwrite(buf, 1, max_len, fp) == max_len;
}

static bool read_uint8(FILE *fp, uint8_t *val) {
    return fread(val, 1, 1, fp) == 1;
}

static bool read_uint16(FILE *fp, uint16_t *val) {
    return fread(val, sizeof(*val), 1, fp) == 1;
}

static bool read_uint32(FILE *fp, uint32_t *val) {
    return fread(val, sizeof(*val), 1, fp) == 1;
}

static bool read_uint64(FILE *fp, uint64_t *val) {
    return fread(val, sizeof(*val), 1, fp) == 1;
}

static bool read_int32(FILE *fp, int32_t *val) {
    return fread(val, sizeof(*val), 1, fp) == 1;
}

static bool read_float(FILE *fp, float *val) {
    return fread(val, sizeof(*val), 1, fp) == 1;
}

static bool read_string(FILE *fp, char *str, size_t max_len) {
    return fread(str, 1, max_len, fp) == max_len;
}

static bool write_param(FILE *fp, const Agentite_CommandParam *param) {
    /* Key length + key */
    uint8_t key_len = (uint8_t)strlen(param->key);
    if (!write_uint8(fp, key_len)) return false;
    if (key_len > 0 && fwrite(param->key, 1, key_len, fp) != key_len) return false;

    /* Type */
    if (!write_uint8(fp, (uint8_t)param->type)) return false;

    /* Value based on type */
    switch (param->type) {
        case AGENTITE_CMD_PARAM_INT:
            return write_int32(fp, param->i32);
        case AGENTITE_CMD_PARAM_INT64:
            return fwrite(&param->i64, sizeof(param->i64), 1, fp) == 1;
        case AGENTITE_CMD_PARAM_FLOAT:
            return write_float(fp, param->f32);
        case AGENTITE_CMD_PARAM_DOUBLE:
            return fwrite(&param->f64, sizeof(param->f64), 1, fp) == 1;
        case AGENTITE_CMD_PARAM_BOOL:
            return write_uint8(fp, param->b ? 1 : 0);
        case AGENTITE_CMD_PARAM_ENTITY:
            return write_uint32(fp, param->entity);
        case AGENTITE_CMD_PARAM_STRING: {
            uint8_t str_len = (uint8_t)strlen(param->str);
            if (!write_uint8(fp, str_len)) return false;
            if (str_len > 0 && fwrite(param->str, 1, str_len, fp) != str_len) return false;
            return true;
        }
        case AGENTITE_CMD_PARAM_PTR:
        case AGENTITE_CMD_PARAM_NONE:
        default:
            return true;
    }
}

static bool read_param(FILE *fp, Agentite_CommandParam *param) {
    memset(param, 0, sizeof(*param));

    /* Key length + key */
    uint8_t key_len;
    if (!read_uint8(fp, &key_len)) return false;
    if (key_len > 0) {
        if (key_len >= AGENTITE_COMMAND_MAX_PARAM_KEY) {
            return false;
        }
        if (fread(param->key, 1, key_len, fp) != key_len) return false;
    }
    param->key[key_len] = '\0';

    /* Type */
    uint8_t type;
    if (!read_uint8(fp, &type)) return false;
    param->type = (Agentite_CommandParamType)type;

    /* Value based on type */
    switch (param->type) {
        case AGENTITE_CMD_PARAM_INT:
            return read_int32(fp, &param->i32);
        case AGENTITE_CMD_PARAM_INT64:
            return fread(&param->i64, sizeof(param->i64), 1, fp) == 1;
        case AGENTITE_CMD_PARAM_FLOAT:
            return read_float(fp, &param->f32);
        case AGENTITE_CMD_PARAM_DOUBLE:
            return fread(&param->f64, sizeof(param->f64), 1, fp) == 1;
        case AGENTITE_CMD_PARAM_BOOL: {
            uint8_t b;
            if (!read_uint8(fp, &b)) return false;
            param->b = (b != 0);
            return true;
        }
        case AGENTITE_CMD_PARAM_ENTITY:
            return read_uint32(fp, &param->entity);
        case AGENTITE_CMD_PARAM_STRING: {
            uint8_t str_len;
            if (!read_uint8(fp, &str_len)) return false;
            if (str_len >= AGENTITE_COMMAND_MAX_PARAM_KEY) {
                return false;
            }
            if (str_len > 0) {
                if (fread(param->str, 1, str_len, fp) != str_len) return false;
            }
            param->str[str_len] = '\0';
            return true;
        }
        case AGENTITE_CMD_PARAM_PTR:
        case AGENTITE_CMD_PARAM_NONE:
        default:
            return true;
    }
}

static bool write_command(FILE *fp, const ReplayCommand *cmd) {
    if (!write_uint16(fp, (uint16_t)cmd->type)) return false;
    if (!write_uint8(fp, (uint8_t)cmd->param_count)) return false;
    if (!write_uint32(fp, cmd->sequence)) return false;
    if (!write_int32(fp, cmd->source_faction)) return false;

    for (int i = 0; i < cmd->param_count; i++) {
        if (!write_param(fp, &cmd->params[i])) return false;
    }

    return true;
}

static bool read_command(FILE *fp, ReplayCommand *cmd) {
    memset(cmd, 0, sizeof(*cmd));

    uint16_t type;
    if (!read_uint16(fp, &type)) return false;
    cmd->type = (int)type;

    uint8_t param_count;
    if (!read_uint8(fp, &param_count)) return false;
    cmd->param_count = (int)param_count;

    if (!read_uint32(fp, &cmd->sequence)) return false;
    if (!read_int32(fp, &cmd->source_faction)) return false;

    if (cmd->param_count > AGENTITE_COMMAND_MAX_PARAMS) {
        return false;
    }

    for (int i = 0; i < cmd->param_count; i++) {
        if (!read_param(fp, &cmd->params[i])) return false;
    }

    return true;
}

bool agentite_replay_save(const Agentite_ReplaySystem *replay,
                           const char *filepath) {
    AGENTITE_VALIDATE_PTR_RET(replay, false);
    AGENTITE_VALIDATE_PTR_RET(filepath, false);

    if (replay->frames.empty()) {
        agentite_set_error("replay: no frames to save");
        return false;
    }

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        agentite_set_error("replay: failed to open file for writing: %s", filepath);
        return false;
    }

    bool success = true;

    /* Write header/metadata */
    success = success && write_uint32(fp, replay->metadata.magic);
    success = success && write_int32(fp, replay->metadata.version);
    success = success && write_int32(fp, replay->metadata.min_compatible_version);
    success = success && write_string(fp, replay->metadata.timestamp,
                                      AGENTITE_REPLAY_MAX_TIMESTAMP);
    success = success && write_string(fp, replay->metadata.game_version,
                                      AGENTITE_REPLAY_MAX_VERSION_STRING);
    success = success && write_string(fp, replay->metadata.map_name,
                                      AGENTITE_REPLAY_MAX_MAP_NAME);
    success = success && write_uint64(fp, replay->metadata.total_frames);
    success = success && write_float(fp, replay->metadata.total_duration);
    success = success && write_uint32(fp, replay->metadata.random_seed);
    success = success && write_int32(fp, replay->metadata.player_count);

    /* Write initial state size and data */
    success = success && write_uint64(fp, replay->initial_state_size);
    if (success && replay->initial_state_size > 0 && replay->initial_state_data) {
        success = (fwrite(replay->initial_state_data, 1,
                          replay->initial_state_size, fp) == replay->initial_state_size);
    }

    /* Write frame count */
    success = success && write_uint64(fp, replay->frames.size());

    /* Write each frame */
    for (const auto &frame : replay->frames) {
        if (!success) break;

        success = success && write_uint64(fp, frame.frame_number);
        success = success && write_float(fp, frame.delta_time);
        success = success && write_uint32(fp, (uint32_t)frame.commands.size());

        for (const auto &cmd : frame.commands) {
            if (!success) break;
            success = write_command(fp, &cmd);
        }
    }

    /* Write snapshot count */
    success = success && write_uint32(fp, (uint32_t)replay->snapshots.size());

    /* Write snapshots */
    for (const auto &snapshot : replay->snapshots) {
        if (!success) break;

        success = success && write_uint64(fp, snapshot.frame_number);
        success = success && write_uint64(fp, snapshot.size);
        if (success && snapshot.size > 0 && snapshot.data) {
            success = (fwrite(snapshot.data, 1, snapshot.size, fp) == snapshot.size);
        }
    }

    fclose(fp);

    if (!success) {
        agentite_set_error("replay: failed to write replay file");
        remove(filepath);
    }

    return success;
}

bool agentite_replay_load(Agentite_ReplaySystem *replay,
                           const char *filepath) {
    AGENTITE_VALIDATE_PTR_RET(replay, false);
    AGENTITE_VALIDATE_PTR_RET(filepath, false);

    if (replay->state != AGENTITE_REPLAY_IDLE) {
        agentite_set_error("replay: cannot load while recording or playing");
        return false;
    }

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        agentite_set_error("replay: failed to open file: %s", filepath);
        return false;
    }

    /* Clear existing data */
    agentite_replay_clear(replay);

    bool success = true;

    /* Read header/metadata */
    success = success && read_uint32(fp, &replay->metadata.magic);
    if (success && replay->metadata.magic != AGENTITE_REPLAY_MAGIC) {
        agentite_set_error("replay: invalid file format (bad magic)");
        fclose(fp);
        return false;
    }

    success = success && read_int32(fp, &replay->metadata.version);
    success = success && read_int32(fp, &replay->metadata.min_compatible_version);

    if (success && replay->metadata.version < AGENTITE_REPLAY_MIN_VERSION) {
        agentite_set_error("replay: file version %d too old (min %d)",
                          replay->metadata.version, AGENTITE_REPLAY_MIN_VERSION);
        fclose(fp);
        return false;
    }

    success = success && read_string(fp, replay->metadata.timestamp,
                                     AGENTITE_REPLAY_MAX_TIMESTAMP);
    success = success && read_string(fp, replay->metadata.game_version,
                                     AGENTITE_REPLAY_MAX_VERSION_STRING);
    success = success && read_string(fp, replay->metadata.map_name,
                                     AGENTITE_REPLAY_MAX_MAP_NAME);
    success = success && read_uint64(fp, &replay->metadata.total_frames);
    success = success && read_float(fp, &replay->metadata.total_duration);
    success = success && read_uint32(fp, &replay->metadata.random_seed);
    success = success && read_int32(fp, &replay->metadata.player_count);

    /* Read initial state */
    uint64_t initial_state_size;
    success = success && read_uint64(fp, &initial_state_size);
    if (success && initial_state_size > 0) {
        replay->initial_state_data = malloc(initial_state_size);
        if (!replay->initial_state_data) {
            agentite_set_error("replay: failed to allocate initial state");
            fclose(fp);
            return false;
        }
        success = (fread(replay->initial_state_data, 1,
                         initial_state_size, fp) == initial_state_size);
        replay->initial_state_size = initial_state_size;
    }

    /* Read frame count */
    uint64_t frame_count;
    success = success && read_uint64(fp, &frame_count);

    /* Read frames */
    if (success) {
        replay->frames.reserve((size_t)frame_count);

        for (uint64_t i = 0; i < frame_count && success; i++) {
            ReplayFrame frame;

            success = success && read_uint64(fp, &frame.frame_number);
            success = success && read_float(fp, &frame.delta_time);

            uint32_t cmd_count;
            success = success && read_uint32(fp, &cmd_count);

            if (success) {
                frame.commands.reserve(cmd_count);
                for (uint32_t j = 0; j < cmd_count && success; j++) {
                    ReplayCommand cmd;
                    success = read_command(fp, &cmd);
                    if (success) {
                        frame.commands.push_back(cmd);
                    }
                }
            }

            if (success) {
                replay->frames.push_back(std::move(frame));
            }
        }
    }

    /* Read snapshot count */
    uint32_t snapshot_count;
    success = success && read_uint32(fp, &snapshot_count);

    /* Read snapshots */
    if (success) {
        for (uint32_t i = 0; i < snapshot_count && success; i++) {
            ReplaySnapshot snapshot;
            snapshot.data = nullptr;

            success = success && read_uint64(fp, &snapshot.frame_number);
            uint64_t snapshot_size = 0;
            success = success && read_uint64(fp, &snapshot_size);
            snapshot.size = static_cast<size_t>(snapshot_size);

            if (success && snapshot.size > 0) {
                snapshot.data = malloc(snapshot.size);
                if (!snapshot.data) {
                    success = false;
                } else {
                    success = (fread(snapshot.data, 1, snapshot.size, fp) == snapshot.size);
                }
            }

            if (success) {
                replay->snapshots.push_back(snapshot);
            } else if (snapshot.data) {
                free(snapshot.data);
            }
        }
    }

    fclose(fp);

    if (!success) {
        agentite_set_error("replay: failed to read replay file");
        agentite_replay_clear(replay);
        return false;
    }

    return true;
}

bool agentite_replay_get_file_info(const char *filepath,
                                    Agentite_ReplayMetadata *out_meta) {
    AGENTITE_VALIDATE_PTR_RET(filepath, false);
    AGENTITE_VALIDATE_PTR_RET(out_meta, false);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        agentite_set_error("replay: failed to open file: %s", filepath);
        return false;
    }

    bool success = true;

    memset(out_meta, 0, sizeof(*out_meta));

    success = success && read_uint32(fp, &out_meta->magic);
    if (success && out_meta->magic != AGENTITE_REPLAY_MAGIC) {
        agentite_set_error("replay: invalid file format (bad magic)");
        fclose(fp);
        return false;
    }

    success = success && read_int32(fp, &out_meta->version);
    success = success && read_int32(fp, &out_meta->min_compatible_version);
    success = success && read_string(fp, out_meta->timestamp,
                                     AGENTITE_REPLAY_MAX_TIMESTAMP);
    success = success && read_string(fp, out_meta->game_version,
                                     AGENTITE_REPLAY_MAX_VERSION_STRING);
    success = success && read_string(fp, out_meta->map_name,
                                     AGENTITE_REPLAY_MAX_MAP_NAME);
    success = success && read_uint64(fp, &out_meta->total_frames);
    success = success && read_float(fp, &out_meta->total_duration);
    success = success && read_uint32(fp, &out_meta->random_seed);
    success = success && read_int32(fp, &out_meta->player_count);

    fclose(fp);

    if (!success) {
        agentite_set_error("replay: failed to read file header");
    }

    return success;
}

bool agentite_replay_is_valid_file(const char *filepath) {
    Agentite_ReplayMetadata meta;
    return agentite_replay_get_file_info(filepath, &meta);
}

/*============================================================================
 * Playback
 *============================================================================*/

bool agentite_replay_start_playback(Agentite_ReplaySystem *replay,
                                     Agentite_CommandSystem *cmd_sys,
                                     void *game_state) {
    AGENTITE_VALIDATE_PTR_RET(replay, false);
    AGENTITE_VALIDATE_PTR_RET(cmd_sys, false);

    if (replay->state != AGENTITE_REPLAY_IDLE) {
        agentite_set_error("replay: cannot start playback, not in idle state");
        return false;
    }

    if (replay->frames.empty()) {
        agentite_set_error("replay: no replay data loaded");
        return false;
    }

    /* Reset game state if callbacks provided */
    if (replay->config.reset && game_state) {
        if (!replay->config.reset(game_state, &replay->metadata)) {
            agentite_set_error("replay: failed to reset game state");
            return false;
        }
    }

    /* Restore initial state if available */
    if (replay->config.deserialize && game_state &&
        replay->initial_state_data && replay->initial_state_size > 0) {
        if (!replay->config.deserialize(game_state, replay->initial_state_data,
                                        replay->initial_state_size)) {
            agentite_set_error("replay: failed to deserialize initial state");
            return false;
        }
    }

    replay->playback_cmd_sys = cmd_sys;
    replay->current_frame = 0;
    replay->current_time = 0.0f;
    replay->accumulated_time = 0.0f;
    replay->state = AGENTITE_REPLAY_PLAYING;

    return true;
}

void agentite_replay_stop_playback(Agentite_ReplaySystem *replay) {
    if (!replay) {
        return;
    }

    if (replay->state == AGENTITE_REPLAY_PLAYING ||
        replay->state == AGENTITE_REPLAY_PAUSED) {
        replay->playback_cmd_sys = nullptr;
        replay->state = AGENTITE_REPLAY_IDLE;
    }
}

int agentite_replay_playback_frame(Agentite_ReplaySystem *replay,
                                    void *game_state,
                                    float delta_time) {
    AGENTITE_VALIDATE_PTR_RET(replay, -1);

    if (replay->state != AGENTITE_REPLAY_PLAYING) {
        return 0;
    }

    if (replay->current_frame >= replay->frames.size()) {
        /* End of replay */
        replay->state = AGENTITE_REPLAY_IDLE;
        if (replay->on_end_callback) {
            replay->on_end_callback(replay, replay->on_end_userdata);
        }
        return 0;
    }

    /* Accumulate time */
    replay->accumulated_time += delta_time * replay->playback_speed;

    /* Get current frame */
    const ReplayFrame &frame = replay->frames[replay->current_frame];

    /* Check if we should advance to this frame based on timing */
    if (replay->accumulated_time < frame.delta_time && replay->current_frame > 0) {
        /* Not time for this frame yet */
        return 0;
    }

    replay->accumulated_time -= frame.delta_time;
    if (replay->accumulated_time < 0) {
        replay->accumulated_time = 0;
    }

    /* Execute commands for this frame */
    int commands_executed = 0;

    for (const auto &rc : frame.commands) {
        Agentite_Command cmd;
        copy_replay_to_command(&cmd, &rc);

        Agentite_CommandResult result = agentite_command_execute(
            replay->playback_cmd_sys, &cmd, game_state);

        if (result.success) {
            commands_executed++;
        }
    }

    replay->current_time += frame.delta_time;
    replay->current_frame++;

    return commands_executed;
}

/*============================================================================
 * Playback Control
 *============================================================================*/

void agentite_replay_pause(Agentite_ReplaySystem *replay) {
    if (replay && replay->state == AGENTITE_REPLAY_PLAYING) {
        replay->state = AGENTITE_REPLAY_PAUSED;
    }
}

void agentite_replay_resume(Agentite_ReplaySystem *replay) {
    if (replay && replay->state == AGENTITE_REPLAY_PAUSED) {
        replay->state = AGENTITE_REPLAY_PLAYING;
    }
}

void agentite_replay_toggle_pause(Agentite_ReplaySystem *replay) {
    if (!replay) return;

    if (replay->state == AGENTITE_REPLAY_PLAYING) {
        replay->state = AGENTITE_REPLAY_PAUSED;
    } else if (replay->state == AGENTITE_REPLAY_PAUSED) {
        replay->state = AGENTITE_REPLAY_PLAYING;
    }
}

bool agentite_replay_seek(Agentite_ReplaySystem *replay,
                           void *game_state,
                           uint64_t target_frame) {
    AGENTITE_VALIDATE_PTR_RET(replay, false);

    if (replay->state != AGENTITE_REPLAY_PLAYING &&
        replay->state != AGENTITE_REPLAY_PAUSED) {
        agentite_set_error("replay: can only seek during playback");
        return false;
    }

    if (target_frame >= replay->frames.size()) {
        target_frame = replay->frames.size() > 0 ? replay->frames.size() - 1 : 0;
    }

    /* Find nearest snapshot before target frame */
    ReplaySnapshot *best_snapshot = nullptr;

    for (size_t i = 0; i < replay->snapshots.size(); i++) {
        if (replay->snapshots[i].frame_number <= target_frame) {
            if (!best_snapshot ||
                replay->snapshots[i].frame_number > best_snapshot->frame_number) {
                best_snapshot = &replay->snapshots[i];
            }
        }
    }

    uint64_t start_frame = 0;

    if (best_snapshot && best_snapshot->data && replay->config.deserialize) {
        /* Restore from snapshot */
        if (!replay->config.deserialize(game_state, best_snapshot->data,
                                        best_snapshot->size)) {
            agentite_set_error("replay: failed to restore snapshot");
            return false;
        }
        start_frame = best_snapshot->frame_number;
    } else if (target_frame < replay->current_frame) {
        /* No snapshot, need to restart from beginning */
        if (replay->config.reset && game_state) {
            if (!replay->config.reset(game_state, &replay->metadata)) {
                agentite_set_error("replay: failed to reset game state");
                return false;
            }
        }

        if (replay->config.deserialize && game_state &&
            replay->initial_state_data && replay->initial_state_size > 0) {
            if (!replay->config.deserialize(game_state, replay->initial_state_data,
                                            replay->initial_state_size)) {
                agentite_set_error("replay: failed to deserialize initial state");
                return false;
            }
        }
        start_frame = 0;
    } else {
        /* Seeking forward, can continue from current position */
        start_frame = replay->current_frame;
    }

    /* Fast-forward from start_frame to target_frame */
    replay->current_time = 0.0f;
    for (uint64_t i = 0; i < start_frame && i < replay->frames.size(); i++) {
        replay->current_time += replay->frames[i].delta_time;
    }

    for (uint64_t i = start_frame; i < target_frame && i < replay->frames.size(); i++) {
        const ReplayFrame &frame = replay->frames[i];

        /* Execute commands */
        for (const auto &rc : frame.commands) {
            Agentite_Command cmd;
            copy_replay_to_command(&cmd, &rc);
            agentite_command_execute(replay->playback_cmd_sys, &cmd, game_state);
        }

        replay->current_time += frame.delta_time;
    }

    replay->current_frame = target_frame;
    replay->accumulated_time = 0.0f;

    if (replay->on_seek_callback) {
        replay->on_seek_callback(replay, replay->on_seek_userdata);
    }

    return true;
}

bool agentite_replay_seek_percent(Agentite_ReplaySystem *replay,
                                   void *game_state,
                                   float percent) {
    AGENTITE_VALIDATE_PTR_RET(replay, false);

    if (percent < 0.0f) percent = 0.0f;
    if (percent > 1.0f) percent = 1.0f;

    uint64_t target_frame = (uint64_t)(percent * (float)replay->frames.size());
    return agentite_replay_seek(replay, game_state, target_frame);
}

int agentite_replay_step_forward(Agentite_ReplaySystem *replay,
                                  void *game_state) {
    AGENTITE_VALIDATE_PTR_RET(replay, -1);

    if (replay->state != AGENTITE_REPLAY_PAUSED) {
        return -1;
    }

    if (replay->current_frame >= replay->frames.size()) {
        return 0;
    }

    const ReplayFrame &frame = replay->frames[replay->current_frame];
    int commands_executed = 0;

    for (const auto &rc : frame.commands) {
        Agentite_Command cmd;
        copy_replay_to_command(&cmd, &rc);

        Agentite_CommandResult result = agentite_command_execute(
            replay->playback_cmd_sys, &cmd, game_state);

        if (result.success) {
            commands_executed++;
        }
    }

    replay->current_time += frame.delta_time;
    replay->current_frame++;

    return commands_executed;
}

bool agentite_replay_step_backward(Agentite_ReplaySystem *replay,
                                    void *game_state) {
    AGENTITE_VALIDATE_PTR_RET(replay, false);

    if (replay->state != AGENTITE_REPLAY_PAUSED) {
        return false;
    }

    if (replay->current_frame == 0) {
        return true; /* Already at start */
    }

    /* Seek to previous frame */
    return agentite_replay_seek(replay, game_state, replay->current_frame - 1);
}

/*============================================================================
 * Speed Control
 *============================================================================*/

void agentite_replay_set_speed(Agentite_ReplaySystem *replay, float multiplier) {
    if (!replay) return;

    if (multiplier < 0.1f) multiplier = 0.1f;
    if (multiplier > 16.0f) multiplier = 16.0f;

    replay->playback_speed = multiplier;
}

float agentite_replay_get_speed(const Agentite_ReplaySystem *replay) {
    return replay ? replay->playback_speed : 1.0f;
}

/*============================================================================
 * Query State
 *============================================================================*/

Agentite_ReplayState agentite_replay_get_state(const Agentite_ReplaySystem *replay) {
    return replay ? replay->state : AGENTITE_REPLAY_IDLE;
}

bool agentite_replay_is_recording(const Agentite_ReplaySystem *replay) {
    return replay && replay->state == AGENTITE_REPLAY_RECORDING;
}

bool agentite_replay_is_playing(const Agentite_ReplaySystem *replay) {
    return replay && replay->state == AGENTITE_REPLAY_PLAYING;
}

bool agentite_replay_is_paused(const Agentite_ReplaySystem *replay) {
    return replay && replay->state == AGENTITE_REPLAY_PAUSED;
}

uint64_t agentite_replay_get_current_frame(const Agentite_ReplaySystem *replay) {
    return replay ? replay->current_frame : 0;
}

uint64_t agentite_replay_get_total_frames(const Agentite_ReplaySystem *replay) {
    return replay ? replay->frames.size() : 0;
}

float agentite_replay_get_current_time(const Agentite_ReplaySystem *replay) {
    return replay ? replay->current_time : 0.0f;
}

float agentite_replay_get_total_duration(const Agentite_ReplaySystem *replay) {
    return replay ? replay->metadata.total_duration : 0.0f;
}

float agentite_replay_get_progress(const Agentite_ReplaySystem *replay) {
    if (!replay || replay->frames.empty()) {
        return 0.0f;
    }
    return (float)replay->current_frame / (float)replay->frames.size();
}

const Agentite_ReplayMetadata *agentite_replay_get_metadata(const Agentite_ReplaySystem *replay) {
    return replay ? &replay->metadata : nullptr;
}

bool agentite_replay_has_data(const Agentite_ReplaySystem *replay) {
    return replay && !replay->frames.empty();
}

int agentite_replay_get_snapshot_count(const Agentite_ReplaySystem *replay) {
    return replay ? (int)replay->snapshots.size() : 0;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void agentite_replay_set_on_end(Agentite_ReplaySystem *replay,
                                 Agentite_ReplayCallback callback,
                                 void *userdata) {
    if (replay) {
        replay->on_end_callback = callback;
        replay->on_end_userdata = userdata;
    }
}

void agentite_replay_set_on_seek(Agentite_ReplaySystem *replay,
                                  Agentite_ReplayCallback callback,
                                  void *userdata) {
    if (replay) {
        replay->on_seek_callback = callback;
        replay->on_seek_userdata = userdata;
    }
}

/*============================================================================
 * UI Widget
 *============================================================================*/

/* Stub implementations - these require AUI integration */

bool agentite_replay_widget(struct AUI_Context *ui,
                             Agentite_ReplaySystem *replay,
                             void *game_state,
                             int flags) {
    if (!ui || !replay) {
        return false;
    }

    bool interacted = false;

    /* This is a placeholder. Full implementation would use AUI functions */
    /* to render play/pause button, timeline slider, speed controls, etc. */

    (void)game_state;
    (void)flags;

    return interacted;
}

bool agentite_replay_widget_timeline(struct AUI_Context *ui,
                                      Agentite_ReplaySystem *replay,
                                      void *game_state,
                                      float width) {
    (void)ui;
    (void)replay;
    (void)game_state;
    (void)width;
    return false;
}

bool agentite_replay_widget_controls(struct AUI_Context *ui,
                                      Agentite_ReplaySystem *replay) {
    (void)ui;
    (void)replay;
    return false;
}

void agentite_replay_widget_time_display(struct AUI_Context *ui,
                                          const Agentite_ReplaySystem *replay) {
    (void)ui;
    (void)replay;
}

bool agentite_replay_widget_speed_selector(struct AUI_Context *ui,
                                            Agentite_ReplaySystem *replay) {
    (void)ui;
    (void)replay;
    return false;
}

/*============================================================================
 * Utility
 *============================================================================*/

void agentite_replay_clear(Agentite_ReplaySystem *replay) {
    if (!replay) {
        return;
    }

    replay->frames.clear();
    replay->pending_commands.clear();

    /* Free snapshots */
    for (auto &snapshot : replay->snapshots) {
        free_snapshot(&snapshot);
    }
    replay->snapshots.clear();

    /* Free initial state */
    if (replay->initial_state_data) {
        free(replay->initial_state_data);
        replay->initial_state_data = nullptr;
        replay->initial_state_size = 0;
    }

    replay->current_frame = 0;
    replay->current_time = 0.0f;
    replay->accumulated_time = 0.0f;
    replay->frames_since_snapshot = 0;

    /* Reset metadata but keep config */
    memset(&replay->metadata, 0, sizeof(replay->metadata));
    replay->metadata.magic = AGENTITE_REPLAY_MAGIC;
    replay->metadata.version = AGENTITE_REPLAY_VERSION;
    replay->metadata.min_compatible_version = AGENTITE_REPLAY_MIN_VERSION;
}

int agentite_replay_format_time(float seconds, char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) {
        return 0;
    }

    int total_seconds = (int)seconds;
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int secs = total_seconds % 60;

    if (hours > 0) {
        return snprintf(buffer, buffer_size, "%d:%02d:%02d", hours, minutes, secs);
    } else {
        return snprintf(buffer, buffer_size, "%d:%02d", minutes, secs);
    }
}
