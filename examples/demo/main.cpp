#include "agentite/agentite.h"
#include "agentite/ui.h"
#include "agentite/ecs.h"
#include "agentite/sprite.h"
#include "agentite/camera.h"
#include "agentite/input.h"
#include "agentite/audio.h"
#include "agentite/tilemap.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Helper: Generate a procedural WAV beep sound in memory */
static void *create_test_beep_wav(int frequency, float duration, float volume, size_t *out_size)
{
    int sample_rate = 48000;
    int num_samples = (int)(sample_rate * duration);
    int num_channels = 2;  /* stereo */
    int bits_per_sample = 16;
    int bytes_per_sample = bits_per_sample / 8;
    int data_size = num_samples * num_channels * bytes_per_sample;

    /* WAV header is 44 bytes */
    int wav_size = 44 + data_size;
    unsigned char *wav = (unsigned char *)malloc(wav_size);
    if (!wav) return NULL;

    /* Write WAV header */
    memcpy(wav, "RIFF", 4);
    *(int *)(wav + 4) = wav_size - 8;
    memcpy(wav + 8, "WAVE", 4);
    memcpy(wav + 12, "fmt ", 4);
    *(int *)(wav + 16) = 16;  /* fmt chunk size */
    *(short *)(wav + 20) = 1;  /* PCM format */
    *(short *)(wav + 22) = num_channels;
    *(int *)(wav + 24) = sample_rate;
    *(int *)(wav + 28) = sample_rate * num_channels * bytes_per_sample;  /* byte rate */
    *(short *)(wav + 32) = num_channels * bytes_per_sample;  /* block align */
    *(short *)(wav + 34) = bits_per_sample;
    memcpy(wav + 36, "data", 4);
    *(int *)(wav + 40) = data_size;

    /* Generate sine wave with fade in/out */
    short *samples = (short *)(wav + 44);
    float fade_samples = sample_rate * 0.02f;  /* 20ms fade */

    for (int i = 0; i < num_samples; i++) {
        float t = (float)i / sample_rate;
        float sample = sinf(2.0f * 3.14159265f * frequency * t);

        /* Apply volume */
        sample *= volume;

        /* Apply fade in/out envelope */
        float envelope = 1.0f;
        if (i < fade_samples) {
            envelope = (float)i / fade_samples;
        } else if (i > num_samples - fade_samples) {
            envelope = (float)(num_samples - i) / fade_samples;
        }
        sample *= envelope;

        /* Convert to 16-bit */
        short s = (short)(sample * 32767);
        samples[i * 2] = s;      /* left */
        samples[i * 2 + 1] = s;  /* right */
    }

    *out_size = wav_size;
    return wav;
}

