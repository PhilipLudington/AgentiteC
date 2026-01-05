/**
 * Agentite Engine - Procedural Noise Example
 *
 * Demonstrates procedural noise generation with:
 * - Perlin noise
 * - Simplex noise
 * - Worley (cellular) noise
 * - Fractal Brownian motion (fBm)
 * - Ridged multifractal noise
 * - Domain warping
 *
 * Controls:
 *   1-4    - Switch noise type (Perlin, Simplex, Worley, Value)
 *   F      - Toggle fractal mode (fBm)
 *   R      - Toggle ridged fractal
 *   W      - Toggle domain warping
 *   +/-    - Adjust octaves
 *   Arrow Keys - Pan view
 *   Scroll - Zoom in/out
 *   Space  - New seed
 *   ESC    - Quit
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/noise.h"
#include "agentite/input.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static const int WINDOW_WIDTH = 800;
static const int WINDOW_HEIGHT = 600;
static const int PREVIEW_SIZE = 256;

typedef struct AppState {
    Agentite_Engine *engine;
    Agentite_SpriteRenderer *sprites;
    Agentite_Input *input;
    Agentite_TextRenderer *text;
    Agentite_Font *font;

    Agentite_Noise *noise;
    Agentite_Texture *preview_texture;

    /* Noise settings */
    Agentite_NoiseType noise_type;
    bool use_fractal;
    bool use_ridged;
    bool use_warp;
    int octaves;
    float scale;
    float offset_x, offset_y;
    uint64_t seed;

    bool needs_update;
} AppState;

static const char *NOISE_NAMES[] = {"Perlin", "Simplex", "Worley", "Value"};

