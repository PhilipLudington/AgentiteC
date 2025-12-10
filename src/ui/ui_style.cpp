/*
 * Agentite UI - Rich Styling System Implementation
 */

#include "agentite/ui_style.h"
#include "agentite/ui.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ============================================================================
 * Gradient Helper Functions
 * ============================================================================ */

AUI_Gradient aui_gradient_linear(float angle_degrees, uint32_t color1, uint32_t color2)
{
    AUI_Gradient g;
    memset(&g, 0, sizeof(g));
    g.type = AUI_GRADIENT_LINEAR;
    g.angle = angle_degrees;
    g.stop_count = 2;
    g.stops[0].position = 0.0f;
    g.stops[0].color = color1;
    g.stops[1].position = 1.0f;
    g.stops[1].color = color2;
    return g;
}

AUI_Gradient aui_gradient_linear_stops(float angle_degrees,
                                        const AUI_GradientStop *stops, int count)
{
    AUI_Gradient g;
    memset(&g, 0, sizeof(g));
    g.type = AUI_GRADIENT_LINEAR;
    g.angle = angle_degrees;
    g.stop_count = count < AUI_MAX_GRADIENT_STOPS ? count : AUI_MAX_GRADIENT_STOPS;
    for (int i = 0; i < g.stop_count; i++) {
        g.stops[i] = stops[i];
    }
    return g;
}

AUI_Gradient aui_gradient_radial(float center_x, float center_y, float radius,
                                  uint32_t inner_color, uint32_t outer_color)
{
    AUI_Gradient g;
    memset(&g, 0, sizeof(g));
    g.type = AUI_GRADIENT_RADIAL;
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

AUI_Style aui_style_default(void)
{
    AUI_Style style;
    memset(&style, 0, sizeof(style));
    style.opacity = 1.0f;
    style.background.type = AUI_BG_NONE;
    style.background_hover.type = AUI_BG_NONE;
    style.background_active.type = AUI_BG_NONE;
    style.background_disabled.type = AUI_BG_NONE;
    style.text_color = 0xFFFFFFFF;          /* White text by default */
    style.text_color_hover = 0xFFFFFFFF;
    style.text_color_disabled = 0x888888FF; /* Gray for disabled */
    style.text = aui_text_style_default();
    return style;
}

AUI_Style aui_style_from_theme(const AUI_Context *ctx)
{
    AUI_Style style = aui_style_default();
    if (!ctx) return style;

    const AUI_Theme *theme = aui_get_theme(ctx);

    /* Set colors from theme */
    style.background = aui_bg_solid(theme->bg_widget);
    style.background_hover = aui_bg_solid(theme->bg_widget_hover);
    style.background_active = aui_bg_solid(theme->bg_widget_active);
    style.background_disabled = aui_bg_solid(theme->bg_widget_disabled);

    style.border = aui_border(theme->border_width, theme->border);
    style.corner_radius = aui_corners_uniform(theme->corner_radius);

    style.text_color = theme->text;
    style.text_color_hover = theme->text_highlight;
    style.text_color_disabled = theme->text_disabled;

    style.padding = aui_edges_uniform(theme->padding);

    return style;
}

void aui_style_merge(AUI_Style *dst, const AUI_Style *src)
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
    if (src->background.type != AUI_BG_NONE) dst->background = src->background;
    if (src->background_hover.type != AUI_BG_NONE) dst->background_hover = src->background_hover;
    if (src->background_active.type != AUI_BG_NONE) dst->background_active = src->background_active;
    if (src->background_disabled.type != AUI_BG_NONE) dst->background_disabled = src->background_disabled;

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

    /* Merge text style */
    if (src->text.align != AUI_TEXT_ALIGN_LEFT) dst->text.align = src->text.align;
    if (src->text.valign != AUI_TEXT_VALIGN_MIDDLE) dst->text.valign = src->text.valign;
    if (src->text.overflow != AUI_TEXT_OVERFLOW_VISIBLE) dst->text.overflow = src->text.overflow;
    if (src->text.line_height != 1.0f && src->text.line_height > 0) dst->text.line_height = src->text.line_height;
    if (src->text.letter_spacing != 0) dst->text.letter_spacing = src->text.letter_spacing;
    if (src->text.word_spacing != 0) dst->text.word_spacing = src->text.word_spacing;
    if (src->text.shadow.enabled) dst->text.shadow = src->text.shadow;
    if (src->text.wrap) dst->text.wrap = src->text.wrap;
    if (src->text.max_lines > 0) dst->text.max_lines = src->text.max_lines;
}

/* ============================================================================
 * Style Stack Implementation
 *
 * The style stack is stored in an extended AUI_Context.
 * For now, we'll use a simple approach with function-local static storage
 * until we extend AUI_Context properly.
 * ============================================================================ */

