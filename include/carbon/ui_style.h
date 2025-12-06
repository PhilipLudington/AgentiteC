/*
 * Carbon UI - Rich Styling System
 *
 * Provides advanced styling capabilities including:
 * - Box model (padding, margin, borders)
 * - Backgrounds (solid, gradient, texture, 9-slice)
 * - Shadows (drop shadow, inner shadow)
 * - Per-corner rounded corners
 *
 * Usage:
 *   CUI_Style style = cui_style_default();
 *   style.background.type = CUI_BG_GRADIENT;
 *   style.background.gradient = cui_gradient_linear(0, color1, color2);
 *   style.corner_radius = cui_corners_uniform(8.0f);
 *   cui_draw_styled_rect(ctx, rect, &style);
 */

#ifndef CARBON_UI_STYLE_H
#define CARBON_UI_STYLE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct CUI_Context CUI_Context;
typedef struct CUI_Rect CUI_Rect;
struct Carbon_Texture;

/* ============================================================================
 * Box Model Types
 * ============================================================================ */

/* Edge values for padding, margin, border width */
typedef struct CUI_Edges {
    float top;
    float right;
    float bottom;
    float left;
} CUI_Edges;

/* Per-corner radius for rounded corners */
typedef struct CUI_CornerRadius {
    float top_left;
    float top_right;
    float bottom_right;
    float bottom_left;
} CUI_CornerRadius;

/* ============================================================================
 * Border Types
 * ============================================================================ */

typedef struct CUI_Border {
    CUI_Edges width;           /* Border width per side */
    uint32_t color;            /* Uniform border color */
    uint32_t colors[4];        /* Per-side colors: top, right, bottom, left */
    bool use_per_side_colors;  /* If true, use colors[] instead of color */
} CUI_Border;

/* ============================================================================
 * Gradient Types
 * ============================================================================ */

/* Maximum gradient stops */
#define CUI_MAX_GRADIENT_STOPS 8

typedef enum CUI_GradientType {
    CUI_GRADIENT_LINEAR,
    CUI_GRADIENT_RADIAL
} CUI_GradientType;

typedef struct CUI_GradientStop {
    float position;            /* 0.0 to 1.0 */
    uint32_t color;
} CUI_GradientStop;

typedef struct CUI_Gradient {
    CUI_GradientType type;
    float angle;               /* For linear: degrees (0 = left-to-right) */
    float center_x, center_y;  /* For radial: 0-1 normalized center */
    float radius;              /* For radial: 0-1 normalized radius */
    CUI_GradientStop stops[CUI_MAX_GRADIENT_STOPS];
    int stop_count;
} CUI_Gradient;

/* ============================================================================
 * Background Types
 * ============================================================================ */

typedef enum CUI_BackgroundType {
    CUI_BG_NONE,
    CUI_BG_SOLID,
    CUI_BG_GRADIENT,
    CUI_BG_TEXTURE,
    CUI_BG_NINESLICE
} CUI_BackgroundType;

typedef struct CUI_Background {
    CUI_BackgroundType type;

    union {
        /* CUI_BG_SOLID */
        uint32_t solid_color;

        /* CUI_BG_GRADIENT */
        CUI_Gradient gradient;

        /* CUI_BG_TEXTURE */
        struct {
            struct Carbon_Texture *texture;
            float src_x, src_y, src_w, src_h;  /* Source region in texture */
            float opacity;
        } texture;

        /* CUI_BG_NINESLICE */
        struct {
            struct Carbon_Texture *texture;
            float src_x, src_y, src_w, src_h;  /* Source region in texture */
            CUI_Edges margins;                  /* 9-slice margins (in texture pixels) */
            float opacity;
        } nineslice;
    };
} CUI_Background;

/* ============================================================================
 * Shadow Types
 * ============================================================================ */

#define CUI_MAX_SHADOWS 4

typedef struct CUI_Shadow {
    float offset_x;
    float offset_y;
    float blur_radius;
    float spread;
    uint32_t color;
    bool inset;                /* true = inner shadow, false = drop shadow */
} CUI_Shadow;

/* ============================================================================
 * Complete Style Definition
 * ============================================================================ */

typedef struct CUI_Style {
    /* Box model */
    CUI_Edges padding;
    CUI_Edges margin;

    /* Border */
    CUI_Border border;
    CUI_CornerRadius corner_radius;

    /* Background (can have different backgrounds for states) */
    CUI_Background background;
    CUI_Background background_hover;
    CUI_Background background_active;
    CUI_Background background_disabled;

    /* Shadows */
    CUI_Shadow shadows[CUI_MAX_SHADOWS];
    int shadow_count;

    /* Opacity (multiplied with all colors) */
    float opacity;

    /* Text styling */
    uint32_t text_color;
    uint32_t text_color_hover;
    uint32_t text_color_active;
    uint32_t text_color_disabled;
    float font_size;           /* 0 = use default from context */

    /* Size constraints */
    float min_width, min_height;
    float max_width, max_height;  /* 0 = no max */
} CUI_Style;

