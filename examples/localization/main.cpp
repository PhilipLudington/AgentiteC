/**
 * Agentite Engine - Localization System Example
 *
 * Demonstrates the localization system with multiple languages,
 * parameter substitution, and pluralization.
 *
 * Controls:
 *   1 - Switch to English
 *   2 - Switch to German (Deutsch)
 *   3 - Switch to Japanese
 *   4 - Switch to Arabic (RTL)
 *   SPACE - Cycle item count (for pluralization demo)
 *   ESC - Quit
 */

#include "agentite/agentite.h"
#include "agentite/ui.h"
#include "agentite/input.h"
#include "agentite/localization.h"
#include "agentite/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int WINDOW_WIDTH = 900;
static const int WINDOW_HEIGHT = 800;

/* Demo state */
static int s_item_count = 1;
static const char *s_player_name = "Hero";
static int s_gold = 1250;
static int s_level = 42;

/* Font paths */
#ifdef __APPLE__
static const char *FONT_DEFAULT = "assets/fonts/Roboto-Regular.ttf";
static const char *FONT_GERMAN = "/System/Library/Fonts/Geneva.ttf";
static const char *FONT_CJK = "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc";
static const char *FONT_ARABIC = "/System/Library/Fonts/Supplemental/Al Nile.ttc";
#else
/* Linux/Windows would need different paths or bundled fonts */
static const char *FONT_DEFAULT = "assets/fonts/Roboto-Regular.ttf";
static const char *FONT_GERMAN = "assets/fonts/Roboto-Regular.ttf";
static const char *FONT_CJK = "assets/fonts/Roboto-Regular.ttf";
static const char *FONT_ARABIC = "assets/fonts/Roboto-Regular.ttf";
#endif

/* Character sets for MSDF font generation (extracted from locale files + ASCII) */
static const char *CHARSET_CJK =
    /* ASCII printable */
    " !\"#$%&'()*+,-./0123456789:;<=>?@"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz{|}~"
    /* Japanese punctuation and fullwidth */
    "、！"
    /* All hiragana from ja.toml */
    "いこさすちっでてとにのはまるをん"
    /* All katakana from ja.toml */
    "アィイオカキクゲコゴシジスズセタッテデトドパビプペムメモャョラルレロンー"
    /* All kanji from ja.toml */
    "了体個値切在基報変始定形情戻持換数敵日更替本枚現用終経置複言設語適量開音験";

static const char *CHARSET_ARABIC =
    /* ASCII printable */
    " !\"#$%&'()*+,-./0123456789:;<=>?@"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz{|}~"
    /* Arabic punctuation */
    "،"
    /* All Arabic letters from ar.toml (U+0621-U+064B) */
    "ءأإابةتجحخدذرسصضطعغفقلمنهوىيً";

static const char *CHARSET_GERMAN =
    /* ASCII printable */
    " !\"#$%&'()*+,-./0123456789:;<=>?@"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz{|}~"
    /* German special characters */
    "ÄÖÜäöüß";

/* Fonts */
static AUI_Font *s_font_default = NULL;
static AUI_Font *s_font_german = NULL;
static AUI_Font *s_font_cjk = NULL;
static AUI_Font *s_font_arabic = NULL;

/* Switch font based on language font key.
 * Always sets the font - no caching of font_key to avoid state bugs
 * when manually switching fonts mid-frame. */
