/*
 * Agentite UI - Rich Styling System
 *
 * Provides advanced styling capabilities including:
 * - Box model (padding, margin, borders)
 * - Backgrounds (solid, gradient, texture, 9-slice)
 * - Shadows (drop shadow, inner shadow)
 * - Per-corner rounded corners
 *
 * Usage:
 *   AUI_Style style = aui_style_default();
 *   style.background.type = AUI_BG_GRADIENT;
 *   style.background.gradient = aui_gradient_linear(0, color1, color2);
 *   style.corner_radius = aui_corners_uniform(8.0f);
 *   aui_draw_styled_rect(ctx, rect, &style);
 */

#ifndef AGENTITE_UI_STYLE_H
#define AGENTITE_UI_STYLE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct AUI_Context AUI_Context;
typedef struct AUI_Rect AUI_Rect;
struct Agentite_Texture;

/* ============================================================================
 * Text Alignment
 * ============================================================================ */

typedef enum AUI_TextAlign {
    AUI_TEXT_ALIGN_LEFT,
    AUI_TEXT_ALIGN_CENTER,
    AUI_TEXT_ALIGN_RIGHT,
    AUI_TEXT_ALIGN_JUSTIFY
} AUI_TextAlign;

typedef enum AUI_TextVAlign {
    AUI_TEXT_VALIGN_TOP,
    AUI_TEXT_VALIGN_MIDDLE,
    AUI_TEXT_VALIGN_BOTTOM
} AUI_TextVAlign;

/* ============================================================================
 * Text Overflow
 * ============================================================================ */

typedef enum AUI_TextOverflow {
    AUI_TEXT_OVERFLOW_VISIBLE,   /* Text can overflow container */
    AUI_TEXT_OVERFLOW_CLIP,      /* Clip text at container edge */
    AUI_TEXT_OVERFLOW_ELLIPSIS,  /* Show "..." when text overflows */
    AUI_TEXT_OVERFLOW_WRAP       /* Wrap text to next line */
} AUI_TextOverflow;

/* ============================================================================
 * Text Shadow
 * ============================================================================ */

typedef struct AUI_TextShadow {
    float offset_x;
    float offset_y;
    float blur_radius;      /* Note: blur may be approximated */
    uint32_t color;
    bool enabled;
} AUI_TextShadow;

/* ============================================================================
 * Text Style (consolidated text styling)
 * ============================================================================ */

typedef struct AUI_TextStyle {
    AUI_TextAlign align;
    AUI_TextVAlign valign;
    AUI_TextOverflow overflow;
    float line_height;          /* Multiplier, 1.0 = normal, 1.5 = 150% */
    float letter_spacing;       /* Extra pixels between characters */
    float word_spacing;         /* Extra pixels between words */
    AUI_TextShadow shadow;
    bool wrap;                  /* Enable word wrapping */
    int max_lines;              /* 0 = unlimited */
} AUI_TextStyle;

/* ============================================================================
 * Box Model Types
 * ============================================================================ */

/* Edge values for padding, margin, border width */
typedef struct AUI_Edges {
    float top;
    float right;
    float bottom;
    float left;
} AUI_Edges;

/* Per-corner radius for rounded corners */
typedef struct AUI_CornerRadius {
    float top_left;
    float top_right;
    float bottom_right;
    float bottom_left;
} AUI_CornerRadius;

/* ============================================================================
 * Border Types
 * ============================================================================ */

typedef struct AUI_Border {
    AUI_Edges width;           /* Border width per side */
    uint32_t color;            /* Uniform border color */
    uint32_t colors[4];        /* Per-side colors: top, right, bottom, left */
    bool use_per_side_colors;  /* If true, use colors[] instead of color */
} AUI_Border;

/* ============================================================================
 * Gradient Types
 * ============================================================================ */

/* Maximum gradient stops */
#define AUI_MAX_GRADIENT_STOPS 8

typedef enum AUI_GradientType {
    AUI_GRADIENT_LINEAR,
    AUI_GRADIENT_RADIAL
} AUI_GradientType;

typedef struct AUI_GradientStop {
    float position;            /* 0.0 to 1.0 */
    uint32_t color;
} AUI_GradientStop;

typedef struct AUI_Gradient {
    AUI_GradientType type;
    float angle;               /* For linear: degrees (0 = left-to-right) */
    float center_x, center_y;  /* For radial: 0-1 normalized center */
    float radius;              /* For radial: 0-1 normalized radius */
    AUI_GradientStop stops[AUI_MAX_GRADIENT_STOPS];
    int stop_count;
} AUI_Gradient;