#define AUI_STYLE_STACK_SIZE 32

typedef struct AUI_StyleStackEntry {
    AUI_Style style;
    bool active;
} AUI_StyleStackEntry;

typedef struct AUI_StyleVarEntry {
    AUI_StyleVar var;
    float old_value;
} AUI_StyleVarEntry;

typedef struct AUI_StyleColorEntry {
    AUI_StyleColor color;
    uint32_t old_value;
} AUI_StyleColorEntry;

/* Static storage for style stack (will be moved to AUI_Context later) */
static AUI_StyleStackEntry s_style_stack[AUI_STYLE_STACK_SIZE];
static int s_style_stack_depth = 0;

static AUI_StyleVarEntry s_var_stack[AUI_STYLE_STACK_SIZE];
static int s_var_stack_depth = 0;

static AUI_StyleColorEntry s_color_stack[AUI_STYLE_STACK_SIZE];
static int s_color_stack_depth = 0;

static AUI_Style s_current_style;
static bool s_current_style_initialized = false;

void aui_push_style(AUI_Context *ctx, const AUI_Style *style)
{
    if (!ctx || !style) return;
    if (s_style_stack_depth >= AUI_STYLE_STACK_SIZE) return;

    /* Initialize current style if needed */
    if (!s_current_style_initialized) {
        s_current_style = aui_style_from_theme(ctx);
        s_current_style_initialized = true;
    }

    /* Save current style */
    s_style_stack[s_style_stack_depth].style = s_current_style;
    s_style_stack[s_style_stack_depth].active = true;
    s_style_stack_depth++;

    /* Merge new style */
    aui_style_merge(&s_current_style, style);
}

void aui_pop_style(AUI_Context *ctx)
{
    (void)ctx;
    if (s_style_stack_depth <= 0) return;

    s_style_stack_depth--;
    s_current_style = s_style_stack[s_style_stack_depth].style;
}

static float *aui_style_get_var_ptr(AUI_Style *style, AUI_StyleVar var)
{
    switch (var) {
        case AUI_STYLEVAR_PADDING_TOP: return &style->padding.top;
        case AUI_STYLEVAR_PADDING_RIGHT: return &style->padding.right;
        case AUI_STYLEVAR_PADDING_BOTTOM: return &style->padding.bottom;
        case AUI_STYLEVAR_PADDING_LEFT: return &style->padding.left;
        case AUI_STYLEVAR_MARGIN_TOP: return &style->margin.top;
        case AUI_STYLEVAR_MARGIN_RIGHT: return &style->margin.right;
        case AUI_STYLEVAR_MARGIN_BOTTOM: return &style->margin.bottom;
        case AUI_STYLEVAR_MARGIN_LEFT: return &style->margin.left;
        case AUI_STYLEVAR_BORDER_WIDTH: return &style->border.width.top;
        case AUI_STYLEVAR_CORNER_RADIUS: return &style->corner_radius.top_left;
        case AUI_STYLEVAR_OPACITY: return &style->opacity;
        case AUI_STYLEVAR_FONT_SIZE: return &style->font_size;
        default: return NULL;
    }
}

void aui_push_style_var(AUI_Context *ctx, AUI_StyleVar var, float value)
{
    if (!ctx) return;
    if (s_var_stack_depth >= AUI_STYLE_STACK_SIZE) return;

    /* Initialize current style if needed */
    if (!s_current_style_initialized) {
        s_current_style = aui_style_from_theme(ctx);
        s_current_style_initialized = true;
    }

    float *ptr = aui_style_get_var_ptr(&s_current_style, var);
    if (!ptr) return;

    /* Save old value */
    s_var_stack[s_var_stack_depth].var = var;
    s_var_stack[s_var_stack_depth].old_value = *ptr;
    s_var_stack_depth++;

    /* Set new value */
    *ptr = value;

    /* For uniform properties, set all related values */
    if (var == AUI_STYLEVAR_BORDER_WIDTH) {
        s_current_style.border.width = aui_edges_uniform(value);
    } else if (var == AUI_STYLEVAR_CORNER_RADIUS) {
        s_current_style.corner_radius = aui_corners_uniform(value);
    }
}

void aui_pop_style_var(AUI_Context *ctx)
{
    (void)ctx;
    if (s_var_stack_depth <= 0) return;

    s_var_stack_depth--;
    AUI_StyleVarEntry *entry = &s_var_stack[s_var_stack_depth];

    float *ptr = aui_style_get_var_ptr(&s_current_style, entry->var);
    if (ptr) {
        *ptr = entry->old_value;
    }
}

