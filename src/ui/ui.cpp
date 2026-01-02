/*
 * Agentite UI - Core Context and Lifecycle
 */

#include "agentite/ui.h"
#include "agentite/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Forward declarations from other UI modules */
extern void aui_state_clear(AUI_Context *ctx);
extern void aui_state_gc(AUI_Context *ctx, uint64_t max_age);
extern bool aui_create_pipeline(AUI_Context *ctx);
extern void aui_destroy_pipeline(AUI_Context *ctx);
extern bool aui_load_font(AUI_Context *ctx, const char *path, float size);
extern void aui_free_font(AUI_Context *ctx);

/* ============================================================================
 * Theme System
 * ============================================================================ */

AUI_Theme aui_theme_dark(void)
{
    AUI_Theme theme = {0};

    /* Background colors */
    theme.bg_panel          = 0xF21A1A2E;  /* Dark blue, slight transparency */
    theme.bg_widget         = 0xFF3D3D4A;  /* Gray */
    theme.bg_widget_hover   = 0xFF4D4D5A;  /* Lighter gray */
    theme.bg_widget_active  = 0xFF2D2D3A;  /* Darker gray */
    theme.bg_widget_disabled = 0xFF252530; /* Very dark */

    /* Border */
    theme.border            = 0xFF4A4A5A;  /* Medium gray */

    /* Text colors */
    theme.text              = 0xFFE0E0E0;  /* Light gray */
    theme.text_dim          = 0xFF808080;  /* Dim gray */
    theme.text_highlight    = 0xFFFFFFFF;  /* White */
    theme.text_disabled     = 0xFF707070;  /* Medium-dark gray (readable on dark bg) */

    /* Accent color (copper/orange) */
    theme.accent            = 0xFFEF9A4D;  /* Copper (ABGR) */
    theme.accent_hover      = 0xFFFFA85D;  /* Lighter copper */
    theme.accent_active     = 0xFFDF8A3D;  /* Darker copper */

    /* Semantic colors */
    theme.success           = 0xFF50C878;  /* Emerald green */
    theme.success_hover     = 0xFF60D888;  /* Lighter green */
    theme.warning           = 0xFF50BFFF;  /* Orange (ABGR) */
    theme.warning_hover     = 0xFF60CFFF;  /* Lighter orange */
    theme.danger            = 0xFF5050EF;  /* Red (ABGR) */
    theme.danger_hover      = 0xFF6060FF;  /* Lighter red */
    theme.info              = 0xFFEFAF50;  /* Blue (ABGR) */
    theme.info_hover        = 0xFFFFBF60;  /* Lighter blue */

    /* Widget-specific colors */
    theme.checkbox_check    = 0xFFFFFFFF;  /* White */
    theme.slider_track      = 0xFF2A2A3A;  /* Dark */
    theme.slider_grab       = 0xFFEF9A4D;  /* Accent */
    theme.scrollbar         = 0x80404050;  /* Semi-transparent */
    theme.scrollbar_grab    = 0xC0606070;  /* Lighter */
    theme.progress_fill     = 0xFFEF9A4D;  /* Accent */
    theme.selection         = 0x804D9AEF;  /* Semi-transparent accent */

    /* Metrics */
    theme.corner_radius     = 4.0f;
    theme.border_width      = 1.0f;
    theme.widget_height     = 28.0f;
    theme.spacing           = 4.0f;
    theme.padding           = 8.0f;
    theme.scrollbar_width   = 12.0f;

    return theme;
}

AUI_Theme aui_theme_light(void)
{
    AUI_Theme theme = {0};

    /* Background colors */
    theme.bg_panel          = 0xF2F5F5F5;  /* Light gray, slight transparency */
    theme.bg_widget         = 0xFFFFFFFF;  /* White */
    theme.bg_widget_hover   = 0xFFE8E8E8;  /* Light gray */
    theme.bg_widget_active  = 0xFFD0D0D0;  /* Medium gray */
    theme.bg_widget_disabled = 0xFFF0F0F0; /* Very light gray */

    /* Border */
    theme.border            = 0xFFC0C0C0;  /* Light gray border */

    /* Text colors */
    theme.text              = 0xFF202020;  /* Dark gray */
    theme.text_dim          = 0xFF707070;  /* Medium gray */
    theme.text_highlight    = 0xFF000000;  /* Black */
    theme.text_disabled     = 0xFFA0A0A0;  /* Light gray */

    /* Accent color (blue) */
    theme.accent            = 0xFFD07020;  /* Blue (ABGR) */
    theme.accent_hover      = 0xFFE08030;  /* Lighter blue */
    theme.accent_active     = 0xFFC06010;  /* Darker blue */

    /* Semantic colors */
    theme.success           = 0xFF40A060;  /* Green */
    theme.success_hover     = 0xFF50B070;  /* Lighter green */
    theme.warning           = 0xFF30A0E0;  /* Orange (ABGR) */
    theme.warning_hover     = 0xFF40B0F0;  /* Lighter orange */
    theme.danger            = 0xFF4040D0;  /* Red (ABGR) */
    theme.danger_hover      = 0xFF5050E0;  /* Lighter red */
    theme.info              = 0xFFD09030;  /* Blue (ABGR) */
    theme.info_hover        = 0xFFE0A040;  /* Lighter blue */

    /* Widget-specific colors */
    theme.checkbox_check    = 0xFFFFFFFF;  /* White (on accent bg) */
    theme.slider_track      = 0xFFD0D0D0;  /* Light gray */
    theme.slider_grab       = 0xFFD07020;  /* Accent */
    theme.scrollbar         = 0x40000000;  /* Semi-transparent black */
    theme.scrollbar_grab    = 0x80606060;  /* Gray */
    theme.progress_fill     = 0xFFD07020;  /* Accent */
    theme.selection         = 0x602070D0;  /* Semi-transparent accent */

    /* Metrics (same as dark) */
    theme.corner_radius     = 4.0f;
    theme.border_width      = 1.0f;
    theme.widget_height     = 28.0f;
    theme.spacing           = 4.0f;
    theme.padding           = 8.0f;
    theme.scrollbar_width   = 12.0f;

    return theme;
}