/* ============================================================================
 * Background Types
 * ============================================================================ */

typedef enum AUI_BackgroundType {
    AUI_BG_NONE,
    AUI_BG_SOLID,
    AUI_BG_GRADIENT,
    AUI_BG_TEXTURE,
    AUI_BG_NINESLICE
} AUI_BackgroundType;

typedef struct AUI_Background {
    AUI_BackgroundType type;

    union {
        /* AUI_BG_SOLID */
        uint32_t solid_color;

        /* AUI_BG_GRADIENT */
        AUI_Gradient gradient;

        /* AUI_BG_TEXTURE */
        struct {
            struct Agentite_Texture *texture;
            float src_x, src_y, src_w, src_h;  /* Source region in texture */
            float opacity;
        } texture;

        /* AUI_BG_NINESLICE */
        struct {
            struct Agentite_Texture *texture;
            float src_x, src_y, src_w, src_h;  /* Source region in texture */
            AUI_Edges margins;                  /* 9-slice margins (in texture pixels) */
            float opacity;
        } nineslice;
    };
} AUI_Background;

/* ============================================================================
 * Shadow Types
 * ============================================================================ */

#define AUI_MAX_SHADOWS 4

typedef struct AUI_Shadow {
    float offset_x;
    float offset_y;
    float blur_radius;
    float spread;
    uint32_t color;
    bool inset;                /* true = inner shadow, false = drop shadow */
} AUI_Shadow;

/* ============================================================================
 * Style Transitions
 * ============================================================================ */

/* Which style properties can be transitioned */
typedef enum AUI_TransitionProperty {
    AUI_TRANSITION_NONE         = 0,
    AUI_TRANSITION_BG_COLOR     = 1 << 0,   /* Background color */
    AUI_TRANSITION_TEXT_COLOR   = 1 << 1,   /* Text color */
    AUI_TRANSITION_BORDER_COLOR = 1 << 2,   /* Border color */
    AUI_TRANSITION_OPACITY      = 1 << 3,   /* Overall opacity */
    AUI_TRANSITION_ALL          = 0xFFFF    /* All properties */
} AUI_TransitionProperty;

/* Easing types for transitions (matches AUI_EaseType from ui_tween.h) */
typedef enum AUI_TransitionEase {
    AUI_TRANS_EASE_LINEAR = 0,
    AUI_TRANS_EASE_IN_QUAD = 4,
    AUI_TRANS_EASE_OUT_QUAD = 5,
    AUI_TRANS_EASE_IN_OUT_QUAD = 6,
    AUI_TRANS_EASE_OUT_CUBIC = 8,
    AUI_TRANS_EASE_IN_OUT_CUBIC = 9
} AUI_TransitionEase;

/* Transition configuration */
typedef struct AUI_StyleTransition {
    float duration;              /* Transition duration in seconds (0 = instant) */
    AUI_TransitionEase ease;     /* Easing function */
    uint32_t properties;         /* Bitmask of AUI_TransitionProperty flags */
} AUI_StyleTransition;

/* ============================================================================
 * Complete Style Definition
 * ============================================================================ */

typedef struct AUI_Style {
    /* Box model */
    AUI_Edges padding;
    AUI_Edges margin;

    /* Border */
    AUI_Border border;
    AUI_CornerRadius corner_radius;

    /* Background (can have different backgrounds for states) */
    AUI_Background background;
    AUI_Background background_hover;
    AUI_Background background_active;
    AUI_Background background_disabled;

    /* Shadows */
    AUI_Shadow shadows[AUI_MAX_SHADOWS];
    int shadow_count;

    /* Opacity (multiplied with all colors) */
    float opacity;

    /* Text styling */
    uint32_t text_color;
    uint32_t text_color_hover;
    uint32_t text_color_active;
    uint32_t text_color_disabled;
    float font_size;           /* 0 = use default from context */
    AUI_TextStyle text;        /* Text alignment, overflow, line height, etc. */

    /* Size constraints */
    float min_width, min_height;
    float max_width, max_height;  /* 0 = no max */

    /* Transitions */
    AUI_StyleTransition transition;  /* Transition configuration for state changes */
} AUI_Style;

/* ============================================================================
 * Style Class (for reusable named styles)
 * ============================================================================ */

#define AUI_STYLE_CLASS_NAME_MAX 64

typedef struct AUI_StyleClass {
    char name[AUI_STYLE_CLASS_NAME_MAX];
    AUI_Style style;
    struct AUI_StyleClass *parent;  /* For inheritance */
} AUI_StyleClass;

