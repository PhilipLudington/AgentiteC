/*
 * Carbon UI - Rich Text System
 *
 * BBCode-style formatted text with inline icons and animations.
 *
 * Supported tags:
 *   [b]bold[/b]
 *   [i]italic[/i]
 *   [u]underline[/u]
 *   [s]strikethrough[/s]
 *   [color=#RRGGBB]colored text[/color]
 *   [color=red]named color[/color]
 *   [size=20]sized text[/size]
 *   [url=http://...]link[/url]
 *   [img]path/to/image.png[/img]
 *   [icon=name]
 *   [wave]wavy text[/wave]
 *   [shake]shaking text[/shake]
 *   [rainbow]rainbow text[/rainbow]
 *   [fade]fading text[/fade]
 *
 * Usage:
 *   CUI_RichText *rt = cui_richtext_parse("[b]Hello[/b] [color=#FF0000]World[/color]!");
 *   cui_richtext_layout(rt, 400);  // max width
 *   cui_richtext_draw(ctx, rt, x, y);
 *   cui_richtext_destroy(rt);
 */

#ifndef CARBON_UI_RICHTEXT_H
#define CARBON_UI_RICHTEXT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct CUI_Context CUI_Context;
typedef struct CUI_Node CUI_Node;
typedef struct CUI_RichText CUI_RichText;

/* ============================================================================
 * Rich Text Span Types
 * ============================================================================ */

typedef enum CUI_RichTagType {
    CUI_RTAG_TEXT,             /* Plain text */
    CUI_RTAG_BOLD,
    CUI_RTAG_ITALIC,
    CUI_RTAG_UNDERLINE,
    CUI_RTAG_STRIKETHROUGH,
    CUI_RTAG_COLOR,
    CUI_RTAG_SIZE,
    CUI_RTAG_URL,
    CUI_RTAG_IMAGE,
    CUI_RTAG_ICON,
    CUI_RTAG_WAVE,
    CUI_RTAG_SHAKE,
    CUI_RTAG_RAINBOW,
    CUI_RTAG_FADE,
    CUI_RTAG_TYPEWRITER,
} CUI_RichTagType;

/* ============================================================================
 * Rich Text Alignment
 * ============================================================================ */

typedef enum CUI_RichTextAlign {
    CUI_RICH_ALIGN_LEFT,
    CUI_RICH_ALIGN_CENTER,
    CUI_RICH_ALIGN_RIGHT,
    CUI_RICH_ALIGN_JUSTIFY,
} CUI_RichTextAlign;

/* ============================================================================
 * Rich Text Callbacks
 * ============================================================================ */

/* Called when a URL is clicked */
typedef void (*CUI_RichLinkCallback)(const char *url, void *userdata);

/* Called for custom tag handling */
typedef void (*CUI_RichCustomTagCallback)(const char *tag, const char *value,
                                           int start, int end, void *userdata);

/* ============================================================================
 * Rich Text Span
 * ============================================================================ */

typedef struct CUI_RichSpan {
    CUI_RichTagType type;
    int start;                 /* Start character index */
    int end;                   /* End character index */

    union {
        uint32_t color;
        float size;
        struct {
            char url[256];
        } link;
        struct {
            char path[256];
            float width, height;
        } image;
        struct {
            char name[64];
            float size;
        } icon;
        struct {
            float amplitude;
            float frequency;
        } wave;
        struct {
            float intensity;
        } shake;
        struct {
            float speed;
        } rainbow;
        struct {
            float min_alpha;
            float max_alpha;
            float speed;
        } fade;
    };
} CUI_RichSpan;

/* ============================================================================
 * Rich Text Line
 * ============================================================================ */

typedef struct CUI_RichLine {
    int start_char;            /* First character index */
    int end_char;              /* Last character index (exclusive) */
    float width;
    float height;
    float baseline;
    float y_offset;            /* Offset from top of text block */
} CUI_RichLine;

/* ============================================================================
 * Rich Text Hotspot (clickable region)
 * ============================================================================ */

typedef struct CUI_RichHotspot {
    float x, y, w, h;
    char url[256];
    int span_index;
} CUI_RichHotspot;

/* ============================================================================
 * Rich Text Configuration
 * ============================================================================ */

typedef struct CUI_RichTextConfig {
    CUI_RichTextAlign alignment;
    float line_height_factor;  /* Multiplier for line height (default 1.2) */
    uint32_t default_color;
    float default_size;
    float max_width;           /* 0 = no wrapping */
    bool selection_enabled;
    bool meta_underlines;      /* Underline URLs */

    /* Callbacks */
    CUI_RichLinkCallback on_link_click;
    void *link_userdata;

    CUI_RichCustomTagCallback on_custom_tag;
    void *custom_tag_userdata;
} CUI_RichTextConfig;

/* ============================================================================
 * Rich Text Parse and Create
 * ============================================================================ */

/* Parse BBCode text into rich text object */
CUI_RichText *cui_richtext_parse(const char *bbcode);

