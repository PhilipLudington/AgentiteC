/*
 * Carbon UI - Rich Styling System Implementation
 */

#include "carbon/ui_style.h"
#include "carbon/ui.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ============================================================================
 * Gradient Helper Functions
 * ============================================================================ */

CUI_Gradient cui_gradient_linear(float angle_degrees, uint32_t color1, uint32_t color2)
{
    CUI_Gradient g;
    memset(&g, 0, sizeof(g));
    g.type = CUI_GRADIENT_LINEAR;
    g.angle = angle_degrees;
    g.stop_count = 2;
    g.stops[0].position = 0.0f;
    g.stops[0].color = color1;
    g.stops[1].position = 1.0f;
    g.stops[1].color = color2;
    return g;
}

CUI_Gradient cui_gradient_linear_stops(float angle_degrees,
                                        const CUI_GradientStop *stops, int count)
{
    CUI_Gradient g;
    memset(&g, 0, sizeof(g));
    g.type = CUI_GRADIENT_LINEAR;
    g.angle = angle_degrees;
    g.stop_count = count < CUI_MAX_GRADIENT_STOPS ? count : CUI_MAX_GRADIENT_STOPS;
    for (int i = 0; i < g.stop_count; i++) {
        g.stops[i] = stops[i];
    }
    return g;
}

CUI_Gradient cui_gradient_radial(float center_x, float center_y, float radius,
                                  uint32_t inner_color, uint32_t outer_color)
{
    CUI_Gradient g;
    memset(&g, 0, sizeof(g));
    g.type = CUI_GRADIENT_RADIAL;
    g.center_x = center_x;
    g.center_y = center_y;
    g.radius = radius;
    g.stop_count = 2;
    g.stops[0].position = 0.0f;
    g.stops[0].color = inner_color;
    g.stops[1].position = 1.0f;
    g.stops[1].color = outer_color;
    return g;
}

/* ============================================================================
 * Style Creation
 * ============================================================================ */

CUI_Style cui_style_default(void)
{
    CUI_Style style;
    memset(&style, 0, sizeof(style));
    style.opacity = 1.0f;
    style.background.type = CUI_BG_NONE;
    style.background_hover.type = CUI_BG_NONE;
    style.background_active.type = CUI_BG_NONE;
    style.background_disabled.type = CUI_BG_NONE;
    style.text_color = 0xFFFFFFFF;          /* White text by default */
    style.text_color_hover = 0xFFFFFFFF;
    style.text_color_disabled = 0x888888FF; /* Gray for disabled */
    return style;
}

CUI_Style cui_style_from_theme(const CUI_Context *ctx)
{
    CUI_Style style = cui_style_default();
    if (!ctx) return style;

    const CUI_Theme *theme = cui_get_theme(ctx);

    /* Set colors from theme */
    style.background = cui_bg_solid(theme->bg_widget);
    style.background_hover = cui_bg_solid(theme->bg_widget_hover);
    style.background_active = cui_bg_solid(theme->bg_widget_active);
    style.background_disabled = cui_bg_solid(theme->bg_widget_disabled);

    style.border = cui_border(theme->border_width, theme->border);
    style.corner_radius = cui_corners_uniform(theme->corner_radius);

    style.text_color = theme->text;
    style.text_color_hover = theme->text_highlight;
    style.text_color_disabled = theme->text_disabled;

    style.padding = cui_edges_uniform(theme->padding);

    return style;
}