static uint32_t *aui_style_get_color_ptr(AUI_Style *style, AUI_StyleColor color)
{
    switch (color) {
        case AUI_STYLECOLOR_BG: return &style->background.solid_color;
        case AUI_STYLECOLOR_BG_HOVER: return &style->background_hover.solid_color;
        case AUI_STYLECOLOR_BG_ACTIVE: return &style->background_active.solid_color;
        case AUI_STYLECOLOR_BORDER: return &style->border.color;
        case AUI_STYLECOLOR_TEXT: return &style->text_color;
        case AUI_STYLECOLOR_TEXT_HOVER: return &style->text_color_hover;
        default: return NULL;
    }
}

void aui_push_style_color(AUI_Context *ctx, AUI_StyleColor color, uint32_t value)
{
    if (!ctx) return;
    if (s_color_stack_depth >= AUI_STYLE_STACK_SIZE) return;

    /* Initialize current style if needed */
    if (!s_current_style_initialized) {
        s_current_style = aui_style_from_theme(ctx);
        s_current_style_initialized = true;
    }

    uint32_t *ptr = aui_style_get_color_ptr(&s_current_style, color);
    if (!ptr) return;

    /* Save old value */
    s_color_stack[s_color_stack_depth].color = color;
    s_color_stack[s_color_stack_depth].old_value = *ptr;
    s_color_stack_depth++;

    /* Set new value */
    *ptr = value;

    /* Ensure background type is solid when setting color */
    if (color == AUI_STYLECOLOR_BG) {
        s_current_style.background.type = AUI_BG_SOLID;
    } else if (color == AUI_STYLECOLOR_BG_HOVER) {
        s_current_style.background_hover.type = AUI_BG_SOLID;
    } else if (color == AUI_STYLECOLOR_BG_ACTIVE) {
        s_current_style.background_active.type = AUI_BG_SOLID;
    }
}

void aui_pop_style_color(AUI_Context *ctx)
{
    (void)ctx;
    if (s_color_stack_depth <= 0) return;

    s_color_stack_depth--;
    AUI_StyleColorEntry *entry = &s_color_stack[s_color_stack_depth];

    uint32_t *ptr = aui_style_get_color_ptr(&s_current_style, entry->color);
    if (ptr) {
        *ptr = entry->old_value;
    }
}

const AUI_Style *aui_get_current_style(const AUI_Context *ctx)
{
    if (!s_current_style_initialized && ctx) {
        s_current_style = aui_style_from_theme(ctx);
        s_current_style_initialized = true;
    }
    return &s_current_style;
}

/* ============================================================================
 * Style Class Registry
 * ============================================================================ */

#define AUI_MAX_STYLE_CLASSES 64

static AUI_StyleClass s_style_classes[AUI_MAX_STYLE_CLASSES];
static int s_style_class_count = 0;

bool aui_register_style_class(AUI_Context *ctx, const char *name,
                               const AUI_Style *style, const char *parent_name)
{
    (void)ctx;
    if (!name || !style) return false;
    if (s_style_class_count >= AUI_MAX_STYLE_CLASSES) return false;

    AUI_StyleClass *sc = &s_style_classes[s_style_class_count];
    strncpy(sc->name, name, AUI_STYLE_CLASS_NAME_MAX - 1);
    sc->name[AUI_STYLE_CLASS_NAME_MAX - 1] = '\0';
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

AUI_StyleClass *aui_get_style_class(AUI_Context *ctx, const char *name)
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

AUI_Style aui_resolve_style_class(const AUI_StyleClass *style_class)
{
    AUI_Style result = aui_style_default();
    if (!style_class) return result;

    /* Build inheritance chain */
    const AUI_StyleClass *chain[16];
    int chain_len = 0;

    const AUI_StyleClass *current = style_class;
    while (current && chain_len < 16) {
        chain[chain_len++] = current;
        current = current->parent;
    }

    /* Apply from root to leaf */
    for (int i = chain_len - 1; i >= 0; i--) {
        aui_style_merge(&result, &chain[i]->style);
    }

    return result;
}

/* ============================================================================
 * Color Interpolation Helpers
 * ============================================================================ */

static inline uint8_t aui_lerp_u8(uint8_t a, uint8_t b, float t)
{
    return (uint8_t)(a + (b - a) * t);
}

static uint32_t aui_color_at_position(const AUI_Gradient *g, float pos)
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

            uint8_t r = aui_lerp_u8((c1 >> 0) & 0xFF, (c2 >> 0) & 0xFF, t);
            uint8_t g_ch = aui_lerp_u8((c1 >> 8) & 0xFF, (c2 >> 8) & 0xFF, t);
            uint8_t b = aui_lerp_u8((c1 >> 16) & 0xFF, (c2 >> 16) & 0xFF, t);
            uint8_t a = aui_lerp_u8((c1 >> 24) & 0xFF, (c2 >> 24) & 0xFF, t);

            return (a << 24) | (b << 16) | (g_ch << 8) | r;
        }
    }

    return g->stops[g->stop_count - 1].color;
}

