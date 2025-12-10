/*
 * Agentite UI - Rich Text System
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
 *   AUI_RichText *rt = aui_richtext_parse("[b]Hello[/b] [color=#FF0000]World[/color]!");
 *   aui_richtext_layout(rt, 400);  // max width
 *   aui_richtext_draw(ctx, rt, x, y);
 *   aui_richtext_destroy(rt);
 */

#ifndef AGENTITE_UI_RICHTEXT_H
#define AGENTITE_UI_RICHTEXT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct AUI_Context AUI_Context;
typedef struct AUI_Node AUI_Node;
typedef struct AUI_RichText AUI_RichText;

/* ============================================================================
 * Rich Text Span Types
 * ============================================================================ */

typedef enum AUI_RichTagType {
    AUI_RTAG_TEXT,             /* Plain text */
    AUI_RTAG_BOLD,
    AUI_RTAG_ITALIC,
    AUI_RTAG_UNDERLINE,
    AUI_RTAG_STRIKETHROUGH,
    AUI_RTAG_COLOR,
    AUI_RTAG_SIZE,
    AUI_RTAG_URL,
    AUI_RTAG_IMAGE,
    AUI_RTAG_ICON,
    AUI_RTAG_WAVE,
    AUI_RTAG_SHAKE,
    AUI_RTAG_RAINBOW,
    AUI_RTAG_FADE,
    AUI_RTAG_TYPEWRITER,
} AUI_RichTagType;

/* ============================================================================
 * Rich Text Alignment
 * ============================================================================ */

typedef enum AUI_RichTextAlign {
    AUI_RICH_ALIGN_LEFT,
    AUI_RICH_ALIGN_CENTER,
    AUI_RICH_ALIGN_RIGHT,
    AUI_RICH_ALIGN_JUSTIFY,
} AUI_RichTextAlign;

/* ============================================================================
 * Rich Text Callbacks
 * ============================================================================ */

/* Called when a URL is clicked */
typedef void (*AUI_RichLinkCallback)(const char *url, void *userdata);

/* Called for custom tag handling */
typedef void (*AUI_RichCustomTagCallback)(const char *tag, const char *value,
                                           int start, int end, void *userdata);

/* ============================================================================
 * Rich Text Span
 * ============================================================================ */

typedef struct AUI_RichSpan {
    AUI_RichTagType type;
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
} AUI_RichSpan;

/* ============================================================================
 * Rich Text Line
 * ============================================================================ */

typedef struct AUI_RichLine {
    int start_char;            /* First character index */
    int end_char;              /* Last character index (exclusive) */
    float width;
    float height;
    float baseline;
    float y_offset;            /* Offset from top of text block */
} AUI_RichLine;

/* ============================================================================
 * Rich Text Hotspot (clickable region)
 * ============================================================================ */

typedef struct AUI_RichHotspot {
    float x, y, w, h;
    char url[256];
    int span_index;
} AUI_RichHotspot;

/* ============================================================================
 * Rich Text Configuration
 * ============================================================================ */

typedef struct AUI_RichTextConfig {
    AUI_RichTextAlign alignment;
    float line_height_factor;  /* Multiplier for line height (default 1.2) */
    uint32_t default_color;
    float default_size;
    float max_width;           /* 0 = no wrapping */
    bool selection_enabled;
    bool meta_underlines;      /* Underline URLs */

    /* Callbacks */
    AUI_RichLinkCallback on_link_click;
    void *link_userdata;

    AUI_RichCustomTagCallback on_custom_tag;
    void *custom_tag_userdata;
} AUI_RichTextConfig;

/* ============================================================================
 * Rich Text Parse and Create
 * ============================================================================ */

/* Parse BBCode text into rich text object */
AUI_RichText *aui_richtext_parse(const char *bbcode);

/* Parse with custom config */
AUI_RichText *aui_richtext_parse_ex(const char *bbcode, const AUI_RichTextConfig *config);

/* Create from plain text (no parsing) */
AUI_RichText *aui_richtext_create(const char *plain_text);

/* Destroy rich text object */
void aui_richtext_destroy(AUI_RichText *rt);

/* ============================================================================
 * Rich Text Modification
 * ============================================================================ */

/* Set/get the source BBCode text */
void aui_richtext_set_bbcode(AUI_RichText *rt, const char *bbcode);
const char *aui_richtext_get_bbcode(const AUI_RichText *rt);