void cui_style_merge(CUI_Style *dst, const CUI_Style *src)
{
    if (!dst || !src) return;

    /* Merge padding if non-zero */
    if (src->padding.top != 0) dst->padding.top = src->padding.top;
    if (src->padding.right != 0) dst->padding.right = src->padding.right;
    if (src->padding.bottom != 0) dst->padding.bottom = src->padding.bottom;
    if (src->padding.left != 0) dst->padding.left = src->padding.left;

    /* Merge margin if non-zero */
    if (src->margin.top != 0) dst->margin.top = src->margin.top;
    if (src->margin.right != 0) dst->margin.right = src->margin.right;
    if (src->margin.bottom != 0) dst->margin.bottom = src->margin.bottom;
    if (src->margin.left != 0) dst->margin.left = src->margin.left;

    /* Merge border */
    if (src->border.width.top != 0 || src->border.width.right != 0 ||
        src->border.width.bottom != 0 || src->border.width.left != 0) {
        dst->border = src->border;
    }

    /* Merge corner radius */
    if (src->corner_radius.top_left != 0 || src->corner_radius.top_right != 0 ||
        src->corner_radius.bottom_right != 0 || src->corner_radius.bottom_left != 0) {
        dst->corner_radius = src->corner_radius;
    }

    /* Merge backgrounds */
    if (src->background.type != CUI_BG_NONE) dst->background = src->background;
    if (src->background_hover.type != CUI_BG_NONE) dst->background_hover = src->background_hover;
    if (src->background_active.type != CUI_BG_NONE) dst->background_active = src->background_active;
    if (src->background_disabled.type != CUI_BG_NONE) dst->background_disabled = src->background_disabled;

    /* Merge shadows */
    if (src->shadow_count > 0) {
        dst->shadow_count = src->shadow_count;
        for (int i = 0; i < src->shadow_count; i++) {
            dst->shadows[i] = src->shadows[i];
        }
    }

    /* Merge opacity (only if explicitly set to non-1.0) */
    if (src->opacity != 1.0f) dst->opacity = src->opacity;

    /* Merge text colors */
    if (src->text_color != 0) dst->text_color = src->text_color;
    if (src->text_color_hover != 0) dst->text_color_hover = src->text_color_hover;
    if (src->text_color_active != 0) dst->text_color_active = src->text_color_active;
    if (src->text_color_disabled != 0) dst->text_color_disabled = src->text_color_disabled;

    /* Merge font size */
    if (src->font_size > 0) dst->font_size = src->font_size;

    /* Merge size constraints */
    if (src->min_width > 0) dst->min_width = src->min_width;
    if (src->min_height > 0) dst->min_height = src->min_height;
    if (src->max_width > 0) dst->max_width = src->max_width;
    if (src->max_height > 0) dst->max_height = src->max_height;
}

/* ============================================================================
 * Style Stack Implementation
 *
 * The style stack is stored in an extended CUI_Context.
 * For now, we'll use a simple approach with function-local static storage
 * until we extend CUI_Context properly.
 * ============================================================================ */

#define CUI_STYLE_STACK_SIZE 32

typedef struct CUI_StyleStackEntry {
    CUI_Style style;
    bool active;
} CUI_StyleStackEntry;

typedef struct CUI_StyleVarEntry {
    CUI_StyleVar var;
    float old_value;
} CUI_StyleVarEntry;

typedef struct CUI_StyleColorEntry {
    CUI_StyleColor color;
    uint32_t old_value;
} CUI_StyleColorEntry;

/* Static storage for style stack (will be moved to CUI_Context later) */
static CUI_StyleStackEntry s_style_stack[CUI_STYLE_STACK_SIZE];
static int s_style_stack_depth = 0;

static CUI_StyleVarEntry s_var_stack[CUI_STYLE_STACK_SIZE];
static int s_var_stack_depth = 0;

static CUI_StyleColorEntry s_color_stack[CUI_STYLE_STACK_SIZE];
static int s_color_stack_depth = 0;

static CUI_Style s_current_style;
static bool s_current_style_initialized = false;

void cui_push_style(CUI_Context *ctx, const CUI_Style *style)
{
    if (!ctx || !style) return;
    if (s_style_stack_depth >= CUI_STYLE_STACK_SIZE) return;

    /* Initialize current style if needed */
    if (!s_current_style_initialized) {
        s_current_style = cui_style_from_theme(ctx);
        s_current_style_initialized = true;
    }

    /* Save current style */
    s_style_stack[s_style_stack_depth].style = s_current_style;
    s_style_stack[s_style_stack_depth].active = true;
    s_style_stack_depth++;

    /* Merge new style */
    cui_style_merge(&s_current_style, style);
}

void cui_pop_style(CUI_Context *ctx)
{
    (void)ctx;
    if (s_style_stack_depth <= 0) return;

    s_style_stack_depth--;
    s_current_style = s_style_stack[s_style_stack_depth].style;
}

static float *cui_style_get_var_ptr(CUI_Style *style, CUI_StyleVar var)
{
    switch (var) {
        case CUI_STYLEVAR_PADDING_TOP: return &style->padding.top;
        case CUI_STYLEVAR_PADDING_RIGHT: return &style->padding.right;
        case CUI_STYLEVAR_PADDING_BOTTOM: return &style->padding.bottom;
        case CUI_STYLEVAR_PADDING_LEFT: return &style->padding.left;
        case CUI_STYLEVAR_MARGIN_TOP: return &style->margin.top;
        case CUI_STYLEVAR_MARGIN_RIGHT: return &style->margin.right;
        case CUI_STYLEVAR_MARGIN_BOTTOM: return &style->margin.bottom;
        case CUI_STYLEVAR_MARGIN_LEFT: return &style->margin.left;
        case CUI_STYLEVAR_BORDER_WIDTH: return &style->border.width.top;
        case CUI_STYLEVAR_CORNER_RADIUS: return &style->corner_radius.top_left;
        case CUI_STYLEVAR_OPACITY: return &style->opacity;
        case CUI_STYLEVAR_FONT_SIZE: return &style->font_size;
        default: return NULL;
    }
}

