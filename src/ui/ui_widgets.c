/*
 * Carbon UI - Widget Implementations
 */

#include "carbon/ui.h"
#include <string.h>
#include <stdio.h>

/* Forward declarations */
extern CUI_Id cui_make_id(CUI_Context *ctx, const char *str);
extern CUI_Id cui_make_id_int(CUI_Context *ctx, const char *str, int n);
extern CUI_Rect cui_allocate_rect(CUI_Context *ctx, float width, float height);

/* ============================================================================
 * Widget Helpers
 * ============================================================================ */

/* Check if mouse is over a widget and handle hot/active state */
static bool cui_widget_behavior(CUI_Context *ctx, CUI_Id id, CUI_Rect rect,
                                bool *out_hovered, bool *out_held)
{
    bool hovered = cui_rect_contains(rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool pressed = false;

    if (hovered) {
        ctx->hot = id;
    }

    if (ctx->active == id) {
        if (ctx->input.mouse_released[0]) {
            pressed = hovered;  /* Click = released while hovered */
            ctx->active = CUI_ID_NONE;
        }
    } else if (hovered && ctx->input.mouse_pressed[0]) {
        ctx->active = id;
    }

    if (out_hovered) *out_hovered = hovered;
    if (out_held) *out_held = (ctx->active == id);

    return pressed;
}

/* Get widget background color based on state */
static uint32_t cui_widget_bg_color(CUI_Context *ctx, bool hovered, bool held, bool disabled)
{
    if (disabled) return ctx->theme.bg_widget_disabled;
    if (held) return ctx->theme.bg_widget_active;
    if (hovered) return ctx->theme.bg_widget_hover;
    return ctx->theme.bg_widget;
}

/* ============================================================================
 * Labels
 * ============================================================================ */

void cui_label(CUI_Context *ctx, const char *text)
{
    cui_label_colored(ctx, text, ctx->theme.text);
}

void cui_label_colored(CUI_Context *ctx, const char *text, uint32_t color)
{
    if (!ctx || !text) return;

    float text_w = cui_text_width(ctx, text);
    float text_h = cui_text_height(ctx);

    CUI_Rect rect = cui_allocate_rect(ctx, text_w, text_h);

    /* Center text vertically */
    float y = rect.y + (rect.h - text_h) * 0.5f;

    cui_draw_text(ctx, text, rect.x, y, color);
}

/* ============================================================================
 * Buttons
 * ============================================================================ */

bool cui_button(CUI_Context *ctx, const char *label)
{
    return cui_button_ex(ctx, label, 0, 0);
}

bool cui_button_ex(CUI_Context *ctx, const char *label, float width, float height)
{
    if (!ctx || !label) return false;

    CUI_Id id = cui_make_id(ctx, label);

    /* Calculate button size */
    float text_w = cui_text_width(ctx, label);
    float text_h = cui_text_height(ctx);
    float btn_w = width > 0 ? width : text_w + ctx->theme.padding * 2;
    float btn_h = height > 0 ? height : ctx->theme.widget_height;

    CUI_Rect rect = cui_allocate_rect(ctx, btn_w, btn_h);

    /* Handle interaction */
    bool hovered, held;
    bool pressed = cui_widget_behavior(ctx, id, rect, &hovered, &held);

    /* Draw button background */
    uint32_t bg = cui_widget_bg_color(ctx, hovered, held, false);
    cui_draw_rect_rounded(ctx, rect.x, rect.y, rect.w, rect.h, bg, ctx->theme.corner_radius);

    /* Draw text centered */
    float text_x = rect.x + (rect.w - text_w) * 0.5f;
    float text_y = rect.y + (rect.h - text_h) * 0.5f;
    cui_draw_text(ctx, label, text_x, text_y, ctx->theme.text);

    return pressed;
}

/* ============================================================================
 * Checkbox
 * ============================================================================ */

bool cui_checkbox(CUI_Context *ctx, const char *label, bool *value)
{
    if (!ctx || !label || !value) return false;

    CUI_Id id = cui_make_id(ctx, label);

    float box_size = ctx->theme.widget_height - 8;
    float text_w = cui_text_width(ctx, label);
    float total_w = box_size + ctx->theme.spacing + text_w;

    CUI_Rect rect = cui_allocate_rect(ctx, total_w, ctx->theme.widget_height);

    /* Checkbox box rect */
    CUI_Rect box_rect = {
        rect.x,
        rect.y + (rect.h - box_size) * 0.5f,
        box_size,
        box_size
    };

    /* Handle interaction */
    bool hovered, held;
    bool pressed = cui_widget_behavior(ctx, id, rect, &hovered, &held);

    if (pressed) {
        *value = !*value;
    }

    /* Draw checkbox box */
    uint32_t bg = cui_widget_bg_color(ctx, hovered, held, false);
    cui_draw_rect_rounded(ctx, box_rect.x, box_rect.y, box_rect.w, box_rect.h,
                          bg, ctx->theme.corner_radius);

    /* Draw checkmark if checked */
    if (*value) {
        float pad = box_size * 0.2f;
        cui_draw_rect(ctx, box_rect.x + pad, box_rect.y + pad,
                      box_rect.w - pad * 2, box_rect.h - pad * 2,
                      ctx->theme.accent);
    }

    /* Draw label */
    float text_x = box_rect.x + box_size + ctx->theme.spacing;
    float text_y = rect.y + (rect.h - cui_text_height(ctx)) * 0.5f;
    cui_draw_text(ctx, label, text_x, text_y, ctx->theme.text);

    return pressed;
}

bool cui_radio(CUI_Context *ctx, const char *label, int *value, int option)
{
    if (!ctx || !label || !value) return false;

    CUI_Id id = cui_make_id_int(ctx, label, option);

    float box_size = ctx->theme.widget_height - 8;
    float text_w = cui_text_width(ctx, label);
    float total_w = box_size + ctx->theme.spacing + text_w;

    CUI_Rect rect = cui_allocate_rect(ctx, total_w, ctx->theme.widget_height);

    CUI_Rect box_rect = {
        rect.x,
        rect.y + (rect.h - box_size) * 0.5f,
        box_size,
        box_size
    };

    bool hovered, held;
    bool pressed = cui_widget_behavior(ctx, id, rect, &hovered, &held);

    if (pressed) {
        *value = option;
    }

    /* Draw radio circle (approximated with rect for now) */
    uint32_t bg = cui_widget_bg_color(ctx, hovered, held, false);
    cui_draw_rect_rounded(ctx, box_rect.x, box_rect.y, box_rect.w, box_rect.h,
                          bg, box_size * 0.5f);

    /* Draw inner dot if selected */
    if (*value == option) {
        float pad = box_size * 0.3f;
        cui_draw_rect_rounded(ctx, box_rect.x + pad, box_rect.y + pad,
                              box_rect.w - pad * 2, box_rect.h - pad * 2,
                              ctx->theme.accent, (box_size - pad * 2) * 0.5f);
    }

    /* Draw label */
    float text_x = box_rect.x + box_size + ctx->theme.spacing;
    float text_y = rect.y + (rect.h - cui_text_height(ctx)) * 0.5f;
    cui_draw_text(ctx, label, text_x, text_y, ctx->theme.text);

    return pressed;
}

/* ============================================================================
 * Sliders
 * ============================================================================ */

bool cui_slider_float(CUI_Context *ctx, const char *label, float *value,
                      float min, float max)
{
    if (!ctx || !label || !value) return false;

    CUI_Id id = cui_make_id(ctx, label);

    /* Layout: label on left, slider on right */
    float label_w = cui_text_width(ctx, label);
    float slider_w = 150.0f;  /* Fixed slider width */
    float total_w = label_w + ctx->theme.spacing + slider_w;

    CUI_Rect rect = cui_allocate_rect(ctx, total_w, ctx->theme.widget_height);

    /* Slider track rect */
    float track_h = 6.0f;
    CUI_Rect track_rect = {
        rect.x + label_w + ctx->theme.spacing,
        rect.y + (rect.h - track_h) * 0.5f,
        slider_w,
        track_h
    };

    /* Handle interaction */
    bool hovered = cui_rect_contains(track_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool changed = false;

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
            ctx->active = CUI_ID_NONE;
        }
    }

    /* Draw label */
    float text_y = rect.y + (rect.h - cui_text_height(ctx)) * 0.5f;
    cui_draw_text(ctx, label, rect.x, text_y, ctx->theme.text);

    /* Draw track */
    cui_draw_rect_rounded(ctx, track_rect.x, track_rect.y, track_rect.w, track_rect.h,
                          ctx->theme.slider_track, track_h * 0.5f);

    /* Draw filled portion */
    float t = (*value - min) / (max - min);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float filled_w = track_rect.w * t;
    if (filled_w > 0) {
        cui_draw_rect_rounded(ctx, track_rect.x, track_rect.y, filled_w, track_rect.h,
                              ctx->theme.accent, track_h * 0.5f);
    }

    /* Draw grab handle */
    float grab_size = 16.0f;
    float grab_x = track_rect.x + filled_w - grab_size * 0.5f;
    float grab_y = rect.y + (rect.h - grab_size) * 0.5f;
    uint32_t grab_color = (ctx->active == id || hovered) ?
        ctx->theme.bg_widget_hover : ctx->theme.slider_grab;
    cui_draw_rect_rounded(ctx, grab_x, grab_y, grab_size, grab_size,
                          grab_color, grab_size * 0.5f);

    return changed;
}