void aui_set_theme(AUI_Context *ctx, const AUI_Theme *theme)
{
    if (!ctx || !theme) return;
    ctx->theme = *theme;
}

const AUI_Theme *aui_get_theme(const AUI_Context *ctx)
{
    if (!ctx) return NULL;
    return &ctx->theme;
}

void aui_theme_set_accent(AUI_Theme *theme, uint32_t color)
{
    if (!theme) return;
    theme->accent = color;
    theme->accent_hover = aui_color_brighten(color, 0.15f);
    theme->accent_active = aui_color_darken(color, 0.15f);
    theme->slider_grab = color;
    theme->progress_fill = color;
    theme->selection = aui_color_alpha(color, 0.5f);
}

void aui_theme_set_semantic_colors(AUI_Theme *theme,
                                    uint32_t success, uint32_t warning,
                                    uint32_t danger, uint32_t info)
{
    if (!theme) return;
    theme->success = success;
    theme->success_hover = aui_color_brighten(success, 0.15f);
    theme->warning = warning;
    theme->warning_hover = aui_color_brighten(warning, 0.15f);
    theme->danger = danger;
    theme->danger_hover = aui_color_brighten(danger, 0.15f);
    theme->info = info;
    theme->info_hover = aui_color_brighten(info, 0.15f);
}

void aui_theme_scale(AUI_Theme *theme, float dpi_scale)
{
    if (!theme || dpi_scale <= 0.0f) return;

    /* Scale all metric values by DPI factor */
    theme->corner_radius *= dpi_scale;
    theme->border_width *= dpi_scale;
    theme->widget_height *= dpi_scale;
    theme->spacing *= dpi_scale;
    theme->padding *= dpi_scale;
    theme->scrollbar_width *= dpi_scale;
}

void aui_set_dpi_scale(AUI_Context *ctx, float dpi_scale)
{
    if (!ctx) return;
    ctx->dpi_scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
}

float aui_get_dpi_scale(const AUI_Context *ctx)
{
    return ctx ? ctx->dpi_scale : 1.0f;
}

