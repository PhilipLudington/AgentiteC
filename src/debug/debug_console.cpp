/**
 * @file debug_console.cpp
 * @brief Enhanced Debug Tools - Console Panel Implementation
 */

#include "agentite/debug.h"
#include "agentite/ui.h"
#include "debug_internal.h"

#include <SDL3/SDL.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * Console Panel Drawing
 * ============================================================================ */

bool agentite_debug_console_panel(Agentite_DebugSystem *debug,
                                   AUI_Context *ui,
                                   float x, float y, float w, float h)
{
    if (!debug || !ui) return false;

    DebugConsole *console = debug_get_console(debug);
    Agentite_DebugConfig *config = debug_get_config(debug);
    if (!console || !config || !console->is_open) return false;

    bool consumed_input = false;

    /* Calculate layout */
    float padding = 8.0f;
    float input_height = 24.0f;
    float output_height = h - input_height - padding * 3;

    /* Draw background */
    aui_draw_rect(ui, x, y, w, h, config->console_bg_color);

    /* Draw output area */
    float output_x = x + padding;
    float output_y = y + padding;
    float output_w = w - padding * 2;

    /* Get output lines */
    const char *lines[256];
    int line_count = agentite_debug_get_output(debug, lines, 256);

    /* Calculate how many lines fit */
    float line_height = 16.0f;  /* Approximate text height */
    int visible_lines = (int)(output_height / line_height);

    /* Draw output lines (newest at bottom) */
    int start_line = line_count > visible_lines ? line_count - visible_lines : 0;
    float text_y = output_y;

    for (int i = start_line; i < line_count; i++) {
        /* Determine color - check if line is error */
        uint32_t color = config->console_text_color;

        /* Note: We can't easily check is_error without access to the internal struct,
         * so for now all output uses the same color.
         * TODO: Add a function to get line metadata */

        aui_draw_text(ui, lines[i], output_x, text_y, color);
        text_y += line_height;
    }

    /* Draw separator line */
    float sep_y = y + output_height + padding * 2;
    aui_draw_line(ui, x + padding, sep_y, x + w - padding, sep_y, 0x404040FF, 1.0f);

    /* Draw input area */
    float input_x = x + padding;
    float input_y = sep_y + padding;
    float input_w = w - padding * 2;

    /* Draw input background */
    aui_draw_rect(ui, input_x, input_y, input_w, input_height, 0x2A2A2AFF);

    /* Draw prompt */
    aui_draw_text(ui, "> ", input_x + 4, input_y + 4, config->console_input_color);

    /* Draw input text */
    float text_x = input_x + 20;
    if (console->input_len > 0) {
        aui_draw_text(ui, console->input_buffer, text_x, input_y + 4, config->console_text_color);
    }

    /* Draw cursor (blinking) */
    static float cursor_timer = 0;
    cursor_timer += 0.016f;  /* Approximate frame time */
    if ((int)(cursor_timer * 2) % 2 == 0) {
        /* Calculate cursor position */
        float cursor_x = text_x;
        /* Approximate character width */
        cursor_x += (float)console->cursor_pos * 8.0f;
        aui_draw_line(ui, cursor_x, input_y + 4, cursor_x, input_y + input_height - 4, config->console_input_color, 2.0f);
    }

    /* Consume keyboard focus when console is open */
    consumed_input = true;

    return consumed_input;
}

/* ============================================================================
 * Console Event Handling
 * ============================================================================ */