/* Generate noise preview texture */
static void update_preview(AppState *app) {
    unsigned char *pixels = (unsigned char *)malloc(PREVIEW_SIZE * PREVIEW_SIZE * 4);
    if (!pixels) return;

    Agentite_NoiseDomainWarpConfig warp = AGENTITE_NOISE_DOMAIN_WARP_DEFAULT;
    warp.amplitude = 30.0f;
    warp.frequency = 0.02f;

    for (int y = 0; y < PREVIEW_SIZE; y++) {
        for (int x = 0; x < PREVIEW_SIZE; x++) {
            float nx = (x + app->offset_x) * app->scale;
            float ny = (y + app->offset_y) * app->scale;

            /* Apply domain warp if enabled */
            if (app->use_warp) {
                agentite_noise_domain_warp2d(app->noise, &nx, &ny, &warp);
            }

            float value = 0.0f;
            float amplitude = 1.0f;
            float frequency = 1.0f;
            float max_amplitude = 0.0f;
            int num_octaves = app->use_fractal ? app->octaves : 1;

            for (int oct = 0; oct < num_octaves; oct++) {
                float sample_x = nx * frequency;
                float sample_y = ny * frequency;
                float noise_val;

                switch (app->noise_type) {
                    case AGENTITE_NOISE_PERLIN:
                        noise_val = agentite_noise_perlin2d(app->noise, sample_x, sample_y);
                        break;
                    case AGENTITE_NOISE_SIMPLEX:
                        noise_val = agentite_noise_simplex2d(app->noise, sample_x, sample_y);
                        break;
                    case AGENTITE_NOISE_WORLEY:
                        noise_val = agentite_noise_worley2d(app->noise, sample_x * 10, sample_y * 10);
                        break;
                    default:
                        noise_val = agentite_noise_value2d(app->noise, sample_x, sample_y);
                        break;
                }

                if (app->use_ridged) {
                    noise_val = 1.0f - fabsf(noise_val);
                    noise_val = noise_val * noise_val;
                }

                value += noise_val * amplitude;
                max_amplitude += amplitude;
                amplitude *= 0.5f;  /* persistence */
                frequency *= 2.0f;  /* lacunarity */
            }

            value = value / max_amplitude;

            /* Normalize to 0-1 range */
            value = (value + 1.0f) * 0.5f;
            if (value < 0.0f) value = 0.0f;
            if (value > 1.0f) value = 1.0f;

            /* Apply colormap (grayscale or terrain) */
            unsigned char r, g, b;
            if (app->noise_type == AGENTITE_NOISE_WORLEY) {
                /* Cellular look - invert and colorize */
                value = 1.0f - value;
                r = (unsigned char)(value * 200 + 55);
                g = (unsigned char)(value * 180 + 40);
                b = (unsigned char)(value * 100 + 30);
            } else {
                /* Terrain-like coloring */
                if (value < 0.3f) {
                    /* Deep water */
                    r = (unsigned char)(20 + value * 100);
                    g = (unsigned char)(40 + value * 120);
                    b = (unsigned char)(100 + value * 200);
                } else if (value < 0.5f) {
                    /* Shallow water / beach */
                    float t = (value - 0.3f) / 0.2f;
                    r = (unsigned char)(50 + t * 150);
                    g = (unsigned char)(80 + t * 140);
                    b = (unsigned char)(180 - t * 80);
                } else if (value < 0.7f) {
                    /* Grass */
                    float t = (value - 0.5f) / 0.2f;
                    r = (unsigned char)(60 + t * 60);
                    g = (unsigned char)(140 - t * 40);
                    b = (unsigned char)(40 + t * 30);
                } else {
                    /* Mountain */
                    float t = (value - 0.7f) / 0.3f;
                    r = (unsigned char)(100 + t * 155);
                    g = (unsigned char)(100 + t * 155);
                    b = (unsigned char)(100 + t * 155);
                }
            }

            int idx = (y * PREVIEW_SIZE + x) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = 255;
        }
    }

    /* Update texture */
    if (app->preview_texture) {
        agentite_texture_destroy(app->sprites, app->preview_texture);
    }
    app->preview_texture = agentite_texture_create(app->sprites, PREVIEW_SIZE, PREVIEW_SIZE, pixels);
    free(pixels);

    app->needs_update = false;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    AppState app = {0};
    app.noise_type = AGENTITE_NOISE_SIMPLEX;
    app.use_fractal = true;
    app.octaves = 4;
    app.scale = 0.01f;
    app.seed = 12345;
    app.needs_update = true;

    Agentite_Config config = {
        .window_title = "Agentite - Procedural Noise Example",
        .window_width = WINDOW_WIDTH,
        .window_height = WINDOW_HEIGHT,
        .vsync = true
    };

    app.engine = agentite_init(&config);
    if (!app.engine) return 1;

    SDL_GPUDevice *gpu = agentite_get_gpu_device(app.engine);
    SDL_Window *window = agentite_get_window(app.engine);

    app.sprites = agentite_sprite_init(gpu, window);
    app.input = agentite_input_init();
    app.text = agentite_text_init(gpu, window);
    if (app.text) {
        app.font = agentite_font_load(app.text, "assets/fonts/Roboto-Regular.ttf", 16);
    }

    app.noise = agentite_noise_create(app.seed);

    printf("Procedural Noise Example\n");
    printf("========================\n");
    printf("1-4: Noise type  F: Fractal  R: Ridged  W: Warp\n");
    printf("+/-: Octaves  Arrows: Pan  Space: New seed\n");

    while (agentite_is_running(app.engine)) {
        agentite_begin_frame(app.engine);

        agentite_input_begin_frame(app.input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(app.input, &event);
            if (event.type == SDL_EVENT_QUIT) agentite_quit(app.engine);
        }
        agentite_input_update(app.input);

        /* Noise type selection */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_1)) {
            app.noise_type = AGENTITE_NOISE_PERLIN; app.needs_update = true;
        }
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_2)) {
            app.noise_type = AGENTITE_NOISE_SIMPLEX; app.needs_update = true;
        }
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_3)) {
            app.noise_type = AGENTITE_NOISE_WORLEY; app.needs_update = true;
        }
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_4)) {
            app.noise_type = AGENTITE_NOISE_VALUE; app.needs_update = true;
        }

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_F)) {
            app.use_fractal = !app.use_fractal; app.needs_update = true;
        }
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_R)) {
            app.use_ridged = !app.use_ridged; app.needs_update = true;
        }
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_W)) {
            app.use_warp = !app.use_warp; app.needs_update = true;
        }

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_EQUALS)) {
            app.octaves = (app.octaves < 8) ? app.octaves + 1 : 8; app.needs_update = true;
        }
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_MINUS)) {
            app.octaves = (app.octaves > 1) ? app.octaves - 1 : 1; app.needs_update = true;
        }

        /* Pan */
        float pan_speed = 50.0f;
        if (agentite_input_key_pressed(app.input, SDL_SCANCODE_LEFT)) {
            app.offset_x -= pan_speed; app.needs_update = true;
        }
        if (agentite_input_key_pressed(app.input, SDL_SCANCODE_RIGHT)) {
            app.offset_x += pan_speed; app.needs_update = true;
        }
        if (agentite_input_key_pressed(app.input, SDL_SCANCODE_UP)) {
            app.offset_y -= pan_speed; app.needs_update = true;
        }
        if (agentite_input_key_pressed(app.input, SDL_SCANCODE_DOWN)) {
            app.offset_y += pan_speed; app.needs_update = true;
        }

        /* Zoom */
        float scroll_x, scroll_y;
        agentite_input_get_scroll(app.input, &scroll_x, &scroll_y);
        if (scroll_y != 0) {
            app.scale *= (scroll_y > 0) ? 0.9f : 1.1f;
            if (app.scale < 0.001f) app.scale = 0.001f;
            if (app.scale > 0.1f) app.scale = 0.1f;
            app.needs_update = true;
        }

        /* New seed */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_SPACE)) {
            app.seed = (uint64_t)rand() * rand();
            agentite_noise_destroy(app.noise);
            app.noise = agentite_noise_create(app.seed);
            app.needs_update = true;
        }

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_ESCAPE))
            agentite_quit(app.engine);

        /* Update preview if needed */
        if (app.needs_update) {
            update_preview(&app);
        }

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);
        if (cmd) {
            agentite_sprite_begin(app.sprites, NULL);

            /* Draw preview centered (sprite origin is at center, so use window center) */
            if (app.preview_texture) {
                Agentite_Sprite sprite = agentite_sprite_from_texture(app.preview_texture);
                float px = WINDOW_WIDTH / 2.0f;
                float py = WINDOW_HEIGHT / 2.0f;
                agentite_sprite_draw(app.sprites, &sprite, px, py);
            }

            agentite_sprite_upload(app.sprites, cmd);

            if (app.text && app.font) {
                agentite_text_begin(app.text);
                char info[256];
                snprintf(info, sizeof(info), "Type: %s  Fractal: %s  Ridged: %s  Warp: %s  Octaves: %d",
                    NOISE_NAMES[app.noise_type],
                    app.use_fractal ? "ON" : "OFF",
                    app.use_ridged ? "ON" : "OFF",
                    app.use_warp ? "ON" : "OFF",
                    app.octaves);
                agentite_text_draw_colored(app.text, app.font, info, 10, 10, 1, 1, 1, 0.9f);
                agentite_text_draw_colored(app.text, app.font,
                    "1-4: Type  F: Fractal  R: Ridged  W: Warp  +/-: Octaves",
                    10, 30, 0.7f, 0.7f, 0.7f, 0.9f);
                agentite_text_draw_colored(app.text, app.font,
                    "Arrows: Pan  Scroll: Zoom  Space: New Seed",
                    10, 50, 0.7f, 0.7f, 0.7f, 0.9f);
                agentite_text_end(app.text);
                agentite_text_upload(app.text, cmd);
            }

            if (agentite_begin_render_pass(app.engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                agentite_sprite_render(app.sprites, cmd, pass);
                if (app.text) agentite_text_render(app.text, cmd, pass);
                agentite_end_render_pass(app.engine);
            }

            agentite_sprite_end(app.sprites, NULL, NULL);
        }

        agentite_end_frame(app.engine);
    }

    if (app.preview_texture) agentite_texture_destroy(app.sprites, app.preview_texture);
    agentite_noise_destroy(app.noise);
    if (app.font) agentite_font_destroy(app.text, app.font);
    if (app.text) agentite_text_shutdown(app.text);
    agentite_input_shutdown(app.input);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    return 0;
}