static void aui_init_theme(AUI_Context *ctx)
{
    ctx->theme = aui_theme_dark();
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

AUI_Context *aui_init(SDL_GPUDevice *gpu, SDL_Window *window, int width, int height,
                      const char *font_path, float font_size)
{
    AUI_Context *ctx = (AUI_Context *)calloc(1, sizeof(AUI_Context));
    if (!ctx) {
        agentite_set_error("CUI: Failed to allocate context");
        return NULL;
    }

    ctx->gpu = gpu;
    ctx->window = window;
    ctx->width = width;
    ctx->height = height;
    ctx->dpi_scale = 1.0f;  /* Default to 1.0, caller can adjust via aui_set_dpi_scale */

    /* Initialize theme */
    aui_init_theme(ctx);

    /* Allocate vertex/index buffers (CPU side) */
    ctx->vertex_capacity = 65536;
    ctx->index_capacity = 98304;  /* 1.5x vertices for quads */
    ctx->vertices = (AUI_Vertex *)malloc(ctx->vertex_capacity * sizeof(AUI_Vertex));
    ctx->indices = (uint16_t *)malloc(ctx->index_capacity * sizeof(uint16_t));

    if (!ctx->vertices || !ctx->indices) {
        agentite_set_error("CUI: Failed to allocate vertex/index arrays");
        aui_shutdown(ctx);
        return NULL;
    }

    /* Create GPU pipeline and resources */
    if (!aui_create_pipeline(ctx)) {
        agentite_set_error("CUI: Failed to create GPU pipeline");
        aui_shutdown(ctx);
        return NULL;
    }

    /* Load font */
    if (font_path && !aui_load_font(ctx, font_path, font_size)) {
        agentite_set_error("CUI: Failed to load font '%s'", font_path);
        aui_shutdown(ctx);
        return NULL;
    }

    /* Initialize layout with full screen */
    ctx->layout_stack[0].bounds = (AUI_Rect){0, 0, (float)width, (float)height};
    ctx->layout_stack[0].cursor_x = 0;
    ctx->layout_stack[0].cursor_y = 0;
    ctx->layout_stack[0].spacing = ctx->theme.spacing;
    ctx->layout_stack[0].padding = ctx->theme.padding;
    ctx->layout_stack[0].horizontal = false;
    ctx->layout_depth = 1;

    SDL_Log("CUI: Initialized (%dx%d)", width, height);
    return ctx;
}

void aui_shutdown(AUI_Context *ctx)
{
    if (!ctx) return;

    aui_destroy_pipeline(ctx);
    aui_free_font(ctx);
    aui_state_clear(ctx);

    free(ctx->vertices);
    free(ctx->indices);
    free(ctx->path_points);
    free(ctx);

    SDL_Log("CUI: Shutdown complete");
}

/* Forward declaration from ui_draw.cpp */
extern void aui_reset_draw_state(AUI_Context *ctx);

void aui_begin_frame(AUI_Context *ctx, float delta_time)
{
    if (!ctx) return;

    ctx->delta_time = delta_time;
    ctx->frame_count++;

    /* Reset draw state (buffers, command queue, layers) */
    aui_reset_draw_state(ctx);

    /* Reset layout to root */
    ctx->layout_depth = 1;
    ctx->layout_stack[0].cursor_x = ctx->layout_stack[0].padding;
    ctx->layout_stack[0].cursor_y = ctx->layout_stack[0].padding;

    /* Reset scissor stack */
    ctx->scissor_depth = 0;

    /* Clear hot widget (will be set during widget processing) */
    ctx->hot = AUI_ID_NONE;

    /* Reset focus navigation state for this frame */
    ctx->first_focusable = AUI_ID_NONE;
    ctx->last_focusable = AUI_ID_NONE;
    ctx->prev_focusable = AUI_ID_NONE;
    ctx->focus_found_this_frame = false;

    /* Reset spatial focus tracking for this frame */
    ctx->focusable_widget_count = 0;

    /* Clear gamepad per-frame states */
    memset(ctx->input.gamepad_button_pressed, 0, sizeof(ctx->input.gamepad_button_pressed));
    memset(ctx->input.gamepad_button_released, 0, sizeof(ctx->input.gamepad_button_released));

    /* Garbage collect old state entries every 60 frames */
    if (ctx->frame_count % 60 == 0) {
        aui_state_gc(ctx, 300);  /* Remove entries not used for 5 seconds */
    }
}

/* Forward declaration for deferred popup rendering */
extern void aui_draw_rect(AUI_Context *ctx, float x, float y, float w, float h, uint32_t color);
extern void aui_draw_rect_outline(AUI_Context *ctx, float x, float y, float w, float h, uint32_t color, float thickness);
extern float aui_draw_text(AUI_Context *ctx, const char *text, float x, float y, uint32_t color);
extern float aui_text_height(AUI_Context *ctx);

/* Find the widget position by ID in the focusable widgets array */
static bool aui_find_widget_position(AUI_Context *ctx, AUI_Id id,
                                      float *out_x, float *out_y)
{
    for (int i = 0; i < ctx->focusable_widget_count; i++) {
        if (ctx->focusable_widgets[i].id == id) {
            *out_x = ctx->focusable_widgets[i].center_x;
            *out_y = ctx->focusable_widgets[i].center_y;
            return true;
        }
    }
    return false;
}

/* Find the best widget in a direction for spatial navigation */
static AUI_Id aui_find_widget_in_direction(AUI_Context *ctx, float from_x, float from_y,
                                            bool up, bool down, bool left, bool right)
{
    AUI_Id best_id = AUI_ID_NONE;
    float best_score = 1e10f;

    for (int i = 0; i < ctx->focusable_widget_count; i++) {
        AUI_Id id = ctx->focusable_widgets[i].id;
        if (id == ctx->focused) continue;  /* Skip current */

        float wx = ctx->focusable_widgets[i].center_x;
        float wy = ctx->focusable_widgets[i].center_y;

        float dx = wx - from_x;
        float dy = wy - from_y;

        /* Check if widget is in the right direction */
        bool valid = false;
        float primary_dist = 0;
        float secondary_dist = 0;

        if (up && dy < -5.0f) {
            valid = true;
            primary_dist = -dy;  /* Distance upward (positive = further up) */
            secondary_dist = fabsf(dx);  /* Lateral offset */
        } else if (down && dy > 5.0f) {
            valid = true;
            primary_dist = dy;   /* Distance downward */
            secondary_dist = fabsf(dx);
        } else if (left && dx < -5.0f) {
            valid = true;
            primary_dist = -dx;  /* Distance leftward */
            secondary_dist = fabsf(dy);
        } else if (right && dx > 5.0f) {
            valid = true;
            primary_dist = dx;   /* Distance rightward */
            secondary_dist = fabsf(dy);
        }

        if (!valid) continue;

        /* Score: prefer widgets that are more aligned (lower secondary distance)
           and closer (lower primary distance). Secondary is weighted more heavily
           to favor aligned widgets even if slightly further away. */
        float score = primary_dist + secondary_dist * 2.0f;

        if (score < best_score) {
            best_score = score;
            best_id = id;
        }
    }

    return best_id;
}

void aui_end_frame(AUI_Context *ctx)
{
    if (!ctx) return;

    /* Store previous mouse position */
    ctx->input.mouse_prev_x = ctx->input.mouse_x;
    ctx->input.mouse_prev_y = ctx->input.mouse_y;

    /* Handle spatial (D-pad/gamepad) focus navigation */
    bool any_direction = ctx->focus_up_requested || ctx->focus_down_requested ||
                         ctx->focus_left_requested || ctx->focus_right_requested;

    if (any_direction && ctx->focusable_widget_count > 0) {
        float from_x = ctx->width * 0.5f;   /* Default to screen center */
        float from_y = ctx->height * 0.5f;

        /* Get current focused widget position */
        if (ctx->focused != AUI_ID_NONE) {
            aui_find_widget_position(ctx, ctx->focused, &from_x, &from_y);
        }

        /* Find best widget in requested direction */
        AUI_Id target = aui_find_widget_in_direction(ctx, from_x, from_y,
            ctx->focus_up_requested, ctx->focus_down_requested,
            ctx->focus_left_requested, ctx->focus_right_requested);

        if (target != AUI_ID_NONE) {
            ctx->focused = target;
        } else if (ctx->focused == AUI_ID_NONE && ctx->first_focusable != AUI_ID_NONE) {
            /* No current focus and no target found - focus first widget */
            ctx->focused = ctx->first_focusable;
        }
    }

    /* Clear directional focus requests */
    ctx->focus_up_requested = false;
    ctx->focus_down_requested = false;
    ctx->focus_left_requested = false;
    ctx->focus_right_requested = false;

    /* Handle focus wrap-around for Tab navigation */
    if (ctx->focus_next_requested) {
        /* Tab was pressed but no widget grabbed focus - wrap to first */
        if (ctx->first_focusable != AUI_ID_NONE) {
            ctx->focused = ctx->first_focusable;
        }
        ctx->focus_next_requested = false;
    }
    if (ctx->focus_prev_requested) {
        /* Shift+Tab was pressed but no widget grabbed focus - wrap to last */
        if (ctx->last_focusable != AUI_ID_NONE) {
            ctx->focused = ctx->last_focusable;
        }
        ctx->focus_prev_requested = false;
    }

    /* Handle text input start/stop based on focus changes */
    if (ctx->focused != ctx->prev_focused) {
        if (ctx->focused != AUI_ID_NONE && ctx->window) {
            /* A widget gained focus - start text input */
            SDL_StartTextInput(ctx->window);
        } else if (ctx->prev_focused != AUI_ID_NONE && ctx->window) {
            /* Focus was lost - stop text input */
            SDL_StopTextInput(ctx->window);
        }
        ctx->prev_focused = ctx->focused;
    }

    /* Draw deferred popup (renders on top of everything) */
    if (ctx->open_popup != AUI_ID_NONE && ctx->popup_items && ctx->popup_selected) {
        /* Draw popup background */
        aui_draw_rect(ctx, ctx->popup_rect.x, ctx->popup_rect.y,
                      ctx->popup_rect.w, ctx->popup_rect.h,
                      ctx->theme.bg_panel);
        aui_draw_rect_outline(ctx, ctx->popup_rect.x, ctx->popup_rect.y,
                              ctx->popup_rect.w, ctx->popup_rect.h,
                              ctx->theme.border, 1.0f);

        /* Draw popup items */
        for (int i = 0; i < ctx->popup_count; i++) {
            AUI_Rect item_rect = {
                ctx->popup_rect.x,
                ctx->popup_rect.y + i * ctx->theme.widget_height,
                ctx->popup_rect.w,
                ctx->theme.widget_height
            };

            bool item_hovered = aui_rect_contains(item_rect,
                                                   ctx->input.mouse_x,
                                                   ctx->input.mouse_y);

            if (item_hovered) {
                aui_draw_rect(ctx, item_rect.x, item_rect.y, item_rect.w, item_rect.h,
                              ctx->theme.bg_widget_hover);

                if (ctx->input.mouse_pressed[0]) {
                    *ctx->popup_selected = i;
                    ctx->open_popup = AUI_ID_NONE;
                    ctx->popup_changed = true;
                }
            }

            float item_text_y = item_rect.y + (item_rect.h - aui_text_height(ctx)) * 0.5f;
            aui_draw_text(ctx, ctx->popup_items[i], item_rect.x + ctx->theme.padding,
                          item_text_y, ctx->theme.text);
        }
    }

    /* Close popup if clicked outside */
    if (ctx->open_popup != AUI_ID_NONE && ctx->input.mouse_pressed[0]) {
        if (!aui_rect_contains(ctx->popup_rect,
                               ctx->input.mouse_x, ctx->input.mouse_y)) {
            ctx->open_popup = AUI_ID_NONE;
        }
    }

    /* NOTE: Tooltip is drawn in aui_render() for proper z-ordering */

    /* Clear per-frame input state (pressed/released are one-shot) */
    for (int i = 0; i < 3; i++) {
        ctx->input.mouse_pressed[i] = false;
        ctx->input.mouse_released[i] = false;
    }
    memset(ctx->input.keys_pressed, 0, sizeof(ctx->input.keys_pressed));

    /* Clear text input */
    ctx->input.text_input[0] = '\0';
    ctx->input.text_input_len = 0;

    /* Reset scroll (consumed this frame) */
    ctx->input.scroll_x = 0;
    ctx->input.scroll_y = 0;
}

void aui_set_screen_size(AUI_Context *ctx, int width, int height)
{
    if (!ctx) return;
    ctx->width = width;
    ctx->height = height;
    ctx->layout_stack[0].bounds.w = (float)width;
    ctx->layout_stack[0].bounds.h = (float)height;
}

/* ============================================================================
 * Event Processing
 * ============================================================================ */

bool aui_process_event(AUI_Context *ctx, const SDL_Event *event)
{
    if (!ctx || !event) return false;

    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION:
        ctx->input.mouse_x = event->motion.x;
        ctx->input.mouse_y = event->motion.y;
        /* Switch to mouse mode on significant mouse movement */
        if (ctx->gamepad_mode) {
            float dx = event->motion.xrel;
            float dy = event->motion.yrel;
            if (dx * dx + dy * dy > 4.0f) {  /* Movement threshold */
                ctx->gamepad_mode = false;
            }
        }
        return false;  /* Don't consume motion events */

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button <= 3) {
            int btn = event->button.button - 1;
            ctx->input.mouse_down[btn] = true;
            ctx->input.mouse_pressed[btn] = true;
        }
        return ctx->hot != AUI_ID_NONE;  /* Consume if over UI */

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button <= 3) {
            int btn = event->button.button - 1;
            ctx->input.mouse_down[btn] = false;
            ctx->input.mouse_released[btn] = true;
        }
        return ctx->active != AUI_ID_NONE;

    case SDL_EVENT_MOUSE_WHEEL:
        ctx->input.scroll_x = event->wheel.x;
        ctx->input.scroll_y = event->wheel.y;
        /* Never consume scroll events - let game handle camera zoom even over UI */
        return false;

    case SDL_EVENT_KEY_DOWN:
        if (event->key.scancode < 512) {
            ctx->input.keys_down[event->key.scancode] = true;
            ctx->input.keys_pressed[event->key.scancode] = true;
        }
        ctx->input.shift = (event->key.mod & SDL_KMOD_SHIFT) != 0;
        ctx->input.ctrl = (event->key.mod & SDL_KMOD_CTRL) != 0;
        ctx->input.alt = (event->key.mod & SDL_KMOD_ALT) != 0;

        /* Handle Tab key for focus navigation */
        if (event->key.scancode == SDL_SCANCODE_TAB) {
            if (ctx->input.shift) {
                ctx->focus_prev_requested = true;
            } else {
                ctx->focus_next_requested = true;
            }
            return true;  /* Consume Tab key */
        }

        /* Process global keyboard shortcuts (only when no textbox has focus) */
        if (ctx->focused == AUI_ID_NONE) {
            if (aui_shortcuts_process(ctx)) {
                return true;  /* Consume if shortcut triggered */
            }
        }

        /* Don't consume function keys (F1-F12) or ESC - let game handle them */
        if ((event->key.scancode >= SDL_SCANCODE_F1 &&
             event->key.scancode <= SDL_SCANCODE_F12) ||
            event->key.scancode == SDL_SCANCODE_ESCAPE) {
            return false;
        }

        return ctx->focused != AUI_ID_NONE;

    case SDL_EVENT_KEY_UP:
        if (event->key.scancode < 512) {
            ctx->input.keys_down[event->key.scancode] = false;
        }
        ctx->input.shift = (event->key.mod & SDL_KMOD_SHIFT) != 0;
        ctx->input.ctrl = (event->key.mod & SDL_KMOD_CTRL) != 0;
        ctx->input.alt = (event->key.mod & SDL_KMOD_ALT) != 0;
        return false;

    case SDL_EVENT_TEXT_INPUT:
        if (ctx->focused != AUI_ID_NONE) {
            size_t len = strlen(event->text.text);
            if (ctx->input.text_input_len + len < sizeof(ctx->input.text_input) - 1) {
                memcpy(ctx->input.text_input + ctx->input.text_input_len,
                       event->text.text, len);
                ctx->input.text_input_len += (int)len;
                ctx->input.text_input[ctx->input.text_input_len] = '\0';
            }
            return true;
        }
        return false;

    /* Gamepad events */
    case SDL_EVENT_GAMEPAD_ADDED: {
        SDL_JoystickID id = event->gdevice.which;
        if (ctx->gamepad_id == 0) {
            /* Open the gamepad if we don't have one */
            SDL_Gamepad *gp = SDL_OpenGamepad(id);
            if (gp) {
                ctx->gamepad_id = id;
                SDL_Log("AUI: Gamepad connected (id=%d)", (int)id);
            }
        }
        return false;
    }

    case SDL_EVENT_GAMEPAD_REMOVED: {
        SDL_JoystickID id = event->gdevice.which;
        if (ctx->gamepad_id == id) {
            ctx->gamepad_id = 0;
            ctx->gamepad_mode = false;
            memset(ctx->input.gamepad_button_down, 0,
                   sizeof(ctx->input.gamepad_button_down));
            SDL_Log("AUI: Gamepad disconnected (id=%d)", (int)id);
        }
        return false;
    }

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
        int btn = event->gbutton.button;
        if (btn >= 0 && btn < AUI_GAMEPAD_BUTTON_COUNT) {
            ctx->input.gamepad_button_down[btn] = true;
            ctx->input.gamepad_button_pressed[btn] = true;
        }

        /* Switch to gamepad mode on any button press */
        ctx->gamepad_mode = true;

        /* D-pad navigation */
        if (btn == SDL_GAMEPAD_BUTTON_DPAD_UP) {
            ctx->focus_up_requested = true;
            return true;
        }
        if (btn == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
            ctx->focus_down_requested = true;
            return true;
        }
        if (btn == SDL_GAMEPAD_BUTTON_DPAD_LEFT) {
            ctx->focus_left_requested = true;
            return true;
        }
        if (btn == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
            ctx->focus_right_requested = true;
            return true;
        }

        /* A button = activate (like Enter/Space) */
        if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
            /* Simulate space key press for widget activation */
            ctx->input.keys_pressed[SDL_SCANCODE_RETURN] = true;
            ctx->input.keys_down[SDL_SCANCODE_RETURN] = true;
            return true;
        }

        /* B button = cancel (like Escape) */
        if (btn == SDL_GAMEPAD_BUTTON_EAST) {
            /* Simulate escape key press for cancel */
            ctx->input.keys_pressed[SDL_SCANCODE_ESCAPE] = true;
            ctx->input.keys_down[SDL_SCANCODE_ESCAPE] = true;
            return true;
        }

        return false;
    }

    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        int btn = event->gbutton.button;
        if (btn >= 0 && btn < AUI_GAMEPAD_BUTTON_COUNT) {
            ctx->input.gamepad_button_down[btn] = false;
            ctx->input.gamepad_button_released[btn] = true;
        }

        /* Release simulated keys for A and B buttons */
        if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
            ctx->input.keys_down[SDL_SCANCODE_RETURN] = false;
        }
        if (btn == SDL_GAMEPAD_BUTTON_EAST) {
            ctx->input.keys_down[SDL_SCANCODE_ESCAPE] = false;
        }

        return false;
    }

    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        int axis = event->gaxis.axis;
        float value = event->gaxis.value / 32767.0f;  /* Normalize to -1..1 */

        /* Apply deadzone (0.2) */
        if (value > -0.2f && value < 0.2f) {
            value = 0.0f;
        }

        switch (axis) {
        case SDL_GAMEPAD_AXIS_LEFTX:
            ctx->input.gamepad_axis_left_x = value;
            break;
        case SDL_GAMEPAD_AXIS_LEFTY:
            ctx->input.gamepad_axis_left_y = value;
            break;
        case SDL_GAMEPAD_AXIS_RIGHTX:
            ctx->input.gamepad_axis_right_x = value;
            break;
        case SDL_GAMEPAD_AXIS_RIGHTY:
            ctx->input.gamepad_axis_right_y = value;
            break;
        }

        /* Switch to gamepad mode on significant stick movement */
        if (value < -0.5f || value > 0.5f) {
            ctx->gamepad_mode = true;
        }

        return false;
    }

    default:
        return false;
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

