/*
 * Agentite UI - Widget Implementations
 */

#include "agentite/ui.h"
#include <string.h>
#include <stdio.h>

/* Forward declarations */
extern AUI_Id aui_make_id(AUI_Context *ctx, const char *str);
extern AUI_Id aui_make_id_int(AUI_Context *ctx, const char *str, int n);
extern AUI_Rect aui_allocate_rect(AUI_Context *ctx, float width, float height);

/* ============================================================================
 * Widget Helpers
 * ============================================================================ */

/* Check if mouse is over a widget and handle hot/active state */
static bool aui_widget_behavior(AUI_Context *ctx, AUI_Id id, AUI_Rect rect,
                                bool *out_hovered, bool *out_held)
{
    bool hovered = aui_rect_contains(rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool pressed = false;

    /* Track this widget as the last processed (for tooltip association) */
    ctx->last_widget_id = id;

    if (hovered) {
        ctx->hot = id;
    }

    if (ctx->active == id) {
        if (ctx->input.mouse_released[0]) {
            pressed = hovered;  /* Click = released while hovered */
            ctx->active = AUI_ID_NONE;
        }
    } else if (hovered && ctx->input.mouse_pressed[0]) {
        ctx->active = id;
    }

    if (out_hovered) *out_hovered = hovered;
    if (out_held) *out_held = (ctx->active == id);

    return pressed;
}

/* Get widget background color based on state */
static uint32_t aui_widget_bg_color(AUI_Context *ctx, bool hovered, bool held, bool disabled)
{
    if (disabled) return ctx->theme.bg_widget_disabled;
    if (held) return ctx->theme.bg_widget_active;
    if (hovered) return ctx->theme.bg_widget_hover;
    return ctx->theme.bg_widget;
}

/* ============================================================================
 * Labels
 * ============================================================================ */

void aui_label(AUI_Context *ctx, const char *text)
{
    aui_label_colored(ctx, text, ctx->theme.text);
}

void aui_label_colored(AUI_Context *ctx, const char *text, uint32_t color)
{
    if (!ctx || !text) return;

    float text_w = aui_text_width(ctx, text);
    float text_h = aui_text_height(ctx);

    AUI_Rect rect = aui_allocate_rect(ctx, text_w, text_h);

    /* Generate an ID and track for tooltip support */
    AUI_Id id = aui_make_id(ctx, text);
    ctx->last_widget_id = id;
    if (aui_rect_contains(rect, ctx->input.mouse_x, ctx->input.mouse_y)) {
        ctx->hot = id;
    }

    /* Center text vertically */
    float y = rect.y + (rect.h - text_h) * 0.5f;

    aui_draw_text(ctx, text, rect.x, y, color);
}

/* ============================================================================
 * Buttons
 * ============================================================================ */

bool aui_button(AUI_Context *ctx, const char *label)
{
    return aui_button_ex(ctx, label, 0, 0);
}

bool aui_button_ex(AUI_Context *ctx, const char *label, float width, float height)
{
    if (!ctx || !label) return false;

    AUI_Id id = aui_make_id(ctx, label);

    /* Calculate button size */
    float text_w = aui_text_width(ctx, label);
    float text_h = aui_text_height(ctx);
    float btn_w = width > 0 ? width : text_w + ctx->theme.padding * 2;
    float btn_h = height > 0 ? height : ctx->theme.widget_height;

    AUI_Rect rect = aui_allocate_rect(ctx, btn_w, btn_h);

    /* Register for Tab navigation */
    aui_focus_register(ctx, id);

    /* Handle interaction */
    bool hovered, held;
    bool pressed = aui_widget_behavior(ctx, id, rect, &hovered, &held);

    /* Handle keyboard activation when focused */
    bool focused = (ctx->focused == id);
    if (focused && (ctx->input.keys_pressed[SDL_SCANCODE_RETURN] ||
                    ctx->input.keys_pressed[SDL_SCANCODE_SPACE])) {
        pressed = true;
    }

    /* Focus on click */
    if (pressed) {
        ctx->focused = id;
    }

    /* Draw button background */
    uint32_t bg = aui_widget_bg_color(ctx, hovered, held, false);
    aui_draw_rect_rounded(ctx, rect.x, rect.y, rect.w, rect.h, bg, ctx->theme.corner_radius);

    /* Draw focus ring */
    if (focused) {
        aui_draw_rect_outline(ctx, rect.x - 2, rect.y - 2,
                              rect.w + 4, rect.h + 4,
                              ctx->theme.accent, 2.0f);
    }

    /* Draw text centered */
    float text_x = rect.x + (rect.w - text_w) * 0.5f;
    float text_y = rect.y + (rect.h - text_h) * 0.5f;
    aui_draw_text(ctx, label, text_x, text_y, ctx->theme.text);

    return pressed;
}

/* Semantic button variants using theme semantic colors */
static bool aui_button_semantic(AUI_Context *ctx, const char *label,
                                 uint32_t color, uint32_t color_hover,
                                 float width, float height)
{
    if (!ctx || !label) return false;

    AUI_Id id = aui_make_id(ctx, label);

    float text_w = aui_text_width(ctx, label);
    float text_h = aui_text_height(ctx);
    float btn_w = width > 0 ? width : text_w + ctx->theme.padding * 2;
    float btn_h = height > 0 ? height : ctx->theme.widget_height;

    AUI_Rect rect = aui_allocate_rect(ctx, btn_w, btn_h);

    /* Register for Tab navigation */
    aui_focus_register(ctx, id);

    bool hovered, held;
    bool pressed = aui_widget_behavior(ctx, id, rect, &hovered, &held);

    /* Handle keyboard activation when focused */
    bool focused = (ctx->focused == id);
    if (focused && (ctx->input.keys_pressed[SDL_SCANCODE_RETURN] ||
                    ctx->input.keys_pressed[SDL_SCANCODE_SPACE])) {
        pressed = true;
    }

    /* Focus on click */
    if (pressed) {
        ctx->focused = id;
    }

    /* Use semantic color based on state */
    uint32_t bg = held ? aui_color_darken(color, 0.15f) :
                  (hovered ? color_hover : color);
    aui_draw_rect_rounded(ctx, rect.x, rect.y, rect.w, rect.h, bg, ctx->theme.corner_radius);

    /* Draw focus ring */
    if (focused) {
        aui_draw_rect_outline(ctx, rect.x - 2, rect.y - 2,
                              rect.w + 4, rect.h + 4,
                              ctx->theme.text_highlight, 2.0f);
    }

    /* Use highlight text for contrast on colored backgrounds */
    float text_x = rect.x + (rect.w - text_w) * 0.5f;
    float text_y = rect.y + (rect.h - text_h) * 0.5f;
    aui_draw_text(ctx, label, text_x, text_y, ctx->theme.text_highlight);

    return pressed;
}

bool aui_button_primary(AUI_Context *ctx, const char *label)
{
    return aui_button_semantic(ctx, label,
                                ctx->theme.accent, ctx->theme.accent_hover, 0, 0);
}

bool aui_button_success(AUI_Context *ctx, const char *label)
{
    return aui_button_semantic(ctx, label,
                                ctx->theme.success, ctx->theme.success_hover, 0, 0);
}

bool aui_button_warning(AUI_Context *ctx, const char *label)
{
    return aui_button_semantic(ctx, label,
                                ctx->theme.warning, ctx->theme.warning_hover, 0, 0);
}

bool aui_button_danger(AUI_Context *ctx, const char *label)
{
    return aui_button_semantic(ctx, label,
                                ctx->theme.danger, ctx->theme.danger_hover, 0, 0);
}

bool aui_button_info(AUI_Context *ctx, const char *label)
{
    return aui_button_semantic(ctx, label,
                                ctx->theme.info, ctx->theme.info_hover, 0, 0);
}

/* ============================================================================
 * Checkbox
 * ============================================================================ */

bool aui_checkbox(AUI_Context *ctx, const char *label, bool *value)
{
    if (!ctx || !label || !value) return false;

    AUI_Id id = aui_make_id(ctx, label);

    float box_size = ctx->theme.widget_height - 8;
    float text_w = aui_text_width(ctx, label);
    float total_w = box_size + ctx->theme.spacing + text_w;

    AUI_Rect rect = aui_allocate_rect(ctx, total_w, ctx->theme.widget_height);

    /* Checkbox box rect */
    AUI_Rect box_rect = {
        rect.x,
        rect.y + (rect.h - box_size) * 0.5f,
        box_size,
        box_size
    };

    /* Register for Tab navigation */
    aui_focus_register(ctx, id);

    /* Handle interaction */
    bool hovered, held;
    bool pressed = aui_widget_behavior(ctx, id, rect, &hovered, &held);

    /* Handle keyboard activation when focused */
    bool focused = (ctx->focused == id);
    if (focused && (ctx->input.keys_pressed[SDL_SCANCODE_RETURN] ||
                    ctx->input.keys_pressed[SDL_SCANCODE_SPACE])) {
        pressed = true;
    }

    if (pressed) {
        *value = !*value;
        ctx->focused = id;
    }

    /* Draw checkbox box */
    uint32_t bg = aui_widget_bg_color(ctx, hovered, held, false);
    aui_draw_rect_rounded(ctx, box_rect.x, box_rect.y, box_rect.w, box_rect.h,
                          bg, ctx->theme.corner_radius);

    /* Draw focus ring */
    if (focused) {
        aui_draw_rect_outline(ctx, box_rect.x - 2, box_rect.y - 2,
                              box_rect.w + 4, box_rect.h + 4,
                              ctx->theme.accent, 2.0f);
    }

    /* Draw checkmark if checked */
    if (*value) {
        float pad = box_size * 0.2f;
        aui_draw_rect(ctx, box_rect.x + pad, box_rect.y + pad,
                      box_rect.w - pad * 2, box_rect.h - pad * 2,
                      ctx->theme.accent);
    }

    /* Draw label */
    float text_x = box_rect.x + box_size + ctx->theme.spacing;
    float text_y = rect.y + (rect.h - aui_text_height(ctx)) * 0.5f;
    aui_draw_text(ctx, label, text_x, text_y, ctx->theme.text);

    return pressed;
}

bool aui_radio(AUI_Context *ctx, const char *label, int *value, int option)
{
    if (!ctx || !label || !value) return false;

    AUI_Id id = aui_make_id_int(ctx, label, option);

    float box_size = ctx->theme.widget_height - 8;
    float text_w = aui_text_width(ctx, label);
    float total_w = box_size + ctx->theme.spacing + text_w;

    AUI_Rect rect = aui_allocate_rect(ctx, total_w, ctx->theme.widget_height);

    AUI_Rect box_rect = {
        rect.x,
        rect.y + (rect.h - box_size) * 0.5f,
        box_size,
        box_size
    };

    /* Register for Tab navigation */
    aui_focus_register(ctx, id);

    bool hovered, held;
    bool pressed = aui_widget_behavior(ctx, id, rect, &hovered, &held);

    /* Handle keyboard activation when focused */
    bool focused = (ctx->focused == id);
    if (focused && (ctx->input.keys_pressed[SDL_SCANCODE_RETURN] ||
                    ctx->input.keys_pressed[SDL_SCANCODE_SPACE])) {
        pressed = true;
    }

    if (pressed) {
        *value = option;
        ctx->focused = id;
    }

    /* Draw radio circle (approximated with rect for now) */
    uint32_t bg = aui_widget_bg_color(ctx, hovered, held, false);
    aui_draw_rect_rounded(ctx, box_rect.x, box_rect.y, box_rect.w, box_rect.h,
                          bg, box_size * 0.5f);

    /* Draw focus ring */
    if (focused) {
        aui_draw_rect_outline(ctx, box_rect.x - 2, box_rect.y - 2,
                              box_rect.w + 4, box_rect.h + 4,
                              ctx->theme.accent, 2.0f);
    }

    /* Draw inner dot if selected */
    if (*value == option) {
        float pad = box_size * 0.3f;
        aui_draw_rect_rounded(ctx, box_rect.x + pad, box_rect.y + pad,
                              box_rect.w - pad * 2, box_rect.h - pad * 2,
                              ctx->theme.accent, (box_size - pad * 2) * 0.5f);
    }

    /* Draw label */
    float text_x = box_rect.x + box_size + ctx->theme.spacing;
    float text_y = rect.y + (rect.h - aui_text_height(ctx)) * 0.5f;
    aui_draw_text(ctx, label, text_x, text_y, ctx->theme.text);

    return pressed;
}

/* ============================================================================
 * Sliders
 * ============================================================================ */

bool aui_slider_float(AUI_Context *ctx, const char *label, float *value,
                      float min, float max)
{
    if (!ctx || !label || !value) return false;

    AUI_Id id = aui_make_id(ctx, label);

    /* Layout: label on left, slider on right */
    float label_w = aui_text_width(ctx, label);
    float slider_w = 150.0f;  /* Fixed slider width */
    float total_w = label_w + ctx->theme.spacing + slider_w;

    AUI_Rect rect = aui_allocate_rect(ctx, total_w, ctx->theme.widget_height);

    /* Slider track rect */
    float track_h = 6.0f;
    AUI_Rect track_rect = {
        rect.x + label_w + ctx->theme.spacing,
        rect.y + (rect.h - track_h) * 0.5f,
        slider_w,
        track_h
    };

    /* Calculate grab handle position for hit testing */
    float grab_size = 16.0f;
    float t_current = (*value - min) / (max - min);
    if (t_current < 0) t_current = 0;
    if (t_current > 1) t_current = 1;
    float grab_x = track_rect.x + track_rect.w * t_current - grab_size * 0.5f;
    float grab_y = rect.y + (rect.h - grab_size) * 0.5f;
    AUI_Rect grab_rect = { grab_x, grab_y, grab_size, grab_size };

    /* Handle interaction - check both track and grab handle */
    bool track_hovered = aui_rect_contains(track_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool grab_hovered = aui_rect_contains(grab_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool hovered = track_hovered || grab_hovered;
    bool changed = false;

    if (hovered) {
        ctx->hot = id;
    }

    if (hovered && ctx->input.mouse_pressed[0]) {
        ctx->active = id;
    }

    if (ctx->active == id) {
        if (ctx->input.mouse_down[0]) {
            /* Update value based on mouse position */
            float t = (ctx->input.mouse_x - track_rect.x) / track_rect.w;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            float new_value = min + t * (max - min);
            if (new_value != *value) {
                *value = new_value;
                changed = true;
            }
        } else {
            ctx->active = AUI_ID_NONE;
        }
    }

    /* Draw label */
    float text_y = rect.y + (rect.h - aui_text_height(ctx)) * 0.5f;
    aui_draw_text(ctx, label, rect.x, text_y, ctx->theme.text);

    /* Draw track */
    aui_draw_rect_rounded(ctx, track_rect.x, track_rect.y, track_rect.w, track_rect.h,
                          ctx->theme.slider_track, track_h * 0.5f);

    /* Draw filled portion */
    float filled_w = track_rect.w * t_current;
    if (filled_w > 0) {
        aui_draw_rect_rounded(ctx, track_rect.x, track_rect.y, filled_w, track_rect.h,
                              ctx->theme.accent, track_h * 0.5f);
    }

    /* Draw grab handle (using pre-calculated position) */
    uint32_t grab_color = (ctx->active == id || hovered) ?
        ctx->theme.bg_widget_hover : ctx->theme.slider_grab;
    aui_draw_rect_rounded(ctx, grab_rect.x, grab_rect.y, grab_rect.w, grab_rect.h,
                          grab_color, grab_size * 0.5f);

    return changed;
}

bool aui_slider_int(AUI_Context *ctx, const char *label, int *value,
                    int min, int max)
{
    if (!value) return false;

    float fval = (float)*value;
    bool changed = aui_slider_float(ctx, label, &fval, (float)min, (float)max);
    if (changed) {
        *value = (int)(fval + 0.5f);  /* Round to nearest */
    }
    return changed;
}

/* ============================================================================
 * Spinbox
 * ============================================================================ */

bool aui_spinbox_float(AUI_Context *ctx, const char *label, float *value,
                       float min, float max, float step)
{
    if (!ctx || !label || !value) return false;

    AUI_Id id = aui_make_id(ctx, label);
    AUI_Id dec_id = aui_make_id_int(ctx, label, -1);
    AUI_Id inc_id = aui_make_id_int(ctx, label, 1);

    float label_w = aui_text_width(ctx, label);
    float button_w = ctx->theme.widget_height;  /* Square buttons */
    float value_w = 80.0f;  /* Width for value display */
    float total_w = label_w + ctx->theme.spacing + button_w + value_w + button_w;

    AUI_Rect rect = aui_allocate_rect(ctx, total_w, ctx->theme.widget_height);

    /* Calculate sub-rects */
    float x = rect.x + label_w + ctx->theme.spacing;
    AUI_Rect dec_rect = { x, rect.y, button_w, rect.h };
    AUI_Rect val_rect = { x + button_w, rect.y, value_w, rect.h };
    AUI_Rect inc_rect = { x + button_w + value_w, rect.y, button_w, rect.h };

    /* Register for Tab navigation (value field is focusable) */
    aui_focus_register(ctx, id);
    bool focused = (ctx->focused == id);

    bool changed = false;

    /* Handle decrement button */
    bool dec_hovered = aui_rect_contains(dec_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool dec_held = false;
    if (dec_hovered) ctx->hot = dec_id;
    if (dec_hovered && ctx->input.mouse_pressed[0]) ctx->active = dec_id;
    if (ctx->active == dec_id) {
        dec_held = ctx->input.mouse_down[0];
        if (ctx->input.mouse_released[0] && dec_hovered) {
            *value -= step;
            if (*value < min) *value = min;
            changed = true;
            ctx->active = AUI_ID_NONE;
        } else if (!ctx->input.mouse_down[0]) {
            ctx->active = AUI_ID_NONE;
        }
    }

    /* Handle increment button */
    bool inc_hovered = aui_rect_contains(inc_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool inc_held = false;
    if (inc_hovered) ctx->hot = inc_id;
    if (inc_hovered && ctx->input.mouse_pressed[0]) ctx->active = inc_id;
    if (ctx->active == inc_id) {
        inc_held = ctx->input.mouse_down[0];
        if (ctx->input.mouse_released[0] && inc_hovered) {
            *value += step;
            if (*value > max) *value = max;
            changed = true;
            ctx->active = AUI_ID_NONE;
        } else if (!ctx->input.mouse_down[0]) {
            ctx->active = AUI_ID_NONE;
        }
    }

    /* Handle keyboard when focused */
    if (focused) {
        if (ctx->input.keys_pressed[SDL_SCANCODE_UP] ||
            ctx->input.keys_pressed[SDL_SCANCODE_RIGHT]) {
            *value += step;
            if (*value > max) *value = max;
            changed = true;
        }
        if (ctx->input.keys_pressed[SDL_SCANCODE_DOWN] ||
            ctx->input.keys_pressed[SDL_SCANCODE_LEFT]) {
            *value -= step;
            if (*value < min) *value = min;
            changed = true;
        }
    }

    /* Click on value area to focus */
    bool val_hovered = aui_rect_contains(val_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    if (val_hovered) ctx->hot = id;
    if (val_hovered && ctx->input.mouse_pressed[0]) {
        ctx->focused = id;
    }

    /* Draw label */
    float text_y = rect.y + (rect.h - aui_text_height(ctx)) * 0.5f;
    aui_draw_text(ctx, label, rect.x, text_y, ctx->theme.text);

    /* Draw decrement button */
    uint32_t dec_bg = aui_widget_bg_color(ctx, dec_hovered, dec_held, false);
    aui_draw_rect_rounded(ctx, dec_rect.x, dec_rect.y, dec_rect.w, dec_rect.h,
                          dec_bg, ctx->theme.corner_radius);
    float minus_x = dec_rect.x + (dec_rect.w - aui_text_width(ctx, "-")) * 0.5f;
    aui_draw_text(ctx, "-", minus_x, text_y, ctx->theme.text);

    /* Draw value area */
    uint32_t val_bg = focused ? ctx->theme.bg_widget_active : ctx->theme.bg_widget;
    aui_draw_rect(ctx, val_rect.x, val_rect.y, val_rect.w, val_rect.h, val_bg);

    /* Draw focus ring around value area */
    if (focused) {
        aui_draw_rect_outline(ctx, val_rect.x - 1, val_rect.y - 1,
                              val_rect.w + 2, val_rect.h + 2,
                              ctx->theme.accent, 2.0f);
    }

    /* Draw value text (centered) */
    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%.2f", *value);
    float val_text_w = aui_text_width(ctx, value_str);
    float val_text_x = val_rect.x + (val_rect.w - val_text_w) * 0.5f;
    aui_draw_text(ctx, value_str, val_text_x, text_y, ctx->theme.text);

    /* Draw increment button */
    uint32_t inc_bg = aui_widget_bg_color(ctx, inc_hovered, inc_held, false);
    aui_draw_rect_rounded(ctx, inc_rect.x, inc_rect.y, inc_rect.w, inc_rect.h,
                          inc_bg, ctx->theme.corner_radius);
    float plus_x = inc_rect.x + (inc_rect.w - aui_text_width(ctx, "+")) * 0.5f;
    aui_draw_text(ctx, "+", plus_x, text_y, ctx->theme.text);

    return changed;
}

bool aui_spinbox_int(AUI_Context *ctx, const char *label, int *value,
                     int min, int max, int step)
{
    if (!ctx || !label || !value) return false;

    AUI_Id id = aui_make_id(ctx, label);
    AUI_Id dec_id = aui_make_id_int(ctx, label, -1);
    AUI_Id inc_id = aui_make_id_int(ctx, label, 1);

    float label_w = aui_text_width(ctx, label);
    float button_w = ctx->theme.widget_height;  /* Square buttons */
    float value_w = 60.0f;  /* Width for value display */
    float total_w = label_w + ctx->theme.spacing + button_w + value_w + button_w;

    AUI_Rect rect = aui_allocate_rect(ctx, total_w, ctx->theme.widget_height);

    /* Calculate sub-rects */
    float x = rect.x + label_w + ctx->theme.spacing;
    AUI_Rect dec_rect = { x, rect.y, button_w, rect.h };
    AUI_Rect val_rect = { x + button_w, rect.y, value_w, rect.h };
    AUI_Rect inc_rect = { x + button_w + value_w, rect.y, button_w, rect.h };

    /* Register for Tab navigation */
    aui_focus_register(ctx, id);
    bool focused = (ctx->focused == id);

    bool changed = false;

    /* Handle decrement button */
    bool dec_hovered = aui_rect_contains(dec_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool dec_held = false;
    if (dec_hovered) ctx->hot = dec_id;
    if (dec_hovered && ctx->input.mouse_pressed[0]) ctx->active = dec_id;
    if (ctx->active == dec_id) {
        dec_held = ctx->input.mouse_down[0];
        if (ctx->input.mouse_released[0] && dec_hovered) {
            *value -= step;
            if (*value < min) *value = min;
            changed = true;
            ctx->active = AUI_ID_NONE;
        } else if (!ctx->input.mouse_down[0]) {
            ctx->active = AUI_ID_NONE;
        }
    }

    /* Handle increment button */
    bool inc_hovered = aui_rect_contains(inc_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool inc_held = false;
    if (inc_hovered) ctx->hot = inc_id;
    if (inc_hovered && ctx->input.mouse_pressed[0]) ctx->active = inc_id;
    if (ctx->active == inc_id) {
        inc_held = ctx->input.mouse_down[0];
        if (ctx->input.mouse_released[0] && inc_hovered) {
            *value += step;
            if (*value > max) *value = max;
            changed = true;
            ctx->active = AUI_ID_NONE;
        } else if (!ctx->input.mouse_down[0]) {
            ctx->active = AUI_ID_NONE;
        }
    }

    /* Handle keyboard when focused */
    if (focused) {
        if (ctx->input.keys_pressed[SDL_SCANCODE_UP] ||
            ctx->input.keys_pressed[SDL_SCANCODE_RIGHT]) {
            *value += step;
            if (*value > max) *value = max;
            changed = true;
        }
        if (ctx->input.keys_pressed[SDL_SCANCODE_DOWN] ||
            ctx->input.keys_pressed[SDL_SCANCODE_LEFT]) {
            *value -= step;
            if (*value < min) *value = min;
            changed = true;
        }
    }

    /* Click on value area to focus */
    bool val_hovered = aui_rect_contains(val_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    if (val_hovered) ctx->hot = id;
    if (val_hovered && ctx->input.mouse_pressed[0]) {
        ctx->focused = id;
    }

    /* Draw label */
    float text_y = rect.y + (rect.h - aui_text_height(ctx)) * 0.5f;
    aui_draw_text(ctx, label, rect.x, text_y, ctx->theme.text);

    /* Draw decrement button */
    uint32_t dec_bg = aui_widget_bg_color(ctx, dec_hovered, dec_held, false);
    aui_draw_rect_rounded(ctx, dec_rect.x, dec_rect.y, dec_rect.w, dec_rect.h,
                          dec_bg, ctx->theme.corner_radius);
    float minus_x = dec_rect.x + (dec_rect.w - aui_text_width(ctx, "-")) * 0.5f;
    aui_draw_text(ctx, "-", minus_x, text_y, ctx->theme.text);

    /* Draw value area */
    uint32_t val_bg = focused ? ctx->theme.bg_widget_active : ctx->theme.bg_widget;
    aui_draw_rect(ctx, val_rect.x, val_rect.y, val_rect.w, val_rect.h, val_bg);

    /* Draw focus ring around value area */
    if (focused) {
        aui_draw_rect_outline(ctx, val_rect.x - 1, val_rect.y - 1,
                              val_rect.w + 2, val_rect.h + 2,
                              ctx->theme.accent, 2.0f);
    }

    /* Draw value text (centered) */
    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%d", *value);
    float val_text_w = aui_text_width(ctx, value_str);
    float val_text_x = val_rect.x + (val_rect.w - val_text_w) * 0.5f;
    aui_draw_text(ctx, value_str, val_text_x, text_y, ctx->theme.text);

    /* Draw increment button */
    uint32_t inc_bg = aui_widget_bg_color(ctx, inc_hovered, inc_held, false);
    aui_draw_rect_rounded(ctx, inc_rect.x, inc_rect.y, inc_rect.w, inc_rect.h,
                          inc_bg, ctx->theme.corner_radius);
    float plus_x = inc_rect.x + (inc_rect.w - aui_text_width(ctx, "+")) * 0.5f;
    aui_draw_text(ctx, "+", plus_x, text_y, ctx->theme.text);

    return changed;
}

/* ============================================================================
 * Progress Bar
 * ============================================================================ */

void aui_progress_bar(AUI_Context *ctx, float value, float min, float max)
{
    if (!ctx) return;

    AUI_Rect rect = aui_allocate_rect(ctx, 0, ctx->theme.widget_height);

    /* Draw background */
    aui_draw_rect_rounded(ctx, rect.x, rect.y, rect.w, rect.h,
                          ctx->theme.slider_track, ctx->theme.corner_radius);

    /* Draw filled portion */
    float t = (value - min) / (max - min);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float filled_w = rect.w * t;
    if (filled_w > 0) {
        aui_draw_rect_rounded(ctx, rect.x, rect.y, filled_w, rect.h,
                              ctx->theme.progress_fill, ctx->theme.corner_radius);
    }
}

void aui_progress_bar_colored(AUI_Context *ctx, float value, float min, float max,
                               uint32_t fill_color)
{
    if (!ctx) return;

    AUI_Rect rect = aui_allocate_rect(ctx, 0, ctx->theme.widget_height);

    /* Draw background */
    aui_draw_rect_rounded(ctx, rect.x, rect.y, rect.w, rect.h,
                          ctx->theme.slider_track, ctx->theme.corner_radius);

    /* Draw filled portion with custom color */
    float t = (value - min) / (max - min);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float filled_w = rect.w * t;
    if (filled_w > 0) {
        aui_draw_rect_rounded(ctx, rect.x, rect.y, filled_w, rect.h,
                              fill_color, ctx->theme.corner_radius);
    }
}

/* ============================================================================
 * Text Input
 * ============================================================================ */

bool aui_textbox(AUI_Context *ctx, const char *label, char *buffer, int buffer_size)
{
    return aui_textbox_ex(ctx, label, buffer, buffer_size, 0);
}

/* Helper: Get character index from X position in textbox */
static int textbox_char_from_x(AUI_Context *ctx, const char *buffer, float text_x, float mouse_x)
{
    int len = (int)strlen(buffer);
    if (len == 0) return 0;

    for (int i = 0; i <= len; i++) {
        char temp[256];
        int copy_len = i < (int)sizeof(temp) - 1 ? i : (int)sizeof(temp) - 1;
        memcpy(temp, buffer, copy_len);
        temp[copy_len] = '\0';
        float char_x = text_x + aui_text_width(ctx, temp);
        if (mouse_x < char_x) {
            return i > 0 ? i - 1 : 0;
        }
    }
    return len;
}

/* Helper: Get X position from character index */
static float textbox_x_from_char(AUI_Context *ctx, const char *buffer, int pos, float text_x)
{
    if (pos <= 0) return text_x;
    int len = (int)strlen(buffer);
    if (pos > len) pos = len;

    char temp[256];
    int copy_len = pos < (int)sizeof(temp) - 1 ? pos : (int)sizeof(temp) - 1;
    memcpy(temp, buffer, copy_len);
    temp[copy_len] = '\0';
    return text_x + aui_text_width(ctx, temp);
}

/* Helper: Delete selection and return new cursor position */
static int textbox_delete_selection(char *buffer, int sel_start, int sel_end)
{
    if (sel_start > sel_end) {
        int tmp = sel_start;
        sel_start = sel_end;
        sel_end = tmp;
    }
    int len = (int)strlen(buffer);
    if (sel_start >= len) return len;
    if (sel_end > len) sel_end = len;

    memmove(buffer + sel_start, buffer + sel_end, len - sel_end + 1);
    return sel_start;
}

bool aui_textbox_ex(AUI_Context *ctx, const char *label, char *buffer,
                    int buffer_size, float width)
{
    if (!ctx || !label || !buffer) return false;

    AUI_Id id = aui_make_id(ctx, label);
    AUI_WidgetState *state = aui_get_state(ctx, id);

    float label_w = aui_text_width(ctx, label);
    float input_w = width > 0 ? width : 150.0f;
    float total_w = label_w + ctx->theme.spacing + input_w;

    AUI_Rect rect = aui_allocate_rect(ctx, total_w, ctx->theme.widget_height);

    /* Input field rect */
    AUI_Rect input_rect = {
        rect.x + label_w + ctx->theme.spacing,
        rect.y,
        input_w,
        rect.h
    };

    float text_x = input_rect.x + ctx->theme.padding;
    int text_len = (int)strlen(buffer);

    /* Initialize cursor state if needed */
    if (state) {
        if (state->cursor_pos < 0) state->cursor_pos = text_len;
        if (state->cursor_pos > text_len) state->cursor_pos = text_len;
        if (state->selection_start < -1) state->selection_start = -1;
        if (state->selection_end < 0) state->selection_end = state->cursor_pos;
    }

    /* Register for Tab navigation */
    aui_focus_register(ctx, id);

    /* Handle interaction */
    bool hovered = aui_rect_contains(input_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool changed = false;
    bool focused = (ctx->focused == id);

    /* Handle mouse click to position cursor / start selection */
    if (hovered && ctx->input.mouse_pressed[0] && state) {
        ctx->focused = id;
        focused = true;
        int click_pos = textbox_char_from_x(ctx, buffer, text_x, ctx->input.mouse_x);

        if (ctx->input.shift && state->selection_start >= 0) {
            /* Extend selection */
            state->selection_end = click_pos;
            state->cursor_pos = click_pos;
        } else {
            /* Start new selection or just position cursor */
            state->cursor_pos = click_pos;
            state->selection_start = click_pos;
            state->selection_end = click_pos;
        }
    }

    /* Handle text input when focused */
    if (focused && state) {
        bool has_selection = (state->selection_start >= 0 &&
                              state->selection_start != state->selection_end);

        /* Handle text input */
        if (ctx->input.text_input_len > 0) {
            /* Delete selection first if any */
            if (has_selection) {
                state->cursor_pos = textbox_delete_selection(buffer, state->selection_start, state->selection_end);
                text_len = (int)strlen(buffer);
                has_selection = false;
            }

            /* Insert text at cursor */
            int to_insert = ctx->input.text_input_len;
            int space = buffer_size - text_len - 1;
            if (to_insert > space) to_insert = space;
            if (to_insert > 0) {
                /* Make room */
                memmove(buffer + state->cursor_pos + to_insert,
                        buffer + state->cursor_pos,
                        text_len - state->cursor_pos + 1);
                memcpy(buffer + state->cursor_pos, ctx->input.text_input, to_insert);
                state->cursor_pos += to_insert;
                changed = true;
            }
            state->selection_start = -1;
        }

        /* Handle backspace */
        if (ctx->input.keys_pressed[SDL_SCANCODE_BACKSPACE]) {
            if (has_selection) {
                state->cursor_pos = textbox_delete_selection(buffer, state->selection_start, state->selection_end);
                changed = true;
            } else if (state->cursor_pos > 0) {
                memmove(buffer + state->cursor_pos - 1,
                        buffer + state->cursor_pos,
                        strlen(buffer + state->cursor_pos) + 1);
                state->cursor_pos--;
                changed = true;
            }
            state->selection_start = -1;
        }

        /* Handle delete */
        if (ctx->input.keys_pressed[SDL_SCANCODE_DELETE]) {
            text_len = (int)strlen(buffer);
            if (has_selection) {
                state->cursor_pos = textbox_delete_selection(buffer, state->selection_start, state->selection_end);
                changed = true;
            } else if (state->cursor_pos < text_len) {
                memmove(buffer + state->cursor_pos,
                        buffer + state->cursor_pos + 1,
                        strlen(buffer + state->cursor_pos));
                changed = true;
            }
            state->selection_start = -1;
        }

        /* Handle arrow keys */
        if (ctx->input.keys_pressed[SDL_SCANCODE_LEFT]) {
            if (ctx->input.shift) {
                if (state->selection_start < 0) {
                    state->selection_start = state->cursor_pos;
                    state->selection_end = state->cursor_pos;
                }
                if (state->cursor_pos > 0) state->cursor_pos--;
                state->selection_end = state->cursor_pos;
            } else {
                if (has_selection) {
                    state->cursor_pos = state->selection_start < state->selection_end ?
                                        state->selection_start : state->selection_end;
                } else if (state->cursor_pos > 0) {
                    state->cursor_pos--;
                }
                state->selection_start = -1;
            }
        }

        if (ctx->input.keys_pressed[SDL_SCANCODE_RIGHT]) {
            text_len = (int)strlen(buffer);
            if (ctx->input.shift) {
                if (state->selection_start < 0) {
                    state->selection_start = state->cursor_pos;
                    state->selection_end = state->cursor_pos;
                }
                if (state->cursor_pos < text_len) state->cursor_pos++;
                state->selection_end = state->cursor_pos;
            } else {
                if (has_selection) {
                    state->cursor_pos = state->selection_start > state->selection_end ?
                                        state->selection_start : state->selection_end;
                } else if (state->cursor_pos < text_len) {
                    state->cursor_pos++;
                }
                state->selection_start = -1;
            }
        }

        /* Handle Home/End */
        if (ctx->input.keys_pressed[SDL_SCANCODE_HOME]) {
            if (ctx->input.shift) {
                if (state->selection_start < 0) state->selection_start = state->cursor_pos;
                state->cursor_pos = 0;
                state->selection_end = 0;
            } else {
                state->cursor_pos = 0;
                state->selection_start = -1;
            }
        }
        if (ctx->input.keys_pressed[SDL_SCANCODE_END]) {
            text_len = (int)strlen(buffer);
            if (ctx->input.shift) {
                if (state->selection_start < 0) state->selection_start = state->cursor_pos;
                state->cursor_pos = text_len;
                state->selection_end = text_len;
            } else {
                state->cursor_pos = text_len;
                state->selection_start = -1;
            }
        }

        /* Handle Ctrl+A (select all) */
        if (ctx->input.ctrl && ctx->input.keys_pressed[SDL_SCANCODE_A]) {
            text_len = (int)strlen(buffer);
            state->selection_start = 0;
            state->selection_end = text_len;
            state->cursor_pos = text_len;
        }

        /* Handle Ctrl+C (copy) */
        if (ctx->input.ctrl && ctx->input.keys_pressed[SDL_SCANCODE_C] && has_selection) {
            int sel_min = state->selection_start < state->selection_end ?
                          state->selection_start : state->selection_end;
            int sel_max = state->selection_start > state->selection_end ?
                          state->selection_start : state->selection_end;
            char clipboard[256];
            int copy_len = sel_max - sel_min;
            if (copy_len > (int)sizeof(clipboard) - 1) copy_len = (int)sizeof(clipboard) - 1;
            memcpy(clipboard, buffer + sel_min, copy_len);
            clipboard[copy_len] = '\0';
            SDL_SetClipboardText(clipboard);
        }

        /* Handle Ctrl+X (cut) */
        if (ctx->input.ctrl && ctx->input.keys_pressed[SDL_SCANCODE_X] && has_selection) {
            int sel_min = state->selection_start < state->selection_end ?
                          state->selection_start : state->selection_end;
            int sel_max = state->selection_start > state->selection_end ?
                          state->selection_start : state->selection_end;
            char clipboard[256];
            int copy_len = sel_max - sel_min;
            if (copy_len > (int)sizeof(clipboard) - 1) copy_len = (int)sizeof(clipboard) - 1;
            memcpy(clipboard, buffer + sel_min, copy_len);
            clipboard[copy_len] = '\0';
            SDL_SetClipboardText(clipboard);

            state->cursor_pos = textbox_delete_selection(buffer, state->selection_start, state->selection_end);
            state->selection_start = -1;
            changed = true;
        }

        /* Handle Ctrl+V (paste) */
        if (ctx->input.ctrl && ctx->input.keys_pressed[SDL_SCANCODE_V]) {
            char *clipboard = SDL_GetClipboardText();
            if (clipboard && clipboard[0]) {
                /* Delete selection first if any */
                if (has_selection) {
                    state->cursor_pos = textbox_delete_selection(buffer, state->selection_start, state->selection_end);
                    state->selection_start = -1;
                }

                text_len = (int)strlen(buffer);
                int paste_len = (int)strlen(clipboard);
                int space = buffer_size - text_len - 1;
                if (paste_len > space) paste_len = space;
                if (paste_len > 0) {
                    memmove(buffer + state->cursor_pos + paste_len,
                            buffer + state->cursor_pos,
                            text_len - state->cursor_pos + 1);
                    memcpy(buffer + state->cursor_pos, clipboard, paste_len);
                    state->cursor_pos += paste_len;
                    changed = true;
                }
            }
            SDL_free(clipboard);
        }

        /* Handle escape/enter to unfocus */
        if (ctx->input.keys_pressed[SDL_SCANCODE_ESCAPE] ||
            ctx->input.keys_pressed[SDL_SCANCODE_RETURN]) {
            ctx->focused = AUI_ID_NONE;
            focused = false;
        }
    }

    /* Draw label */
    float text_y = rect.y + (rect.h - aui_text_height(ctx)) * 0.5f;
    aui_draw_text(ctx, label, rect.x, text_y, ctx->theme.text);

    /* Draw input background */
    uint32_t bg = focused ? ctx->theme.bg_widget_active :
                  (hovered ? ctx->theme.bg_widget_hover : ctx->theme.bg_widget);
    aui_draw_rect_rounded(ctx, input_rect.x, input_rect.y, input_rect.w, input_rect.h,
                          bg, ctx->theme.corner_radius);

    /* Draw border when focused */
    if (focused) {
        aui_draw_rect_outline(ctx, input_rect.x, input_rect.y, input_rect.w, input_rect.h,
                              ctx->theme.accent, 2.0f);
    }

    /* Push scissor for text area */
    aui_push_scissor(ctx, input_rect.x + ctx->theme.padding, input_rect.y,
                     input_rect.w - ctx->theme.padding * 2, input_rect.h);

    /* Draw selection highlight */
    if (focused && state && state->selection_start >= 0 &&
        state->selection_start != state->selection_end) {
        int sel_min = state->selection_start < state->selection_end ?
                      state->selection_start : state->selection_end;
        int sel_max = state->selection_start > state->selection_end ?
                      state->selection_start : state->selection_end;

        float sel_x1 = textbox_x_from_char(ctx, buffer, sel_min, text_x);
        float sel_x2 = textbox_x_from_char(ctx, buffer, sel_max, text_x);

        uint32_t sel_color = (ctx->theme.accent & 0x00FFFFFF) | 0x60000000;  /* Semi-transparent accent */
        aui_draw_rect(ctx, sel_x1, input_rect.y + 2, sel_x2 - sel_x1, input_rect.h - 4, sel_color);
    }

    /* Draw text content */
    aui_draw_text(ctx, buffer, text_x, text_y, ctx->theme.text);

    aui_pop_scissor(ctx);

    /* Draw cursor when focused */
    if (focused && state) {
        float cursor_x = textbox_x_from_char(ctx, buffer, state->cursor_pos, text_x);
        /* Clamp cursor to visible area */
        float max_cursor_x = input_rect.x + input_rect.w - ctx->theme.padding;
        if (cursor_x > max_cursor_x) cursor_x = max_cursor_x;
        aui_draw_rect(ctx, cursor_x, input_rect.y + 4, 2, input_rect.h - 8,
                      ctx->theme.text);
    }

    return changed;
}

/* ============================================================================
 * Dropdown
 * ============================================================================ */

bool aui_dropdown(AUI_Context *ctx, const char *label, int *selected,
                  const char **items, int count)
{
    if (!ctx || !label || !selected || !items || count <= 0) return false;

    AUI_Id id = aui_make_id(ctx, label);

    float label_w = aui_text_width(ctx, label);
    float dropdown_w = 150.0f;
    float total_w = label_w + ctx->theme.spacing + dropdown_w;

    AUI_Rect rect = aui_allocate_rect(ctx, total_w, ctx->theme.widget_height);

    /* Dropdown button rect */
    AUI_Rect btn_rect = {
        rect.x + label_w + ctx->theme.spacing,
        rect.y,
        dropdown_w,
        rect.h
    };

    /* Register for Tab navigation */
    aui_focus_register(ctx, id);

    /* Handle interaction */
    bool hovered, held;
    bool pressed = aui_widget_behavior(ctx, id, btn_rect, &hovered, &held);

    /* Handle keyboard activation when focused */
    bool focused = (ctx->focused == id);
    if (focused && (ctx->input.keys_pressed[SDL_SCANCODE_RETURN] ||
                    ctx->input.keys_pressed[SDL_SCANCODE_SPACE])) {
        pressed = true;
    }

    /* Check if this dropdown's popup changed selection last frame */
    bool changed = false;
    if (ctx->popup_changed && ctx->popup_selected == selected) {
        changed = true;
        ctx->popup_changed = false;
    }

    if (pressed) {
        ctx->focused = id;  /* Focus on activation */
        if (ctx->open_popup == id) {
            ctx->open_popup = AUI_ID_NONE;
            ctx->popup_items = NULL;
            ctx->popup_selected = NULL;
        } else {
            ctx->open_popup = id;
            ctx->popup_rect.x = btn_rect.x;
            ctx->popup_rect.y = btn_rect.y + btn_rect.h;
            ctx->popup_rect.w = btn_rect.w;
            ctx->popup_rect.h = count * ctx->theme.widget_height;
            /* Store popup data for deferred rendering */
            ctx->popup_selected = selected;
            ctx->popup_items = items;
            ctx->popup_count = count;
            ctx->popup_changed = false;
        }
    }

    /* Draw label */
    float text_y = rect.y + (rect.h - aui_text_height(ctx)) * 0.5f;
    aui_draw_text(ctx, label, rect.x, text_y, ctx->theme.text);

    /* Draw dropdown button */
    uint32_t bg = aui_widget_bg_color(ctx, hovered, held, false);
    aui_draw_rect_rounded(ctx, btn_rect.x, btn_rect.y, btn_rect.w, btn_rect.h,
                          bg, ctx->theme.corner_radius);

    /* Draw focus ring */
    if (focused) {
        aui_draw_rect_outline(ctx, btn_rect.x - 2, btn_rect.y - 2,
                              btn_rect.w + 4, btn_rect.h + 4,
                              ctx->theme.accent, 2.0f);
    }

    /* Draw selected item text */
    const char *selected_text = (*selected >= 0 && *selected < count) ?
                                items[*selected] : "";
    float item_x = btn_rect.x + ctx->theme.padding;
    aui_draw_text(ctx, selected_text, item_x, text_y, ctx->theme.text);

    /* Draw dropdown arrow */
    float arrow_x = btn_rect.x + btn_rect.w - 20;
    aui_draw_text(ctx, "v", arrow_x, text_y, ctx->theme.text_dim);

    /* Popup is now drawn in aui_end_frame() for proper z-ordering */

    return changed;
}

/* ============================================================================
 * Listbox
 * ============================================================================ */

bool aui_listbox(AUI_Context *ctx, const char *label, int *selected,
                 const char **items, int count, float height)
{
    if (!ctx || !label || !selected || !items) return false;

    AUI_Id id = aui_make_id(ctx, label);
    AUI_Id scrollbar_id = aui_make_id_int(ctx, label, 0x5C801L);  /* Unique ID for scrollbar */

    /* Draw label */
    float label_h = aui_text_height(ctx) + ctx->theme.spacing;
    AUI_Rect label_rect = aui_allocate_rect(ctx, 0, label_h);
    aui_draw_text(ctx, label, label_rect.x, label_rect.y, ctx->theme.text);

    /* List area (full width) */
    float list_h = height > 0 ? height : 150.0f;
    AUI_Rect full_rect = aui_allocate_rect(ctx, 0, list_h);

    /* Calculate content height and whether scrollbar is needed */
    float content_h = count * ctx->theme.widget_height;
    float scrollbar_w = ctx->theme.scrollbar_width;
    bool needs_scrollbar = content_h > list_h;

    /* Content area (minus scrollbar if needed) */
    AUI_Rect list_rect = full_rect;
    if (needs_scrollbar) {
        list_rect.w -= scrollbar_w;
    }

    /* Draw background */
    aui_draw_rect(ctx, full_rect.x, full_rect.y, full_rect.w, full_rect.h,
                  ctx->theme.bg_widget);
    aui_draw_rect_outline(ctx, full_rect.x, full_rect.y, full_rect.w, full_rect.h,
                          ctx->theme.border, 1.0f);

    /* Get scroll state */
    AUI_WidgetState *state = aui_get_state(ctx, id);
    float scroll_y = state ? state->scroll_y : 0;
    float max_scroll = content_h - list_h;
    if (max_scroll < 0) max_scroll = 0;

    /* Clamp scroll */
    if (state) {
        if (state->scroll_y < 0) state->scroll_y = 0;
        if (state->scroll_y > max_scroll) state->scroll_y = max_scroll;
        scroll_y = state->scroll_y;
    }

    /* Handle scroll wheel */
    bool list_hovered = aui_rect_contains(full_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    if (list_hovered) {
        ctx->hot = id;
        if (state && ctx->input.scroll_y != 0.0f && needs_scrollbar) {
            float scroll_speed = ctx->theme.widget_height * 2.0f;
            state->scroll_y -= ctx->input.scroll_y * scroll_speed;
            if (state->scroll_y < 0) state->scroll_y = 0;
            if (state->scroll_y > max_scroll) state->scroll_y = max_scroll;
            scroll_y = state->scroll_y;
        }
    }

    /* Scrollbar handling */
    if (needs_scrollbar && state) {
        AUI_Rect scrollbar_rect = {
            full_rect.x + full_rect.w - scrollbar_w,
            full_rect.y,
            scrollbar_w,
            full_rect.h
        };

        /* Draw scrollbar track */
        aui_draw_rect(ctx, scrollbar_rect.x, scrollbar_rect.y,
                      scrollbar_rect.w, scrollbar_rect.h,
                      ctx->theme.scrollbar);

        /* Calculate thumb size and position */
        float visible_ratio = list_h / content_h;
        float thumb_h = scrollbar_rect.h * visible_ratio;
        if (thumb_h < 20.0f) thumb_h = 20.0f;  /* Minimum thumb size */

        float thumb_travel = scrollbar_rect.h - thumb_h;
        float scroll_ratio = (max_scroll > 0) ? (scroll_y / max_scroll) : 0;
        float thumb_y = scrollbar_rect.y + thumb_travel * scroll_ratio;

        AUI_Rect thumb_rect = {
            scrollbar_rect.x + 2,
            thumb_y,
            scrollbar_rect.w - 4,
            thumb_h
        };

        /* Handle scrollbar interaction */
        bool thumb_hovered = aui_rect_contains(thumb_rect, ctx->input.mouse_x, ctx->input.mouse_y);
        bool track_hovered = aui_rect_contains(scrollbar_rect, ctx->input.mouse_x, ctx->input.mouse_y);

        if (thumb_hovered || track_hovered) {
            ctx->hot = scrollbar_id;
        }

        /* Start dragging or clicking on scrollbar */
        if (track_hovered && ctx->input.mouse_pressed[0]) {
            ctx->active = scrollbar_id;
            /* Store the offset from thumb center for smooth dragging */
            if (thumb_hovered) {
                /* Clicked on thumb - store offset from thumb top */
                state->cursor_pos = (int)(ctx->input.mouse_y - thumb_y);
            } else {
                /* Clicked on track - center thumb on click position */
                state->cursor_pos = (int)(thumb_h * 0.5f);
            }
        }

        /* Handle active scrollbar (dragging) */
        if (ctx->active == scrollbar_id) {
            if (ctx->input.mouse_down[0]) {
                /* Calculate scroll based on absolute mouse position */
                float target_thumb_y = ctx->input.mouse_y - (float)state->cursor_pos;
                float new_ratio = (target_thumb_y - scrollbar_rect.y) / thumb_travel;
                if (new_ratio < 0) new_ratio = 0;
                if (new_ratio > 1) new_ratio = 1;
                state->scroll_y = new_ratio * max_scroll;
                scroll_y = state->scroll_y;
            } else {
                ctx->active = AUI_ID_NONE;
            }
        }

        /* Recalculate thumb position after potential scroll change */
        scroll_ratio = (max_scroll > 0) ? (scroll_y / max_scroll) : 0;
        thumb_y = scrollbar_rect.y + thumb_travel * scroll_ratio;
        thumb_rect.y = thumb_y;

        /* Draw thumb */
        bool thumb_active = (ctx->active == scrollbar_id);
        uint32_t thumb_color = thumb_active ? ctx->theme.accent :
                               (thumb_hovered ? ctx->theme.bg_widget_hover : ctx->theme.scrollbar_grab);
        aui_draw_rect_rounded(ctx, thumb_rect.x, thumb_rect.y, thumb_rect.w, thumb_rect.h,
                              thumb_color, ctx->theme.corner_radius);
    }

    /* Draw items (clipped to content area) */
    aui_push_scissor(ctx, list_rect.x, list_rect.y, list_rect.w, list_rect.h);

    bool changed = false;
    for (int i = 0; i < count; i++) {
        float item_y = list_rect.y + i * ctx->theme.widget_height - scroll_y;

        /* Skip items outside visible area */
        if (item_y + ctx->theme.widget_height < list_rect.y ||
            item_y > list_rect.y + list_rect.h) {
            continue;
        }

        AUI_Rect item_rect = {
            list_rect.x,
            item_y,
            list_rect.w,
            ctx->theme.widget_height
        };

        bool item_hovered = aui_rect_contains(item_rect,
                                               ctx->input.mouse_x,
                                               ctx->input.mouse_y);
        item_hovered = item_hovered && aui_rect_contains(list_rect,
                                                          ctx->input.mouse_x,
                                                          ctx->input.mouse_y);

        /* Draw selection/hover background */
        if (i == *selected) {
            aui_draw_rect(ctx, item_rect.x, item_rect.y, item_rect.w, item_rect.h,
                          ctx->theme.accent);
        } else if (item_hovered) {
            aui_draw_rect(ctx, item_rect.x, item_rect.y, item_rect.w, item_rect.h,
                          ctx->theme.bg_widget_hover);
        }

        /* Handle click */
        if (item_hovered && ctx->input.mouse_pressed[0]) {
            *selected = i;
            changed = true;
        }

        /* Draw item text */
        float text_y = item_rect.y + (item_rect.h - aui_text_height(ctx)) * 0.5f;
        aui_draw_text(ctx, items[i], item_rect.x + ctx->theme.padding,
                      text_y, ctx->theme.text);
    }

    aui_pop_scissor(ctx);

    return changed;
}

/* ============================================================================
 * Collapsing Header
 * ============================================================================ */

bool aui_collapsing_header(AUI_Context *ctx, const char *label)
{
    if (!ctx || !label) return false;

    AUI_Id id = aui_make_id(ctx, label);
    AUI_WidgetState *state = aui_get_state(ctx, id);

    AUI_Rect rect = aui_allocate_rect(ctx, 0, ctx->theme.widget_height);

    bool hovered, held;
    bool pressed = aui_widget_behavior(ctx, id, rect, &hovered, &held);

    if (pressed && state) {
        state->expanded = !state->expanded;
    }

    /* Draw background */
    uint32_t bg = aui_widget_bg_color(ctx, hovered, held, false);
    aui_draw_rect_rounded(ctx, rect.x, rect.y, rect.w, rect.h, bg, ctx->theme.corner_radius);

    /* Draw arrow */
    float arrow_x = rect.x + ctx->theme.padding;
    float text_y = rect.y + (rect.h - aui_text_height(ctx)) * 0.5f;
    const char *arrow = (state && state->expanded) ? "v" : ">";
    aui_draw_text(ctx, arrow, arrow_x, text_y, ctx->theme.text);

    /* Draw label */
    float label_x = arrow_x + 20;
    aui_draw_text(ctx, label, label_x, text_y, ctx->theme.text);

    return state && state->expanded;
}

/* ============================================================================
 * Panels
 * ============================================================================ */

bool aui_begin_panel(AUI_Context *ctx, const char *name,
                     float x, float y, float w, float h, uint32_t flags)
{
    if (!ctx || !name) return false;

    AUI_Id id = aui_make_id(ctx, name);
    (void)id;  /* Used for dragging in full implementation */

    AUI_Rect rect = {x, y, w, h};

    /* Handle dragging if movable */
    if (flags & AUI_PANEL_MOVABLE) {
        AUI_Rect title_rect = {x, y, w, ctx->theme.widget_height};
        bool in_title = aui_rect_contains(title_rect, ctx->input.mouse_x, ctx->input.mouse_y);

        if (in_title && ctx->input.mouse_pressed[0]) {
            ctx->active = id;
        }

        if (ctx->active == id && ctx->input.mouse_down[0]) {
            /* Note: Would need to store panel position in state for proper dragging */
        }

        if (ctx->active == id && ctx->input.mouse_released[0]) {
            ctx->active = AUI_ID_NONE;
        }
    }

    /* Draw panel background */
    aui_draw_rect_rounded(ctx, rect.x, rect.y, rect.w, rect.h,
                          ctx->theme.bg_panel, ctx->theme.corner_radius);

    /* Draw border */
    if (flags & AUI_PANEL_BORDER) {
        aui_draw_rect_outline(ctx, rect.x, rect.y, rect.w, rect.h,
                              ctx->theme.border, ctx->theme.border_width);
    }

    /* Draw title bar */
    float content_start_y = rect.y;
    if (flags & AUI_PANEL_TITLE_BAR) {
        float title_h = ctx->theme.widget_height;
        aui_draw_rect(ctx, rect.x, rect.y, rect.w, title_h,
                      ctx->theme.bg_widget);

        float text_y = rect.y + (title_h - aui_text_height(ctx)) * 0.5f;
        aui_draw_text(ctx, name, rect.x + ctx->theme.padding, text_y, ctx->theme.text);

        content_start_y = rect.y + title_h;
    }

    /* Push layout for panel content */
    AUI_LayoutFrame *frame = &ctx->layout_stack[ctx->layout_depth++];
    frame->bounds = rect;
    frame->bounds.y = content_start_y;
    frame->bounds.h = rect.h - (content_start_y - rect.y);
    frame->cursor_x = rect.x + ctx->theme.padding;
    frame->cursor_y = content_start_y + ctx->theme.padding;
    frame->spacing = ctx->theme.spacing;
    frame->padding = ctx->theme.padding;
    frame->horizontal = false;
    frame->has_clip = false;

    aui_push_id(ctx, name);

    return true;
}

void aui_end_panel(AUI_Context *ctx)
{
    if (!ctx) return;

    aui_pop_id(ctx);

    if (ctx->layout_depth > 1) {
        ctx->layout_depth--;
    }
}

/* ============================================================================
 * Tooltip
 * ============================================================================ */

void aui_tooltip(AUI_Context *ctx, const char *text)
{
    if (!ctx || !text) return;

    /* Only show if the last widget processed is currently hovered */
    if (ctx->hot != ctx->last_widget_id) return;
    if (ctx->last_widget_id == AUI_ID_NONE) return;

    /* Store tooltip for deferred rendering in aui_upload() */
    size_t len = strlen(text);
    if (len >= sizeof(ctx->pending_tooltip)) {
        len = sizeof(ctx->pending_tooltip) - 1;
    }
    memcpy(ctx->pending_tooltip, text, len);
    ctx->pending_tooltip[len] = '\0';
    ctx->pending_tooltip_active = true;
}

/* ============================================================================
 * Multi-Select
 * ============================================================================ */

#include <stdlib.h>

AUI_MultiSelectState aui_multi_select_create(int capacity)
{
    AUI_MultiSelectState state = {0};
    state.capacity = capacity;
    state.selected_indices = (int *)calloc(capacity, sizeof(int));
    state.anchor_index = -1;
    state.last_clicked = -1;
    return state;
}

void aui_multi_select_destroy(AUI_MultiSelectState *state)
{
    if (!state) return;
    free(state->selected_indices);
    state->selected_indices = NULL;
    state->capacity = 0;
    state->selected_count = 0;
}

void aui_multi_select_clear(AUI_MultiSelectState *state)
{
    if (!state) return;
    state->selected_count = 0;
    state->anchor_index = -1;
    state->last_clicked = -1;
}

bool aui_multi_select_is_selected(AUI_MultiSelectState *state, int index)
{
    if (!state) return false;
    for (int i = 0; i < state->selected_count; i++) {
        if (state->selected_indices[i] == index) return true;
    }
    return false;
}

/* Internal: add index to selection if not already selected */
static void aui_multi_select_add(AUI_MultiSelectState *state, int index)
{
    if (!state || state->selected_count >= state->capacity) return;
    if (aui_multi_select_is_selected(state, index)) return;
    state->selected_indices[state->selected_count++] = index;
}

/* Internal: remove index from selection */
static void aui_multi_select_remove(AUI_MultiSelectState *state, int index)
{
    if (!state) return;
    for (int i = 0; i < state->selected_count; i++) {
        if (state->selected_indices[i] == index) {
            /* Shift remaining elements */
            for (int j = i; j < state->selected_count - 1; j++) {
                state->selected_indices[j] = state->selected_indices[j + 1];
            }
            state->selected_count--;
            return;
        }
    }
}

/* Internal: toggle selection of index */
static void aui_multi_select_toggle(AUI_MultiSelectState *state, int index)
{
    if (aui_multi_select_is_selected(state, index)) {
        aui_multi_select_remove(state, index);
    } else {
        aui_multi_select_add(state, index);
    }
}

void aui_multi_select_begin(AUI_Context *ctx, AUI_MultiSelectState *state)
{
    if (!ctx || !state) return;
    ctx->multi_select = state;
}

bool aui_multi_select_item(AUI_Context *ctx, AUI_MultiSelectState *state,
                           int index, bool *is_selected)
{
    if (!ctx || !state) return false;

    bool changed = false;

    /* Check for click interaction */
    /* This should be called after the item's visual rect is determined */
    /* For now, we just handle the selection logic when clicked */

    /* Determine selection behavior based on modifiers */
    if (ctx->input.mouse_pressed[0]) {
        /* We need to be called from within an actual clickable item context */
        /* The calling code should check if this item was clicked */
        /* For simplicity, we assume the caller has determined a click occurred */

        if (ctx->input.ctrl) {
            /* Ctrl+Click: toggle single item */
            aui_multi_select_toggle(state, index);
            state->anchor_index = index;
            changed = true;
        } else if (ctx->input.shift && state->anchor_index >= 0) {
            /* Shift+Click: select range from anchor to clicked */
            int start = state->anchor_index < index ? state->anchor_index : index;
            int end = state->anchor_index > index ? state->anchor_index : index;

            /* Clear existing selection */
            state->selected_count = 0;

            /* Select range */
            for (int i = start; i <= end && state->selected_count < state->capacity; i++) {
                state->selected_indices[state->selected_count++] = i;
            }
            changed = true;
        } else {
            /* Regular click: select single, clear others */
            state->selected_count = 0;
            aui_multi_select_add(state, index);
            state->anchor_index = index;
            changed = true;
        }

        state->last_clicked = index;
    }

    if (is_selected) *is_selected = aui_multi_select_is_selected(state, index);
    return changed;
}

void aui_multi_select_end(AUI_Context *ctx)
{
    if (!ctx) return;
    ctx->multi_select = NULL;
}

/* ============================================================================
 * Tab Bar
 * ============================================================================ */

bool aui_begin_tab_bar(AUI_Context *ctx, const char *id)
{
    if (!ctx || !id) return false;

    AUI_Id bar_id = aui_make_id(ctx, id);

    /* Get persistent state for active tab */
    AUI_WidgetState *state = aui_get_state(ctx, bar_id);
    int active_tab = state ? state->cursor_pos : 0;  /* Reuse cursor_pos for active tab */

    /* Allocate space for tab bar + content */
    AUI_Rect available = aui_get_available_rect(ctx);
    float bar_height = ctx->theme.widget_height;

    /* Store tab bar state */
    ctx->tab_bar.id = bar_id;
    ctx->tab_bar.active_tab = active_tab;
    ctx->tab_bar.tab_count = 0;
    ctx->tab_bar.tab_x = available.x;
    ctx->tab_bar.bar_y = available.y;
    ctx->tab_bar.bar_height = bar_height;

    /* Draw tab bar background */
    aui_draw_rect(ctx, available.x, available.y, available.w, bar_height,
                  ctx->theme.bg_widget);

    /* Calculate content area (below tab bar) */
    ctx->tab_bar.content_rect.x = available.x;
    ctx->tab_bar.content_rect.y = available.y + bar_height;
    ctx->tab_bar.content_rect.w = available.w;
    ctx->tab_bar.content_rect.h = available.h - bar_height;

    /* Push ID for scoping */
    aui_push_id(ctx, id);

    return true;
}

bool aui_tab(AUI_Context *ctx, const char *label)
{
    if (!ctx || !label) return false;
    if (ctx->tab_bar.id == AUI_ID_NONE) return false;  /* Not in a tab bar */

    int tab_index = ctx->tab_bar.tab_count++;
    AUI_Id tab_id = aui_make_id_int(ctx, label, tab_index);

    /* Calculate tab button size */
    float text_w = aui_text_width(ctx, label);
    float tab_w = text_w + ctx->theme.padding * 2;
    float tab_h = ctx->tab_bar.bar_height;

    AUI_Rect tab_rect = {
        ctx->tab_bar.tab_x,
        ctx->tab_bar.bar_y,
        tab_w,
        tab_h
    };

    /* Advance for next tab */
    ctx->tab_bar.tab_x += tab_w + 2.0f;  /* Small gap between tabs */

    /* Register for Tab navigation */
    aui_focus_register(ctx, tab_id);

    /* Handle interaction */
    bool hovered = aui_rect_contains(tab_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool is_active = (ctx->tab_bar.active_tab == tab_index);
    bool focused = (ctx->focused == tab_id);

    if (hovered) ctx->hot = tab_id;

    bool clicked = false;
    if (hovered && ctx->input.mouse_pressed[0]) {
        clicked = true;
    }
    /* Keyboard activation */
    if (focused && (ctx->input.keys_pressed[SDL_SCANCODE_RETURN] ||
                    ctx->input.keys_pressed[SDL_SCANCODE_SPACE])) {
        clicked = true;
    }

    if (clicked) {
        ctx->tab_bar.active_tab = tab_index;
        ctx->focused = tab_id;
        /* Update persistent state */
        AUI_WidgetState *state = aui_get_state(ctx, ctx->tab_bar.id);
        if (state) state->cursor_pos = tab_index;
    }

    /* Draw tab button */
    uint32_t bg;
    if (is_active) {
        bg = ctx->theme.bg_panel;  /* Active tab blends with content */
    } else if (hovered) {
        bg = ctx->theme.bg_widget_hover;
    } else {
        bg = ctx->theme.bg_widget_active;  /* Darker for inactive tabs */
    }

    aui_draw_rect(ctx, tab_rect.x, tab_rect.y, tab_rect.w, tab_rect.h, bg);

    /* Draw focus ring */
    if (focused) {
        aui_draw_rect_outline(ctx, tab_rect.x, tab_rect.y,
                              tab_rect.w, tab_rect.h,
                              ctx->theme.accent, 2.0f);
    }

    /* Draw active tab indicator (bottom border) */
    if (is_active) {
        aui_draw_rect(ctx, tab_rect.x, tab_rect.y + tab_rect.h - 3,
                      tab_rect.w, 3, ctx->theme.accent);
    }

    /* Draw label */
    float text_x = tab_rect.x + (tab_rect.w - text_w) * 0.5f;
    float text_y = tab_rect.y + (tab_rect.h - aui_text_height(ctx)) * 0.5f;
    uint32_t text_color = is_active ? ctx->theme.text_highlight : ctx->theme.text;
    aui_draw_text(ctx, label, text_x, text_y, text_color);

    /* If this tab is active, set up layout for content */
    if (is_active) {
        /* Allocate the tab bar height in parent layout */
        aui_allocate_rect(ctx, 0, ctx->tab_bar.bar_height);

        /* Draw content background */
        aui_draw_rect(ctx, ctx->tab_bar.content_rect.x, ctx->tab_bar.content_rect.y,
                      ctx->tab_bar.content_rect.w, ctx->tab_bar.content_rect.h,
                      ctx->theme.bg_panel);
        aui_draw_rect_outline(ctx, ctx->tab_bar.content_rect.x, ctx->tab_bar.content_rect.y,
                              ctx->tab_bar.content_rect.w, ctx->tab_bar.content_rect.h,
                              ctx->theme.border, 1.0f);

        /* Push layout for content area */
        if (ctx->layout_depth < 32) {
            AUI_LayoutFrame *frame = &ctx->layout_stack[ctx->layout_depth++];
            frame->bounds = ctx->tab_bar.content_rect;
            frame->cursor_x = ctx->tab_bar.content_rect.x + ctx->theme.padding;
            frame->cursor_y = ctx->tab_bar.content_rect.y + ctx->theme.padding;
            frame->spacing = ctx->theme.spacing;
            frame->padding = ctx->theme.padding;
            frame->horizontal = false;
            frame->has_clip = false;
        }

        return true;  /* Render content for this tab */
    }

    return false;  /* Don't render content for inactive tabs */
}

void aui_end_tab_bar(AUI_Context *ctx)
{
    if (!ctx) return;

    /* Pop the content layout if a tab was active */
    if (ctx->layout_depth > 1) {
        ctx->layout_depth--;
    }

    aui_pop_id(ctx);

    /* Clear tab bar state */
    ctx->tab_bar.id = AUI_ID_NONE;
}