/* ============================================================================
 * Style Class (for reusable named styles)
 * ============================================================================ */

#define CUI_STYLE_CLASS_NAME_MAX 64

typedef struct CUI_StyleClass {
    char name[CUI_STYLE_CLASS_NAME_MAX];
    CUI_Style style;
    struct CUI_StyleClass *parent;  /* For inheritance */
} CUI_StyleClass;

/* ============================================================================
 * Style Variable Identifiers (for push/pop)
 * ============================================================================ */

typedef enum CUI_StyleVar {
    CUI_STYLEVAR_PADDING_TOP,
    CUI_STYLEVAR_PADDING_RIGHT,
    CUI_STYLEVAR_PADDING_BOTTOM,
    CUI_STYLEVAR_PADDING_LEFT,
    CUI_STYLEVAR_MARGIN_TOP,
    CUI_STYLEVAR_MARGIN_RIGHT,
    CUI_STYLEVAR_MARGIN_BOTTOM,
    CUI_STYLEVAR_MARGIN_LEFT,
    CUI_STYLEVAR_BORDER_WIDTH,
    CUI_STYLEVAR_CORNER_RADIUS,
    CUI_STYLEVAR_OPACITY,
    CUI_STYLEVAR_FONT_SIZE,
    CUI_STYLEVAR_COUNT
} CUI_StyleVar;

typedef enum CUI_StyleColor {
    CUI_STYLECOLOR_BG,
    CUI_STYLECOLOR_BG_HOVER,
    CUI_STYLECOLOR_BG_ACTIVE,
    CUI_STYLECOLOR_BORDER,
    CUI_STYLECOLOR_TEXT,
    CUI_STYLECOLOR_TEXT_HOVER,
    CUI_STYLECOLOR_COUNT
} CUI_StyleColor;

/* ============================================================================
 * Helper Functions - Edges
 * ============================================================================ */

/* Create uniform edges (all sides same) */
static inline CUI_Edges cui_edges_uniform(float value) {
    return (CUI_Edges){value, value, value, value};
}

/* Create edges with vertical/horizontal values */
static inline CUI_Edges cui_edges_vh(float vertical, float horizontal) {
    return (CUI_Edges){vertical, horizontal, vertical, horizontal};
}

/* Create edges with all four values */
static inline CUI_Edges cui_edges(float top, float right, float bottom, float left) {
    return (CUI_Edges){top, right, bottom, left};
}

/* Zero edges */
static inline CUI_Edges cui_edges_zero(void) {
    return (CUI_Edges){0, 0, 0, 0};
}

/* ============================================================================
 * Helper Functions - Corner Radius
 * ============================================================================ */

/* Create uniform corners */
static inline CUI_CornerRadius cui_corners_uniform(float radius) {
    return (CUI_CornerRadius){radius, radius, radius, radius};
}

/* Create corners with top/bottom values */
static inline CUI_CornerRadius cui_corners_tb(float top, float bottom) {
    return (CUI_CornerRadius){top, top, bottom, bottom};
}

/* Create corners with left/right values */
static inline CUI_CornerRadius cui_corners_lr(float left, float right) {
    return (CUI_CornerRadius){left, right, right, left};
}

/* Create corners with all four values */
static inline CUI_CornerRadius cui_corners(float tl, float tr, float br, float bl) {
    return (CUI_CornerRadius){tl, tr, br, bl};
}

/* Zero corners */
static inline CUI_CornerRadius cui_corners_zero(void) {
    return (CUI_CornerRadius){0, 0, 0, 0};
}

/* ============================================================================
 * Helper Functions - Borders
 * ============================================================================ */

/* Create uniform border */
static inline CUI_Border cui_border(float width, uint32_t color) {
    CUI_Border b;
    memset(&b, 0, sizeof(b));
    b.width = cui_edges_uniform(width);
    b.color = color;
    b.use_per_side_colors = false;
    return b;
}

/* Create border with different widths */
static inline CUI_Border cui_border_ex(CUI_Edges width, uint32_t color) {
    CUI_Border b;
    memset(&b, 0, sizeof(b));
    b.width = width;
    b.color = color;
    b.use_per_side_colors = false;
    return b;
}

/* No border */
static inline CUI_Border cui_border_none(void) {
    CUI_Border b;
    memset(&b, 0, sizeof(b));
    return b;
}

/* ============================================================================
 * Helper Functions - Gradients
 * ============================================================================ */

/* Create a simple two-color linear gradient */
CUI_Gradient cui_gradient_linear(float angle_degrees, uint32_t color1, uint32_t color2);

/* Create a linear gradient with custom stops */
CUI_Gradient cui_gradient_linear_stops(float angle_degrees,
                                        const CUI_GradientStop *stops, int count);

/* Create a simple two-color radial gradient */
CUI_Gradient cui_gradient_radial(float center_x, float center_y, float radius,
                                  uint32_t inner_color, uint32_t outer_color);