bool cui_slider_int(CUI_Context *ctx, const char *label, int *value,
                    int min, int max)
{
    if (!value) return false;

    float fval = (float)*value;
    bool changed = cui_slider_float(ctx, label, &fval, (float)min, (float)max);
    if (changed) {
        *value = (int)(fval + 0.5f);  /* Round to nearest */
    }
    return changed;
}

/* ============================================================================
 * Progress Bar
 * ============================================================================ */

void cui_progress_bar(CUI_Context *ctx, float value, float min, float max)
{
    if (!ctx) return;

    CUI_Rect rect = cui_allocate_rect(ctx, 0, ctx->theme.widget_height);

    /* Draw background */
    cui_draw_rect_rounded(ctx, rect.x, rect.y, rect.w, rect.h,
                          ctx->theme.slider_track, ctx->theme.corner_radius);

    /* Draw filled portion */
    float t = (value - min) / (max - min);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float filled_w = rect.w * t;
    if (filled_w > 0) {
        cui_draw_rect_rounded(ctx, rect.x, rect.y, filled_w, rect.h,
                              ctx->theme.accent, ctx->theme.corner_radius);
    }
}

/* ============================================================================
 * Text Input
 * ============================================================================ */

bool cui_textbox(CUI_Context *ctx, const char *label, char *buffer, int buffer_size)
{
    return cui_textbox_ex(ctx, label, buffer, buffer_size, 0);
}

