/*
 * Agentite UI - Color Picker Widget
 *
 * Advanced color picker with HSV wheel/square, alpha, and various input modes
 */

#include "agentite/ui.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Forward declarations from other UI modules */
extern void aui_draw_rect(AUI_Context *ctx, float x, float y, float w, float h, uint32_t color);
extern void aui_draw_rect_outline(AUI_Context *ctx, float x, float y, float w, float h, uint32_t color, float thickness);
extern void aui_draw_line(AUI_Context *ctx, float x1, float y1, float x2, float y2, uint32_t color, float thickness);
extern float aui_draw_text(AUI_Context *ctx, const char *text, float x, float y, uint32_t color);
extern float aui_text_width(AUI_Context *ctx, const char *text);
extern float aui_text_height(AUI_Context *ctx);
extern void aui_draw_triangle(AUI_Context *ctx, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color);
extern AUI_Id aui_make_id(AUI_Context *ctx, const char *str);
extern AUI_Rect aui_allocate_rect(AUI_Context *ctx, float width, float height);

/* ============================================================================
 * Color Conversion Utilities
 * ============================================================================ */

void aui_rgb_to_hsv(float r, float g, float b, float *h, float *s, float *v)
{
    float max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float delta = max - min;

    *v = max;

    if (max == 0.0f) {
        *s = 0.0f;
        *h = 0.0f;
        return;
    }

    *s = delta / max;

    if (delta == 0.0f) {
        *h = 0.0f;
        return;
    }

    if (r == max) {
        *h = (g - b) / delta;
    } else if (g == max) {
        *h = 2.0f + (b - r) / delta;
    } else {
        *h = 4.0f + (r - g) / delta;
    }

    *h /= 6.0f;
    if (*h < 0.0f) *h += 1.0f;
}