/* ============================================================================
 * Styled Drawing Functions
 * ============================================================================ */

/* Helper: Apply opacity to a color */
static uint32_t aui_apply_opacity(uint32_t color, float opacity)
{
    if (opacity >= 1.0f) return color;
    if (opacity <= 0.0f) return color & 0x00FFFFFF;

    uint8_t a = (uint8_t)((color >> 24) & 0xFF);
    a = (uint8_t)(a * opacity);
    return (color & 0x00FFFFFF) | ((uint32_t)a << 24);
}

void aui_draw_gradient(AUI_Context *ctx, float x, float y, float w, float h,
                       const AUI_Gradient *gradient)
{
    if (!ctx || !gradient || w <= 0 || h <= 0) return;

    if (gradient->type == AUI_GRADIENT_LINEAR) {
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

            uint32_t c0 = aui_color_at_position(gradient, t0);
            uint32_t c1 = aui_color_at_position(gradient, t1);

            /* For horizontal gradient (angle = 0) */
            if (fabsf(cos_a) > fabsf(sin_a)) {
                float x0 = x + w * t0;
                float x1 = x + w * t1;
                /* Draw quad with gradient colors at vertices */
                /* Simplified: use average color per strip */
                uint32_t avg = aui_color_at_position(gradient, (t0 + t1) * 0.5f);
                aui_draw_rect(ctx, x0, y, x1 - x0, h, avg);
            } else {
                /* Vertical gradient */
                float y0 = y + h * t0;
                float y1 = y + h * t1;
                uint32_t avg = aui_color_at_position(gradient, (t0 + t1) * 0.5f);
                aui_draw_rect(ctx, x, y0, w, y1 - y0, avg);
            }
        }
    } else if (gradient->type == AUI_GRADIENT_RADIAL) {
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
            uint32_t color = aui_color_at_position(gradient, t);

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
                aui_draw_rect(ctx, rx, ry, rw, rh, color);
            }
        }
    }
}

void aui_draw_shadow(AUI_Context *ctx, float x, float y, float w, float h,
                     const AUI_Shadow *shadow, AUI_CornerRadius corners)
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
                    uint32_t c = aui_apply_opacity(color, t * 0.5f);
                    aui_draw_rect(ctx, x, y + i, w, 1, c);
                }
            }
        }

        /* Left edge */
        if (shadow->offset_x > 0 || blur > 0) {
            float edge_w = blur + shadow->offset_x;
            if (edge_w > 0) {
                for (int i = 0; i < (int)edge_w; i++) {
                    float t = 1.0f - (float)i / edge_w;
                    uint32_t c = aui_apply_opacity(color, t * 0.5f);
                    aui_draw_rect(ctx, x + i, y, 1, h, c);
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
                uint32_t c = aui_apply_opacity(shadow->color, alpha);
                aui_draw_rect(ctx, sx - expand, sy - expand,
                              sw + expand * 2, sh + expand * 2, c);
            }
        }

        /* Core shadow */
        aui_draw_rect(ctx, sx, sy, sw, sh, aui_apply_opacity(shadow->color, 0.4f));
    }
}

void aui_draw_nineslice(AUI_Context *ctx, float x, float y, float w, float h,
                        struct Agentite_Texture *texture,
                        float src_x, float src_y, float src_w, float src_h,
                        AUI_Edges margins)
{
    if (!ctx || !texture || w <= 0 || h <= 0) return;

    /* TODO: Implement proper 9-slice rendering when Agentite_Texture is available */
    /* For now, just draw a placeholder rect */
    (void)src_x; (void)src_y; (void)src_w; (void)src_h;
    (void)margins;

    /* Placeholder: draw a simple rect */
    aui_draw_rect(ctx, x, y, w, h, 0x80808080);
}

/* Draw a filled quarter circle using horizontal scanlines */
static void aui_draw_corner_filled(AUI_Context *ctx, float cx, float cy, float r,
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
        aui_draw_triangle(ctx, cx, cy, x0, y0, x1, y1, color);
    }
}