/* ============================================================================
 * Style Variable Identifiers (for push/pop)
 * ============================================================================ */

typedef enum AUI_StyleVar {
    AUI_STYLEVAR_PADDING_TOP,
    AUI_STYLEVAR_PADDING_RIGHT,
    AUI_STYLEVAR_PADDING_BOTTOM,
    AUI_STYLEVAR_PADDING_LEFT,
    AUI_STYLEVAR_MARGIN_TOP,
    AUI_STYLEVAR_MARGIN_RIGHT,
    AUI_STYLEVAR_MARGIN_BOTTOM,
    AUI_STYLEVAR_MARGIN_LEFT,
    AUI_STYLEVAR_BORDER_WIDTH,
    AUI_STYLEVAR_CORNER_RADIUS,
    AUI_STYLEVAR_OPACITY,
    AUI_STYLEVAR_FONT_SIZE,
    AUI_STYLEVAR_COUNT
} AUI_StyleVar;

typedef enum AUI_StyleColor {
    AUI_STYLECOLOR_BG,
    AUI_STYLECOLOR_BG_HOVER,
    AUI_STYLECOLOR_BG_ACTIVE,
    AUI_STYLECOLOR_BORDER,
    AUI_STYLECOLOR_TEXT,
    AUI_STYLECOLOR_TEXT_HOVER,
    AUI_STYLECOLOR_COUNT
} AUI_StyleColor;

/* ============================================================================
 * Helper Functions - Edges
 * ============================================================================ */

/* Create uniform edges (all sides same) */
static inline AUI_Edges aui_edges_uniform(float value) {
    return (AUI_Edges){value, value, value, value};
}

/* Create edges with vertical/horizontal values */
static inline AUI_Edges aui_edges_vh(float vertical, float horizontal) {
    return (AUI_Edges){vertical, horizontal, vertical, horizontal};
}

/* Create edges with all four values */
static inline AUI_Edges aui_edges(float top, float right, float bottom, float left) {
    return (AUI_Edges){top, right, bottom, left};
}

/* Zero edges */
static inline AUI_Edges aui_edges_zero(void) {
    return (AUI_Edges){0, 0, 0, 0};
}

/* ============================================================================
 * Helper Functions - Corner Radius
 * ============================================================================ */

/* Create uniform corners */
static inline AUI_CornerRadius aui_corners_uniform(float radius) {
    return (AUI_CornerRadius){radius, radius, radius, radius};
}

/* Create corners with top/bottom values */
static inline AUI_CornerRadius aui_corners_tb(float top, float bottom) {
    return (AUI_CornerRadius){top, top, bottom, bottom};
}

/* Create corners with left/right values */
static inline AUI_CornerRadius aui_corners_lr(float left, float right) {
    return (AUI_CornerRadius){left, right, right, left};
}

/* Create corners with all four values */
static inline AUI_CornerRadius aui_corners(float tl, float tr, float br, float bl) {
    return (AUI_CornerRadius){tl, tr, br, bl};
}

/* Zero corners */
static inline AUI_CornerRadius aui_corners_zero(void) {
    return (AUI_CornerRadius){0, 0, 0, 0};
}

/* ============================================================================
 * Helper Functions - Borders
 * ============================================================================ */

/* Create uniform border */
static inline AUI_Border aui_border(float width, uint32_t color) {
    AUI_Border b;
    memset(&b, 0, sizeof(b));
    b.width = aui_edges_uniform(width);
    b.color = color;
    b.use_per_side_colors = false;
    return b;
}

/* Create border with different widths */
static inline AUI_Border aui_border_ex(AUI_Edges width, uint32_t color) {
    AUI_Border b;
    memset(&b, 0, sizeof(b));
    b.width = width;
    b.color = color;
    b.use_per_side_colors = false;
    return b;
}

/* No border */
static inline AUI_Border aui_border_none(void) {
    AUI_Border b;
    memset(&b, 0, sizeof(b));
    return b;
}

/* ============================================================================
 * Helper Functions - Gradients
 * ============================================================================ */

/* Create a simple two-color linear gradient */
AUI_Gradient aui_gradient_linear(float angle_degrees, uint32_t color1, uint32_t color2);

/* Create a linear gradient with custom stops */
AUI_Gradient aui_gradient_linear_stops(float angle_degrees,
                                        const AUI_GradientStop *stops, int count);