uint32_t aui_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) |
           ((uint32_t)g << 8) | (uint32_t)r;
}

uint32_t aui_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return aui_rgba(r, g, b, 255);
}

uint32_t aui_color_lerp(uint32_t a, uint32_t b, float t)
{
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;

    uint8_t ar = (a >> 0) & 0xFF;
    uint8_t ag = (a >> 8) & 0xFF;
    uint8_t ab = (a >> 16) & 0xFF;
    uint8_t aa = (a >> 24) & 0xFF;

    uint8_t br = (b >> 0) & 0xFF;
    uint8_t bg = (b >> 8) & 0xFF;
    uint8_t bb = (b >> 16) & 0xFF;
    uint8_t ba = (b >> 24) & 0xFF;

    uint8_t rr = (uint8_t)(ar + (br - ar) * t);
    uint8_t rg = (uint8_t)(ag + (bg - ag) * t);
    uint8_t rb = (uint8_t)(ab + (bb - ab) * t);
    uint8_t ra = (uint8_t)(aa + (ba - aa) * t);

    return aui_rgba(rr, rg, rb, ra);
}

uint32_t aui_color_alpha(uint32_t color, float alpha)
{
    uint8_t a = (uint8_t)(((color >> 24) & 0xFF) * alpha);
    return (color & 0x00FFFFFF) | ((uint32_t)a << 24);
}