void aui_draw_rect_rounded_ex(AUI_Context *ctx, float x, float y, float w, float h,
                               uint32_t color, AUI_CornerRadius corners)
{
    if (!ctx || w <= 0 || h <= 0) return;

    /* Check if all corners are zero or very small */
    float max_r = fmaxf(fmaxf(corners.top_left, corners.top_right),
                        fmaxf(corners.bottom_left, corners.bottom_right));

    if (max_r < 1.0f) {
        aui_draw_rect(ctx, x, y, w, h, color);
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
        aui_draw_rect(ctx, x, y + mid_top, w, mid_h, color);
    }

    /* Top band (between left and right corners) */
    if (mid_top > 0) {
        float top_x = x + tl;
        float top_w = w - tl - tr;
        if (top_w > 0) {
            aui_draw_rect(ctx, top_x, y, top_w, mid_top, color);
        }
    }

    /* Bottom band (between left and right corners) */
    if (mid_bot > 0) {
        float bot_x = x + bl;
        float bot_w = w - bl - br;
        if (bot_w > 0) {
            aui_draw_rect(ctx, bot_x, y + h - mid_bot, bot_w, mid_bot, color);
        }
    }

    /* Draw corner fills */
    if (tl > 0) aui_draw_corner_filled(ctx, x + tl, y + tl, tl, 0, color);
    if (tr > 0) aui_draw_corner_filled(ctx, x + w - tr, y + tr, tr, 1, color);
    if (br > 0) aui_draw_corner_filled(ctx, x + w - br, y + h - br, br, 2, color);
    if (bl > 0) aui_draw_corner_filled(ctx, x + bl, y + h - bl, bl, 3, color);
}

void aui_draw_rect_rounded_outline(AUI_Context *ctx, float x, float y, float w, float h,
                                    uint32_t color, float thickness,
                                    AUI_CornerRadius corners)
{
    if (!ctx || w <= 0 || h <= 0 || thickness <= 0) return;

    float t = thickness;

    /* Check if corners are significant */
    float max_r = fmaxf(fmaxf(corners.top_left, corners.top_right),
                        fmaxf(corners.bottom_left, corners.bottom_right));

    if (max_r < 1.0f) {
        aui_draw_rect_outline(ctx, x, y, w, h, color, thickness);
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
        aui_draw_rect(ctx, x + tl, y, w - tl - tr, t, color);

    /* Bottom edge */
    if (w - bl - br > 0)
        aui_draw_rect(ctx, x + bl, y + h - t, w - bl - br, t, color);

    /* Left edge */
    if (h - tl - bl > 0)
        aui_draw_rect(ctx, x, y + tl, t, h - tl - bl, color);

    /* Right edge */
    if (h - tr - br > 0)
        aui_draw_rect(ctx, x + w - t, y + tr, t, h - tr - br, color);

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
                aui_draw_triangle(ctx, cx + ox0, cy - oy0, cx + ox1, cy - oy1, cx + ix1, cy - iy1, color);
                aui_draw_triangle(ctx, cx + ox0, cy - oy0, cx + ix1, cy - iy1, cx + ix0, cy - iy0, color);
            } else {
                aui_draw_triangle(ctx, cx + ox0, cy - oy0, cx + ox1, cy - oy1, cx, cy, color);
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
                aui_draw_triangle(ctx, cx + ox0, cy - oy0, cx + ox1, cy - oy1, cx + ix1, cy - iy1, color);
                aui_draw_triangle(ctx, cx + ox0, cy - oy0, cx + ix1, cy - iy1, cx + ix0, cy - iy0, color);
            } else {
                aui_draw_triangle(ctx, cx + ox0, cy - oy0, cx + ox1, cy - oy1, cx, cy, color);
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
                aui_draw_triangle(ctx, cx + ox0, cy + oy0, cx + ox1, cy + oy1, cx + ix1, cy + iy1, color);
                aui_draw_triangle(ctx, cx + ox0, cy + oy0, cx + ix1, cy + iy1, cx + ix0, cy + iy0, color);
            } else {
                aui_draw_triangle(ctx, cx + ox0, cy + oy0, cx + ox1, cy + oy1, cx, cy, color);
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
                aui_draw_triangle(ctx, cx + ox0, cy + oy0, cx + ox1, cy + oy1, cx + ix1, cy + iy1, color);
                aui_draw_triangle(ctx, cx + ox0, cy + oy0, cx + ix1, cy + iy1, cx + ix0, cy + iy0, color);
            } else {
                aui_draw_triangle(ctx, cx + ox0, cy + oy0, cx + ox1, cy + oy1, cx, cy, color);
            }
        }
    }
}