/* Create a simple two-color radial gradient */
AUI_Gradient aui_gradient_radial(float center_x, float center_y, float radius,
                                  uint32_t inner_color, uint32_t outer_color);

/* ============================================================================
 * Helper Functions - Backgrounds
 * ============================================================================ */

/* No background */
static inline AUI_Background aui_bg_none(void) {
    AUI_Background bg;
    memset(&bg, 0, sizeof(bg));
    bg.type = AUI_BG_NONE;
    return bg;
}

/* Solid color background */
static inline AUI_Background aui_bg_solid(uint32_t color) {
    AUI_Background bg;
    memset(&bg, 0, sizeof(bg));
    bg.type = AUI_BG_SOLID;
    bg.solid_color = color;
    return bg;
}

/* Gradient background */
static inline AUI_Background aui_bg_gradient(AUI_Gradient gradient) {
    AUI_Background bg;
    memset(&bg, 0, sizeof(bg));
    bg.type = AUI_BG_GRADIENT;
    bg.gradient = gradient;
    return bg;
}

/* ============================================================================
 * Helper Functions - Shadows
 * ============================================================================ */

/* Create a drop shadow */
static inline AUI_Shadow aui_shadow(float offset_x, float offset_y,
                                     float blur, uint32_t color) {
    return (AUI_Shadow){
        .offset_x = offset_x,
        .offset_y = offset_y,
        .blur_radius = blur,
        .spread = 0,
        .color = color,
        .inset = false
    };
}

/* Create a drop shadow with spread */
static inline AUI_Shadow aui_shadow_ex(float offset_x, float offset_y,
                                        float blur, float spread, uint32_t color) {
    return (AUI_Shadow){
        .offset_x = offset_x,
        .offset_y = offset_y,
        .blur_radius = blur,
        .spread = spread,
        .color = color,
        .inset = false
    };
}