void cui_push_style_var(CUI_Context *ctx, CUI_StyleVar var, float value)
{
    if (!ctx) return;
    if (s_var_stack_depth >= CUI_STYLE_STACK_SIZE) return;

    /* Initialize current style if needed */
    if (!s_current_style_initialized) {
        s_current_style = cui_style_from_theme(ctx);
        s_current_style_initialized = true;
    }

    float *ptr = cui_style_get_var_ptr(&s_current_style, var);
    if (!ptr) return;

    /* Save old value */
    s_var_stack[s_var_stack_depth].var = var;
    s_var_stack[s_var_stack_depth].old_value = *ptr;
    s_var_stack_depth++;

    /* Set new value */
    *ptr = value;

    /* For uniform properties, set all related values */
    if (var == CUI_STYLEVAR_BORDER_WIDTH) {
        s_current_style.border.width = cui_edges_uniform(value);
    } else if (var == CUI_STYLEVAR_CORNER_RADIUS) {
        s_current_style.corner_radius = cui_corners_uniform(value);
    }
}

void cui_pop_style_var(CUI_Context *ctx)
{
    (void)ctx;
    if (s_var_stack_depth <= 0) return;

    s_var_stack_depth--;
    CUI_StyleVarEntry *entry = &s_var_stack[s_var_stack_depth];

    float *ptr = cui_style_get_var_ptr(&s_current_style, entry->var);
    if (ptr) {
        *ptr = entry->old_value;
    }
}

static uint32_t *cui_style_get_color_ptr(CUI_Style *style, CUI_StyleColor color)
{
    switch (color) {
        case CUI_STYLECOLOR_BG: return &style->background.solid_color;
        case CUI_STYLECOLOR_BG_HOVER: return &style->background_hover.solid_color;
        case CUI_STYLECOLOR_BG_ACTIVE: return &style->background_active.solid_color;
        case CUI_STYLECOLOR_BORDER: return &style->border.color;
        case CUI_STYLECOLOR_TEXT: return &style->text_color;
        case CUI_STYLECOLOR_TEXT_HOVER: return &style->text_color_hover;
        default: return NULL;
    }
}

void cui_push_style_color(CUI_Context *ctx, CUI_StyleColor color, uint32_t value)
{
    if (!ctx) return;
    if (s_color_stack_depth >= CUI_STYLE_STACK_SIZE) return;

    /* Initialize current style if needed */
    if (!s_current_style_initialized) {
        s_current_style = cui_style_from_theme(ctx);
        s_current_style_initialized = true;
    }

    uint32_t *ptr = cui_style_get_color_ptr(&s_current_style, color);
    if (!ptr) return;

    /* Save old value */
    s_color_stack[s_color_stack_depth].color = color;
    s_color_stack[s_color_stack_depth].old_value = *ptr;
    s_color_stack_depth++;

    /* Set new value */
    *ptr = value;

    /* Ensure background type is solid when setting color */
    if (color == CUI_STYLECOLOR_BG) {
        s_current_style.background.type = CUI_BG_SOLID;
    } else if (color == CUI_STYLECOLOR_BG_HOVER) {
        s_current_style.background_hover.type = CUI_BG_SOLID;
    } else if (color == CUI_STYLECOLOR_BG_ACTIVE) {
        s_current_style.background_active.type = CUI_BG_SOLID;
    }
}

void cui_pop_style_color(CUI_Context *ctx)
{
    (void)ctx;
    if (s_color_stack_depth <= 0) return;

    s_color_stack_depth--;
    CUI_StyleColorEntry *entry = &s_color_stack[s_color_stack_depth];

    uint32_t *ptr = cui_style_get_color_ptr(&s_current_style, entry->color);
    if (ptr) {
        *ptr = entry->old_value;
    }
}

const CUI_Style *cui_get_current_style(const CUI_Context *ctx)
{
    if (!s_current_style_initialized && ctx) {
        s_current_style = cui_style_from_theme(ctx);
        s_current_style_initialized = true;
    }
    return &s_current_style;
}

/* ============================================================================
 * Style Class Registry
 * ============================================================================ */

#define CUI_MAX_STYLE_CLASSES 64

static CUI_StyleClass s_style_classes[CUI_MAX_STYLE_CLASSES];
static int s_style_class_count = 0;

