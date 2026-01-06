/**
 * Agentite Engine - Replay System Example
 *
 * Demonstrates the replay system with command recording and playback.
 * Controls:
 *
 *   WASD / Arrow Keys - Move the player (while recording)
 *   SPACE - Attack (while recording)
 *
 *   R - Start/Stop Recording
 *   P - Start Playback
 *   S - Save replay to file
 *   L - Load replay from file
 *
 *   LEFT/RIGHT - Seek backward/forward (while playing)
 *   UP/DOWN - Speed up/slow down playback
 *   ENTER - Pause/Resume playback
 *
 *   ESC - Quit
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/replay.h"
#include "agentite/command.h"
#include "agentite/input.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;
static const char *REPLAY_FILE = "game_replay.replay";

/* Command types */
enum GameCommandType {
    CMD_MOVE = 1,
    CMD_ATTACK = 2,
};

/* Simple game state */
struct GameState {
    float player_x;
    float player_y;
    int health;
    int score;
    int move_count;
    int attack_count;
    bool is_attacking;
    float attack_timer;
};

/* Static instances */
static Agentite_ReplaySystem *s_replay = NULL;
static Agentite_CommandSystem *s_cmd_sys = NULL;
static GameState s_game_state;
static bool s_recording = false;
static bool s_playing = false;

/* Serialization callbacks */
static bool serialize_game_state(void *game_state, void **out_data, size_t *out_size) {
    GameState *state = static_cast<GameState *>(game_state);
    GameState *copy = static_cast<GameState *>(malloc(sizeof(GameState)));
    if (!copy) return false;

    *copy = *state;
    *out_data = copy;
    *out_size = sizeof(GameState);
    return true;
}

static bool deserialize_game_state(void *game_state, const void *data, size_t size) {
    if (size != sizeof(GameState)) return false;

    GameState *state = static_cast<GameState *>(game_state);
    const GameState *saved = static_cast<const GameState *>(data);
    *state = *saved;
    return true;
}

static bool reset_game_state(void *game_state, const Agentite_ReplayMetadata *metadata) {
    (void)metadata;
    GameState *state = static_cast<GameState *>(game_state);
    state->player_x = WINDOW_WIDTH / 2.0f;
    state->player_y = WINDOW_HEIGHT / 2.0f;
    state->health = 100;
    state->score = 0;
    state->move_count = 0;
    state->attack_count = 0;
    state->is_attacking = false;
    state->attack_timer = 0.0f;
    return true;
}

/* Command validators and executors */
static bool validate_move(const Agentite_Command *cmd, void *game_state,
                          char *error_buf, size_t error_size) {
    (void)game_state;
    float dx = agentite_command_get_float(cmd, "dx");
    float dy = agentite_command_get_float(cmd, "dy");

    if (fabsf(dx) > 1000.0f || fabsf(dy) > 1000.0f) {
        snprintf(error_buf, error_size, "Invalid move delta");
        return false;
    }
    return true;
}

static bool execute_move(const Agentite_Command *cmd, void *game_state) {
    GameState *state = static_cast<GameState *>(game_state);
    float dx = agentite_command_get_float(cmd, "dx");
    float dy = agentite_command_get_float(cmd, "dy");

    state->player_x += dx;
    state->player_y += dy;
    state->move_count++;

    /* Clamp to window bounds */
    if (state->player_x < 20) state->player_x = 20;
    if (state->player_x > WINDOW_WIDTH - 20) state->player_x = WINDOW_WIDTH - 20;
    if (state->player_y < 20) state->player_y = 20;
    if (state->player_y > WINDOW_HEIGHT - 20) state->player_y = WINDOW_HEIGHT - 20;

    return true;
}

static bool execute_attack(const Agentite_Command *cmd, void *game_state) {
    (void)cmd;
    GameState *state = static_cast<GameState *>(game_state);
    state->is_attacking = true;
    state->attack_timer = 0.2f;
    state->attack_count++;
    state->score += 10;
    return true;
}

/* Issue a move command */
static void issue_move(float dx, float dy) {
    Agentite_Command *cmd = agentite_command_new(CMD_MOVE);
    agentite_command_set_float(cmd, "dx", dx);
    agentite_command_set_float(cmd, "dy", dy);
    agentite_command_execute(s_cmd_sys, cmd, &s_game_state);
    agentite_command_free(cmd);
}