void aui_draw_styled_rect(AUI_Context *ctx, float x, float y, float w, float h,
                          const AUI_Style *style)
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
            aui_draw_shadow(ctx, x, y, w, h, &style->shadows[i], style->corner_radius);
        }
    }

    /* Draw background */
    const AUI_Background *bg = &style->background;
    float opacity = style->opacity;

    switch (bg->type) {
        case AUI_BG_NONE:
            break;

        case AUI_BG_SOLID:
            aui_draw_rect_rounded_ex(ctx, x, y, w, h,
                                      aui_apply_opacity(bg->solid_color, opacity),
                                      style->corner_radius);
            break;

        case AUI_BG_GRADIENT:
            aui_draw_gradient(ctx, x, y, w, h, &bg->gradient);
            break;

        case AUI_BG_TEXTURE:
            /* TODO: Implement textured background */
            break;

        case AUI_BG_NINESLICE:
            aui_draw_nineslice(ctx, x, y, w, h,
                               bg->nineslice.texture,
                               bg->nineslice.src_x, bg->nineslice.src_y,
                               bg->nineslice.src_w, bg->nineslice.src_h,
                               bg->nineslice.margins);
            break;
    }

    /* Draw inset shadows (after background) */
    for (int i = 0; i < style->shadow_count; i++) {
        if (style->shadows[i].inset) {
            aui_draw_shadow(ctx, x, y, w, h, &style->shadows[i], style->corner_radius);
        }
    }

    /* Draw border */
    if (style->border.width.top > 0 || style->border.width.right > 0 ||
        style->border.width.bottom > 0 || style->border.width.left > 0) {

        uint32_t border_color = aui_apply_opacity(style->border.color, opacity);

        if (style->border.use_per_side_colors) {
            /* Per-side border colors */
            uint32_t colors[4];
            for (int i = 0; i < 4; i++) {
                colors[i] = aui_apply_opacity(style->border.colors[i], opacity);
            }

            /* Top */
            if (style->border.width.top > 0) {
                aui_draw_rect(ctx, x, y, w, style->border.width.top, colors[0]);
            }
            /* Right */
            if (style->border.width.right > 0) {
                aui_draw_rect(ctx, x + w - style->border.width.right, y,
                              style->border.width.right, h, colors[1]);
            }
            /* Bottom */
            if (style->border.width.bottom > 0) {
                aui_draw_rect(ctx, x, y + h - style->border.width.bottom,
                              w, style->border.width.bottom, colors[2]);
            }
            /* Left */
            if (style->border.width.left > 0) {
                aui_draw_rect(ctx, x, y, style->border.width.left, h, colors[3]);
            }
        } else {
            /* Uniform border */
            float avg_width = (style->border.width.top + style->border.width.right +
                               style->border.width.bottom + style->border.width.left) / 4.0f;
            aui_draw_rect_rounded_outline(ctx, x, y, w, h, border_color,
                                           avg_width, style->corner_radius);
        }
    }
}

/* ============================================================================
 * Styled Text Drawing Functions
 * ============================================================================ */

/* Static buffer for ellipsis truncation */
static char s_ellipsis_buffer[1024];

const char *aui_truncate_text_ellipsis(AUI_Context *ctx, const char *text,
                                        float max_width)
{
    if (!ctx || !text || max_width <= 0) return text;

    float text_w = aui_text_width(ctx, text);
    if (text_w <= max_width) return text;

    /* Binary search for the right truncation point */
    size_t len = strlen(text);
    if (len == 0) return text;

    /* Measure ellipsis width */
    float ellipsis_w = aui_text_width(ctx, "...");
    float available = max_width - ellipsis_w;
    if (available <= 0) {
        s_ellipsis_buffer[0] = '.';
        s_ellipsis_buffer[1] = '.';
        s_ellipsis_buffer[2] = '.';
        s_ellipsis_buffer[3] = '\0';
        return s_ellipsis_buffer;
    }

    /* Find how many characters fit */
    size_t fit = 0;
    for (size_t i = 1; i <= len && i < sizeof(s_ellipsis_buffer) - 4; i++) {
        strncpy(s_ellipsis_buffer, text, i);
        s_ellipsis_buffer[i] = '\0';
        if (aui_text_width(ctx, s_ellipsis_buffer) > available) {
            break;
        }
        fit = i;
    }

    /* Build truncated string */
    if (fit > 0) {
        strncpy(s_ellipsis_buffer, text, fit);
        s_ellipsis_buffer[fit] = '\0';
        strcat(s_ellipsis_buffer, "...");
    } else {
        strcpy(s_ellipsis_buffer, "...");
    }

    return s_ellipsis_buffer;
}

/* Word wrap helper - splits text into lines that fit within max_width */
typedef struct {
    const char *start;
    size_t length;
} AUI_TextLine;

#define AUI_MAX_WRAP_LINES 64