/* Get the plain text (tags stripped) */
const char *aui_richtext_get_plain(const AUI_RichText *rt);

/* Get length of plain text */
int aui_richtext_get_length(const AUI_RichText *rt);

/* Append BBCode text */
void aui_richtext_append(AUI_RichText *rt, const char *bbcode);

/* Clear all text */
void aui_richtext_clear(AUI_RichText *rt);

/* ============================================================================
 * Rich Text Layout
 * ============================================================================ */

/* Calculate layout with max width (call after parse/modify) */
void aui_richtext_layout(AUI_RichText *rt, float max_width);

/* Get computed dimensions */
void aui_richtext_get_size(const AUI_RichText *rt, float *width, float *height);

/* Get line count */
int aui_richtext_get_line_count(const AUI_RichText *rt);

/* Get line info */
const AUI_RichLine *aui_richtext_get_line(const AUI_RichText *rt, int index);

/* ============================================================================
 * Rich Text Rendering
 * ============================================================================ */

/* Draw rich text at position */
void aui_richtext_draw(AUI_Context *ctx, AUI_RichText *rt, float x, float y);

/* Draw with custom config */
void aui_richtext_draw_ex(AUI_Context *ctx, AUI_RichText *rt, float x, float y,
                           const AUI_RichTextConfig *config);

/* Update animation state (call each frame for animated text) */
void aui_richtext_update(AUI_RichText *rt, float delta_time);

/* ============================================================================
 * Rich Text Interaction
 * ============================================================================ */

/* Get URL at position (NULL if none) */
const char *aui_richtext_get_link_at(const AUI_RichText *rt, float x, float y);

/* Get character index at position */
int aui_richtext_get_char_at(const AUI_RichText *rt, float x, float y);

/* Get position of character */
void aui_richtext_get_char_pos(const AUI_RichText *rt, int char_index,
                                float *x, float *y);

/* Hit test */
bool aui_richtext_hit_test(const AUI_RichText *rt, float x, float y);

/* ============================================================================
 * Rich Text Selection
 * ============================================================================ */

/* Set selection range */
void aui_richtext_set_selection(AUI_RichText *rt, int start, int end);

/* Get selection range */
void aui_richtext_get_selection(const AUI_RichText *rt, int *start, int *end);

/* Clear selection */
void aui_richtext_clear_selection(AUI_RichText *rt);

/* Get selected text (plain) */
const char *aui_richtext_get_selected_text(const AUI_RichText *rt);

/* ============================================================================
 * Rich Text Node Widget
 * ============================================================================ */

/* Create a rich text node */
AUI_Node *aui_richtext_node_create(AUI_Context *ctx, const char *name,
                                    const char *bbcode);

/* Set BBCode text on node */
void aui_richtext_node_set_text(AUI_Node *node, const char *bbcode);

/* Get BBCode text from node */
const char *aui_richtext_node_get_text(AUI_Node *node);

/* Set link callback for node */
void aui_richtext_node_set_link_callback(AUI_Node *node,
                                          AUI_RichLinkCallback callback,
                                          void *userdata);

/* Set alignment */
void aui_richtext_node_set_alignment(AUI_Node *node, AUI_RichTextAlign alignment);

/* ============================================================================
 * Immediate Mode Rich Text
 * ============================================================================ */

/* Draw BBCode text immediately (parses each call, use for simple cases) */
void aui_rich_label(AUI_Context *ctx, const char *bbcode);

/* Draw with link callback */
bool aui_rich_label_ex(AUI_Context *ctx, const char *bbcode,
                        AUI_RichLinkCallback on_link, void *userdata);

/* ============================================================================
 * Color Name Parsing
 * ============================================================================ */

/* Parse a color name or hex value */
uint32_t aui_richtext_parse_color(const char *color_str);

/* Register a custom color name */
void aui_richtext_register_color(const char *name, uint32_t color);

/* ============================================================================
 * Icon Registry
 * ============================================================================ */

/* Register an icon for use with [icon=name] */
void aui_richtext_register_icon(const char *name, const char *texture_path,
                                 float src_x, float src_y, float src_w, float src_h);

/* Get icon info by name */
bool aui_richtext_get_icon(const char *name, const char **texture_path,
                            float *src_x, float *src_y, float *src_w, float *src_h);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_UI_RICHTEXT_H */