bool agentite_debug_console_event(Agentite_DebugSystem *debug, const void *event_ptr)
{
    if (!debug) return false;

    DebugConsole *console = debug_get_console(debug);
    if (!console || !console->is_open) return false;

    const SDL_Event *event = (const SDL_Event *)event_ptr;

    /* Handle keyboard events */
    if (event->type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode key = event->key.key;

        /* Execute command on Enter */
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            if (console->input_len > 0) {
                console->input_buffer[console->input_len] = '\0';
                agentite_debug_execute(debug, console->input_buffer);
                console->input_len = 0;
                console->cursor_pos = 0;
                console->input_buffer[0] = '\0';
                console->history_index = -1;
            }
            return true;
        }

        /* Backspace */
        if (key == SDLK_BACKSPACE) {
            if (console->cursor_pos > 0) {
                /* Remove character before cursor */
                memmove(&console->input_buffer[console->cursor_pos - 1],
                        &console->input_buffer[console->cursor_pos],
                        (size_t)(console->input_len - console->cursor_pos + 1));
                console->cursor_pos--;
                console->input_len--;
            }
            return true;
        }

        /* Delete */
        if (key == SDLK_DELETE) {
            if (console->cursor_pos < console->input_len) {
                memmove(&console->input_buffer[console->cursor_pos],
                        &console->input_buffer[console->cursor_pos + 1],
                        (size_t)(console->input_len - console->cursor_pos));
                console->input_len--;
            }
            return true;
        }

        /* Cursor movement */
        if (key == SDLK_LEFT) {
            if (console->cursor_pos > 0) console->cursor_pos--;
            return true;
        }
        if (key == SDLK_RIGHT) {
            if (console->cursor_pos < console->input_len) console->cursor_pos++;
            return true;
        }
        if (key == SDLK_HOME) {
            console->cursor_pos = 0;
            return true;
        }
        if (key == SDLK_END) {
            console->cursor_pos = console->input_len;
            return true;
        }

        /* History navigation */
        if (key == SDLK_UP) {
            if (console->history && console->history_count > 0) {
                if (console->history_index < console->history_count - 1) {
                    console->history_index++;
                    int hist_idx = console->history_count - 1 - console->history_index;
                    if (hist_idx >= 0 && console->history[hist_idx]) {
                        snprintf(console->input_buffer, DEBUG_MAX_INPUT, "%s",
                                 console->history[hist_idx]);
                        console->input_len = (int)strlen(console->input_buffer);
                        console->cursor_pos = console->input_len;
                    }
                }
            }
            return true;
        }
        if (key == SDLK_DOWN) {
            if (console->history_index > 0) {
                console->history_index--;
                int hist_idx = console->history_count - 1 - console->history_index;
                if (hist_idx >= 0 && hist_idx < console->history_count &&
                    console->history[hist_idx]) {
                    snprintf(console->input_buffer, DEBUG_MAX_INPUT, "%s",
                             console->history[hist_idx]);
                    console->input_len = (int)strlen(console->input_buffer);
                    console->cursor_pos = console->input_len;
                }
            } else if (console->history_index == 0) {
                console->history_index = -1;
                console->input_buffer[0] = '\0';
                console->input_len = 0;
                console->cursor_pos = 0;
            }
            return true;
        }

        /* Escape to close console */
        if (key == SDLK_ESCAPE) {
            agentite_debug_toggle_console(debug);
            return true;
        }

        /* Tab for potential auto-complete (not implemented yet) */
        if (key == SDLK_TAB) {
            /* TODO: Auto-complete */
            return true;
        }
    }

    /* Handle text input */
    if (event->type == SDL_EVENT_TEXT_INPUT) {
        const char *text = event->text.text;
        size_t text_len = strlen(text);

        /* Don't process backtick (console toggle key) */
        if (text[0] == '`' || text[0] == '~') {
            return true;
        }

        /* Insert text at cursor */
        if (console->input_len + (int)text_len < DEBUG_MAX_INPUT - 1) {
            /* Make room for new text */
            memmove(&console->input_buffer[console->cursor_pos + text_len],
                    &console->input_buffer[console->cursor_pos],
                    (size_t)(console->input_len - console->cursor_pos + 1));

            /* Insert new text */
            memcpy(&console->input_buffer[console->cursor_pos], text, text_len);
            console->cursor_pos += (int)text_len;
            console->input_len += (int)text_len;
        }
        return true;
    }

    return false;
}
