/*
 * Carbon UI - Color Picker Widget
 *
 * Advanced color picker with HSV wheel/square, alpha, and various input modes
 */

#include "carbon/ui.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Forward declarations from other UI modules */
extern void cui_draw_rect(CUI_Context *ctx, float x, float y, float w, float h, uint32_t color);
extern void cui_draw_rect_outline(CUI_Context *ctx, float x, float y, float w, float h, uint32_t color, float thickness);
extern void cui_draw_line(CUI_Context *ctx, float x1, float y1, float x2, float y2, uint32_t color, float thickness);
extern float cui_draw_text(CUI_Context *ctx, const char *text, float x, float y, uint32_t color);
extern float cui_text_width(CUI_Context *ctx, const char *text);
extern float cui_text_height(CUI_Context *ctx);
extern void cui_draw_triangle(CUI_Context *ctx, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color);
extern CUI_Id cui_make_id(CUI_Context *ctx, const char *str);
extern CUI_Rect cui_allocate_rect(CUI_Context *ctx, float width, float height);

/* ============================================================================
 * Color Conversion Utilities
 * ============================================================================ */

void cui_rgb_to_hsv(float r, float g, float b, float *h, float *s, float *v)
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

void cui_hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b)
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
static uint32_t cui_float_rgba_to_u32(float r, float g, float b, float a)
{
    uint8_t ri = (uint8_t)(r * 255.0f + 0.5f);
    uint8_t gi = (uint8_t)(g * 255.0f + 0.5f);
    uint8_t bi = (uint8_t)(b * 255.0f + 0.5f);
    uint8_t ai = (uint8_t)(a * 255.0f + 0.5f);
    return cui_rgba(ri, gi, bi, ai);
}

/* ============================================================================
 * Internal Drawing Helpers
 * ============================================================================ */

/* Draw a filled circle (approximated with triangles) */
static void cui_draw_circle(CUI_Context *ctx, float cx, float cy, float radius,
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
        cui_draw_triangle(ctx, cx, cy, prev_x, prev_y, x, y, color);
        prev_x = x;
        prev_y = y;
    }
}

/* Draw a ring (circle outline with thickness) */
static void cui_draw_ring(CUI_Context *ctx, float cx, float cy, float radius,
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
        cui_draw_line(ctx, x1, y1, x2, y2, color, thickness);
    }
}

/* Draw the SV (saturation/value) square */
static void cui_draw_sv_square(CUI_Context *ctx, float x, float y, float size,
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
            cui_hsv_to_rgb(hue, s, v, &r, &g, &b);
            uint32_t color = cui_float_rgba_to_u32(r, g, b, 1.0f);

            cui_draw_rect(ctx, x + sx * cell_size, y + sy * cell_size,
                          cell_size + 1, cell_size + 1, color);
        }
    }
}

/* Draw a hue bar (vertical) */
static void cui_draw_hue_bar(CUI_Context *ctx, float x, float y, float w, float h)
{
    int steps = 32;
    float cell_h = h / steps;

    for (int i = 0; i < steps; i++) {
        float hue = (float)i / steps;
        float r, g, b;
        cui_hsv_to_rgb(hue, 1.0f, 1.0f, &r, &g, &b);
        uint32_t color = cui_float_rgba_to_u32(r, g, b, 1.0f);

        cui_draw_rect(ctx, x, y + i * cell_h, w, cell_h + 1, color);
    }
}

/* Draw an alpha bar */
static void cui_draw_alpha_bar(CUI_Context *ctx, float x, float y, float w, float h,
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
            cui_draw_rect(ctx, x + cx, y + cy, cw, ch, check_color);
        }
    }

    /* Draw alpha gradient over checkerboard */
    int steps = 16;
    float cell_w = w / steps;
    for (int i = 0; i < steps; i++) {
        float alpha = (float)i / (steps - 1);
        uint32_t color = cui_float_rgba_to_u32(r, g, b, alpha);
        cui_draw_rect(ctx, x + i * cell_w, y, cell_w + 1, h, color);
    }
}

/* ============================================================================
 * Color Picker Widgets
 * ============================================================================ */

bool cui_color_button(CUI_Context *ctx, const char *label,
                      float *rgba, float size)
{
    if (!ctx || !rgba) return false;

    CUI_Id id = cui_make_id(ctx, label);
    if (size <= 0) size = ctx->theme.widget_height;

    CUI_Rect rect = cui_allocate_rect(ctx, size, size);

    /* Check interaction */
    bool hovered = cui_rect_contains(rect, ctx->input.mouse_x, ctx->input.mouse_y);
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
            cui_draw_rect(ctx, rect.x + x, rect.y + y, cw, ch, check_color);
        }
    }

    /* Draw color */
    uint32_t color = cui_float_rgba_to_u32(rgba[0], rgba[1], rgba[2], rgba[3]);
    cui_draw_rect(ctx, rect.x, rect.y, rect.w, rect.h, color);

    /* Draw border */
    uint32_t border_color = hovered ? ctx->theme.accent : ctx->theme.border;
    cui_draw_rect_outline(ctx, rect.x, rect.y, rect.w, rect.h, border_color, 1.0f);

    return clicked;
}

bool cui_color_edit3(CUI_Context *ctx, const char *label, float *rgb)
{
    if (!ctx || !rgb) return false;
    float rgba[4] = {rgb[0], rgb[1], rgb[2], 1.0f};
    bool changed = cui_color_picker(ctx, label, rgba, CUI_COLORPICKER_NO_ALPHA);
    if (changed) {
        rgb[0] = rgba[0];
        rgb[1] = rgba[1];
        rgb[2] = rgba[2];
    }
    return changed;
}

bool cui_color_edit4(CUI_Context *ctx, const char *label, float *rgba)
{
    return cui_color_picker(ctx, label, rgba, 0);
}