/* Create an inner shadow */
static inline AUI_Shadow aui_shadow_inset(float offset_x, float offset_y,
                                           float blur, uint32_t color) {
    return (AUI_Shadow){
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
AUI_Style aui_style_default(void);

/* Create a style from theme defaults */
AUI_Style aui_style_from_theme(const AUI_Context *ctx);

/* Merge two styles (src overrides dst where set) */
void aui_style_merge(AUI_Style *dst, const AUI_Style *src);

/* ============================================================================
 * Style Stack (for immediate mode)
 * ============================================================================ */

/* Push a complete style onto the stack */
void aui_push_style(AUI_Context *ctx, const AUI_Style *style);
void aui_pop_style(AUI_Context *ctx);

/* Push individual style variables */
void aui_push_style_var(AUI_Context *ctx, AUI_StyleVar var, float value);
void aui_pop_style_var(AUI_Context *ctx);

/* Push individual style colors */
void aui_push_style_color(AUI_Context *ctx, AUI_StyleColor color, uint32_t value);
void aui_pop_style_color(AUI_Context *ctx);

/* Get current style from stack */
const AUI_Style *aui_get_current_style(const AUI_Context *ctx);

/* ============================================================================
 * Style Class Registry
 * ============================================================================ */

/* Register a style class */
bool aui_register_style_class(AUI_Context *ctx, const char *name,
                               const AUI_Style *style, const char *parent_name);

/* Get a style class by name */
AUI_StyleClass *aui_get_style_class(AUI_Context *ctx, const char *name);

/* Apply a style class (resolves inheritance) */
AUI_Style aui_resolve_style_class(const AUI_StyleClass *style_class);

/* ============================================================================
 * Styled Drawing Functions
 * ============================================================================ */

/* Draw a rectangle with full styling (background, border, shadows, corners) */
void aui_draw_styled_rect(AUI_Context *ctx, float x, float y, float w, float h,
                          const AUI_Style *style);

/* Draw just a gradient */
void aui_draw_gradient(AUI_Context *ctx, float x, float y, float w, float h,
                       const AUI_Gradient *gradient);

/* Draw a 9-slice texture */
void aui_draw_nineslice(AUI_Context *ctx, float x, float y, float w, float h,
                        struct Agentite_Texture *texture,
                        float src_x, float src_y, float src_w, float src_h,
                        AUI_Edges margins);

/* Draw a shadow */
void aui_draw_shadow(AUI_Context *ctx, float x, float y, float w, float h,
                     const AUI_Shadow *shadow, AUI_CornerRadius corners);

/* Draw rounded rectangle with per-corner radius */
void aui_draw_rect_rounded_ex(AUI_Context *ctx, float x, float y, float w, float h,
                               uint32_t color, AUI_CornerRadius corners);

/* Draw rounded rectangle outline with per-corner radius */
void aui_draw_rect_rounded_outline(AUI_Context *ctx, float x, float y, float w, float h,
                                    uint32_t color, float thickness,
                                    AUI_CornerRadius corners);

/* ============================================================================
 * Helper Functions - Text Style
 * ============================================================================ */

/* Create a default text style */
static inline AUI_TextStyle aui_text_style_default(void) {
    AUI_TextStyle ts;
    memset(&ts, 0, sizeof(ts));
    ts.align = AUI_TEXT_ALIGN_LEFT;
    ts.valign = AUI_TEXT_VALIGN_MIDDLE;
    ts.overflow = AUI_TEXT_OVERFLOW_VISIBLE;
    ts.line_height = 1.0f;
    ts.letter_spacing = 0.0f;
    ts.word_spacing = 0.0f;
    ts.wrap = false;
    ts.max_lines = 0;
    return ts;
}

/* Create a text shadow */
static inline AUI_TextShadow aui_text_shadow(float offset_x, float offset_y,
                                              float blur, uint32_t color) {
    return (AUI_TextShadow){
        .offset_x = offset_x,
        .offset_y = offset_y,
        .blur_radius = blur,
        .color = color,
        .enabled = true
    };
}

/* No text shadow */
static inline AUI_TextShadow aui_text_shadow_none(void) {
    AUI_TextShadow ts;
    memset(&ts, 0, sizeof(ts));
    ts.enabled = false;
    return ts;
}

/* ============================================================================
 * Styled Text Drawing Functions
 * ============================================================================ */

/* Draw text with full styling (alignment, overflow, shadow, etc.)
 * Returns the actual height used (useful for wrapped text) */
float aui_draw_styled_text(AUI_Context *ctx, const char *text,
                           float x, float y, float max_width, float max_height,
                           uint32_t color, const AUI_TextStyle *style);

/* Measure text with styling applied
 * Returns width, and optionally fills out_height with wrapped height */
float aui_measure_styled_text(AUI_Context *ctx, const char *text,
                              float max_width, const AUI_TextStyle *style,
                              float *out_height);

/* Truncate text with ellipsis to fit within max_width
 * Returns pointer to static buffer containing truncated text */
const char *aui_truncate_text_ellipsis(AUI_Context *ctx, const char *text,
                                        float max_width);

/* ============================================================================
 * Helper Functions - Style Transitions
 * ============================================================================ */

/* Create a transition with all properties */
static inline AUI_StyleTransition aui_transition(float duration, AUI_TransitionEase ease) {
    return (AUI_StyleTransition){
        .duration = duration,
        .ease = ease,
        .properties = AUI_TRANSITION_ALL
    };
}

/* Create a transition for specific properties */
static inline AUI_StyleTransition aui_transition_props(float duration, AUI_TransitionEase ease,
                                                        uint32_t properties) {
    return (AUI_StyleTransition){
        .duration = duration,
        .ease = ease,
        .properties = properties
    };
}

/* No transition (instant changes) */
static inline AUI_StyleTransition aui_transition_none(void) {
    AUI_StyleTransition t;
    memset(&t, 0, sizeof(t));
    return t;
}

/* Common transition presets */
#define AUI_TRANSITION_FAST   aui_transition(0.1f, AUI_TRANS_EASE_OUT_QUAD)
#define AUI_TRANSITION_NORMAL aui_transition(0.2f, AUI_TRANS_EASE_OUT_QUAD)
#define AUI_TRANSITION_SLOW   aui_transition(0.4f, AUI_TRANS_EASE_IN_OUT_QUAD)

/* ============================================================================
 * Color Utilities for Transitions
 * ============================================================================ */

/* Interpolate between two RGBA colors */
uint32_t aui_color_lerp(uint32_t from, uint32_t to, float t);

/* Extract RGBA components from a color (format: 0xAABBGGRR) */
static inline void aui_color_unpack(uint32_t color, uint8_t *r, uint8_t *g,
                                     uint8_t *b, uint8_t *a) {
    if (r) *r = (uint8_t)(color & 0xFF);
    if (g) *g = (uint8_t)((color >> 8) & 0xFF);
    if (b) *b = (uint8_t)((color >> 16) & 0xFF);
    if (a) *a = (uint8_t)((color >> 24) & 0xFF);
}

/* Pack RGBA components into a color */
static inline uint32_t aui_color_pack(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_UI_STYLE_H */