static void set_font_for_language(AUI_Context *ui, Agentite_Localization *loc) {
    const char *font_key = agentite_loc_get_font_key(loc);

    if (strcmp(font_key, "cjk") == 0 && s_font_cjk) {
        aui_set_font(ui, s_font_cjk);
    } else if (strcmp(font_key, "arabic") == 0 && s_font_arabic) {
        aui_set_font(ui, s_font_arabic);
    } else if (strcmp(font_key, "german") == 0 && s_font_german) {
        aui_set_font(ui, s_font_german);
    } else if (s_font_default) {
        aui_set_font(ui, s_font_default);
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Initialize engine */
    Agentite_Config config = {
        .window_title = "Localization Demo",
        .window_width = WINDOW_WIDTH,
        .window_height = WINDOW_HEIGHT,
        .vsync = true
    };

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine: %s\n", agentite_get_last_error());
        return 1;
    }

    /* Initialize UI system with default font */
    AUI_Context *ui = aui_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine),
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        FONT_DEFAULT,
        18.0f
    );
    if (!ui) {
        fprintf(stderr, "Failed to initialize UI: %s\n", agentite_get_last_error());
        agentite_shutdown(engine);
        return 1;
    }

    /* Set DPI scale */
    float dpi_scale = agentite_get_dpi_scale(engine);
    aui_set_dpi_scale(ui, dpi_scale);

    /* Load additional fonts for other languages using runtime MSDF generation */
    s_font_default = aui_font_load(ui, FONT_DEFAULT, 18.0f);

    /* Generate MSDF fonts with Unicode character sets */
    printf("Generating German font...\n");
    /* Geneva renders larger than Roboto, so use 14pt to match Roboto 18pt visually */
    s_font_german = aui_font_generate_msdf(ui, FONT_GERMAN, 14.0f, CHARSET_GERMAN);
    if (!s_font_german) {
        printf("Warning: Could not generate German MSDF font: %s\n", agentite_get_last_error());
    }

    printf("Generating CJK font (this may take a moment)...\n");
    /* Hiragino renders larger than Roboto, so use 14pt to match Roboto 18pt visually */
    s_font_cjk = aui_font_generate_msdf(ui, FONT_CJK, 14.0f, CHARSET_CJK);
    if (!s_font_cjk) {
        printf("Warning: Could not generate CJK font: %s\n", agentite_get_last_error());
    }

    printf("Generating Arabic font...\n");
    /* Al Nile renders larger than Roboto, so use 14pt to match Roboto 18pt visually */
    s_font_arabic = aui_font_generate_msdf(ui, FONT_ARABIC, 14.0f, CHARSET_ARABIC);
    if (!s_font_arabic) {
        printf("Warning: Could not generate Arabic font: %s\n", agentite_get_last_error());
    }

    /* Initialize input */
    Agentite_Input *input = agentite_input_init();
    if (!input) {
        fprintf(stderr, "Failed to initialize input\n");
        aui_shutdown(ui);
        agentite_shutdown(engine);
        return 1;
    }

    /* Initialize localization */
    Agentite_LocalizationConfig loc_config = AGENTITE_LOCALIZATION_CONFIG_DEFAULT;
    loc_config.locales_path = "examples/localization/locales";
    loc_config.fallback_locale = "en";

    Agentite_Localization *loc = agentite_loc_create(&loc_config);
    if (!loc) {
        fprintf(stderr, "Failed to create localization: %s\n", agentite_get_last_error());
        agentite_input_shutdown(input);
        aui_shutdown(ui);
        agentite_shutdown(engine);
        return 1;
    }

    /* Set as global for LOC/LOCF/LOCP macros */
    agentite_loc_set_global(loc);

    /* Print available languages */
    printf("Loaded %zu languages:\n", agentite_loc_get_language_count(loc));
    for (size_t i = 0; i < agentite_loc_get_language_count(loc); i++) {
        const Agentite_LanguageInfo *info = agentite_loc_get_language_at(loc, i);
        printf("  [%zu] %s: %s (%s, font=%s)\n",
               i + 1, info->locale, info->name,
               info->direction == AGENTITE_TEXT_RTL ? "RTL" : "LTR",
               info->font_key);
    }

    /* Set initial language */
    agentite_loc_set_language(loc, "en");

    /* Main loop */
    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);

        agentite_input_begin_frame(input);

        /* Process events */
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

        /* Handle input */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE)) {
            agentite_quit(engine);
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_1)) {
            agentite_loc_set_language(loc, "en");
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_2)) {
            agentite_loc_set_language(loc, "de");
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_3)) {
            agentite_loc_set_language(loc, "ja");
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_4)) {
            agentite_loc_set_language(loc, "ar");
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_SPACE)) {
            s_item_count = (s_item_count % 12) + 1;
        }

        /* Begin UI frame */
        aui_begin_frame(ui, dt);

        /* Get current language info */
        const Agentite_LanguageInfo *lang_info = agentite_loc_get_language_info(loc);

        /* Set the language-specific font for the main panel */
        set_font_for_language(ui, loc);

        /* Main demo panel */
        if (aui_begin_panel(ui, LOC("title"), 20, 20, 860, 640,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

            /* Instructions */
            aui_label(ui, LOC("instructions"));
            aui_spacing(ui, 10);
            aui_separator(ui);
            aui_spacing(ui, 10);

            /* Language Info Section */
            aui_label(ui, LOC("section_info"));
            aui_spacing(ui, 5);

            char buf[256];
            snprintf(buf, sizeof(buf), "  %s: %s",
                     LOC("current_language"),
                     lang_info ? lang_info->name : "?");
            aui_label(ui, buf);

            /* Technical info uses default font (Arabic font lacks Latin chars) */
            aui_set_font(ui, s_font_default);
            snprintf(buf, sizeof(buf), "  Locale: %s | Direction: %s | Font: %s",
                     lang_info ? lang_info->locale : "?",
                     agentite_loc_get_text_direction(loc) == AGENTITE_TEXT_RTL ? "RTL" : "LTR",
                     agentite_loc_get_font_key(loc));
            aui_label(ui, buf);
            set_font_for_language(ui, loc);  /* Switch back */

            aui_spacing(ui, 10);
            aui_separator(ui);
            aui_spacing(ui, 10);

            /* Basic Strings Section */
            aui_label(ui, LOC("section_basic"));
            aui_spacing(ui, 5);

            const char *menu_keys[] = { "menu.start", "menu.options", "menu.credits", "menu.quit" };
            for (int i = 0; i < 4; i++) {
                snprintf(buf, sizeof(buf), "  %s", LOC(menu_keys[i]));
                aui_label(ui, buf);
            }

            aui_spacing(ui, 10);
            aui_separator(ui);
            aui_spacing(ui, 10);

            /* Parameter Substitution Section */
            aui_label(ui, LOC("section_params"));
            aui_spacing(ui, 5);

            /* Lines with English player name use default font */
            aui_set_font(ui, s_font_default);

            /* Greeting with name */
            snprintf(buf, sizeof(buf), "  %s", LOCF("greeting", "name", s_player_name, NULL));
            aui_label(ui, buf);

            /* Status with multiple params */
            char gold_str[32], level_str[32];
            snprintf(gold_str, sizeof(gold_str), "%d", s_gold);
            snprintf(level_str, sizeof(level_str), "%d", s_level);
            snprintf(buf, sizeof(buf), "  %s",
                     LOCF("status", "name", s_player_name, "gold", gold_str, "level", level_str, NULL));
            aui_label(ui, buf);

            /* Volume setting */
            snprintf(buf, sizeof(buf), "  %s", LOCF("settings.volume", "value", "75", NULL));
            aui_label(ui, buf);

            set_font_for_language(ui, loc);  /* Switch back */

            aui_spacing(ui, 10);
            aui_separator(ui);
            aui_spacing(ui, 10);

            /* Pluralization Section */
            aui_label(ui, LOC("section_plural"));
            aui_spacing(ui, 5);

            /* English instruction uses default font */
            aui_set_font(ui, s_font_default);
            snprintf(buf, sizeof(buf), "  Count = %d (press SPACE to change)", s_item_count);
            aui_label(ui, buf);
            set_font_for_language(ui, loc);
            aui_spacing(ui, 5);

            /* Use default font for English labels, Arabic font for localized count */
            aui_set_font(ui, s_font_default);
            aui_label(ui, "  items:   ");
            aui_same_line(ui);
            set_font_for_language(ui, loc);
            aui_label(ui, LOCP("items", s_item_count));

            aui_set_font(ui, s_font_default);
            aui_label(ui, "  coins:   ");
            aui_same_line(ui);
            set_font_for_language(ui, loc);
            aui_label(ui, LOCP("coins", s_item_count));

            aui_set_font(ui, s_font_default);
            aui_label(ui, "  enemies: ");
            aui_same_line(ui);
            set_font_for_language(ui, loc);
            aui_label(ui, LOCP("enemies", s_item_count));

            aui_end_panel(ui);
        }

        /* Switch to default bitmap font for Controls panel (English text) */
        aui_set_font(ui, s_font_default);

        /* Instructions panel at bottom */
        if (aui_begin_panel(ui, "Controls", 20, 680, 860, 80,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {
            aui_label(ui, "[1] English   [2] Deutsch   [3] Nihongo   [4] Arabiyya");
            aui_label(ui, "[SPACE] Cycle count   [ESC] Quit");
            aui_end_panel(ui);
        }

        aui_end_frame(ui);

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            aui_upload(ui, cmd);

            if (agentite_begin_render_pass(engine, 0.15f, 0.15f, 0.2f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
                aui_render(ui, cmd, pass);
                agentite_end_render_pass(engine);
            }
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_loc_destroy(loc);
    agentite_input_shutdown(input);
    aui_shutdown(ui);
    agentite_shutdown(engine);

    return 0;
}