uint32_t aui_color_brighten(uint32_t color, float amount)
{
    uint8_t r = (color >> 0) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = (color >> 16) & 0xFF;
    uint8_t a = (color >> 24) & 0xFF;

    int nr = (int)(r + (255 - r) * amount);
    int ng = (int)(g + (255 - g) * amount);
    int nb = (int)(b + (255 - b) * amount);

    if (nr > 255) nr = 255;
    if (ng > 255) ng = 255;
    if (nb > 255) nb = 255;

    return aui_rgba((uint8_t)nr, (uint8_t)ng, (uint8_t)nb, a);
}

uint32_t aui_color_darken(uint32_t color, float amount)
{
    uint8_t r = (color >> 0) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = (color >> 16) & 0xFF;
    uint8_t a = (color >> 24) & 0xFF;

    int nr = (int)(r * (1.0f - amount));
    int ng = (int)(g * (1.0f - amount));
    int nb = (int)(b * (1.0f - amount));

    if (nr < 0) nr = 0;
    if (ng < 0) ng = 0;
    if (nb < 0) nb = 0;

    return aui_rgba((uint8_t)nr, (uint8_t)ng, (uint8_t)nb, a);
}

bool aui_rect_contains(AUI_Rect rect, float x, float y)
{
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

AUI_Rect aui_rect_intersect(AUI_Rect a, AUI_Rect b)
{
    float x1 = (a.x > b.x) ? a.x : b.x;
    float y1 = (a.y > b.y) ? a.y : b.y;
    float x2 = (a.x + a.w < b.x + b.w) ? a.x + a.w : b.x + b.w;
    float y2 = (a.y + a.h < b.y + b.h) ? a.y + a.h : b.y + b.h;

    AUI_Rect result;
    result.x = x1;
    result.y = y1;
    result.w = (x2 > x1) ? x2 - x1 : 0;
    result.h = (y2 > y1) ? y2 - y1 : 0;
    return result;
}

/* ============================================================================
 * Focus Navigation
 * ============================================================================ */

bool aui_focus_register_rect(AUI_Context *ctx, AUI_Id id, AUI_Rect rect)
{
    if (!ctx || id == AUI_ID_NONE) return false;

    /* Track widget position for spatial (gamepad) navigation */
    if (ctx->focusable_widget_count < 128) {
        int idx = ctx->focusable_widget_count++;
        ctx->focusable_widgets[idx].id = id;
        ctx->focusable_widgets[idx].center_x = rect.x + rect.w * 0.5f;
        ctx->focusable_widgets[idx].center_y = rect.y + rect.h * 0.5f;
    }

    /* Track first focusable widget */
    if (ctx->first_focusable == AUI_ID_NONE) {
        ctx->first_focusable = id;
    }

    /* Track last focusable widget */
    ctx->last_focusable = id;

    bool should_focus = false;

    /* Handle focus navigation */
    if (ctx->focused == id) {
        /* This widget is currently focused */
        ctx->focus_found_this_frame = true;
    } else if (ctx->focus_next_requested && ctx->focus_found_this_frame) {
        /* Tab was pressed and we just passed the focused widget - grab focus */
        ctx->focused = id;
        ctx->focus_next_requested = false;
        should_focus = true;
    } else if (ctx->focus_prev_requested && !ctx->focus_found_this_frame &&
               ctx->focused != AUI_ID_NONE) {
        /* Shift+Tab: track the widget before focused one */
        ctx->prev_focusable = id;
    }

    /* When we encounter the focused widget with Shift+Tab pending,
       focus the previously tracked widget */
    if (ctx->focus_prev_requested && ctx->focused == id &&
        ctx->prev_focusable != AUI_ID_NONE) {
        ctx->focused = ctx->prev_focusable;
        ctx->focus_prev_requested = false;
        /* The previously focused widget is now unfocused, but we need
           to signal the newly focused one - it was already processed this frame.
           The focus will take effect next frame. */
    }

    return should_focus;
}

bool aui_focus_register(AUI_Context *ctx, AUI_Id id)
{
    /* Register with default position (will use first tracked position for
       this widget, or screen center if none) */
    AUI_Rect default_rect = { 0, 0, 0, 0 };
    return aui_focus_register_rect(ctx, id, default_rect);
}

bool aui_has_focus(AUI_Context *ctx, AUI_Id id)
{
    if (!ctx) return false;
    return ctx->focused == id;
}

void aui_set_focus(AUI_Context *ctx, AUI_Id id)
{
    if (!ctx) return;
    ctx->focused = id;
}

void aui_clear_focus(AUI_Context *ctx)
{
    if (!ctx) return;
    ctx->focused = AUI_ID_NONE;
}

/* ============================================================================
 * Gamepad Navigation
 * ============================================================================ */

bool aui_is_gamepad_mode(AUI_Context *ctx)
{
    if (!ctx) return false;
    return ctx->gamepad_mode;
}

void aui_set_gamepad_mode(AUI_Context *ctx, bool enabled)
{
    if (!ctx) return;
    ctx->gamepad_mode = enabled;
}

SDL_JoystickID aui_get_gamepad_id(AUI_Context *ctx)
{
    if (!ctx) return 0;
    return ctx->gamepad_id;
}

bool aui_gamepad_button_down(AUI_Context *ctx, int button)
{
    if (!ctx || button < 0 || button >= AUI_GAMEPAD_BUTTON_COUNT) return false;
    return ctx->input.gamepad_button_down[button];
}

bool aui_gamepad_button_pressed(AUI_Context *ctx, int button)
{
    if (!ctx || button < 0 || button >= AUI_GAMEPAD_BUTTON_COUNT) return false;
    return ctx->input.gamepad_button_pressed[button];
}

bool aui_gamepad_button_released(AUI_Context *ctx, int button)
{
    if (!ctx || button < 0 || button >= AUI_GAMEPAD_BUTTON_COUNT) return false;
    return ctx->input.gamepad_button_released[button];
}

float aui_gamepad_axis(AUI_Context *ctx, int axis)
{
    if (!ctx) return 0.0f;

    switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFTX:  return ctx->input.gamepad_axis_left_x;
    case SDL_GAMEPAD_AXIS_LEFTY:  return ctx->input.gamepad_axis_left_y;
    case SDL_GAMEPAD_AXIS_RIGHTX: return ctx->input.gamepad_axis_right_x;
    case SDL_GAMEPAD_AXIS_RIGHTY: return ctx->input.gamepad_axis_right_y;
    default: return 0.0f;
    }
}

