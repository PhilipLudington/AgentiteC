/**
 * Agentite Engine - Minimal Example
 *
 * The absolute minimum code to create a window and render a clear color.
 * Use this as a starting template for new projects.
 */

#include "agentite/agentite.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Configure engine with minimal settings */
    Agentite_Config config = {
        .window_title = "Agentite - Minimal Example",
        .window_width = 800,
        .window_height = 600,
        .fullscreen = false,
        .vsync = true
    };

    /* Initialize engine */
    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize Agentite Engine\n");
        return 1;
    }

    /* Main loop */
    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        agentite_poll_events(engine);

        /* Begin render pass with clear color (cornflower blue) */
        if (agentite_begin_render_pass(engine, 0.39f, 0.58f, 0.93f, 1.0f)) {
            /* All rendering would go here */
            agentite_end_render_pass(engine);
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_shutdown(engine);
    return 0;
}