bool cui_register_style_class(CUI_Context *ctx, const char *name,
                               const CUI_Style *style, const char *parent_name)
{
    (void)ctx;
    if (!name || !style) return false;
    if (s_style_class_count >= CUI_MAX_STYLE_CLASSES) return false;

    CUI_StyleClass *sc = &s_style_classes[s_style_class_count];
    strncpy(sc->name, name, CUI_STYLE_CLASS_NAME_MAX - 1);
    sc->name[CUI_STYLE_CLASS_NAME_MAX - 1] = '\0';
    sc->style = *style;
    sc->parent = NULL;

    /* Find parent if specified */
    if (parent_name) {
        for (int i = 0; i < s_style_class_count; i++) {
            if (strcmp(s_style_classes[i].name, parent_name) == 0) {
                sc->parent = &s_style_classes[i];
                break;
            }
        }
    }

    s_style_class_count++;
    return true;
}

CUI_StyleClass *cui_get_style_class(CUI_Context *ctx, const char *name)
{
    (void)ctx;
    if (!name) return NULL;

    for (int i = 0; i < s_style_class_count; i++) {
        if (strcmp(s_style_classes[i].name, name) == 0) {
            return &s_style_classes[i];
        }
    }
    return NULL;
}

CUI_Style cui_resolve_style_class(const CUI_StyleClass *style_class)
{
    CUI_Style result = cui_style_default();
    if (!style_class) return result;

    /* Build inheritance chain */
    const CUI_StyleClass *chain[16];
    int chain_len = 0;

    const CUI_StyleClass *current = style_class;
    while (current && chain_len < 16) {
        chain[chain_len++] = current;
        current = current->parent;
    }

    /* Apply from root to leaf */
    for (int i = chain_len - 1; i >= 0; i--) {
        cui_style_merge(&result, &chain[i]->style);
    }

    return result;
}

/* ============================================================================
 * Color Interpolation Helpers
 * ============================================================================ */

static inline uint8_t cui_lerp_u8(uint8_t a, uint8_t b, float t)
{
    return (uint8_t)(a + (b - a) * t);
}

static uint32_t cui_color_at_position(const CUI_Gradient *g, float pos)
{
    if (g->stop_count == 0) return 0xFFFFFFFF;
    if (g->stop_count == 1) return g->stops[0].color;

    /* Clamp position */
    if (pos <= g->stops[0].position) return g->stops[0].color;
    if (pos >= g->stops[g->stop_count - 1].position) return g->stops[g->stop_count - 1].color;

    /* Find surrounding stops */
    for (int i = 0; i < g->stop_count - 1; i++) {
        if (pos >= g->stops[i].position && pos <= g->stops[i + 1].position) {
            float range = g->stops[i + 1].position - g->stops[i].position;
            float t = (range > 0.0001f) ? (pos - g->stops[i].position) / range : 0.0f;

            uint32_t c1 = g->stops[i].color;
            uint32_t c2 = g->stops[i + 1].color;

            uint8_t r = cui_lerp_u8((c1 >> 0) & 0xFF, (c2 >> 0) & 0xFF, t);
            uint8_t g_ch = cui_lerp_u8((c1 >> 8) & 0xFF, (c2 >> 8) & 0xFF, t);
            uint8_t b = cui_lerp_u8((c1 >> 16) & 0xFF, (c2 >> 16) & 0xFF, t);
            uint8_t a = cui_lerp_u8((c1 >> 24) & 0xFF, (c2 >> 24) & 0xFF, t);

            return (a << 24) | (b << 16) | (g_ch << 8) | r;
        }
    }

    return g->stops[g->stop_count - 1].color;
}

/* ============================================================================
 * Styled Drawing Functions
 * ============================================================================ */

/* Helper: Apply opacity to a color */
static uint32_t cui_apply_opacity(uint32_t color, float opacity)
{
    if (opacity >= 1.0f) return color;
    if (opacity <= 0.0f) return color & 0x00FFFFFF;

    uint8_t a = (uint8_t)((color >> 24) & 0xFF);
    a = (uint8_t)(a * opacity);
    return (color & 0x00FFFFFF) | ((uint32_t)a << 24);
}