/* Parse with custom config */
CUI_RichText *cui_richtext_parse_ex(const char *bbcode, const CUI_RichTextConfig *config);

/* Create from plain text (no parsing) */
CUI_RichText *cui_richtext_create(const char *plain_text);

/* Destroy rich text object */
void cui_richtext_destroy(CUI_RichText *rt);

/* ============================================================================
 * Rich Text Modification
 * ============================================================================ */

/* Set/get the source BBCode text */
void cui_richtext_set_bbcode(CUI_RichText *rt, const char *bbcode);
const char *cui_richtext_get_bbcode(const CUI_RichText *rt);

/* Get the plain text (tags stripped) */
const char *cui_richtext_get_plain(const CUI_RichText *rt);

/* Get length of plain text */
int cui_richtext_get_length(const CUI_RichText *rt);

/* Append BBCode text */
void cui_richtext_append(CUI_RichText *rt, const char *bbcode);

/* Clear all text */
void cui_richtext_clear(CUI_RichText *rt);

/* ============================================================================
 * Rich Text Layout
 * ============================================================================ */

/* Calculate layout with max width (call after parse/modify) */
void cui_richtext_layout(CUI_RichText *rt, float max_width);

/* Get computed dimensions */
void cui_richtext_get_size(const CUI_RichText *rt, float *width, float *height);

/* Get line count */
int cui_richtext_get_line_count(const CUI_RichText *rt);

/* Get line info */
const CUI_RichLine *cui_richtext_get_line(const CUI_RichText *rt, int index);

/* ============================================================================
 * Rich Text Rendering
 * ============================================================================ */

/* Draw rich text at position */
void cui_richtext_draw(CUI_Context *ctx, CUI_RichText *rt, float x, float y);

/* Draw with custom config */
void cui_richtext_draw_ex(CUI_Context *ctx, CUI_RichText *rt, float x, float y,
                           const CUI_RichTextConfig *config);

/* Update animation state (call each frame for animated text) */
void cui_richtext_update(CUI_RichText *rt, float delta_time);

/* ============================================================================
 * Rich Text Interaction
 * ============================================================================ */

/* Get URL at position (NULL if none) */
const char *cui_richtext_get_link_at(const CUI_RichText *rt, float x, float y);

/* Get character index at position */
int cui_richtext_get_char_at(const CUI_RichText *rt, float x, float y);

/* Get position of character */
void cui_richtext_get_char_pos(const CUI_RichText *rt, int char_index,
                                float *x, float *y);

/* Hit test */
bool cui_richtext_hit_test(const CUI_RichText *rt, float x, float y);

/* ============================================================================
 * Rich Text Selection
 * ============================================================================ */

/* Set selection range */
void cui_richtext_set_selection(CUI_RichText *rt, int start, int end);

/* Get selection range */
void cui_richtext_get_selection(const CUI_RichText *rt, int *start, int *end);

/* Clear selection */
void cui_richtext_clear_selection(CUI_RichText *rt);

/* Get selected text (plain) */
const char *cui_richtext_get_selected_text(const CUI_RichText *rt);

/* ============================================================================
 * Rich Text Node Widget
 * ============================================================================ */

/* Create a rich text node */
CUI_Node *cui_richtext_node_create(CUI_Context *ctx, const char *name,
                                    const char *bbcode);

/* Set BBCode text on node */
void cui_richtext_node_set_text(CUI_Node *node, const char *bbcode);

/* Get BBCode text from node */
const char *cui_richtext_node_get_text(CUI_Node *node);

/* Set link callback for node */
void cui_richtext_node_set_link_callback(CUI_Node *node,
                                          CUI_RichLinkCallback callback,
                                          void *userdata);

/* Set alignment */
void cui_richtext_node_set_alignment(CUI_Node *node, CUI_RichTextAlign alignment);

/* ============================================================================
 * Immediate Mode Rich Text
 * ============================================================================ */

/* Draw BBCode text immediately (parses each call, use for simple cases) */
void cui_rich_label(CUI_Context *ctx, const char *bbcode);

/* Draw with link callback */
bool cui_rich_label_ex(CUI_Context *ctx, const char *bbcode,
                        CUI_RichLinkCallback on_link, void *userdata);

/* ============================================================================
 * Color Name Parsing
 * ============================================================================ */

/* Parse a color name or hex value */
uint32_t cui_richtext_parse_color(const char *color_str);

/* Register a custom color name */
void cui_richtext_register_color(const char *name, uint32_t color);

/* ============================================================================
 * Icon Registry
 * ============================================================================ */

/* Register an icon for use with [icon=name] */
void cui_richtext_register_icon(const char *name, const char *texture_path,
                                 float src_x, float src_y, float src_w, float src_h);

/* Get icon info by name */
bool cui_richtext_get_icon(const char *name, const char **texture_path,
                            float *src_x, float *src_y, float *src_w, float *src_h);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_UI_RICHTEXT_H */