void aui_hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b)
{
    if (s == 0.0f) {
        *r = *g = *b = v;
        return;
    }

    h = fmodf(h, 1.0f);
    if (h < 0.0f) h += 1.0f;
    h *= 6.0f;

    int i = (int)h;
    float f = h - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    switch (i) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

/* Convert float RGBA to packed uint32_t */
static uint32_t aui_float_rgba_to_u32(float r, float g, float b, float a)
{
    uint8_t ri = (uint8_t)(r * 255.0f + 0.5f);
    uint8_t gi = (uint8_t)(g * 255.0f + 0.5f);
    uint8_t bi = (uint8_t)(b * 255.0f + 0.5f);
    uint8_t ai = (uint8_t)(a * 255.0f + 0.5f);
    return aui_rgba(ri, gi, bi, ai);
}

/* ============================================================================
 * Internal Drawing Helpers
 * ============================================================================ */

/* Draw a filled circle (approximated with triangles) */
static void aui_draw_circle(AUI_Context *ctx, float cx, float cy, float radius,
                            uint32_t color, int segments)
{
    if (segments < 6) segments = 6;
    if (segments > 64) segments = 64;

    float angle_step = 2.0f * 3.14159265f / segments;
    float prev_x = cx + radius;
    float prev_y = cy;

    for (int i = 1; i <= segments; i++) {
        float angle = angle_step * i;
        float x = cx + cosf(angle) * radius;
        float y = cy + sinf(angle) * radius;
        aui_draw_triangle(ctx, cx, cy, prev_x, prev_y, x, y, color);
        prev_x = x;
        prev_y = y;
    }
}

/* Draw a ring (circle outline with thickness) */
static void aui_draw_ring(AUI_Context *ctx, float cx, float cy, float radius,
                          uint32_t color, float thickness, int segments)
{
    if (segments < 6) segments = 6;
    if (segments > 64) segments = 64;

    float angle_step = 2.0f * 3.14159265f / segments;

    for (int i = 0; i < segments; i++) {
        float angle1 = angle_step * i;
        float angle2 = angle_step * (i + 1);
        float x1 = cx + cosf(angle1) * radius;
        float y1 = cy + sinf(angle1) * radius;
        float x2 = cx + cosf(angle2) * radius;
        float y2 = cy + sinf(angle2) * radius;
        aui_draw_line(ctx, x1, y1, x2, y2, color, thickness);
    }
}

/* Draw the SV (saturation/value) square */
static void aui_draw_sv_square(AUI_Context *ctx, float x, float y, float size,
                                float hue)
{
    /* Draw a grid of colored rectangles to approximate the gradient */
    int steps = 16;
    float cell_size = size / steps;

    for (int sy = 0; sy < steps; sy++) {
        for (int sx = 0; sx < steps; sx++) {
            float s = (float)sx / (steps - 1);
            float v = 1.0f - (float)sy / (steps - 1);

            float r, g, b;
            aui_hsv_to_rgb(hue, s, v, &r, &g, &b);
            uint32_t color = aui_float_rgba_to_u32(r, g, b, 1.0f);

            aui_draw_rect(ctx, x + sx * cell_size, y + sy * cell_size,
                          cell_size + 1, cell_size + 1, color);
        }
    }
}

/* Draw a hue bar (vertical) */
static void aui_draw_hue_bar(AUI_Context *ctx, float x, float y, float w, float h)
{
    int steps = 32;
    float cell_h = h / steps;

    for (int i = 0; i < steps; i++) {
        float hue = (float)i / steps;
        float r, g, b;
        aui_hsv_to_rgb(hue, 1.0f, 1.0f, &r, &g, &b);
        uint32_t color = aui_float_rgba_to_u32(r, g, b, 1.0f);

        aui_draw_rect(ctx, x, y + i * cell_h, w, cell_h + 1, color);
    }
}

/* Draw an alpha bar */
static void aui_draw_alpha_bar(AUI_Context *ctx, float x, float y, float w, float h,
                                float r, float g, float b)
{
    /* Draw checkerboard background first */
    float check_size = 6.0f;
    for (float cy = 0; cy < h; cy += check_size) {
        for (float cx = 0; cx < w; cx += check_size) {
            int ix = (int)(cx / check_size);
            int iy = (int)(cy / check_size);
            uint32_t check_color = ((ix + iy) % 2) ? 0xFF808080 : 0xFFC0C0C0;
            float cw = (cx + check_size > w) ? w - cx : check_size;
            float ch = (cy + check_size > h) ? h - cy : check_size;
            aui_draw_rect(ctx, x + cx, y + cy, cw, ch, check_color);
        }
    }

    /* Draw alpha gradient over checkerboard */
    int steps = 16;
    float cell_w = w / steps;
    for (int i = 0; i < steps; i++) {
        float alpha = (float)i / (steps - 1);
        uint32_t color = aui_float_rgba_to_u32(r, g, b, alpha);
        aui_draw_rect(ctx, x + i * cell_w, y, cell_w + 1, h, color);
    }
}

/* ============================================================================
 * Color Picker Widgets
 * ============================================================================ */

bool aui_color_button(AUI_Context *ctx, const char *label,
                      float *rgba, float size)
{
    if (!ctx || !rgba) return false;

    AUI_Id id = aui_make_id(ctx, label);
    if (size <= 0) size = ctx->theme.widget_height;

    AUI_Rect rect = aui_allocate_rect(ctx, size, size);

    /* Check interaction */
    bool hovered = aui_rect_contains(rect, ctx->input.mouse_x, ctx->input.mouse_y);
    bool clicked = false;

    if (hovered) {
        ctx->hot = id;
        if (ctx->input.mouse_pressed[0]) {
            clicked = true;
        }
    }

    /* Draw checkerboard for alpha */
    float check_size = 4.0f;
    for (float y = 0; y < rect.h; y += check_size) {
        for (float x = 0; x < rect.w; x += check_size) {
            int ix = (int)(x / check_size);
            int iy = (int)(y / check_size);
            uint32_t check_color = ((ix + iy) % 2) ? 0xFF606060 : 0xFF909090;
            float cw = (x + check_size > rect.w) ? rect.w - x : check_size;
            float ch = (y + check_size > rect.h) ? rect.h - y : check_size;
            aui_draw_rect(ctx, rect.x + x, rect.y + y, cw, ch, check_color);
        }
    }

    /* Draw color */
    uint32_t color = aui_float_rgba_to_u32(rgba[0], rgba[1], rgba[2], rgba[3]);
    aui_draw_rect(ctx, rect.x, rect.y, rect.w, rect.h, color);

    /* Draw border */
    uint32_t border_color = hovered ? ctx->theme.accent : ctx->theme.border;
    aui_draw_rect_outline(ctx, rect.x, rect.y, rect.w, rect.h, border_color, 1.0f);

    return clicked;
}

bool aui_color_edit3(AUI_Context *ctx, const char *label, float *rgb)
{
    if (!ctx || !rgb) return false;
    float rgba[4] = {rgb[0], rgb[1], rgb[2], 1.0f};
    bool changed = aui_color_picker(ctx, label, rgba, AUI_COLORPICKER_NO_ALPHA);
    if (changed) {
        rgb[0] = rgba[0];
        rgb[1] = rgba[1];
        rgb[2] = rgba[2];
    }
    return changed;
}

bool aui_color_edit4(AUI_Context *ctx, const char *label, float *rgba)
{
    return aui_color_picker(ctx, label, rgba, 0);
}

bool aui_color_picker(AUI_Context *ctx, const char *label,
                      float *rgba, uint32_t flags)
{
    if (!ctx || !rgba) return false;

    AUI_Id id = aui_make_id(ctx, label);

    /* Picker dimensions */
    float picker_size = 150.0f;
    float hue_bar_width = 20.0f;
    float alpha_bar_height = 20.0f;
    float spacing = ctx->theme.spacing;
    float total_width = picker_size + spacing + hue_bar_width;
    float total_height = picker_size;

    if (!(flags & AUI_COLORPICKER_NO_ALPHA)) {
        total_height += spacing + alpha_bar_height;
    }

    /* Add space for label */
    float label_width = aui_text_width(ctx, label);
    if (label_width > 0) {
        total_height += aui_text_height(ctx) + spacing;
    }

    AUI_Rect rect = aui_allocate_rect(ctx, total_width, total_height);

    float y = rect.y;
    bool changed = false;

    /* Draw label */
    if (label && label[0]) {
        aui_draw_text(ctx, label, rect.x, y, ctx->theme.text);
        y += aui_text_height(ctx) + spacing;
    }

    /* Convert to HSV for editing */
    float h, s, v;
    aui_rgb_to_hsv(rgba[0], rgba[1], rgba[2], &h, &s, &v);

    /* Draw SV square */
    float sv_x = rect.x;
    float sv_y = y;
    aui_draw_sv_square(ctx, sv_x, sv_y, picker_size, h);

    /* SV square interaction */
    AUI_Id sv_id = id + 1;
    AUI_Rect sv_rect = {sv_x, sv_y, picker_size, picker_size};
    bool sv_hovered = aui_rect_contains(sv_rect, ctx->input.mouse_x, ctx->input.mouse_y);

    if (sv_hovered) ctx->hot = sv_id;

    if (sv_hovered && ctx->input.mouse_pressed[0]) {
        ctx->active = sv_id;
    }

    if (ctx->active == sv_id) {
        if (ctx->input.mouse_down[0]) {
            s = (ctx->input.mouse_x - sv_x) / picker_size;
            v = 1.0f - (ctx->input.mouse_y - sv_y) / picker_size;
            s = s < 0 ? 0 : (s > 1 ? 1 : s);
            v = v < 0 ? 0 : (v > 1 ? 1 : v);
            aui_hsv_to_rgb(h, s, v, &rgba[0], &rgba[1], &rgba[2]);
            changed = true;
        } else {
            ctx->active = AUI_ID_NONE;
        }
    }

    /* Draw SV cursor */
    float cursor_x = sv_x + s * picker_size;
    float cursor_y = sv_y + (1.0f - v) * picker_size;
    aui_draw_ring(ctx, cursor_x, cursor_y, 5.0f, 0xFFFFFFFF, 2.0f, 16);
    aui_draw_ring(ctx, cursor_x, cursor_y, 4.0f, 0xFF000000, 1.0f, 16);

    /* Draw SV border */
    aui_draw_rect_outline(ctx, sv_x, sv_y, picker_size, picker_size,
                          ctx->theme.border, 1.0f);

    /* Draw hue bar */
    float hue_x = sv_x + picker_size + spacing;
    float hue_y = sv_y;
    aui_draw_hue_bar(ctx, hue_x, hue_y, hue_bar_width, picker_size);

    /* Hue bar interaction */
    AUI_Id hue_id = id + 2;
    AUI_Rect hue_rect = {hue_x, hue_y, hue_bar_width, picker_size};
    bool hue_hovered = aui_rect_contains(hue_rect, ctx->input.mouse_x, ctx->input.mouse_y);

    if (hue_hovered) ctx->hot = hue_id;

    if (hue_hovered && ctx->input.mouse_pressed[0]) {
        ctx->active = hue_id;
    }

    if (ctx->active == hue_id) {
        if (ctx->input.mouse_down[0]) {
            h = (ctx->input.mouse_y - hue_y) / picker_size;
            h = h < 0 ? 0 : (h > 1 ? 1 : h);
            aui_hsv_to_rgb(h, s, v, &rgba[0], &rgba[1], &rgba[2]);
            changed = true;
        } else {
            ctx->active = AUI_ID_NONE;
        }
    }

    /* Draw hue cursor */
    float hue_cursor_y = hue_y + h * picker_size;
    aui_draw_rect(ctx, hue_x - 2, hue_cursor_y - 2, hue_bar_width + 4, 4, 0xFFFFFFFF);
    aui_draw_rect_outline(ctx, hue_x - 2, hue_cursor_y - 2, hue_bar_width + 4, 4, 0xFF000000, 1.0f);

    /* Draw hue border */
    aui_draw_rect_outline(ctx, hue_x, hue_y, hue_bar_width, picker_size,
                          ctx->theme.border, 1.0f);

    /* Draw alpha bar if enabled */
    if (!(flags & AUI_COLORPICKER_NO_ALPHA)) {
        float alpha_x = rect.x;
        float alpha_y = sv_y + picker_size + spacing;
        aui_draw_alpha_bar(ctx, alpha_x, alpha_y, total_width, alpha_bar_height,
                           rgba[0], rgba[1], rgba[2]);

        /* Alpha bar interaction */
        AUI_Id alpha_id = id + 3;
        AUI_Rect alpha_rect = {alpha_x, alpha_y, total_width, alpha_bar_height};
        bool alpha_hovered = aui_rect_contains(alpha_rect, ctx->input.mouse_x, ctx->input.mouse_y);

        if (alpha_hovered) ctx->hot = alpha_id;

        if (alpha_hovered && ctx->input.mouse_pressed[0]) {
            ctx->active = alpha_id;
        }

        if (ctx->active == alpha_id) {
            if (ctx->input.mouse_down[0]) {
                rgba[3] = (ctx->input.mouse_x - alpha_x) / total_width;
                rgba[3] = rgba[3] < 0 ? 0 : (rgba[3] > 1 ? 1 : rgba[3]);
                changed = true;
            } else {
                ctx->active = AUI_ID_NONE;
            }
        }

        /* Draw alpha cursor */
        float alpha_cursor_x = alpha_x + rgba[3] * total_width;
        aui_draw_rect(ctx, alpha_cursor_x - 2, alpha_y - 2, 4, alpha_bar_height + 4, 0xFFFFFFFF);
        aui_draw_rect_outline(ctx, alpha_cursor_x - 2, alpha_y - 2, 4, alpha_bar_height + 4, 0xFF000000, 1.0f);

        /* Draw alpha border */
        aui_draw_rect_outline(ctx, alpha_x, alpha_y, total_width, alpha_bar_height,
                              ctx->theme.border, 1.0f);
    }

    return changed;
}
