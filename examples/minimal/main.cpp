/**
 * Carbon Engine - Minimal Example
 *
 * The absolute minimum code to create a window and render a clear color.
 * Use this as a starting template for new projects.
 */

#include "carbon/carbon.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Configure engine with minimal settings */
    Carbon_Config config = {
        .window_title = "Carbon - Minimal Example",
        .window_width = 800,
        .window_height = 600,
        .fullscreen = false,
        .vsync = true
    };

    /* Initialize engine */
    Carbon_Engine *engine = carbon_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize Carbon Engine\n");
        return 1;
    }

    /* Main loop */
    while (carbon_is_running(engine)) {
        carbon_begin_frame(engine);
        carbon_poll_events(engine);

        /* Begin render pass with clear color (cornflower blue) */
        if (carbon_begin_render_pass(engine, 0.39f, 0.58f, 0.93f, 1.0f)) {
            /* All rendering would go here */
            carbon_end_render_pass(engine);
        }

        carbon_end_frame(engine);
    }

    /* Cleanup */
    carbon_shutdown(engine);
    return 0;
}