static int aui_wrap_text(AUI_Context *ctx, const char *text, float max_width,
                          float letter_spacing, float word_spacing,
                          AUI_TextLine *lines, int max_lines_count)
{
    if (!ctx || !text || !lines || max_width <= 0) return 0;

    int line_count = 0;
    const char *line_start = text;
    const char *word_start = text;
    const char *p = text;

    /* Temporary buffer for measuring */
    char measure_buf[512];

    while (*p && line_count < max_lines_count) {
        /* Find next word boundary */
        while (*p && *p != ' ' && *p != '\n') p++;

        /* Measure line up to current word */
        size_t line_len = p - line_start;
        if (line_len >= sizeof(measure_buf)) line_len = sizeof(measure_buf) - 1;
        strncpy(measure_buf, line_start, line_len);
        measure_buf[line_len] = '\0';

        /* Account for letter spacing (approximate) */
        float line_w = aui_text_width(ctx, measure_buf);
        if (letter_spacing != 0 && line_len > 1) {
            line_w += letter_spacing * (line_len - 1);
        }

        /* Check if line fits */
        if (line_w > max_width && word_start > line_start) {
            /* Line too long, break at previous word */
            lines[line_count].start = line_start;
            lines[line_count].length = word_start - line_start;
            if (lines[line_count].length > 0 &&
                lines[line_count].start[lines[line_count].length - 1] == ' ') {
                lines[line_count].length--;  /* Trim trailing space */
            }
            line_count++;
            line_start = word_start;
            /* Skip leading space */
            while (*line_start == ' ') line_start++;
            word_start = line_start;
        }

        /* Handle explicit newline */
        if (*p == '\n') {
            lines[line_count].start = line_start;
            lines[line_count].length = p - line_start;
            line_count++;
            p++;
            line_start = p;
            word_start = p;
            continue;
        }

        /* Move past spaces */
        if (*p == ' ') {
            p++;
            word_start = p;
        }
    }

    /* Add final line if there's remaining text */
    if (line_start < p && line_count < max_lines_count) {
        lines[line_count].start = line_start;
        lines[line_count].length = p - line_start;
        line_count++;
    }

    return line_count;
}

float aui_measure_styled_text(AUI_Context *ctx, const char *text,
                              float max_width, const AUI_TextStyle *style,
                              float *out_height)
{
    if (!ctx || !text) {
        if (out_height) *out_height = 0;
        return 0;
    }

    AUI_TextStyle default_style = aui_text_style_default();
    if (!style) style = &default_style;

    float base_height = aui_text_height(ctx);
    float line_h = base_height * style->line_height;

    /* Simple case: no wrapping */
    if (!style->wrap && style->overflow != AUI_TEXT_OVERFLOW_WRAP) {
        float w = aui_text_width(ctx, text);
        /* Add letter spacing */
        if (style->letter_spacing != 0) {
            size_t len = strlen(text);
            if (len > 1) w += style->letter_spacing * (len - 1);
        }
        if (out_height) *out_height = line_h;
        return w;
    }

    /* Wrapping case */
    AUI_TextLine lines[AUI_MAX_WRAP_LINES];
    int line_count = aui_wrap_text(ctx, text, max_width,
                                    style->letter_spacing, style->word_spacing,
                                    lines, AUI_MAX_WRAP_LINES);

    if (style->max_lines > 0 && line_count > style->max_lines) {
        line_count = style->max_lines;
    }

    /* Find max width among lines */
    float max_w = 0;
    char line_buf[512];
    for (int i = 0; i < line_count; i++) {
        size_t len = lines[i].length;
        if (len >= sizeof(line_buf)) len = sizeof(line_buf) - 1;
        strncpy(line_buf, lines[i].start, len);
        line_buf[len] = '\0';
        float w = aui_text_width(ctx, line_buf);
        if (w > max_w) max_w = w;
    }

    if (out_height) *out_height = line_count * line_h;
    return max_w;
}