bool cui_color_picker(CUI_Context *ctx, const char *label,
                      float *rgba, uint32_t flags)
{
    if (!ctx || !rgba) return false;

    CUI_Id id = cui_make_id(ctx, label);

    /* Picker dimensions */
    float picker_size = 150.0f;
    float hue_bar_width = 20.0f;
    float alpha_bar_height = 20.0f;
    float spacing = ctx->theme.spacing;
    float total_width = picker_size + spacing + hue_bar_width;
    float total_height = picker_size;

    if (!(flags & CUI_COLORPICKER_NO_ALPHA)) {
        total_height += spacing + alpha_bar_height;
    }

    /* Add space for label */
    float label_width = cui_text_width(ctx, label);
    if (label_width > 0) {
        total_height += cui_text_height(ctx) + spacing;
    }

    CUI_Rect rect = cui_allocate_rect(ctx, total_width, total_height);

    float y = rect.y;
    bool changed = false;

    /* Draw label */
    if (label && label[0]) {
        cui_draw_text(ctx, label, rect.x, y, ctx->theme.text);
        y += cui_text_height(ctx) + spacing;
    }

    /* Convert to HSV for editing */
    float h, s, v;
    cui_rgb_to_hsv(rgba[0], rgba[1], rgba[2], &h, &s, &v);

    /* Draw SV square */
    float sv_x = rect.x;
    float sv_y = y;
    cui_draw_sv_square(ctx, sv_x, sv_y, picker_size, h);

    /* SV square interaction */
    CUI_Id sv_id = id + 1;
    CUI_Rect sv_rect = {sv_x, sv_y, picker_size, picker_size};
    bool sv_hovered = cui_rect_contains(sv_rect, ctx->input.mouse_x, ctx->input.mouse_y);

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
            cui_hsv_to_rgb(h, s, v, &rgba[0], &rgba[1], &rgba[2]);
            changed = true;
        } else {
            ctx->active = CUI_ID_NONE;
        }
    }

    /* Draw SV cursor */
    float cursor_x = sv_x + s * picker_size;
    float cursor_y = sv_y + (1.0f - v) * picker_size;
    cui_draw_ring(ctx, cursor_x, cursor_y, 5.0f, 0xFFFFFFFF, 2.0f, 16);
    cui_draw_ring(ctx, cursor_x, cursor_y, 4.0f, 0xFF000000, 1.0f, 16);

    /* Draw SV border */
    cui_draw_rect_outline(ctx, sv_x, sv_y, picker_size, picker_size,
                          ctx->theme.border, 1.0f);

    /* Draw hue bar */
    float hue_x = sv_x + picker_size + spacing;
    float hue_y = sv_y;
    cui_draw_hue_bar(ctx, hue_x, hue_y, hue_bar_width, picker_size);

    /* Hue bar interaction */
    CUI_Id hue_id = id + 2;
    CUI_Rect hue_rect = {hue_x, hue_y, hue_bar_width, picker_size};
    bool hue_hovered = cui_rect_contains(hue_rect, ctx->input.mouse_x, ctx->input.mouse_y);

    if (hue_hovered) ctx->hot = hue_id;

    if (hue_hovered && ctx->input.mouse_pressed[0]) {
        ctx->active = hue_id;
    }

    if (ctx->active == hue_id) {
        if (ctx->input.mouse_down[0]) {
            h = (ctx->input.mouse_y - hue_y) / picker_size;
            h = h < 0 ? 0 : (h > 1 ? 1 : h);
            cui_hsv_to_rgb(h, s, v, &rgba[0], &rgba[1], &rgba[2]);
            changed = true;
        } else {
            ctx->active = CUI_ID_NONE;
        }
    }

    /* Draw hue cursor */
    float hue_cursor_y = hue_y + h * picker_size;
    cui_draw_rect(ctx, hue_x - 2, hue_cursor_y - 2, hue_bar_width + 4, 4, 0xFFFFFFFF);
    cui_draw_rect_outline(ctx, hue_x - 2, hue_cursor_y - 2, hue_bar_width + 4, 4, 0xFF000000, 1.0f);

    /* Draw hue border */
    cui_draw_rect_outline(ctx, hue_x, hue_y, hue_bar_width, picker_size,
                          ctx->theme.border, 1.0f);

    /* Draw alpha bar if enabled */
    if (!(flags & CUI_COLORPICKER_NO_ALPHA)) {
        float alpha_x = rect.x;
        float alpha_y = sv_y + picker_size + spacing;
        cui_draw_alpha_bar(ctx, alpha_x, alpha_y, total_width, alpha_bar_height,
                           rgba[0], rgba[1], rgba[2]);

        /* Alpha bar interaction */
        CUI_Id alpha_id = id + 3;
        CUI_Rect alpha_rect = {alpha_x, alpha_y, total_width, alpha_bar_height};
        bool alpha_hovered = cui_rect_contains(alpha_rect, ctx->input.mouse_x, ctx->input.mouse_y);

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
                ctx->active = CUI_ID_NONE;
            }
        }

        /* Draw alpha cursor */
        float alpha_cursor_x = alpha_x + rgba[3] * total_width;
        cui_draw_rect(ctx, alpha_cursor_x - 2, alpha_y - 2, 4, alpha_bar_height + 4, 0xFFFFFFFF);
        cui_draw_rect_outline(ctx, alpha_cursor_x - 2, alpha_y - 2, 4, alpha_bar_height + 4, 0xFF000000, 1.0f);

        /* Draw alpha border */
        cui_draw_rect_outline(ctx, alpha_x, alpha_y, total_width, alpha_bar_height,
                              ctx->theme.border, 1.0f);
    }

    return changed;
}
