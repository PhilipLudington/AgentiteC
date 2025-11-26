#include "carbon/carbon.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // Configure engine
    Carbon_Config config = {
        .window_title = "Carbon Engine - Strategy Game",
        .window_width = 1280,
        .window_height = 720,
        .fullscreen = false,
        .vsync = true
    };

    // Initialize engine
    Carbon_Engine *engine = carbon_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize Carbon Engine\n");
        return 1;
    }

    // Main game loop
    while (carbon_is_running(engine)) {
        carbon_begin_frame(engine);
        carbon_poll_events(engine);

        // Get delta time for game logic
        float dt = carbon_get_delta_time(engine);
        (void)dt; // Will be used for game logic

        // Render frame - clear to a dark blue color
        if (carbon_begin_render_pass(engine, 0.1f, 0.1f, 0.2f, 1.0f)) {
            // TODO: Add rendering code here
            // - Draw game world
            // - Draw UI
            // - Draw debug info

            carbon_end_render_pass(engine);
        }

        carbon_end_frame(engine);
    }

    // Cleanup
    carbon_shutdown(engine);

    return 0;
}