void cui_draw_gradient(CUI_Context *ctx, float x, float y, float w, float h,
                       const CUI_Gradient *gradient)
{
    if (!ctx || !gradient || w <= 0 || h <= 0) return;

    if (gradient->type == CUI_GRADIENT_LINEAR) {
        /* Linear gradient: draw as a series of vertical/horizontal strips */
        float angle_rad = gradient->angle * 3.14159265f / 180.0f;
        float cos_a = cosf(angle_rad);
        float sin_a = sinf(angle_rad);

        /* Number of strips for smooth gradient */
        int strips = (int)(fmaxf(w, h) / 2.0f);
        if (strips < 8) strips = 8;
        if (strips > 64) strips = 64;

        for (int i = 0; i < strips; i++) {
            float t0 = (float)i / strips;
            float t1 = (float)(i + 1) / strips;

            uint32_t c0 = cui_color_at_position(gradient, t0);
            uint32_t c1 = cui_color_at_position(gradient, t1);

            /* For horizontal gradient (angle = 0) */
            if (fabsf(cos_a) > fabsf(sin_a)) {
                float x0 = x + w * t0;
                float x1 = x + w * t1;
                /* Draw quad with gradient colors at vertices */
                /* Simplified: use average color per strip */
                uint32_t avg = cui_color_at_position(gradient, (t0 + t1) * 0.5f);
                cui_draw_rect(ctx, x0, y, x1 - x0, h, avg);
            } else {
                /* Vertical gradient */
                float y0 = y + h * t0;
                float y1 = y + h * t1;
                uint32_t avg = cui_color_at_position(gradient, (t0 + t1) * 0.5f);
                cui_draw_rect(ctx, x, y0, w, y1 - y0, avg);
            }
        }
    } else if (gradient->type == CUI_GRADIENT_RADIAL) {
        /* Radial gradient: draw as concentric circles */
        /* Simplified: draw as a series of rectangles (full implementation would need circles) */
        float cx = x + w * gradient->center_x;
        float cy = y + h * gradient->center_y;
        float max_r = fmaxf(w, h) * gradient->radius;

        int rings = (int)(max_r / 4.0f);
        if (rings < 8) rings = 8;
        if (rings > 32) rings = 32;

        for (int i = rings - 1; i >= 0; i--) {
            float t = (float)i / rings;
            float r = max_r * (1.0f - t);
            uint32_t color = cui_color_at_position(gradient, t);

            /* Draw approximate circle as rect (proper implementation would draw arcs) */
            float rx = cx - r;
            float ry = cy - r;
            float rw = r * 2;
            float rh = r * 2;

            /* Clip to bounds */
            if (rx < x) { rw -= (x - rx); rx = x; }
            if (ry < y) { rh -= (y - ry); ry = y; }
            if (rx + rw > x + w) rw = x + w - rx;
            if (ry + rh > y + h) rh = y + h - ry;

            if (rw > 0 && rh > 0) {
                cui_draw_rect(ctx, rx, ry, rw, rh, color);
            }
        }
    }
}

void cui_draw_shadow(CUI_Context *ctx, float x, float y, float w, float h,
                     const CUI_Shadow *shadow, CUI_CornerRadius corners)
{
    if (!ctx || !shadow) return;

    (void)corners; /* TODO: Use corners for shadow shape */

    if (shadow->inset) {
        /* Inner shadow: draw darkened edges inside the rect */
        float blur = shadow->blur_radius;
        uint32_t color = shadow->color;

        /* Top edge */
        if (shadow->offset_y > 0 || blur > 0) {
            float edge_h = blur + shadow->offset_y;
            if (edge_h > 0) {
                for (int i = 0; i < (int)edge_h; i++) {
                    float t = 1.0f - (float)i / edge_h;
                    uint32_t c = cui_apply_opacity(color, t * 0.5f);
                    cui_draw_rect(ctx, x, y + i, w, 1, c);
                }
            }
        }

        /* Left edge */
        if (shadow->offset_x > 0 || blur > 0) {
            float edge_w = blur + shadow->offset_x;
            if (edge_w > 0) {
                for (int i = 0; i < (int)edge_w; i++) {
                    float t = 1.0f - (float)i / edge_w;
                    uint32_t c = cui_apply_opacity(color, t * 0.5f);
                    cui_draw_rect(ctx, x + i, y, 1, h, c);
                }
            }
        }
    } else {
        /* Drop shadow: draw shadow behind the rect */
        float ox = shadow->offset_x;
        float oy = shadow->offset_y;
        float blur = shadow->blur_radius;
        float spread = shadow->spread;

        /* Expand shadow rect by spread */
        float sx = x + ox - spread;
        float sy = y + oy - spread;
        float sw = w + spread * 2;
        float sh = h + spread * 2;

        /* Draw shadow with blur (simplified: just fade opacity) */
        if (blur > 0) {
            int layers = (int)(blur / 2);
            if (layers < 2) layers = 2;
            if (layers > 8) layers = 8;

            for (int i = layers - 1; i >= 0; i--) {
                float t = (float)i / layers;
                float expand = blur * t;
                float alpha = (1.0f - t) * 0.3f;
                uint32_t c = cui_apply_opacity(shadow->color, alpha);
                cui_draw_rect(ctx, sx - expand, sy - expand,
                              sw + expand * 2, sh + expand * 2, c);
            }
        }

        /* Core shadow */
        cui_draw_rect(ctx, sx, sy, sw, sh, cui_apply_opacity(shadow->color, 0.4f));
    }
}