float aui_draw_styled_text(AUI_Context *ctx, const char *text,
                           float x, float y, float max_width, float max_height,
                           uint32_t color, const AUI_TextStyle *style)
{
    if (!ctx || !text || !text[0]) return 0;

    AUI_TextStyle default_style = aui_text_style_default();
    if (!style) style = &default_style;

    float base_height = aui_text_height(ctx);
    float line_h = base_height * style->line_height;

    /* Handle overflow modes */
    bool should_wrap = style->wrap || style->overflow == AUI_TEXT_OVERFLOW_WRAP;
    bool should_clip = style->overflow == AUI_TEXT_OVERFLOW_CLIP;
    bool should_ellipsis = style->overflow == AUI_TEXT_OVERFLOW_ELLIPSIS;

    /* Build list of lines */
    AUI_TextLine lines[AUI_MAX_WRAP_LINES];
    int line_count = 1;
    char single_line_buf[512];

    if (should_wrap && max_width > 0) {
        line_count = aui_wrap_text(ctx, text, max_width,
                                    style->letter_spacing, style->word_spacing,
                                    lines, AUI_MAX_WRAP_LINES);
    } else {
        /* Single line */
        lines[0].start = text;
        lines[0].length = strlen(text);
    }

    /* Apply max_lines limit */
    if (style->max_lines > 0 && line_count > style->max_lines) {
        line_count = style->max_lines;
    }

    /* Calculate total text height for vertical alignment */
    float total_height = line_count * line_h;

    /* Vertical alignment offset */
    float y_offset = 0;
    if (max_height > 0) {
        switch (style->valign) {
            case AUI_TEXT_VALIGN_TOP:
                y_offset = 0;
                break;
            case AUI_TEXT_VALIGN_MIDDLE:
                y_offset = (max_height - total_height) / 2;
                break;
            case AUI_TEXT_VALIGN_BOTTOM:
                y_offset = max_height - total_height;
                break;
        }
        if (y_offset < 0) y_offset = 0;
    }

    /* Set up clipping if needed */
    bool pushed_scissor = false;
    if (should_clip && max_width > 0 && max_height > 0) {
        aui_push_scissor(ctx, x, y, max_width, max_height);
        pushed_scissor = true;
    }

    /* Draw each line */
    float current_y = y + y_offset;
    char line_buf[512];

    for (int i = 0; i < line_count; i++) {
        /* Copy line to buffer */
        size_t len = lines[i].length;
        if (len >= sizeof(line_buf)) len = sizeof(line_buf) - 1;
        strncpy(line_buf, lines[i].start, len);
        line_buf[len] = '\0';

        /* Handle ellipsis for last line if we hit max_lines */
        if (should_ellipsis && max_width > 0) {
            float line_w = aui_text_width(ctx, line_buf);
            if (line_w > max_width) {
                const char *truncated = aui_truncate_text_ellipsis(ctx, line_buf, max_width);
                strncpy(line_buf, truncated, sizeof(line_buf) - 1);
                line_buf[sizeof(line_buf) - 1] = '\0';
            }
        }

        /* Measure line for horizontal alignment */
        float line_w = aui_text_width(ctx, line_buf);
        if (style->letter_spacing != 0 && len > 1) {
            line_w += style->letter_spacing * (len - 1);
        }

        /* Horizontal alignment offset */
        float x_offset = 0;
        if (max_width > 0) {
            switch (style->align) {
                case AUI_TEXT_ALIGN_LEFT:
                    x_offset = 0;
                    break;
                case AUI_TEXT_ALIGN_CENTER:
                    x_offset = (max_width - line_w) / 2;
                    break;
                case AUI_TEXT_ALIGN_RIGHT:
                    x_offset = max_width - line_w;
                    break;
                case AUI_TEXT_ALIGN_JUSTIFY:
                    /* TODO: Implement justify by adjusting word_spacing */
                    x_offset = 0;
                    break;
            }
            if (x_offset < 0) x_offset = 0;
        }

        float draw_x = x + x_offset;
        float draw_y = current_y;

        /* Draw text shadow first */
        if (style->shadow.enabled) {
            float shadow_x = draw_x + style->shadow.offset_x;
            float shadow_y = draw_y + style->shadow.offset_y;

            /* For blur, we can approximate by drawing multiple times with slight offsets */
            if (style->shadow.blur_radius > 0) {
                float blur = style->shadow.blur_radius;
                int passes = (int)(blur / 2);
                if (passes < 1) passes = 1;
                if (passes > 4) passes = 4;

                uint32_t shadow_color = style->shadow.color;
                uint8_t base_alpha = (shadow_color >> 24) & 0xFF;

                for (int p = passes; p >= 1; p--) {
                    float offset = blur * p / passes;
                    uint8_t alpha = (uint8_t)(base_alpha / (p + 1));
                    uint32_t c = (shadow_color & 0x00FFFFFF) | ((uint32_t)alpha << 24);

                    /* Draw at offsets to simulate blur */
                    aui_draw_text(ctx, line_buf, shadow_x - offset, shadow_y, c);
                    aui_draw_text(ctx, line_buf, shadow_x + offset, shadow_y, c);
                    aui_draw_text(ctx, line_buf, shadow_x, shadow_y - offset, c);
                    aui_draw_text(ctx, line_buf, shadow_x, shadow_y + offset, c);
                }
            }

            /* Core shadow */
            aui_draw_text(ctx, line_buf, shadow_x, shadow_y, style->shadow.color);
        }

        /* Draw the actual text */
        /* TODO: If letter_spacing is non-zero, draw character by character */
        if (style->letter_spacing != 0) {
            float char_x = draw_x;
            char char_buf[2] = {0, 0};
            for (size_t c = 0; c < len; c++) {
                char_buf[0] = line_buf[c];
                aui_draw_text(ctx, char_buf, char_x, draw_y, color);
                char_x += aui_text_width(ctx, char_buf) + style->letter_spacing;
            }
        } else {
            aui_draw_text(ctx, line_buf, draw_x, draw_y, color);
        }

        current_y += line_h;
    }

    if (pushed_scissor) {
        aui_pop_scissor(ctx);
    }

    return total_height;
}