/* ============================================================================
 * Helper Functions - Backgrounds
 * ============================================================================ */

/* No background */
static inline CUI_Background cui_bg_none(void) {
    CUI_Background bg;
    memset(&bg, 0, sizeof(bg));
    bg.type = CUI_BG_NONE;
    return bg;
}

/* Solid color background */
static inline CUI_Background cui_bg_solid(uint32_t color) {
    CUI_Background bg;
    memset(&bg, 0, sizeof(bg));
    bg.type = CUI_BG_SOLID;
    bg.solid_color = color;
    return bg;
}

/* Gradient background */
static inline CUI_Background cui_bg_gradient(CUI_Gradient gradient) {
    CUI_Background bg;
    memset(&bg, 0, sizeof(bg));
    bg.type = CUI_BG_GRADIENT;
    bg.gradient = gradient;
    return bg;
}

/* ============================================================================
 * Helper Functions - Shadows
 * ============================================================================ */

/* Create a drop shadow */
static inline CUI_Shadow cui_shadow(float offset_x, float offset_y,
                                     float blur, uint32_t color) {
    return (CUI_Shadow){
        .offset_x = offset_x,
        .offset_y = offset_y,
        .blur_radius = blur,
        .spread = 0,
        .color = color,
        .inset = false
    };
}

/* Create a drop shadow with spread */
static inline CUI_Shadow cui_shadow_ex(float offset_x, float offset_y,
                                        float blur, float spread, uint32_t color) {
    return (CUI_Shadow){
        .offset_x = offset_x,
        .offset_y = offset_y,
        .blur_radius = blur,
        .spread = spread,
        .color = color,
        .inset = false
    };
}

/* Create an inner shadow */
static inline CUI_Shadow cui_shadow_inset(float offset_x, float offset_y,
                                           float blur, uint32_t color) {
    return (CUI_Shadow){
        .offset_x = offset_x,
        .offset_y = offset_y,
        .blur_radius = blur,
        .spread = 0,
        .color = color,
        .inset = true
    };
}

/* ============================================================================
 * Style Creation and Manipulation
 * ============================================================================ */

/* Create a default (empty) style */
CUI_Style cui_style_default(void);

/* Create a style from theme defaults */
CUI_Style cui_style_from_theme(const CUI_Context *ctx);

/* Merge two styles (src overrides dst where set) */
void cui_style_merge(CUI_Style *dst, const CUI_Style *src);

/* ============================================================================
 * Style Stack (for immediate mode)
 * ============================================================================ */

/* Push a complete style onto the stack */
void cui_push_style(CUI_Context *ctx, const CUI_Style *style);
void cui_pop_style(CUI_Context *ctx);

/* Push individual style variables */
void cui_push_style_var(CUI_Context *ctx, CUI_StyleVar var, float value);
void cui_pop_style_var(CUI_Context *ctx);

/* Push individual style colors */
void cui_push_style_color(CUI_Context *ctx, CUI_StyleColor color, uint32_t value);
void cui_pop_style_color(CUI_Context *ctx);

/* Get current style from stack */
const CUI_Style *cui_get_current_style(const CUI_Context *ctx);

/* ============================================================================
 * Style Class Registry
 * ============================================================================ */

/* Register a style class */
bool cui_register_style_class(CUI_Context *ctx, const char *name,
                               const CUI_Style *style, const char *parent_name);

/* Get a style class by name */
CUI_StyleClass *cui_get_style_class(CUI_Context *ctx, const char *name);

/* Apply a style class (resolves inheritance) */
CUI_Style cui_resolve_style_class(const CUI_StyleClass *style_class);

/* ============================================================================
 * Styled Drawing Functions
 * ============================================================================ */

/* Draw a rectangle with full styling (background, border, shadows, corners) */
void cui_draw_styled_rect(CUI_Context *ctx, float x, float y, float w, float h,
                          const CUI_Style *style);

/* Draw just a gradient */
void cui_draw_gradient(CUI_Context *ctx, float x, float y, float w, float h,
                       const CUI_Gradient *gradient);

/* Draw a 9-slice texture */
void cui_draw_nineslice(CUI_Context *ctx, float x, float y, float w, float h,
                        struct Carbon_Texture *texture,
                        float src_x, float src_y, float src_w, float src_h,
                        CUI_Edges margins);

/* Draw a shadow */
void cui_draw_shadow(CUI_Context *ctx, float x, float y, float w, float h,
                     const CUI_Shadow *shadow, CUI_CornerRadius corners);

/* Draw rounded rectangle with per-corner radius */
void cui_draw_rect_rounded_ex(CUI_Context *ctx, float x, float y, float w, float h,
                               uint32_t color, CUI_CornerRadius corners);

/* Draw rounded rectangle outline with per-corner radius */
void cui_draw_rect_rounded_outline(CUI_Context *ctx, float x, float y, float w, float h,
                                    uint32_t color, float thickness,
                                    CUI_CornerRadius corners);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_UI_STYLE_H */