/* Issue an attack command */
static void issue_attack(void) {
    Agentite_Command *cmd = agentite_command_new(CMD_ATTACK);
    agentite_command_execute(s_cmd_sys, cmd, &s_game_state);
    agentite_command_free(cmd);
}

/* Create a colored square texture */
static Agentite_Texture *create_square_texture(Agentite_SpriteRenderer *sr,
                                                unsigned char r, unsigned char g,
                                                unsigned char b) {
    const int size = 32;
    unsigned char *pixels = static_cast<unsigned char *>(malloc(size * size * 4));
    if (!pixels) return NULL;

    for (int i = 0; i < size * size; i++) {
        pixels[i * 4 + 0] = r;
        pixels[i * 4 + 1] = g;
        pixels[i * 4 + 2] = b;
        pixels[i * 4 + 3] = 255;
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Initialize engine */
    Agentite_Config config = AGENTITE_DEFAULT_CONFIG;
    config.window_title = "Agentite Replay Example";
    config.window_width = WINDOW_WIDTH;
    config.window_height = WINDOW_HEIGHT;

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine: %s\n", agentite_get_last_error());
        return 1;
    }

    /* Initialize subsystems */
    SDL_GPUDevice *gpu = agentite_get_gpu_device(engine);
    SDL_Window *window = agentite_get_window(engine);

    Agentite_SpriteRenderer *sr = agentite_sprite_init(gpu, window);
    if (!sr) {
        fprintf(stderr, "Failed to create sprite renderer\n");
        agentite_shutdown(engine);
        return 1;
    }

    Agentite_TextRenderer *tr = agentite_text_init(gpu, window);
    if (!tr) {
        fprintf(stderr, "Failed to create text renderer\n");
        agentite_sprite_shutdown(sr);
        agentite_shutdown(engine);
        return 1;
    }

    Agentite_Font *font = agentite_font_load(tr, "assets/fonts/Roboto-Regular.ttf", 20);
    if (!font) {
        /* Try fallback path */
        font = agentite_font_load(tr, "/System/Library/Fonts/Helvetica.ttc", 20);
    }
    if (!font) {
        fprintf(stderr, "Warning: Could not load font\n");
    }

    Agentite_Input *input = agentite_input_init();

    /* Create command system */
    s_cmd_sys = agentite_command_create();
    agentite_command_register(s_cmd_sys, CMD_MOVE, validate_move, execute_move);
    agentite_command_register(s_cmd_sys, CMD_ATTACK, NULL, execute_attack);

    /* Create replay system */
    Agentite_ReplayConfig replay_config = AGENTITE_REPLAY_CONFIG_DEFAULT;
    replay_config.serialize = serialize_game_state;
    replay_config.deserialize = deserialize_game_state;
    replay_config.reset = reset_game_state;
    replay_config.snapshot_interval = 60; /* Snapshot every ~1 second at 60fps */

    s_replay = agentite_replay_create(&replay_config);
    if (!s_replay) {
        fprintf(stderr, "Failed to create replay system\n");
        return 1;
    }

    /* Initialize game state */
    reset_game_state(&s_game_state, NULL);

    /* Create textures */
    Agentite_Texture *player_tex = create_square_texture(sr, 50, 150, 255);
    Agentite_Texture *attack_tex = create_square_texture(sr, 255, 100, 100);

    /* Player sprite */
    Agentite_Sprite player_sprite = {};
    player_sprite.texture = player_tex;
    player_sprite.src_x = 0;
    player_sprite.src_y = 0;
    player_sprite.src_w = 32;
    player_sprite.src_h = 32;
    player_sprite.origin_x = 0.5f;
    player_sprite.origin_y = 0.5f;

    /* Attack indicator sprite */
    Agentite_Sprite attack_sprite = {};
    attack_sprite.texture = attack_tex;
    attack_sprite.src_x = 0;
    attack_sprite.src_y = 0;
    attack_sprite.src_w = 32;
    attack_sprite.src_h = 32;
    attack_sprite.origin_x = 0.5f;
    attack_sprite.origin_y = 0.5f;

    const float MOVE_SPEED = 200.0f;

    printf("Replay System Example\n");
    printf("=====================\n");
    printf("WASD/Arrows: Move | SPACE: Attack | R: Record | P: Play\n");
    printf("S: Save | L: Load | UP/DOWN: Speed | LEFT/RIGHT: Seek\n");
    printf("ENTER: Pause/Resume | ESC: Quit\n\n");

    /* Main loop */
    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);

        /* Input handling */
        agentite_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                SDL_Keycode key = event.key.key;

                /* Recording controls */
                if (key == SDLK_R) {
                    if (s_recording) {
                        agentite_replay_stop_recording(s_replay);
                        s_recording = false;
                        printf("Recording stopped. %llu frames recorded.\n",
                               (unsigned long long)agentite_replay_get_total_frames(s_replay));
                    } else if (!s_playing) {
                        reset_game_state(&s_game_state, NULL);
                        Agentite_ReplayMetadata meta = {};
                        strncpy(meta.map_name, "Example Map", sizeof(meta.map_name) - 1);
                        strncpy(meta.game_version, "1.0.0", sizeof(meta.game_version) - 1);

                        if (agentite_replay_start_recording(s_replay, s_cmd_sys,
                                                            &s_game_state, &meta)) {
                            s_recording = true;
                            printf("Recording started.\n");
                        }
                    }
                }
                /* Playback controls */
                else if (key == SDLK_P && !s_recording) {
                    if (s_playing) {
                        agentite_replay_stop_playback(s_replay);
                        s_playing = false;
                        printf("Playback stopped.\n");
                    } else if (agentite_replay_has_data(s_replay)) {
                        if (agentite_replay_start_playback(s_replay, s_cmd_sys,
                                                           &s_game_state)) {
                            s_playing = true;
                            printf("Playback started.\n");
                        }
                    } else {
                        printf("No replay data to play.\n");
                    }
                }
                /* Save replay */
                else if (key == SDLK_S && !s_recording && !s_playing) {
                    if (agentite_replay_has_data(s_replay)) {
                        if (agentite_replay_save(s_replay, REPLAY_FILE)) {
                            printf("Replay saved to %s\n", REPLAY_FILE);
                        } else {
                            printf("Failed to save replay: %s\n", agentite_get_last_error());
                        }
                    } else {
                        printf("No replay data to save.\n");
                    }
                }
                /* Load replay */
                else if (key == SDLK_L && !s_recording && !s_playing) {
                    if (agentite_replay_load(s_replay, REPLAY_FILE)) {
                        const Agentite_ReplayMetadata *meta = agentite_replay_get_metadata(s_replay);
                        printf("Replay loaded: %llu frames, %.1f seconds\n",
                               (unsigned long long)meta->total_frames,
                               meta->total_duration);
                    } else {
                        printf("Failed to load replay: %s\n", agentite_get_last_error());
                    }
                }
                /* Pause/resume playback */
                else if (key == SDLK_RETURN && s_playing) {
                    agentite_replay_toggle_pause(s_replay);
                    printf("Playback %s\n", agentite_replay_is_paused(s_replay) ? "paused" : "resumed");
                }
                /* Speed controls */
                else if (key == SDLK_UP && s_playing) {
                    float speed = agentite_replay_get_speed(s_replay);
                    agentite_replay_set_speed(s_replay, speed * 2.0f);
                    printf("Playback speed: %.1fx\n", agentite_replay_get_speed(s_replay));
                }
                else if (key == SDLK_DOWN && s_playing) {
                    float speed = agentite_replay_get_speed(s_replay);
                    agentite_replay_set_speed(s_replay, speed * 0.5f);
                    printf("Playback speed: %.1fx\n", agentite_replay_get_speed(s_replay));
                }
                /* Seek controls */
                else if (key == SDLK_LEFT && s_playing) {
                    uint64_t frame = agentite_replay_get_current_frame(s_replay);
                    if (frame > 60) {
                        agentite_replay_seek(s_replay, &s_game_state, frame - 60);
                    } else {
                        agentite_replay_seek(s_replay, &s_game_state, 0);
                    }
                }
                else if (key == SDLK_RIGHT && s_playing) {
                    uint64_t frame = agentite_replay_get_current_frame(s_replay);
                    agentite_replay_seek(s_replay, &s_game_state, frame + 60);
                }
                /* Attack during recording */
                else if (key == SDLK_SPACE && s_recording) {
                    issue_attack();
                }
                /* Quit */
                else if (key == SDLK_ESCAPE) {
                    agentite_quit(engine);
                }
            }
            agentite_input_process_event(input, &event);
        }
        agentite_input_update(input);

        /* Game logic */
        if (s_recording) {
            /* Handle movement during recording */
            float dx = 0, dy = 0;
            if (agentite_input_key_pressed(input, SDL_SCANCODE_W) ||
                agentite_input_key_pressed(input, SDL_SCANCODE_UP)) {
                dy -= MOVE_SPEED * dt;
            }
            if (agentite_input_key_pressed(input, SDL_SCANCODE_S) ||
                agentite_input_key_pressed(input, SDL_SCANCODE_DOWN)) {
                dy += MOVE_SPEED * dt;
            }
            if (agentite_input_key_pressed(input, SDL_SCANCODE_A) ||
                agentite_input_key_pressed(input, SDL_SCANCODE_LEFT)) {
                dx -= MOVE_SPEED * dt;
            }
            if (agentite_input_key_pressed(input, SDL_SCANCODE_D) ||
                agentite_input_key_pressed(input, SDL_SCANCODE_RIGHT)) {
                dx += MOVE_SPEED * dt;
            }

            if (dx != 0 || dy != 0) {
                issue_move(dx, dy);
            }

            /* Record frame */
            agentite_replay_record_frame(s_replay, dt);

            /* Create snapshot periodically */
            if (agentite_replay_get_current_frame(s_replay) % 60 == 0) {
                agentite_replay_create_snapshot(s_replay, &s_game_state);
            }
        }
        else if (s_playing) {
            /* Playback frame */
            float speed = agentite_replay_get_speed(s_replay);
            int cmds = agentite_replay_playback_frame(s_replay, &s_game_state, dt * speed);
            (void)cmds;

            /* Check if playback ended */
            if (!agentite_replay_is_playing(s_replay) && !agentite_replay_is_paused(s_replay)) {
                s_playing = false;
                printf("Playback finished.\n");
            }
        }

        /* Update attack timer */
        if (s_game_state.is_attacking) {
            s_game_state.attack_timer -= dt;
            if (s_game_state.attack_timer <= 0) {
                s_game_state.is_attacking = false;
            }
        }

        /* Rendering */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);

        /* Upload sprites */
        agentite_sprite_begin(sr, NULL);

        /* Draw player */
        agentite_sprite_draw(sr, &player_sprite,
                            s_game_state.player_x, s_game_state.player_y);

        /* Draw attack indicator */
        if (s_game_state.is_attacking) {
            agentite_sprite_draw(sr, &attack_sprite,
                                s_game_state.player_x, s_game_state.player_y);
        }

        agentite_sprite_upload(sr, cmd);

        /* Upload text */
        if (font) {
            agentite_text_begin(tr);

            char status[256];
            const char *mode = s_recording ? "RECORDING" :
                              (s_playing ? (agentite_replay_is_paused(s_replay) ? "PAUSED" : "PLAYING") : "IDLE");

            if (s_playing || agentite_replay_has_data(s_replay)) {
                char time_str[32];
                agentite_replay_format_time(agentite_replay_get_current_time(s_replay),
                                            time_str, sizeof(time_str));
                char total_str[32];
                agentite_replay_format_time(agentite_replay_get_total_duration(s_replay),
                                            total_str, sizeof(total_str));

                snprintf(status, sizeof(status),
                         "%s | Frame: %llu/%llu | Time: %s/%s | Speed: %.1fx",
                         mode,
                         (unsigned long long)agentite_replay_get_current_frame(s_replay),
                         (unsigned long long)agentite_replay_get_total_frames(s_replay),
                         time_str, total_str,
                         agentite_replay_get_speed(s_replay));
            } else {
                snprintf(status, sizeof(status), "%s", mode);
            }

            agentite_text_draw_colored(tr, font, status, 20, 40, 1.0f, 1.0f, 1.0f, 1.0f);

            /* Game state info */
            char state_info[256];
            snprintf(state_info, sizeof(state_info),
                     "Position: (%.0f, %.0f) | Moves: %d | Attacks: %d | Score: %d",
                     s_game_state.player_x, s_game_state.player_y,
                     s_game_state.move_count, s_game_state.attack_count,
                     s_game_state.score);
            agentite_text_draw_colored(tr, font, state_info, 20, 65, 0.8f, 0.8f, 0.8f, 1.0f);

            /* Controls help - context-sensitive */
            if (s_recording) {
                agentite_text_draw_colored(tr, font, "RECORDING MODE", 20, 100, 1.0f, 0.3f, 0.3f, 1.0f);
                agentite_text_draw_colored(tr, font, "WASD / Arrows - Move the player", 20, 125, 0.7f, 0.7f, 0.7f, 1.0f);
                agentite_text_draw_colored(tr, font, "SPACE - Attack", 20, 145, 0.7f, 0.7f, 0.7f, 1.0f);
                agentite_text_draw_colored(tr, font, "R - Stop recording", 20, 165, 0.7f, 0.7f, 0.7f, 1.0f);
            } else if (s_playing) {
                const char *play_mode = agentite_replay_is_paused(s_replay) ? "PAUSED" : "PLAYING";
                float r = agentite_replay_is_paused(s_replay) ? 1.0f : 0.3f;
                float g = agentite_replay_is_paused(s_replay) ? 0.7f : 1.0f;
                agentite_text_draw_colored(tr, font, play_mode, 20, 100, r, g, 0.3f, 1.0f);
                agentite_text_draw_colored(tr, font, "ENTER - Pause/Resume", 20, 125, 0.7f, 0.7f, 0.7f, 1.0f);
                agentite_text_draw_colored(tr, font, "UP/DOWN - Speed up/slow down", 20, 145, 0.7f, 0.7f, 0.7f, 1.0f);
                agentite_text_draw_colored(tr, font, "LEFT/RIGHT - Seek -/+ 1 second", 20, 165, 0.7f, 0.7f, 0.7f, 1.0f);
                agentite_text_draw_colored(tr, font, "P - Stop playback", 20, 185, 0.7f, 0.7f, 0.7f, 1.0f);
            } else {
                /* Idle mode */
                agentite_text_draw_colored(tr, font, "REPLAY SYSTEM DEMO", 20, 100, 0.5f, 0.8f, 1.0f, 1.0f);
                agentite_text_draw_colored(tr, font, "R - Start recording", 20, 130, 0.7f, 0.7f, 0.7f, 1.0f);
                if (agentite_replay_has_data(s_replay)) {
                    agentite_text_draw_colored(tr, font, "P - Play recording", 20, 150, 0.7f, 0.7f, 0.7f, 1.0f);
                    agentite_text_draw_colored(tr, font, "S - Save to file", 20, 170, 0.7f, 0.7f, 0.7f, 1.0f);
                }
                agentite_text_draw_colored(tr, font, "L - Load from file", 20, 190, 0.7f, 0.7f, 0.7f, 1.0f);
                agentite_text_draw_colored(tr, font, "ESC - Quit", 20, 210, 0.7f, 0.7f, 0.7f, 1.0f);
            }

            /* Bottom help text */
            agentite_text_draw_colored(tr, font, "Blue square = player | Red flash = attack",
                                       20, WINDOW_HEIGHT - 30, 0.5f, 0.5f, 0.5f, 1.0f);

            agentite_text_end(tr);
            agentite_text_upload(tr, cmd);
        }

        /* Render */
        if (agentite_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
            SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
            agentite_sprite_render(sr, cmd, pass);
            if (font) {
                agentite_text_render(tr, cmd, pass);
            }
            agentite_end_render_pass(engine);
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    if (player_tex) agentite_texture_destroy(sr, player_tex);
    if (attack_tex) agentite_texture_destroy(sr, attack_tex);
    if (font) agentite_font_destroy(tr, font);

    agentite_replay_destroy(s_replay);
    agentite_command_destroy(s_cmd_sys);
    agentite_input_shutdown(input);
    agentite_text_shutdown(tr);
    agentite_sprite_shutdown(sr);
    agentite_shutdown(engine);

    return 0;
}