void cui_draw_nineslice(CUI_Context *ctx, float x, float y, float w, float h,
                        struct Carbon_Texture *texture,
                        float src_x, float src_y, float src_w, float src_h,
                        CUI_Edges margins)
{
    if (!ctx || !texture || w <= 0 || h <= 0) return;

    /* TODO: Implement proper 9-slice rendering when Carbon_Texture is available */
    /* For now, just draw a placeholder rect */
    (void)src_x; (void)src_y; (void)src_w; (void)src_h;
    (void)margins;

    /* Placeholder: draw a simple rect */
    cui_draw_rect(ctx, x, y, w, h, 0x80808080);
}

/* Draw a filled quarter circle using horizontal scanlines */
static void cui_draw_corner_filled(CUI_Context *ctx, float cx, float cy, float r,
                                    int quadrant, uint32_t color)
{
    if (r < 1.0f) return;

    /* Use triangle fan to fill the corner - more consistent than scanlines */
    int segments = 8;
    float pi_half = 3.14159265f * 0.5f;

    /* Starting angle for each quadrant */
    float start_angle;
    switch (quadrant) {
        case 0: start_angle = pi_half;          break; /* Top-left: 90° to 180° */
        case 1: start_angle = 0.0f;             break; /* Top-right: 0° to 90° */
        case 2: start_angle = -pi_half;         break; /* Bottom-right: -90° to 0° */
        case 3: start_angle = 3.14159265f;      break; /* Bottom-left: 180° to 270° */
        default: return;
    }

    for (int i = 0; i < segments; i++) {
        float a0 = start_angle + pi_half * i / segments;
        float a1 = start_angle + pi_half * (i + 1) / segments;

        float x0 = cx + cosf(a0) * r;
        float y0 = cy - sinf(a0) * r;
        float x1 = cx + cosf(a1) * r;
        float y1 = cy - sinf(a1) * r;

        /* Draw triangle from center to arc edge */
        cui_draw_triangle(ctx, cx, cy, x0, y0, x1, y1, color);
    }
}

void cui_draw_rect_rounded_ex(CUI_Context *ctx, float x, float y, float w, float h,
                               uint32_t color, CUI_CornerRadius corners)
{
    if (!ctx || w <= 0 || h <= 0) return;

    /* Check if all corners are zero or very small */
    float max_r = fmaxf(fmaxf(corners.top_left, corners.top_right),
                        fmaxf(corners.bottom_left, corners.bottom_right));

    if (max_r < 1.0f) {
        cui_draw_rect(ctx, x, y, w, h, color);
        return;
    }

    /* Clamp radii to half the smaller dimension */
    float half_min = fminf(w, h) * 0.5f;
    float tl = fminf(corners.top_left, half_min);
    float tr = fminf(corners.top_right, half_min);
    float br = fminf(corners.bottom_right, half_min);
    float bl = fminf(corners.bottom_left, half_min);

    /* Draw main body as 3 horizontal rectangles (cross pattern) */
    /* Middle horizontal band (full width, between top and bottom radii) */
    float mid_top = fmaxf(tl, tr);
    float mid_bot = fmaxf(bl, br);
    float mid_h = h - mid_top - mid_bot;
    if (mid_h > 0) {
        cui_draw_rect(ctx, x, y + mid_top, w, mid_h, color);
    }

    /* Top band (between left and right corners) */
    if (mid_top > 0) {
        float top_x = x + tl;
        float top_w = w - tl - tr;
        if (top_w > 0) {
            cui_draw_rect(ctx, top_x, y, top_w, mid_top, color);
        }
    }

    /* Bottom band (between left and right corners) */
    if (mid_bot > 0) {
        float bot_x = x + bl;
        float bot_w = w - bl - br;
        if (bot_w > 0) {
            cui_draw_rect(ctx, bot_x, y + h - mid_bot, bot_w, mid_bot, color);
        }
    }

    /* Draw corner fills */
    if (tl > 0) cui_draw_corner_filled(ctx, x + tl, y + tl, tl, 0, color);
    if (tr > 0) cui_draw_corner_filled(ctx, x + w - tr, y + tr, tr, 1, color);
    if (br > 0) cui_draw_corner_filled(ctx, x + w - br, y + h - br, br, 2, color);
    if (bl > 0) cui_draw_corner_filled(ctx, x + bl, y + h - bl, bl, 3, color);
}