/* Helper: Create a procedural tileset texture (4x4 grid of different colored tiles) */
static Agentite_Texture *create_tileset_texture(Agentite_SpriteRenderer *sr, int tile_size)
{
    int cols = 4, rows = 4;
    int size = tile_size * cols;
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
    if (!pixels) return NULL;

    /* Define 16 different tile colors */
    unsigned char colors[16][3] = {
        {34, 139, 34},    /* 0: Forest green (grass) */
        {50, 205, 50},    /* 1: Lime green (light grass) */
        {107, 142, 35},   /* 2: Olive drab (dark grass) */
        {144, 238, 144},  /* 3: Light green (meadow) */
        {64, 64, 64},     /* 4: Dark gray (stone) */
        {128, 128, 128},  /* 5: Gray (cobblestone) */
        {169, 169, 169},  /* 6: Dark gray (gravel) */
        {192, 192, 192},  /* 7: Silver (marble) */
        {139, 69, 19},    /* 8: Saddle brown (dirt) */
        {160, 82, 45},    /* 9: Sienna (path) */
        {210, 180, 140},  /* 10: Tan (sand) */
        {244, 164, 96},   /* 11: Sandy brown (desert) */
        {65, 105, 225},   /* 12: Royal blue (water) */
        {30, 144, 255},   /* 13: Dodger blue (shallow water) */
        {139, 0, 0},      /* 14: Dark red (lava) */
        {255, 215, 0}     /* 15: Gold (treasure) */
    };

    for (int ty = 0; ty < rows; ty++) {
        for (int tx = 0; tx < cols; tx++) {
            int tile_idx = ty * cols + tx;
            unsigned char r = colors[tile_idx][0];
            unsigned char g = colors[tile_idx][1];
            unsigned char b = colors[tile_idx][2];

            /* Fill this tile with solid color + subtle variation */
            for (int py = 0; py < tile_size; py++) {
                for (int px = 0; px < tile_size; px++) {
                    int x = tx * tile_size + px;
                    int y = ty * tile_size + py;
                    int idx = (y * size + x) * 4;

                    /* Add subtle noise/pattern */
                    int noise = ((px ^ py) & 1) * 8;

                    pixels[idx + 0] = (unsigned char)(r + noise > 255 ? 255 : r + noise);
                    pixels[idx + 1] = (unsigned char)(g + noise > 255 ? 255 : g + noise);
                    pixels[idx + 2] = (unsigned char)(b + noise > 255 ? 255 : b + noise);
                    pixels[idx + 3] = 255;
                }
            }
        }
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

/* Helper: Create a procedural checkerboard texture */
static Agentite_Texture *create_test_texture(Agentite_SpriteRenderer *sr, int size, int tile_size)
{
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
    if (!pixels) return NULL;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int tx = x / tile_size;
            int ty = y / tile_size;
            bool white = ((tx + ty) % 2) == 0;

            int idx = (y * size + x) * 4;
            if (white) {
                pixels[idx + 0] = 255;  /* R */
                pixels[idx + 1] = 200;  /* G */
                pixels[idx + 2] = 100;  /* B */
                pixels[idx + 3] = 255;  /* A */
            } else {
                pixels[idx + 0] = 100;  /* R */
                pixels[idx + 1] = 150;  /* G */
                pixels[idx + 2] = 255;  /* B */
                pixels[idx + 3] = 255;  /* A */
            }
        }
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Configure engine */
    Agentite_Config config = {
        .window_title = "Agentite Engine - Tilemap Demo",
        .window_width = 1280,
        .window_height = 720,
        .fullscreen = false,
        .vsync = true
    };

    /* Initialize engine */
    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize Agentite Engine\n");
        return 1;
    }

    /* Initialize UI system */
    AUI_Context *ui = aui_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine),
        config.window_width,
        config.window_height,
        "assets/fonts/Roboto-Regular.ttf",  /* Font path */
        16.0f                                /* Font size */
    );

    if (!ui) {
        fprintf(stderr, "Failed to initialize UI system\n");
        agentite_shutdown(engine);
        return 1;
    }

    /* Set DPI scale for input coordinate conversion (logical coords used throughout) */
    float dpi_scale = agentite_get_dpi_scale(engine);
    aui_set_dpi_scale(ui, dpi_scale);

    /* Initialize sprite renderer */
    Agentite_SpriteRenderer *sprites = agentite_sprite_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine)
    );

    if (!sprites) {
        fprintf(stderr, "Failed to initialize sprite renderer\n");
        aui_shutdown(ui);
        agentite_shutdown(engine);
        return 1;
    }

    /* Initialize camera */
    Agentite_Camera *camera = agentite_camera_create(
        (float)config.window_width,
        (float)config.window_height
    );
    if (!camera) {
        fprintf(stderr, "Failed to create camera\n");
        agentite_sprite_shutdown(sprites);
        aui_shutdown(ui);
        agentite_shutdown(engine);
        return 1;
    }

    /* Connect camera to sprite renderer */
    agentite_sprite_set_camera(sprites, camera);

    /* Center camera on the tilemap (50x50 tiles * 48px = 2400x2400, center at 1200,1200) */
    agentite_camera_set_position(camera, 1200.0f, 1200.0f);

    SDL_Log("Camera initialized at (1200, 1200)");

    /* Create test texture */
    Agentite_Texture *tex_checker = create_test_texture(sprites, 64, 8);

    if (!tex_checker) {
        fprintf(stderr, "Failed to create test texture\n");
        agentite_sprite_shutdown(sprites);
        aui_shutdown(ui);
        agentite_shutdown(engine);
        return 1;
    }

    /* Create sprite from texture */
    Agentite_Sprite sprite_checker = agentite_sprite_from_texture(tex_checker);

    SDL_Log("Sprite system initialized with test textures");

    /* Initialize text renderer */
    Agentite_TextRenderer *text = agentite_text_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine)
    );
    if (!text) {
        fprintf(stderr, "Failed to initialize text renderer\n");
        agentite_texture_destroy(sprites, tex_checker);
        agentite_camera_destroy(camera);
        agentite_sprite_shutdown(sprites);
        aui_shutdown(ui);
        agentite_shutdown(engine);
        return 1;
    }

    /* Load fonts */
    Agentite_Font *font_large = agentite_font_load(text, "assets/fonts/Roboto-Regular.ttf", 32.0f);
    Agentite_Font *font_small = agentite_font_load(text, "assets/fonts/Roboto-Regular.ttf", 18.0f);

    if (!font_large || !font_small) {
        SDL_Log("Warning: Could not load fonts, text rendering will be skipped");
    } else {
        SDL_Log("Text system initialized with fonts");
    }

    /* Load MSDF font for sharp text at any scale */
    Agentite_SDFFont *msdf_font = agentite_sdf_font_load(text,
        "assets/fonts/Roboto-Regular-msdf.png",
        "assets/fonts/Roboto-Regular-msdf.json");

    if (!msdf_font) {
        SDL_Log("Warning: Could not load MSDF font, SDF text rendering will be skipped");
    } else {
        SDL_Log("MSDF font loaded successfully (type: %s)",
                agentite_sdf_font_get_type(msdf_font) == AGENTITE_SDF_TYPE_MSDF ? "MSDF" : "SDF");
    }

    /* Initialize ECS world */
    Agentite_World *ecs_world = agentite_ecs_init();
    if (!ecs_world) {
        fprintf(stderr, "Failed to initialize ECS world\n");
        agentite_texture_destroy(sprites, tex_checker);
        agentite_sprite_shutdown(sprites);
        aui_shutdown(ui);
        agentite_shutdown(engine);
        return 1;
    }

    /* Create some demo entities */
    ecs_world_t *w = agentite_ecs_get_world(ecs_world);

    ecs_entity_t player = agentite_ecs_entity_new_named(ecs_world, "Player");
    C_Position player_pos = { .x = 100.0f, .y = 100.0f };
    C_Velocity player_vel = { .vx = 0.0f, .vy = 0.0f };
    C_Health player_hp = { .health = 100, .max_health = 100 };
    ecs_set_ptr(w, player, C_Position, &player_pos);
    ecs_set_ptr(w, player, C_Velocity, &player_vel);
    ecs_set_ptr(w, player, C_Health, &player_hp);

    ecs_entity_t enemy = agentite_ecs_entity_new_named(ecs_world, "Enemy");
    C_Position enemy_pos_init = { .x = 500.0f, .y = 300.0f };
    C_Velocity enemy_vel_init = { .vx = -10.0f, .vy = 5.0f };
    C_Health enemy_hp = { .health = 50, .max_health = 50 };
    ecs_set_ptr(w, enemy, C_Position, &enemy_pos_init);
    ecs_set_ptr(w, enemy, C_Velocity, &enemy_vel_init);
    ecs_set_ptr(w, enemy, C_Health, &enemy_hp);

    SDL_Log("Created player entity: %llu", (unsigned long long)player);
    SDL_Log("Created enemy entity: %llu", (unsigned long long)enemy);

    /* Initialize input system */
    Agentite_Input *input = agentite_input_init();
    if (!input) {
        fprintf(stderr, "Failed to initialize input system\n");
        agentite_ecs_shutdown(ecs_world);
        agentite_texture_destroy(sprites, tex_checker);
        agentite_camera_destroy(camera);
        agentite_sprite_shutdown(sprites);
        aui_shutdown(ui);
        agentite_shutdown(engine);
        return 1;
    }

    /* Register input actions and bind keys */
    int action_cam_up = agentite_input_register_action(input, "cam_up");
    int action_cam_down = agentite_input_register_action(input, "cam_down");
    int action_cam_left = agentite_input_register_action(input, "cam_left");
    int action_cam_right = agentite_input_register_action(input, "cam_right");
    int action_cam_rot_left = agentite_input_register_action(input, "cam_rot_left");
    int action_cam_rot_right = agentite_input_register_action(input, "cam_rot_right");
    int action_cam_reset = agentite_input_register_action(input, "cam_reset");
    int action_zoom_in = agentite_input_register_action(input, "zoom_in");
    int action_zoom_out = agentite_input_register_action(input, "zoom_out");
    int action_quit = agentite_input_register_action(input, "quit");

    /* Bind keyboard keys */
    agentite_input_bind_key(input, action_cam_up, SDL_SCANCODE_W);
    agentite_input_bind_key(input, action_cam_up, SDL_SCANCODE_UP);
    agentite_input_bind_key(input, action_cam_down, SDL_SCANCODE_S);
    agentite_input_bind_key(input, action_cam_down, SDL_SCANCODE_DOWN);
    agentite_input_bind_key(input, action_cam_left, SDL_SCANCODE_A);
    agentite_input_bind_key(input, action_cam_left, SDL_SCANCODE_LEFT);
    agentite_input_bind_key(input, action_cam_right, SDL_SCANCODE_D);
    agentite_input_bind_key(input, action_cam_right, SDL_SCANCODE_RIGHT);
    agentite_input_bind_key(input, action_cam_rot_left, SDL_SCANCODE_Q);
    agentite_input_bind_key(input, action_cam_rot_right, SDL_SCANCODE_E);
    agentite_input_bind_key(input, action_cam_reset, SDL_SCANCODE_R);
    agentite_input_bind_key(input, action_quit, SDL_SCANCODE_ESCAPE);

    /* Bind gamepad (if connected) */
    agentite_input_bind_gamepad_axis(input, action_cam_left, SDL_GAMEPAD_AXIS_LEFTX, 0.3f, false);
    agentite_input_bind_gamepad_axis(input, action_cam_right, SDL_GAMEPAD_AXIS_LEFTX, 0.3f, true);
    agentite_input_bind_gamepad_axis(input, action_cam_up, SDL_GAMEPAD_AXIS_LEFTY, 0.3f, false);
    agentite_input_bind_gamepad_axis(input, action_cam_down, SDL_GAMEPAD_AXIS_LEFTY, 0.3f, true);
    agentite_input_bind_gamepad_button(input, action_cam_rot_left, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    agentite_input_bind_gamepad_button(input, action_cam_rot_right, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    agentite_input_bind_gamepad_button(input, action_cam_reset, SDL_GAMEPAD_BUTTON_SOUTH);
    agentite_input_bind_gamepad_axis(input, action_zoom_in, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.1f, true);
    agentite_input_bind_gamepad_axis(input, action_zoom_out, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0.1f, true);
    agentite_input_bind_gamepad_button(input, action_quit, SDL_GAMEPAD_BUTTON_BACK);

    SDL_Log("Input system initialized with action bindings");

    /* Initialize audio system */
    Agentite_Audio *audio = agentite_audio_init();
    if (!audio) {
        fprintf(stderr, "Failed to initialize audio system\n");
        agentite_input_shutdown(input);
        agentite_ecs_shutdown(ecs_world);
        agentite_texture_destroy(sprites, tex_checker);
        agentite_camera_destroy(camera);
        agentite_sprite_shutdown(sprites);
        aui_shutdown(ui);
        agentite_shutdown(engine);
        return 1;
    }

    /* Create procedural test sounds */
    size_t beep_size, click_size, ping_size;
    void *beep_wav = create_test_beep_wav(440, 0.15f, 0.5f, &beep_size);   /* A4 note, short */
    void *click_wav = create_test_beep_wav(880, 0.05f, 0.3f, &click_size); /* A5 note, click */
    void *ping_wav = create_test_beep_wav(1760, 0.3f, 0.4f, &ping_size);   /* A6 note, ping */

    Agentite_Sound *sound_beep = beep_wav ? agentite_sound_load_wav_memory(audio, beep_wav, beep_size) : NULL;
    Agentite_Sound *sound_click = click_wav ? agentite_sound_load_wav_memory(audio, click_wav, click_size) : NULL;
    Agentite_Sound *sound_ping = ping_wav ? agentite_sound_load_wav_memory(audio, ping_wav, ping_size) : NULL;

    free(beep_wav);
    free(click_wav);
    free(ping_wav);

    SDL_Log("Audio system initialized with test sounds");

    /* Initialize tilemap system */
    Agentite_Texture *tileset_tex = create_tileset_texture(sprites, 48);  /* 48x48 pixel tiles */
    if (!tileset_tex) {
        fprintf(stderr, "Failed to create tileset texture\n");
    }

    Agentite_Tileset *tileset = tileset_tex ? agentite_tileset_create(tileset_tex, 48, 48) : NULL;
    Agentite_Tilemap *tilemap = NULL;

    if (tileset) {
        /* Create a 50x50 tile map with 48px tiles (2400x2400 pixels in world space)
         * At 1280x720 viewport, ~27x15 tiles visible at zoom 1.0 (~400 tiles)
         * With 2 layers that's ~800 sprites, well under the 4096 batch limit */
        tilemap = agentite_tilemap_create(tileset, 50, 50);
        if (tilemap) {
            /* Add layers */
            int ground_layer = agentite_tilemap_add_layer(tilemap, "ground");
            int decor_layer = agentite_tilemap_add_layer(tilemap, "decorations");

            /* Fill ground with grass (tile ID 1 = forest green) */
            agentite_tilemap_fill(tilemap, ground_layer, 0, 0, 50, 50, 1);

            /* Add some terrain variety */
            /* Water (tile 13 = blue) in a lake pattern */
            agentite_tilemap_fill(tilemap, ground_layer, 12, 12, 10, 7, 13);
            agentite_tilemap_fill(tilemap, ground_layer, 15, 19, 5, 3, 13);

            /* Sand beach around water (tile 11 = sandy) */
            agentite_tilemap_fill(tilemap, ground_layer, 11, 11, 12, 1, 11);
            agentite_tilemap_fill(tilemap, ground_layer, 11, 19, 12, 1, 11);
            agentite_tilemap_fill(tilemap, ground_layer, 11, 11, 1, 9, 11);
            agentite_tilemap_fill(tilemap, ground_layer, 22, 11, 1, 9, 11);

            /* Stone path (tile 6 = gray) */
            agentite_tilemap_fill(tilemap, ground_layer, 25, 0, 2, 50, 6);

            /* Dirt patches (tile 9 = brown) */
            agentite_tilemap_fill(tilemap, ground_layer, 32, 20, 6, 6, 9);
            agentite_tilemap_fill(tilemap, ground_layer, 40, 35, 5, 5, 9);

            /* Dark grass variation (tile 3 = olive) */
            agentite_tilemap_fill(tilemap, ground_layer, 4, 35, 8, 8, 3);

            /* Light grass patches (tile 2 = lime) */
            agentite_tilemap_fill(tilemap, ground_layer, 35, 4, 6, 6, 2);

            /* Add some decorations (tile 16 = gold, used as decoration markers) */
            agentite_tilemap_set_tile(tilemap, decor_layer, 25, 25, 16);
            agentite_tilemap_set_tile(tilemap, decor_layer, 40, 12, 16);
            agentite_tilemap_set_tile(tilemap, decor_layer, 7, 40, 16);

            /* Set decorations layer to slightly transparent */
            agentite_tilemap_set_layer_opacity(tilemap, decor_layer, 0.8f);

            SDL_Log("Tilemap initialized: 50x50 tiles @ 48px (2400x2400 world units)");
        }
    }

    /* Register audio test action */
    int action_play_sound = agentite_input_register_action(input, "play_sound");
    agentite_input_bind_key(input, action_play_sound, SDL_SCANCODE_SPACE);
    agentite_input_bind_gamepad_button(input, action_play_sound, SDL_GAMEPAD_BUTTON_SOUTH);

    /* Demo state */
    bool checkbox_value = false;
    float slider_value = 0.5f;
    int dropdown_selection = 0;
    const char *dropdown_items[] = {"Easy", "Medium", "Hard", "Extreme"};
    char textbox_buffer[128] = "Player 1";
    int listbox_selection = 0;
    const char *listbox_items[] = {
        "Infantry", "Cavalry", "Archers", "Siege",
        "Navy", "Air Force", "Special Ops"
    };

    /* Sprite demo state */
    float sprite_rotation = 0.0f;
    float sprite_time = 0.0f;

    /* Camera control state */
    float target_zoom = 1.0f;
    float mouse_world_x = 0.0f, mouse_world_y = 0.0f;

    /* Main game loop */
    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);

        /* Begin input frame (reset per-frame state) */
        agentite_input_begin_frame(input);

        /* Process events - UI gets first chance, then input system */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            /* Let UI process the event first */
            if (aui_process_event(ui, &event)) {
                continue;  /* UI consumed the event */
            }

            /* Let input system process the event */
            agentite_input_process_event(input, &event);

            /* Handle quit event */
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }

        /* Update input state (compute just_pressed/released) */
        agentite_input_update(input);

        /* Get delta time */
        float dt = agentite_get_delta_time(engine);

        /* Handle quit action */
        if (agentite_input_action_just_pressed(input, action_quit)) {
            agentite_quit(engine);
        }

        /* Play test sound on spacebar */
        if (agentite_input_action_just_pressed(input, action_play_sound)) {
            if (sound_beep) {
                agentite_sound_play(audio, sound_beep);
            }
        }

        /* Update audio system */
        agentite_audio_update(audio);

        /* Handle mouse wheel zoom */
        float scroll_x, scroll_y;
        agentite_input_get_scroll(input, &scroll_x, &scroll_y);
        if (scroll_y > 0) {
            target_zoom *= 1.15f;
        } else if (scroll_y < 0) {
            target_zoom /= 1.15f;
        }

        /* Handle gamepad trigger zoom */
        if (agentite_input_action_pressed(input, action_zoom_in)) {
            float val = agentite_input_action_value(input, action_zoom_in);
            target_zoom *= 1.0f + 0.5f * val * dt;
        }
        if (agentite_input_action_pressed(input, action_zoom_out)) {
            float val = agentite_input_action_value(input, action_zoom_out);
            target_zoom /= 1.0f + 0.5f * val * dt;
        }

        /* Clamp zoom */
        if (target_zoom < 0.1f) target_zoom = 0.1f;
        if (target_zoom > 10.0f) target_zoom = 10.0f;

        /* Camera controls - using actions (supports keyboard + gamepad) */
        float cam_speed = 300.0f / agentite_camera_get_zoom(camera);  /* Faster when zoomed out */

        if (agentite_input_action_pressed(input, action_cam_up))
            agentite_camera_move(camera, 0, -cam_speed * dt);
        if (agentite_input_action_pressed(input, action_cam_down))
            agentite_camera_move(camera, 0, cam_speed * dt);
        if (agentite_input_action_pressed(input, action_cam_left))
            agentite_camera_move(camera, -cam_speed * dt, 0);
        if (agentite_input_action_pressed(input, action_cam_right))
            agentite_camera_move(camera, cam_speed * dt, 0);
        if (agentite_input_action_pressed(input, action_cam_rot_left)) {
            float rot = agentite_camera_get_rotation(camera);
            agentite_camera_set_rotation(camera, rot - 60.0f * dt);
        }
        if (agentite_input_action_pressed(input, action_cam_rot_right)) {
            float rot = agentite_camera_get_rotation(camera);
            agentite_camera_set_rotation(camera, rot + 60.0f * dt);
        }
        if (agentite_input_action_just_pressed(input, action_cam_reset)) {
            /* Reset camera to tilemap center */
            agentite_camera_set_position(camera, 1200.0f, 1200.0f);
            agentite_camera_set_rotation(camera, 0.0f);
            target_zoom = 1.0f;
        }

        /* Smooth zoom interpolation */
        float current_zoom = agentite_camera_get_zoom(camera);
        float new_zoom = current_zoom + (target_zoom - current_zoom) * 5.0f * dt;
        agentite_camera_set_zoom(camera, new_zoom);

        /* Update camera matrices */
        agentite_camera_update(camera);

        /* Get mouse position in world coordinates */
        float mouse_x, mouse_y;
        agentite_input_get_mouse_position(input, &mouse_x, &mouse_y);
        agentite_camera_screen_to_world(camera, mouse_x, mouse_y, &mouse_world_x, &mouse_world_y);

        /* Update sprite animation */
        sprite_time += dt;
        sprite_rotation += 45.0f * dt;  /* Rotate 45 degrees per second */
        if (sprite_rotation > 360.0f) sprite_rotation -= 360.0f;

        /* Progress ECS systems */
        agentite_ecs_progress(ecs_world, dt);

        /* Update enemy position (simple demo movement) */
        const C_Position *enemy_pos = ecs_get(w, enemy, C_Position);
        const C_Velocity *enemy_vel = ecs_get(w, enemy, C_Velocity);
        if (enemy_pos && enemy_vel) {
            float new_x = enemy_pos->x + enemy_vel->vx * dt;
            float new_y = enemy_pos->y + enemy_vel->vy * dt;
            /* Bounce off edges */
            float new_vx = enemy_vel->vx;
            float new_vy = enemy_vel->vy;
            if (new_x < 0 || new_x > 1280) new_vx = -enemy_vel->vx;
            if (new_y < 0 || new_y > 720) new_vy = -enemy_vel->vy;
            C_Position new_pos = { .x = new_x, .y = new_y };
            C_Velocity new_vel = { .vx = new_vx, .vy = new_vy };
            ecs_set_ptr(w, enemy, C_Position, &new_pos);
            ecs_set_ptr(w, enemy, C_Velocity, &new_vel);
        }

        /* Begin UI frame */
        aui_begin_frame(ui, dt);

        /* Draw a demo panel */
        if (aui_begin_panel(ui, "Game Settings", 50, 50, 300, 400,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

            aui_label(ui, "Welcome to Agentite UI!");
            aui_spacing(ui, 10);

            if (aui_button(ui, "Start Game")) {
                SDL_Log("Start Game clicked!");
            }

            if (aui_button(ui, "Load Game")) {
                SDL_Log("Load Game clicked!");
            }

            aui_separator(ui);

            aui_checkbox(ui, "Enable Music", &checkbox_value);

            aui_slider_float(ui, "Volume", &slider_value, 0.0f, 1.0f);

            aui_spacing(ui, 5);

            aui_dropdown(ui, "Difficulty", &dropdown_selection,
                        dropdown_items, 4);

            aui_spacing(ui, 5);

            aui_textbox(ui, "Name", textbox_buffer, sizeof(textbox_buffer));

            aui_end_panel(ui);
        }

        /* Draw a second panel for unit selection */
        if (aui_begin_panel(ui, "Units", 400, 50, 250, 300,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

            aui_label(ui, "Select Unit Type:");
            aui_listbox(ui, "##units", &listbox_selection,
                       listbox_items, 7, 150);

            aui_spacing(ui, 10);

            if (aui_button(ui, "Deploy Unit")) {
                SDL_Log("Deploying: %s", listbox_items[listbox_selection]);
            }

            aui_end_panel(ui);
        }

        /* Draw ECS entity info panel */
        if (aui_begin_panel(ui, "ECS Entities", 700, 50, 280, 200,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

            aui_label(ui, "Player Entity:");
            const C_Position *p_pos = ecs_get(w, player, C_Position);
            const C_Health *p_hp = ecs_get(w, player, C_Health);
            if (p_pos) {
                char buf[64];
                snprintf(buf, sizeof(buf), "  Pos: (%.0f, %.0f)", p_pos->x, p_pos->y);
                aui_label(ui, buf);
            }
            if (p_hp) {
                char buf[64];
                snprintf(buf, sizeof(buf), "  HP: %d/%d", p_hp->health, p_hp->max_health);
                aui_label(ui, buf);
            }

            aui_separator(ui);

            aui_label(ui, "Enemy Entity:");
            const C_Health *e_hp = ecs_get(w, enemy, C_Health);
            if (enemy_pos) {
                char buf[64];
                snprintf(buf, sizeof(buf), "  Pos: (%.0f, %.0f)", enemy_pos->x, enemy_pos->y);
                aui_label(ui, buf);
            }
            if (e_hp) {
                char buf[64];
                snprintf(buf, sizeof(buf), "  HP: %d/%d", e_hp->health, e_hp->max_health);
                aui_label(ui, buf);
            }

            aui_end_panel(ui);
        }

        /* Draw Camera controls panel */
        if (aui_begin_panel(ui, "Camera", 700, 260, 280, 180,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

            float cam_x, cam_y;
            agentite_camera_get_position(camera, &cam_x, &cam_y);
            float cam_zoom = agentite_camera_get_zoom(camera);
            float cam_rot = agentite_camera_get_rotation(camera);

            char buf[64];
            snprintf(buf, sizeof(buf), "Position: (%.0f, %.0f)", cam_x, cam_y);
            aui_label(ui, buf);
            snprintf(buf, sizeof(buf), "Zoom: %.2fx", cam_zoom);
            aui_label(ui, buf);
            snprintf(buf, sizeof(buf), "Rotation: %.1f deg", cam_rot);
            aui_label(ui, buf);

            aui_separator(ui);

            snprintf(buf, sizeof(buf), "Mouse World: (%.0f, %.0f)", mouse_world_x, mouse_world_y);
            aui_label(ui, buf);

            aui_spacing(ui, 5);
            aui_label(ui, "WASD: Pan | Wheel: Zoom");
            aui_label(ui, "Q/E: Rotate | R: Reset");

            aui_end_panel(ui);
        }

        /* Draw Audio panel */
        if (aui_begin_panel(ui, "Audio", 760, 450, 280, 250,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

            /* Master volume slider */
            float master_vol = agentite_audio_get_master_volume(audio);
            if (aui_slider_float(ui, "Master", &master_vol, 0.0f, 1.0f)) {
                agentite_audio_set_master_volume(audio, master_vol);
            }

            /* Sound volume slider */
            float sound_vol = agentite_audio_get_sound_volume(audio);
            if (aui_slider_float(ui, "Sounds", &sound_vol, 0.0f, 1.0f)) {
                agentite_audio_set_sound_volume(audio, sound_vol);
            }

            aui_separator(ui);

            /* Sound test buttons */
            if (aui_button(ui, "Beep (440Hz)") && sound_beep) {
                agentite_sound_play(audio, sound_beep);
            }
            if (aui_button(ui, "Click (880Hz)") && sound_click) {
                agentite_sound_play(audio, sound_click);
            }
            if (aui_button(ui, "Ping (1760Hz)") && sound_ping) {
                agentite_sound_play(audio, sound_ping);
            }

            aui_spacing(ui, 5);
            aui_label(ui, "Space: Play beep");

            aui_end_panel(ui);
        }

        /* Draw some standalone widgets */
        aui_progress_bar(ui, slider_value, 0.0f, 1.0f);

        /* End UI frame */
        aui_end_frame(ui);

        /* MSDF text scale for pulsing effect */
        float msdf_scale = 0.8f + 0.3f * sinf(sprite_time * 2.5f);

        /* Build sprite batch */
        agentite_sprite_begin(sprites, NULL);

        /* Render tilemap first (background) */
        if (tilemap) {
            agentite_tilemap_render(tilemap, sprites, camera);
        }

        /* Draw some demo sprites in the background area */
        /* Row of static checkerboard sprites */
        for (int i = 0; i < 8; i++) {
            agentite_sprite_draw(sprites, &sprite_checker,
                               700.0f + i * 70.0f, 400.0f);
        }

        /* Rotating sprite */
        agentite_sprite_draw_ex(sprites, &sprite_checker,
                              800.0f, 500.0f,
                              2.0f, 2.0f,
                              sprite_rotation,
                              0.5f, 0.5f);

        /* Bouncing/pulsing sprite */
        float pulse = 1.0f + 0.3f * sinf(sprite_time * 3.0f);
        agentite_sprite_draw_scaled(sprites, &sprite_checker,
                                  950.0f, 500.0f,
                                  pulse, pulse);

        /* Tinted sprites - using same texture for consistent batching */
        agentite_sprite_draw_tinted(sprites, &sprite_checker,
                                  1050.0f, 450.0f,
                                  1.0f, 0.3f, 0.3f, 1.0f);  /* Red tint */
        agentite_sprite_draw_tinted(sprites, &sprite_checker,
                                  1050.0f, 550.0f,
                                  0.3f, 1.0f, 0.3f, 1.0f);  /* Green tint */

        /* Acquire command buffer for GPU operations */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            /* Upload sprite data to GPU (must be done BEFORE render pass) */
            agentite_sprite_upload(sprites, cmd);

            /* Upload UI data to GPU (must be done BEFORE render pass) */
            aui_upload(ui, cmd);

            /* Build text batches - can now queue multiple batches before render */

            /* Batch 1: Bitmap font text */
            if (font_small) {
                agentite_text_begin(text);
                agentite_text_printf(text, font_small, 1100.0f, 20.0f, "FPS: %.0f", 1.0f / dt);
                agentite_text_draw_colored(text, font_small, "Bitmap Font:", 550.0f, 520.0f, 0.8f, 0.8f, 0.8f, 1.0f);
                agentite_text_draw_colored(text, font_small, "Red Text", 550.0f, 540.0f, 1.0f, 0.3f, 0.3f, 1.0f);
                agentite_text_draw_colored(text, font_small, "Green Text", 550.0f, 560.0f, 0.3f, 1.0f, 0.3f, 1.0f);
                agentite_text_draw_colored(text, font_small, "Blue Text", 550.0f, 580.0f, 0.3f, 0.5f, 1.0f, 1.0f);
                float text_scale = 1.0f + 0.2f * sinf(sprite_time * 2.0f);
                agentite_text_draw_scaled(text, font_small, "Pulsing!", 550.0f, 605.0f, text_scale);
                agentite_text_end(text);
            }

            /* Batch 2: MSDF font with outline effect */
            if (msdf_font) {
                agentite_text_begin(text);
                agentite_sdf_text_set_outline(text, 0.2f, 0.1f, 0.1f, 0.1f, 1.0f);
                agentite_sdf_text_draw_colored(text, msdf_font, "MSDF Text Demo", 450.0f, 50.0f, 1.2f,
                                             1.0f, 0.9f, 0.4f, 1.0f);
                agentite_sdf_text_draw_colored(text, msdf_font, "MSDF Font:", 50.0f, 480.0f, 0.8f,
                                             0.8f, 0.8f, 0.8f, 1.0f);
                agentite_sdf_text_draw_colored(text, msdf_font, "Outlined", 50.0f, 520.0f, 1.0f,
                                             1.0f, 1.0f, 1.0f, 1.0f);
                agentite_sdf_text_draw_colored(text, msdf_font, "Sharp!", 200.0f, 520.0f, msdf_scale,
                                             0.4f, 1.0f, 0.6f, 1.0f);
                agentite_sdf_text_draw_colored(text, msdf_font, "With Outline!", 50.0f, 560.0f, 1.0f,
                                             0.5f, 0.8f, 1.0f, 1.0f);
                agentite_text_end(text);
            }

            /* Upload all queued batches at once */
            agentite_text_upload(text, cmd);

            /* Begin render pass */
            if (agentite_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);

                /* Render sprites first (background) */
                agentite_sprite_render(sprites, cmd, pass);

                /* Render UI on top */
                aui_render(ui, cmd, pass);

                /* Render all queued text batches (bitmap and MSDF) */
                agentite_text_render(text, cmd, pass);

                agentite_end_render_pass(engine);
            }
        }

        /* End sprite batch (cleanup state) */
        agentite_sprite_end(sprites, NULL, NULL);

        agentite_end_frame(engine);
    }

    /* Cleanup */
    if (tilemap) agentite_tilemap_destroy(tilemap);
    if (tileset) agentite_tileset_destroy(tileset);
    if (tileset_tex) agentite_texture_destroy(sprites, tileset_tex);
    if (sound_beep) agentite_sound_destroy(audio, sound_beep);
    if (sound_click) agentite_sound_destroy(audio, sound_click);
    if (sound_ping) agentite_sound_destroy(audio, sound_ping);
    agentite_audio_shutdown(audio);
    agentite_input_shutdown(input);
    agentite_ecs_shutdown(ecs_world);
    if (font_large) agentite_font_destroy(text, font_large);
    if (font_small) agentite_font_destroy(text, font_small);
    if (msdf_font) agentite_sdf_font_destroy(text, msdf_font);
    agentite_text_shutdown(text);
    agentite_texture_destroy(sprites, tex_checker);
    agentite_camera_destroy(camera);
    agentite_sprite_shutdown(sprites);
    aui_shutdown(ui);
    agentite_shutdown(engine);

    return 0;
}