/* ============================================================================
 * Keyboard Shortcuts
 * ============================================================================ */

int aui_shortcut_register(AUI_Context *ctx, SDL_Keycode key, uint8_t modifiers,
                          const char *name, AUI_ShortcutCallback callback,
                          void *userdata)
{
    if (!ctx || !callback) return -1;

    /* Find an empty slot */
    int slot = -1;
    for (int i = 0; i < AUI_MAX_SHORTCUTS; i++) {
        if (!ctx->shortcuts[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) return -1;  /* Table full */

    ctx->shortcuts[slot].key = key;
    ctx->shortcuts[slot].modifiers = modifiers;
    ctx->shortcuts[slot].callback = callback;
    ctx->shortcuts[slot].userdata = userdata;
    ctx->shortcuts[slot].active = true;

    if (name) {
        strncpy(ctx->shortcuts[slot].name, name, sizeof(ctx->shortcuts[slot].name) - 1);
        ctx->shortcuts[slot].name[sizeof(ctx->shortcuts[slot].name) - 1] = '\0';
    } else {
        ctx->shortcuts[slot].name[0] = '\0';
    }

    if (slot >= ctx->shortcut_count) {
        ctx->shortcut_count = slot + 1;
    }

    return slot;
}

void aui_shortcut_unregister(AUI_Context *ctx, int id)
{
    if (!ctx || id < 0 || id >= AUI_MAX_SHORTCUTS) return;
    ctx->shortcuts[id].active = false;
}

void aui_shortcuts_clear(AUI_Context *ctx)
{
    if (!ctx) return;
    for (int i = 0; i < AUI_MAX_SHORTCUTS; i++) {
        ctx->shortcuts[i].active = false;
    }
    ctx->shortcut_count = 0;
}

bool aui_shortcuts_process(AUI_Context *ctx)
{
    if (!ctx) return false;

    /* Check each registered shortcut */
    for (int i = 0; i < ctx->shortcut_count; i++) {
        if (!ctx->shortcuts[i].active) continue;

        SDL_Keycode key = ctx->shortcuts[i].key;
        uint8_t mods = ctx->shortcuts[i].modifiers;

        /* Convert SDL keycode to scancode for lookup */
        SDL_Scancode scancode = SDL_GetScancodeFromKey(key, NULL);
        if (scancode == SDL_SCANCODE_UNKNOWN) continue;

        /* Check if this key was pressed this frame */
        if (!ctx->input.keys_pressed[scancode]) continue;

        /* Check modifier state matches */
        bool ctrl_match = ((mods & AUI_MOD_CTRL) != 0) == ctx->input.ctrl;
        bool shift_match = ((mods & AUI_MOD_SHIFT) != 0) == ctx->input.shift;
        bool alt_match = ((mods & AUI_MOD_ALT) != 0) == ctx->input.alt;

        if (ctrl_match && shift_match && alt_match) {
            /* Trigger the callback */
            ctx->shortcuts[i].callback(ctx, ctx->shortcuts[i].userdata);
            return true;
        }
    }

    return false;
}

const char *aui_shortcut_get_display(AUI_Context *ctx, int id, char *buffer,
                                      int buffer_size)
{
    if (!ctx || !buffer || buffer_size < 8 || id < 0 || id >= AUI_MAX_SHORTCUTS) {
        return NULL;
    }

    if (!ctx->shortcuts[id].active) return NULL;

    buffer[0] = '\0';
    int pos = 0;

    uint8_t mods = ctx->shortcuts[id].modifiers;

    /* Add modifier keys */
    if (mods & AUI_MOD_CTRL) {
        pos += snprintf(buffer + pos, buffer_size - pos, "Ctrl+");
    }
    if (mods & AUI_MOD_ALT) {
        pos += snprintf(buffer + pos, buffer_size - pos, "Alt+");
    }
    if (mods & AUI_MOD_SHIFT) {
        pos += snprintf(buffer + pos, buffer_size - pos, "Shift+");
    }

    /* Add key name */
    const char *key_name = SDL_GetKeyName(ctx->shortcuts[id].key);
    if (key_name && key_name[0]) {
        snprintf(buffer + pos, buffer_size - pos, "%s", key_name);
    } else {
        snprintf(buffer + pos, buffer_size - pos, "?");
    }

    return buffer;
}
