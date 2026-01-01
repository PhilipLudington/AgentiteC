/**
 * Agentite Engine - Rich Text Example
 *
 * Demonstrates BBCode-style formatted text with:
 * - Bold, italic, underline, strikethrough
 * - Colored text with hex and named colors
 * - Text size changes
 * - Animated text effects (wave, shake, rainbow, fade)
 * - Clickable links
 * - Inline icons
 */

#include "agentite/agentite.h"
#include "agentite/ui.h"
#include "agentite/ui_richtext.h"
#include "agentite/input.h"
#include <stdio.h>
#include <math.h>

/* Link click callback */
static void on_link_clicked(const char *url, void *userdata) {
    (void)userdata;
    SDL_Log("Link clicked: %s", url);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Agentite_Config config = {
        .window_title = "Agentite - Rich Text Example",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    /* Initialize UI system */
    AUI_Context *ui = aui_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine),
        config.window_width,
        config.window_height,
        "assets/fonts/Roboto-Regular.ttf",
        16.0f
    );

    if (!ui) {
        fprintf(stderr, "Failed to initialize UI (make sure font exists)\n");
        agentite_shutdown(engine);
        return 1;
    }

    float dpi_scale = agentite_get_dpi_scale(engine);
    aui_set_dpi_scale(ui, dpi_scale);

    Agentite_Input *input = agentite_input_init();

    /* Create rich text objects for different demos */
    AUI_RichText *basic_text = aui_richtext_parse(
        "[b]This text is bold[/b]\n"
        "[i]This text is italic (requires italic font)[/i]\n"
        "[u]This text is underlined[/u]\n"
        "[s]This text is strikethrough[/s]\n\n"
        "Combined: [b]Bold[/b], [i]italic[/i], [u]underlined[/u], and [s]strikethrough[/s].\n\n"
        "You can [b][i]combine[/i][/b] multiple styles together.\n\n"
        "[size=12][color=#888888]Note: Italic requires loading an italic font variant.[/color][/size]"
    );

    AUI_RichText *color_text = aui_richtext_parse(
        "[color=#FF6B6B]Red[/color] [color=#4ECDC4]Cyan[/color] [color=#FFE66D]Yellow[/color] "
        "[color=#95E1D3]Mint[/color] [color=#A685E2]Purple[/color]\n\n"
        "Named colors: [color=red]red[/color], [color=green]green[/color], [color=blue]blue[/color], "
        "[color=gold]gold[/color], [color=orange]orange[/color]"
    );

    AUI_RichText *size_text = aui_richtext_parse(
        "[size=12]Small text (12px)[/size]\n"
        "[size=16]Normal text (16px)[/size]\n"
        "[size=20]Medium text (20px)[/size]\n"
        "[size=28]Large text (28px)[/size]\n"
        "[size=36]Extra large (36px)[/size]"
    );

    AUI_RichText *animated_text = aui_richtext_parse(
        "[wave]This text has a wave effect![/wave]\n\n"
        "[shake]Shaking text for emphasis![/shake]\n\n"
        "[rainbow]Rainbow colored animated text![/rainbow]\n\n"
        "[fade]Fading in and out slowly...[/fade]"
    );

    AUI_RichText *link_text = aui_richtext_parse(
        "Click on [url=https://github.com/anthropics/claude-code]this link[/url] to visit the page.\n\n"
        "Links can be [b][url=https://example.com]styled[/url][/b] with other formatting.\n\n"
        "Multiple links: [url=https://one.com]One[/url] | [url=https://two.com]Two[/url] | "
        "[url=https://three.com]Three[/url]"
    );

    AUI_RichText *complex_text = aui_richtext_parse(
        "[size=24][b][color=#FFD700]Welcome to Agentite![/color][/b][/size]\n\n"
        "This engine supports [b]rich text[/b] with [color=#4ECDC4]colors[/color], "
        "[i]styles[/i], and [wave][color=#FF6B6B]animations[/color][/wave]!\n\n"
        "[size=14][color=#888888]Use BBCode tags to format your text. "
        "Nested tags are fully supported for complex formatting.[/color][/size]"
    );

    /* Layout all rich text objects */
    aui_richtext_layout(basic_text, 350);
    aui_richtext_layout(color_text, 350);
    aui_richtext_layout(size_text, 350);
    aui_richtext_layout(animated_text, 350);
    aui_richtext_layout(link_text, 350);
    aui_richtext_layout(complex_text, 500);

    /* Selected demo panel */
    int selected_demo = 0;
    const char *demo_names[] = {
        "Basic Formatting",
        "Colors",
        "Text Sizes",
        "Animations",
        "Links",
        "Complex Example"
    };

    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);

        agentite_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (aui_process_event(ui, &event)) {
                continue;
            }
            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }
        agentite_input_update(input);

        /* Switch demos with number keys */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_1)) selected_demo = 0;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_2)) selected_demo = 1;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_3)) selected_demo = 2;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_4)) selected_demo = 3;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_5)) selected_demo = 4;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_6)) selected_demo = 5;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            agentite_quit(engine);

        /* Update animated text */
        aui_richtext_update(animated_text, dt);
        aui_richtext_update(complex_text, dt);

        /* Begin UI frame */
        aui_begin_frame(ui, dt);

        /* Title panel */
        if (aui_begin_panel(ui, "Rich Text Demo", 50, 30, 500, 60,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {
            aui_label(ui, "Press 1-6 to switch demos. ESC to quit.");
            aui_end_panel(ui);
        }

        /* Demo selector */
        if (aui_begin_panel(ui, "Demos", 50, 110, 200, 260,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {
            for (int i = 0; i < 6; i++) {
                char label[32];
                snprintf(label, sizeof(label), "%d. %s", i + 1, demo_names[i]);
                if (selected_demo == i) {
                    aui_label(ui, label);
                } else {
                    if (aui_button(ui, label)) {
                        selected_demo = i;
                    }
                }
            }
            aui_end_panel(ui);
        }

        /* Main content panel */
        if (aui_begin_panel(ui, demo_names[selected_demo], 280, 110, 700, 450,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

            /* Draw the selected rich text */
            AUI_RichText *current = NULL;
            switch (selected_demo) {
                case 0: current = basic_text; break;
                case 1: current = color_text; break;
                case 2: current = size_text; break;
                case 3: current = animated_text; break;
                case 4: current = link_text; break;
                case 5: current = complex_text; break;
            }

            if (current) {
                /* Draw at panel content position */
                float rt_x = 300, rt_y = 155;
                aui_richtext_draw(ui, current, rt_x, rt_y);

                /* Check for link clicks */
                if (ui->input.mouse_pressed[0]) {
                    const char *link = aui_richtext_get_link_at(current,
                        ui->input.mouse_x - rt_x, ui->input.mouse_y - rt_y);
                    if (link) {
                        SDL_Log("Link clicked: %s", link);
                        /* Open URL in default browser */
                        SDL_OpenURL(link);
                    }
                }
            }

            aui_end_panel(ui);
        }

        /* BBCode reference panel */
        if (aui_begin_panel(ui, "BBCode Reference", 50, 390, 200, 300,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {
            aui_label(ui, "[b]...[/b] Bold");
            aui_label(ui, "[i]...[/i] Italic");
            aui_label(ui, "[u]...[/u] Underline");
            aui_label(ui, "[s]...[/s] Strike");
            aui_separator(ui);
            aui_label(ui, "[color=#HEX]");
            aui_label(ui, "[color=name]");
            aui_label(ui, "[size=N]");
            aui_separator(ui);
            aui_label(ui, "[wave]...[/wave]");
            aui_label(ui, "[shake]...[/shake]");
            aui_label(ui, "[rainbow]");
            aui_label(ui, "[fade]...[/fade]");
            aui_separator(ui);
            aui_label(ui, "[url=...]Link[/url]");
            aui_end_panel(ui);
        }

        /* FPS */
        if (aui_begin_panel(ui, "Info", 1000, 30, 200, 60, AUI_PANEL_BORDER)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "FPS: %.0f", 1.0f / dt);
            aui_label(ui, buf);
            aui_end_panel(ui);
        }

        aui_end_frame(ui);

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            aui_upload(ui, cmd);

            if (agentite_begin_render_pass(engine, 0.08f, 0.08f, 0.12f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
                aui_render(ui, cmd, pass);
                agentite_end_render_pass(engine);
            }
        }

        agentite_end_frame(engine);
    }

    /* Cleanup rich text objects */
    aui_richtext_destroy(basic_text);
    aui_richtext_destroy(color_text);
    aui_richtext_destroy(size_text);
    aui_richtext_destroy(animated_text);
    aui_richtext_destroy(link_text);
    aui_richtext_destroy(complex_text);

    /* Cleanup engine */
    agentite_input_shutdown(input);
    aui_shutdown(ui);
    agentite_shutdown(engine);

    return 0;
}