bool cui_textbox_ex(CUI_Context *ctx, const char *label, char *buffer,
                    int buffer_size, float width)
{
    if (!ctx || !label || !buffer) return false;

    CUI_Id id = cui_make_id(ctx, label);

    float label_w = cui_text_width(ctx, label);
    float input_w = width > 0 ? width : 150.0f;
    float total_w = label_w + ctx->theme.spacing + input_w;

    CUI_Rect rect = cui_allocate_rect(ctx, total_w, ctx->theme.widget_height);

    /* Input field rect */
    CUI_Rect input_rect = {
        rect.x + label_w + ctx->theme.spacing,
        rect.y,
        input_w,
        rect.h
    };

    /* Available width for text (minus padding on both sides) */
    float available_w = input_w - ctx->theme.padding * 2;

    /* Handle interaction */
    bool hovered = cui_rect_contains(input_rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool changed = false;

    if (hovered && ctx->input.mouse_pressed[0]) {
        ctx->focused = id;
    }

    /* Handle text input when focused */
    if (ctx->focused == id) {
        /* Append text input - check both buffer size and visual width */
        if (ctx->input.text_input_len > 0) {
            int current_len = (int)strlen(buffer);
            int space = buffer_size - current_len - 1;
            if (space > 0) {
                /* Try adding text and check if it fits visually */
                char test_buffer[256];
                int to_copy = ctx->input.text_input_len;
                if (to_copy > space) to_copy = space;

                /* Build test string */
                int test_len = current_len + to_copy;
                if (test_len < (int)sizeof(test_buffer) - 1) {
                    memcpy(test_buffer, buffer, current_len);
                    memcpy(test_buffer + current_len, ctx->input.text_input, to_copy);
                    test_buffer[test_len] = '\0';

                    /* Check if text fits visually */
                    float new_text_w = cui_text_width(ctx, test_buffer);
                    if (new_text_w <= available_w) {
                        /* Text fits - apply the change */
                        memcpy(buffer, test_buffer, test_len + 1);
                        changed = true;
                    }
                    /* If text doesn't fit visually, don't add it */
                }
            }
        }

        /* Handle backspace */
        if (ctx->input.keys_pressed[SDL_SCANCODE_BACKSPACE]) {
            int len = (int)strlen(buffer);
            if (len > 0) {
                buffer[len - 1] = '\0';
                changed = true;
            }
        }

        /* Handle escape/enter to unfocus */
        if (ctx->input.keys_pressed[SDL_SCANCODE_ESCAPE] ||
            ctx->input.keys_pressed[SDL_SCANCODE_RETURN]) {
            ctx->focused = CUI_ID_NONE;
        }
    }

    /* Draw label */
    float text_y = rect.y + (rect.h - cui_text_height(ctx)) * 0.5f;
    cui_draw_text(ctx, label, rect.x, text_y, ctx->theme.text);

    /* Draw input background */
    bool focused = (ctx->focused == id);
    uint32_t bg = focused ? ctx->theme.bg_widget_active :
                  (hovered ? ctx->theme.bg_widget_hover : ctx->theme.bg_widget);
    cui_draw_rect_rounded(ctx, input_rect.x, input_rect.y, input_rect.w, input_rect.h,
                          bg, ctx->theme.corner_radius);

    /* Draw border when focused */
    if (focused) {
        cui_draw_rect_outline(ctx, input_rect.x, input_rect.y, input_rect.w, input_rect.h,
                              ctx->theme.accent, 2.0f);
    }

    /* Draw text content (clipped to input area) */
    float text_x = input_rect.x + ctx->theme.padding;
    cui_push_scissor(ctx, input_rect.x + ctx->theme.padding, input_rect.y,
                     input_rect.w - ctx->theme.padding * 2, input_rect.h);
    cui_draw_text(ctx, buffer, text_x, text_y, ctx->theme.text);
    cui_pop_scissor(ctx);

    /* Draw cursor when focused */
    if (focused) {
        float cursor_x = text_x + cui_text_width(ctx, buffer);
        /* Clamp cursor to visible area */
        float max_cursor_x = input_rect.x + input_rect.w - ctx->theme.padding;
        if (cursor_x > max_cursor_x) cursor_x = max_cursor_x;
        cui_draw_rect(ctx, cursor_x, input_rect.y + 4, 2, input_rect.h - 8,
                      ctx->theme.text);
    }

    return changed;
}

/* ============================================================================
 * Dropdown
 * ============================================================================ */

bool cui_dropdown(CUI_Context *ctx, const char *label, int *selected,
                  const char **items, int count)
{
    if (!ctx || !label || !selected || !items || count <= 0) return false;

    CUI_Id id = cui_make_id(ctx, label);

    float label_w = cui_text_width(ctx, label);
    float dropdown_w = 150.0f;
    float total_w = label_w + ctx->theme.spacing + dropdown_w;

    CUI_Rect rect = cui_allocate_rect(ctx, total_w, ctx->theme.widget_height);

    /* Dropdown button rect */
    CUI_Rect btn_rect = {
        rect.x + label_w + ctx->theme.spacing,
        rect.y,
        dropdown_w,
        rect.h
    };

    /* Handle interaction */
    bool hovered, held;
    bool pressed = cui_widget_behavior(ctx, id, btn_rect, &hovered, &held);

    /* Check if this dropdown's popup changed selection last frame */
    bool changed = false;
    if (ctx->popup_changed && ctx->popup_selected == selected) {
        changed = true;
        ctx->popup_changed = false;
    }

    if (pressed) {
        if (ctx->open_popup == id) {
            ctx->open_popup = CUI_ID_NONE;
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
    float text_y = rect.y + (rect.h - cui_text_height(ctx)) * 0.5f;
    cui_draw_text(ctx, label, rect.x, text_y, ctx->theme.text);

    /* Draw dropdown button */
    uint32_t bg = cui_widget_bg_color(ctx, hovered, held, false);
    cui_draw_rect_rounded(ctx, btn_rect.x, btn_rect.y, btn_rect.w, btn_rect.h,
                          bg, ctx->theme.corner_radius);

    /* Draw selected item text */
    const char *selected_text = (*selected >= 0 && *selected < count) ?
                                items[*selected] : "";
    float item_x = btn_rect.x + ctx->theme.padding;
    cui_draw_text(ctx, selected_text, item_x, text_y, ctx->theme.text);

    /* Draw dropdown arrow */
    float arrow_x = btn_rect.x + btn_rect.w - 20;
    cui_draw_text(ctx, "v", arrow_x, text_y, ctx->theme.text_dim);

    /* Popup is now drawn in cui_end_frame() for proper z-ordering */

    return changed;
}

/* ============================================================================
 * Listbox
 * ============================================================================ */

bool cui_listbox(CUI_Context *ctx, const char *label, int *selected,
                 const char **items, int count, float height)
{
    if (!ctx || !label || !selected || !items) return false;

    CUI_Id id = cui_make_id(ctx, label);
    CUI_Id scrollbar_id = cui_make_id_int(ctx, label, 0x5C801L);  /* Unique ID for scrollbar */

    /* Draw label */
    float label_h = cui_text_height(ctx) + ctx->theme.spacing;
    CUI_Rect label_rect = cui_allocate_rect(ctx, 0, label_h);
    cui_draw_text(ctx, label, label_rect.x, label_rect.y, ctx->theme.text);

    /* List area (full width) */
    float list_h = height > 0 ? height : 150.0f;
    CUI_Rect full_rect = cui_allocate_rect(ctx, 0, list_h);

    /* Calculate content height and whether scrollbar is needed */
    float content_h = count * ctx->theme.widget_height;
    float scrollbar_w = ctx->theme.scrollbar_width;
    bool needs_scrollbar = content_h > list_h;

    /* Content area (minus scrollbar if needed) */
    CUI_Rect list_rect = full_rect;
    if (needs_scrollbar) {
        list_rect.w -= scrollbar_w;
    }

    /* Draw background */
    cui_draw_rect(ctx, full_rect.x, full_rect.y, full_rect.w, full_rect.h,
                  ctx->theme.bg_widget);
    cui_draw_rect_outline(ctx, full_rect.x, full_rect.y, full_rect.w, full_rect.h,
                          ctx->theme.border, 1.0f);

    /* Get scroll state */
    CUI_WidgetState *state = cui_get_state(ctx, id);
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
    bool list_hovered = cui_rect_contains(full_rect, ctx->input.mouse_x, ctx->input.mouse_y);
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
        CUI_Rect scrollbar_rect = {
            full_rect.x + full_rect.w - scrollbar_w,
            full_rect.y,
            scrollbar_w,
            full_rect.h
        };

        /* Draw scrollbar track */
        cui_draw_rect(ctx, scrollbar_rect.x, scrollbar_rect.y,
                      scrollbar_rect.w, scrollbar_rect.h,
                      ctx->theme.scrollbar);

        /* Calculate thumb size and position */
        float visible_ratio = list_h / content_h;
        float thumb_h = scrollbar_rect.h * visible_ratio;
        if (thumb_h < 20.0f) thumb_h = 20.0f;  /* Minimum thumb size */

        float thumb_travel = scrollbar_rect.h - thumb_h;
        float scroll_ratio = (max_scroll > 0) ? (scroll_y / max_scroll) : 0;
        float thumb_y = scrollbar_rect.y + thumb_travel * scroll_ratio;

        CUI_Rect thumb_rect = {
            scrollbar_rect.x + 2,
            thumb_y,
            scrollbar_rect.w - 4,
            thumb_h
        };

        /* Handle scrollbar interaction */
        bool thumb_hovered = cui_rect_contains(thumb_rect, ctx->input.mouse_x, ctx->input.mouse_y);
        bool track_hovered = cui_rect_contains(scrollbar_rect, ctx->input.mouse_x, ctx->input.mouse_y);

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
                ctx->active = CUI_ID_NONE;
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
        cui_draw_rect_rounded(ctx, thumb_rect.x, thumb_rect.y, thumb_rect.w, thumb_rect.h,
                              thumb_color, ctx->theme.corner_radius);
    }

    /* Draw items (clipped to content area) */
    cui_push_scissor(ctx, list_rect.x, list_rect.y, list_rect.w, list_rect.h);

    bool changed = false;
    for (int i = 0; i < count; i++) {
        float item_y = list_rect.y + i * ctx->theme.widget_height - scroll_y;

        /* Skip items outside visible area */
        if (item_y + ctx->theme.widget_height < list_rect.y ||
            item_y > list_rect.y + list_rect.h) {
            continue;
        }

        CUI_Rect item_rect = {
            list_rect.x,
            item_y,
            list_rect.w,
            ctx->theme.widget_height
        };

        bool item_hovered = cui_rect_contains(item_rect,
                                               ctx->input.mouse_x,
                                               ctx->input.mouse_y);
        item_hovered = item_hovered && cui_rect_contains(list_rect,
                                                          ctx->input.mouse_x,
                                                          ctx->input.mouse_y);

        /* Draw selection/hover background */
        if (i == *selected) {
            cui_draw_rect(ctx, item_rect.x, item_rect.y, item_rect.w, item_rect.h,
                          ctx->theme.accent);
        } else if (item_hovered) {
            cui_draw_rect(ctx, item_rect.x, item_rect.y, item_rect.w, item_rect.h,
                          ctx->theme.bg_widget_hover);
        }

        /* Handle click */
        if (item_hovered && ctx->input.mouse_pressed[0]) {
            *selected = i;
            changed = true;
        }

        /* Draw item text */
        float text_y = item_rect.y + (item_rect.h - cui_text_height(ctx)) * 0.5f;
        cui_draw_text(ctx, items[i], item_rect.x + ctx->theme.padding,
                      text_y, ctx->theme.text);
    }

    cui_pop_scissor(ctx);

    return changed;
}

/* ============================================================================
 * Collapsing Header
 * ============================================================================ */

bool cui_collapsing_header(CUI_Context *ctx, const char *label)
{
    if (!ctx || !label) return false;

    CUI_Id id = cui_make_id(ctx, label);
    CUI_WidgetState *state = cui_get_state(ctx, id);

    CUI_Rect rect = cui_allocate_rect(ctx, 0, ctx->theme.widget_height);

    bool hovered, held;
    bool pressed = cui_widget_behavior(ctx, id, rect, &hovered, &held);

    if (pressed && state) {
        state->expanded = !state->expanded;
    }

    /* Draw background */
    uint32_t bg = cui_widget_bg_color(ctx, hovered, held, false);
    cui_draw_rect_rounded(ctx, rect.x, rect.y, rect.w, rect.h, bg, ctx->theme.corner_radius);

    /* Draw arrow */
    float arrow_x = rect.x + ctx->theme.padding;
    float text_y = rect.y + (rect.h - cui_text_height(ctx)) * 0.5f;
    const char *arrow = (state && state->expanded) ? "v" : ">";
    cui_draw_text(ctx, arrow, arrow_x, text_y, ctx->theme.text);

    /* Draw label */
    float label_x = arrow_x + 20;
    cui_draw_text(ctx, label, label_x, text_y, ctx->theme.text);

    return state && state->expanded;
}

/* ============================================================================
 * Panels
 * ============================================================================ */

bool cui_begin_panel(CUI_Context *ctx, const char *name,
                     float x, float y, float w, float h, uint32_t flags)
{
    if (!ctx || !name) return false;

    CUI_Id id = cui_make_id(ctx, name);
    (void)id;  /* Used for dragging in full implementation */

    CUI_Rect rect = {x, y, w, h};

    /* Handle dragging if movable */
    if (flags & CUI_PANEL_MOVABLE) {
        CUI_Rect title_rect = {x, y, w, ctx->theme.widget_height};
        bool in_title = cui_rect_contains(title_rect, ctx->input.mouse_x, ctx->input.mouse_y);

        if (in_title && ctx->input.mouse_pressed[0]) {
            ctx->active = id;
        }

        if (ctx->active == id && ctx->input.mouse_down[0]) {
            /* Note: Would need to store panel position in state for proper dragging */
        }

        if (ctx->active == id && ctx->input.mouse_released[0]) {
            ctx->active = CUI_ID_NONE;
        }
    }

    /* Draw panel background */
    cui_draw_rect_rounded(ctx, rect.x, rect.y, rect.w, rect.h,
                          ctx->theme.bg_panel, ctx->theme.corner_radius);

    /* Draw border */
    if (flags & CUI_PANEL_BORDER) {
        cui_draw_rect_outline(ctx, rect.x, rect.y, rect.w, rect.h,
                              ctx->theme.border, ctx->theme.border_width);
    }

    /* Draw title bar */
    float content_start_y = rect.y;
    if (flags & CUI_PANEL_TITLE_BAR) {
        float title_h = ctx->theme.widget_height;
        cui_draw_rect(ctx, rect.x, rect.y, rect.w, title_h,
                      ctx->theme.bg_widget);

        float text_y = rect.y + (title_h - cui_text_height(ctx)) * 0.5f;
        cui_draw_text(ctx, name, rect.x + ctx->theme.padding, text_y, ctx->theme.text);

        content_start_y = rect.y + title_h;
    }

    /* Push layout for panel content */
    CUI_LayoutFrame *frame = &ctx->layout_stack[ctx->layout_depth++];
    frame->bounds = rect;
    frame->bounds.y = content_start_y;
    frame->bounds.h = rect.h - (content_start_y - rect.y);
    frame->cursor_x = rect.x + ctx->theme.padding;
    frame->cursor_y = content_start_y + ctx->theme.padding;
    frame->spacing = ctx->theme.spacing;
    frame->padding = ctx->theme.padding;
    frame->horizontal = false;
    frame->has_clip = false;

    cui_push_id(ctx, name);

    return true;
}

void cui_end_panel(CUI_Context *ctx)
{
    if (!ctx) return;

    cui_pop_id(ctx);

    if (ctx->layout_depth > 1) {
        ctx->layout_depth--;
    }
}

/* ============================================================================
 * Tooltip
 * ============================================================================ */

void cui_tooltip(CUI_Context *ctx, const char *text)
{
    if (!ctx || !text) return;

    /* Only show if something is hovered */
    if (ctx->hot == CUI_ID_NONE) return;

    float text_w = cui_text_width(ctx, text);
    float text_h = cui_text_height(ctx);
    float pad = ctx->theme.padding;

    float x = ctx->input.mouse_x + 16;
    float y = ctx->input.mouse_y + 16;

    /* Keep tooltip on screen */
    if (x + text_w + pad * 2 > ctx->width) {
        x = ctx->width - text_w - pad * 2;
    }
    if (y + text_h + pad * 2 > ctx->height) {
        y = ctx->height - text_h - pad * 2;
    }

    /* Draw tooltip */
    cui_draw_rect_rounded(ctx, x, y, text_w + pad * 2, text_h + pad * 2,
                          ctx->theme.bg_panel, ctx->theme.corner_radius);
    cui_draw_rect_outline(ctx, x, y, text_w + pad * 2, text_h + pad * 2,
                          ctx->theme.border, 1.0f);
    cui_draw_text(ctx, text, x + pad, y + pad, ctx->theme.text);
}