void cui_draw_rect_rounded_outline(CUI_Context *ctx, float x, float y, float w, float h,
                                    uint32_t color, float thickness,
                                    CUI_CornerRadius corners)
{
    if (!ctx || w <= 0 || h <= 0 || thickness <= 0) return;

    float t = thickness;

    /* Check if corners are significant */
    float max_r = fmaxf(fmaxf(corners.top_left, corners.top_right),
                        fmaxf(corners.bottom_left, corners.bottom_right));

    if (max_r < 1.0f) {
        cui_draw_rect_outline(ctx, x, y, w, h, color, thickness);
        return;
    }

    /* Clamp radii to half the smaller dimension */
    float half_min = fminf(w, h) * 0.5f;
    float tl = fminf(corners.top_left, half_min);
    float tr = fminf(corners.top_right, half_min);
    float br = fminf(corners.bottom_right, half_min);
    float bl = fminf(corners.bottom_left, half_min);

    /* For rounded outline, we draw it as the difference between outer and inner rounded rects.
     * But for simplicity, we'll draw the outline using the straight edges and filled corner arcs
     * that create the visual appearance of a rounded outline. */

    /* Draw the outline by tracing the perimeter with filled shapes */

    /* Top edge */
    if (w - tl - tr > 0)
        cui_draw_rect(ctx, x + tl, y, w - tl - tr, t, color);

    /* Bottom edge */
    if (w - bl - br > 0)
        cui_draw_rect(ctx, x + bl, y + h - t, w - bl - br, t, color);

    /* Left edge */
    if (h - tl - bl > 0)
        cui_draw_rect(ctx, x, y + tl, t, h - tl - bl, color);

    /* Right edge */
    if (h - tr - br > 0)
        cui_draw_rect(ctx, x + w - t, y + tr, t, h - tr - br, color);

    /* Draw corner arcs using small filled segments */
    int segments = 8;
    float pi_half = 3.14159265f * 0.5f;

    /* Top-left corner */
    if (tl > 0) {
        float cx = x + tl, cy = y + tl;
        for (int i = 0; i < segments; i++) {
            float a0 = pi_half + pi_half * i / segments;
            float a1 = pi_half + pi_half * (i + 1) / segments;
            float ox0 = cosf(a0) * tl, oy0 = sinf(a0) * tl;
            float ox1 = cosf(a1) * tl, oy1 = sinf(a1) * tl;
            float ix0 = cosf(a0) * (tl - t), iy0 = sinf(a0) * (tl - t);
            float ix1 = cosf(a1) * (tl - t), iy1 = sinf(a1) * (tl - t);
            if (tl > t) {
                cui_draw_triangle(ctx, cx + ox0, cy - oy0, cx + ox1, cy - oy1, cx + ix1, cy - iy1, color);
                cui_draw_triangle(ctx, cx + ox0, cy - oy0, cx + ix1, cy - iy1, cx + ix0, cy - iy0, color);
            } else {
                cui_draw_triangle(ctx, cx + ox0, cy - oy0, cx + ox1, cy - oy1, cx, cy, color);
            }
        }
    }

    /* Top-right corner */
    if (tr > 0) {
        float cx = x + w - tr, cy = y + tr;
        for (int i = 0; i < segments; i++) {
            float a0 = pi_half * i / segments;
            float a1 = pi_half * (i + 1) / segments;
            float ox0 = cosf(a0) * tr, oy0 = sinf(a0) * tr;
            float ox1 = cosf(a1) * tr, oy1 = sinf(a1) * tr;
            float ix0 = cosf(a0) * (tr - t), iy0 = sinf(a0) * (tr - t);
            float ix1 = cosf(a1) * (tr - t), iy1 = sinf(a1) * (tr - t);
            if (tr > t) {
                cui_draw_triangle(ctx, cx + ox0, cy - oy0, cx + ox1, cy - oy1, cx + ix1, cy - iy1, color);
                cui_draw_triangle(ctx, cx + ox0, cy - oy0, cx + ix1, cy - iy1, cx + ix0, cy - iy0, color);
            } else {
                cui_draw_triangle(ctx, cx + ox0, cy - oy0, cx + ox1, cy - oy1, cx, cy, color);
            }
        }
    }

    /* Bottom-right corner */
    if (br > 0) {
        float cx = x + w - br, cy = y + h - br;
        for (int i = 0; i < segments; i++) {
            float a0 = pi_half * i / segments;
            float a1 = pi_half * (i + 1) / segments;
            float ox0 = cosf(a0) * br, oy0 = sinf(a0) * br;
            float ox1 = cosf(a1) * br, oy1 = sinf(a1) * br;
            float ix0 = cosf(a0) * (br - t), iy0 = sinf(a0) * (br - t);
            float ix1 = cosf(a1) * (br - t), iy1 = sinf(a1) * (br - t);
            if (br > t) {
                cui_draw_triangle(ctx, cx + ox0, cy + oy0, cx + ox1, cy + oy1, cx + ix1, cy + iy1, color);
                cui_draw_triangle(ctx, cx + ox0, cy + oy0, cx + ix1, cy + iy1, cx + ix0, cy + iy0, color);
            } else {
                cui_draw_triangle(ctx, cx + ox0, cy + oy0, cx + ox1, cy + oy1, cx, cy, color);
            }
        }
    }

    /* Bottom-left corner */
    if (bl > 0) {
        float cx = x + bl, cy = y + h - bl;
        for (int i = 0; i < segments; i++) {
            float a0 = pi_half + pi_half * i / segments;
            float a1 = pi_half + pi_half * (i + 1) / segments;
            float ox0 = cosf(a0) * bl, oy0 = sinf(a0) * bl;
            float ox1 = cosf(a1) * bl, oy1 = sinf(a1) * bl;
            float ix0 = cosf(a0) * (bl - t), iy0 = sinf(a0) * (bl - t);
            float ix1 = cosf(a1) * (bl - t), iy1 = sinf(a1) * (bl - t);
            if (bl > t) {
                cui_draw_triangle(ctx, cx + ox0, cy + oy0, cx + ox1, cy + oy1, cx + ix1, cy + iy1, color);
                cui_draw_triangle(ctx, cx + ox0, cy + oy0, cx + ix1, cy + iy1, cx + ix0, cy + iy0, color);
            } else {
                cui_draw_triangle(ctx, cx + ox0, cy + oy0, cx + ox1, cy + oy1, cx, cy, color);
            }
        }
    }
}

void cui_draw_styled_rect(CUI_Context *ctx, float x, float y, float w, float h,
                          const CUI_Style *style)
{
    if (!ctx || !style || w <= 0 || h <= 0) return;

    /* Apply margin */
    x += style->margin.left;
    y += style->margin.top;
    w -= style->margin.left + style->margin.right;
    h -= style->margin.top + style->margin.bottom;

    if (w <= 0 || h <= 0) return;

    /* Draw shadows first (behind everything) */
    for (int i = 0; i < style->shadow_count; i++) {
        if (!style->shadows[i].inset) {
            cui_draw_shadow(ctx, x, y, w, h, &style->shadows[i], style->corner_radius);
        }
    }

    /* Draw background */
    const CUI_Background *bg = &style->background;
    float opacity = style->opacity;

    switch (bg->type) {
        case CUI_BG_NONE:
            break;

        case CUI_BG_SOLID:
            cui_draw_rect_rounded_ex(ctx, x, y, w, h,
                                      cui_apply_opacity(bg->solid_color, opacity),
                                      style->corner_radius);
            break;

        case CUI_BG_GRADIENT:
            cui_draw_gradient(ctx, x, y, w, h, &bg->gradient);
            break;

        case CUI_BG_TEXTURE:
            /* TODO: Implement textured background */
            break;

        case CUI_BG_NINESLICE:
            cui_draw_nineslice(ctx, x, y, w, h,
                               bg->nineslice.texture,
                               bg->nineslice.src_x, bg->nineslice.src_y,
                               bg->nineslice.src_w, bg->nineslice.src_h,
                               bg->nineslice.margins);
            break;
    }

    /* Draw inset shadows (after background) */
    for (int i = 0; i < style->shadow_count; i++) {
        if (style->shadows[i].inset) {
            cui_draw_shadow(ctx, x, y, w, h, &style->shadows[i], style->corner_radius);
        }
    }

    /* Draw border */
    if (style->border.width.top > 0 || style->border.width.right > 0 ||
        style->border.width.bottom > 0 || style->border.width.left > 0) {

        uint32_t border_color = cui_apply_opacity(style->border.color, opacity);

        if (style->border.use_per_side_colors) {
            /* Per-side border colors */
            uint32_t colors[4];
            for (int i = 0; i < 4; i++) {
                colors[i] = cui_apply_opacity(style->border.colors[i], opacity);
            }

            /* Top */
            if (style->border.width.top > 0) {
                cui_draw_rect(ctx, x, y, w, style->border.width.top, colors[0]);
            }
            /* Right */
            if (style->border.width.right > 0) {
                cui_draw_rect(ctx, x + w - style->border.width.right, y,
                              style->border.width.right, h, colors[1]);
            }
            /* Bottom */
            if (style->border.width.bottom > 0) {
                cui_draw_rect(ctx, x, y + h - style->border.width.bottom,
                              w, style->border.width.bottom, colors[2]);
            }
            /* Left */
            if (style->border.width.left > 0) {
                cui_draw_rect(ctx, x, y, style->border.width.left, h, colors[3]);
            }
        } else {
            /* Uniform border */
            float avg_width = (style->border.width.top + style->border.width.right +
                               style->border.width.bottom + style->border.width.left) / 4.0f;
            cui_draw_rect_rounded_outline(ctx, x, y, w, h, border_color,
                                           avg_width, style->corner_radius);
        }
    }
}
