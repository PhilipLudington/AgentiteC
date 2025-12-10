/*
 * Agentite UI - Rich Text System Implementation
 */

#include "agentite/ui_richtext.h"
#include "agentite/ui.h"
#include "agentite/ui_node.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_SPANS 128
#define MAX_LINES 256
#define MAX_HOTSPOTS 32
#define MAX_PLAIN_TEXT 4096
#define MAX_BBCODE_TEXT 8192
#define MAX_TAG_STACK 16

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Rich Text Structure
 * ============================================================================ */

struct AUI_RichText {
    /* Source text */
    char *bbcode;
    int bbcode_len;

    /* Parsed plain text */
    char *plain;
    int plain_len;

    /* Spans */
    AUI_RichSpan spans[MAX_SPANS];
    int span_count;

    /* Layout */
    AUI_RichLine lines[MAX_LINES];
    int line_count;
    float total_width;
    float total_height;
    bool layout_valid;

    /* Hotspots (clickable regions) */
    AUI_RichHotspot hotspots[MAX_HOTSPOTS];
    int hotspot_count;

    /* Selection */
    int selection_start;
    int selection_end;

    /* Animation */
    float anim_time;

    /* Config */
    AUI_RichTextConfig config;
};

/* ============================================================================
 * Named Colors
 * ============================================================================ */

typedef struct {
    const char *name;
    uint32_t color;
} AUI_NamedColor;

static AUI_NamedColor s_named_colors[] = {
    {"black",   0xFF000000},
    {"white",   0xFFFFFFFF},
    {"red",     0xFF0000FF},
    {"green",   0xFF00FF00},
    {"blue",    0xFFFF0000},
    {"yellow",  0xFF00FFFF},
    {"cyan",    0xFFFFFF00},
    {"magenta", 0xFFFF00FF},
    {"orange",  0xFF00A5FF},
    {"purple",  0xFF800080},
    {"pink",    0xFFCBC0FF},
    {"gray",    0xFF808080},
    {"grey",    0xFF808080},
    {"gold",    0xFF00D7FF},
    {"silver",  0xFFC0C0C0},
    {NULL, 0}
};

/* Custom registered colors */
static AUI_NamedColor s_custom_colors[32];
static int s_custom_color_count = 0;

/* ============================================================================
 * Icon Registry
 * ============================================================================ */

typedef struct {
    char name[64];
    char texture_path[256];
    float src_x, src_y, src_w, src_h;
} AUI_IconEntry;

static AUI_IconEntry s_icons[64];
static int s_icon_count = 0;

/* ============================================================================
 * Color Parsing
 * ============================================================================ */

uint32_t aui_richtext_parse_color(const char *color_str)
{
    if (!color_str) return 0xFFFFFFFF;

    /* Hex color (#RGB, #RRGGBB, #AARRGGBB) */
    if (color_str[0] == '#') {
        const char *hex = color_str + 1;
        int len = (int)strlen(hex);
        uint32_t value = 0;

        for (int i = 0; i < len; i++) {
            int digit = 0;
            char c = hex[i];
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            value = (value << 4) | digit;
        }

        if (len == 3) {
            /* #RGB -> #RRGGBB */
            uint32_t r = (value >> 8) & 0xF;
            uint32_t g = (value >> 4) & 0xF;
            uint32_t b = value & 0xF;
            return 0xFF000000 | (b << 20) | (b << 16) | (g << 12) | (g << 8) | (r << 4) | r;
        } else if (len == 6) {
            /* #RRGGBB -> ABGR */
            uint32_t r = (value >> 16) & 0xFF;
            uint32_t g = (value >> 8) & 0xFF;
            uint32_t b = value & 0xFF;
            return 0xFF000000 | (b << 16) | (g << 8) | r;
        } else if (len == 8) {
            /* #AARRGGBB -> ABGR */
            uint32_t a = (value >> 24) & 0xFF;
            uint32_t r = (value >> 16) & 0xFF;
            uint32_t g = (value >> 8) & 0xFF;
            uint32_t b = value & 0xFF;
            return (a << 24) | (b << 16) | (g << 8) | r;
        }
    }

    /* Named color */
    for (int i = 0; s_named_colors[i].name; i++) {
        if (strcasecmp(color_str, s_named_colors[i].name) == 0) {
            return s_named_colors[i].color;
        }
    }

    /* Custom color */
    for (int i = 0; i < s_custom_color_count; i++) {
        if (strcasecmp(color_str, s_custom_colors[i].name) == 0) {
            return s_custom_colors[i].color;
        }
    }

    return 0xFFFFFFFF;
}

void aui_richtext_register_color(const char *name, uint32_t color)
{
    if (!name || s_custom_color_count >= 32) return;

    AUI_NamedColor *nc = &s_custom_colors[s_custom_color_count++];
    nc->name = strdup(name);
    nc->color = color;
}

/* ============================================================================
 * Icon Registry
 * ============================================================================ */

void aui_richtext_register_icon(const char *name, const char *texture_path,
                                 float src_x, float src_y, float src_w, float src_h)
{
    if (!name || !texture_path || s_icon_count >= 64) return;

    AUI_IconEntry *ie = &s_icons[s_icon_count++];
    strncpy(ie->name, name, sizeof(ie->name) - 1);
    strncpy(ie->texture_path, texture_path, sizeof(ie->texture_path) - 1);
    ie->src_x = src_x;
    ie->src_y = src_y;
    ie->src_w = src_w;
    ie->src_h = src_h;
}

bool aui_richtext_get_icon(const char *name, const char **texture_path,
                            float *src_x, float *src_y, float *src_w, float *src_h)
{
    if (!name) return false;

    for (int i = 0; i < s_icon_count; i++) {
        if (strcmp(s_icons[i].name, name) == 0) {
            if (texture_path) *texture_path = s_icons[i].texture_path;
            if (src_x) *src_x = s_icons[i].src_x;
            if (src_y) *src_y = s_icons[i].src_y;
            if (src_w) *src_w = s_icons[i].src_w;
            if (src_h) *src_h = s_icons[i].src_h;
            return true;
        }
    }
    return false;
}

/* ============================================================================
 * BBCode Parser
 * ============================================================================ */

typedef struct {
    AUI_RichTagType type;
    int plain_start;           /* Start position in plain text */
    union {
        uint32_t color;
        float size;
        char url[256];
    };
} TagStackEntry;

static bool aui_parse_tag(const char *tag, int tag_len, AUI_RichTagType *type,
                          char *value, int value_size, bool *is_close)
{
    *is_close = (tag_len > 0 && tag[0] == '/');
    if (*is_close) {
        tag++;
        tag_len--;
    }

    /* Find = for value */
    const char *eq = (const char *)memchr(tag, '=', tag_len);
    int name_len = eq ? (int)(eq - tag) : tag_len;

    if (value && eq) {
        int val_len = tag_len - name_len - 1;
        if (val_len > value_size - 1) val_len = value_size - 1;
        memcpy(value, eq + 1, val_len);
        value[val_len] = '\0';
    } else if (value) {
        value[0] = '\0';
    }

    /* Match tag name */
    if (name_len == 1 && tag[0] == 'b') { *type = AUI_RTAG_BOLD; return true; }
    if (name_len == 1 && tag[0] == 'i') { *type = AUI_RTAG_ITALIC; return true; }
    if (name_len == 1 && tag[0] == 'u') { *type = AUI_RTAG_UNDERLINE; return true; }
    if (name_len == 1 && tag[0] == 's') { *type = AUI_RTAG_STRIKETHROUGH; return true; }
    if (name_len == 5 && strncasecmp(tag, "color", 5) == 0) { *type = AUI_RTAG_COLOR; return true; }
    if (name_len == 4 && strncasecmp(tag, "size", 4) == 0) { *type = AUI_RTAG_SIZE; return true; }
    if (name_len == 3 && strncasecmp(tag, "url", 3) == 0) { *type = AUI_RTAG_URL; return true; }
    if (name_len == 3 && strncasecmp(tag, "img", 3) == 0) { *type = AUI_RTAG_IMAGE; return true; }
    if (name_len == 4 && strncasecmp(tag, "icon", 4) == 0) { *type = AUI_RTAG_ICON; return true; }
    if (name_len == 4 && strncasecmp(tag, "wave", 4) == 0) { *type = AUI_RTAG_WAVE; return true; }
    if (name_len == 5 && strncasecmp(tag, "shake", 5) == 0) { *type = AUI_RTAG_SHAKE; return true; }
    if (name_len == 7 && strncasecmp(tag, "rainbow", 7) == 0) { *type = AUI_RTAG_RAINBOW; return true; }
    if (name_len == 4 && strncasecmp(tag, "fade", 4) == 0) { *type = AUI_RTAG_FADE; return true; }

    return false;
}

static void aui_richtext_parse_internal(AUI_RichText *rt, const char *bbcode)
{
    if (!rt || !bbcode) return;

    /* Reset */
    rt->span_count = 0;
    rt->plain_len = 0;
    rt->layout_valid = false;

    /* Allocate buffers if needed */
    if (!rt->plain) {
        rt->plain = (char *)malloc(MAX_PLAIN_TEXT);
    }
    if (!rt->bbcode) {
        rt->bbcode = (char *)malloc(MAX_BBCODE_TEXT);
    }

    /* Copy bbcode */
    rt->bbcode_len = (int)strlen(bbcode);
    if (rt->bbcode_len > MAX_BBCODE_TEXT - 1) {
        rt->bbcode_len = MAX_BBCODE_TEXT - 1;
    }
    memcpy(rt->bbcode, bbcode, rt->bbcode_len);
    rt->bbcode[rt->bbcode_len] = '\0';

    /* Parse */
    TagStackEntry stack[MAX_TAG_STACK];
    int stack_depth = 0;

    const char *p = bbcode;
    while (*p && rt->plain_len < MAX_PLAIN_TEXT - 1) {
        if (*p == '[') {
            /* Find closing ] */
            const char *end = strchr(p + 1, ']');
            if (!end) {
                /* No closing bracket, treat as text */
                rt->plain[rt->plain_len++] = *p++;
                continue;
            }

            /* Parse tag */
            AUI_RichTagType type;
            char value[256];
            bool is_close;
            int tag_len = (int)(end - p - 1);

            if (aui_parse_tag(p + 1, tag_len, &type, value, sizeof(value), &is_close)) {
                if (is_close) {
                    /* Find matching open tag */
                    for (int i = stack_depth - 1; i >= 0; i--) {
                        if (stack[i].type == type) {
                            /* Create span */
                            if (rt->span_count < MAX_SPANS) {
                                AUI_RichSpan *span = &rt->spans[rt->span_count++];
                                span->type = type;
                                span->start = stack[i].plain_start;
                                span->end = rt->plain_len;

                                /* Copy type-specific data */
                                switch (type) {
                                    case AUI_RTAG_COLOR:
                                        span->color = stack[i].color;
                                        break;
                                    case AUI_RTAG_SIZE:
                                        span->size = stack[i].size;
                                        break;
                                    case AUI_RTAG_URL:
                                        strncpy(span->link.url, stack[i].url,
                                                sizeof(span->link.url) - 1);
                                        break;
                                    default:
                                        break;
                                }
                            }

                            /* Remove from stack */
                            stack_depth = i;
                            break;
                        }
                    }
                } else {
                    /* Push to stack */
                    if (stack_depth < MAX_TAG_STACK) {
                        TagStackEntry *entry = &stack[stack_depth++];
                        entry->type = type;
                        entry->plain_start = rt->plain_len;

                        switch (type) {
                            case AUI_RTAG_COLOR:
                                entry->color = aui_richtext_parse_color(value);
                                break;
                            case AUI_RTAG_SIZE:
                                entry->size = (float)atof(value);
                                break;
                            case AUI_RTAG_URL:
                                strncpy(entry->url, value, sizeof(entry->url) - 1);
                                break;
                            case AUI_RTAG_ICON:
                                /* Icon is self-closing, add placeholder char */
                                if (rt->span_count < MAX_SPANS) {
                                    AUI_RichSpan *span = &rt->spans[rt->span_count++];
                                    span->type = AUI_RTAG_ICON;
                                    span->start = rt->plain_len;
                                    span->end = rt->plain_len + 1;
                                    strncpy(span->icon.name, value,
                                            sizeof(span->icon.name) - 1);
                                    span->icon.size = 16;  /* Default */
                                }
                                rt->plain[rt->plain_len++] = ' ';  /* Placeholder */
                                stack_depth--;  /* Pop immediately */
                                break;
                            default:
                                break;
                        }
                    }
                }

                p = end + 1;
                continue;
            }
        }

        /* Regular character */
        rt->plain[rt->plain_len++] = *p++;
    }

    rt->plain[rt->plain_len] = '\0';
}

/* ============================================================================
 * Rich Text Creation
 * ============================================================================ */

AUI_RichText *aui_richtext_parse(const char *bbcode)
{
    return aui_richtext_parse_ex(bbcode, NULL);
}

AUI_RichText *aui_richtext_parse_ex(const char *bbcode, const AUI_RichTextConfig *config)
{
    AUI_RichText *rt = (AUI_RichText *)calloc(1, sizeof(AUI_RichText));
    if (!rt) return NULL;

    /* Set default config */
    rt->config.alignment = AUI_RICH_ALIGN_LEFT;
    rt->config.line_height_factor = 1.2f;
    rt->config.default_color = 0xFFFFFFFF;
    rt->config.default_size = 16;
    rt->config.meta_underlines = true;

    if (config) {
        rt->config = *config;
    }

    if (bbcode) {
        aui_richtext_parse_internal(rt, bbcode);
    }

    return rt;
}

AUI_RichText *aui_richtext_create(const char *plain_text)
{
    AUI_RichText *rt = (AUI_RichText *)calloc(1, sizeof(AUI_RichText));
    if (!rt) return NULL;

    rt->config.alignment = AUI_RICH_ALIGN_LEFT;
    rt->config.line_height_factor = 1.2f;
    rt->config.default_color = 0xFFFFFFFF;
    rt->config.default_size = 16;

    if (plain_text) {
        rt->plain_len = (int)strlen(plain_text);
        if (rt->plain_len > MAX_PLAIN_TEXT - 1) {
            rt->plain_len = MAX_PLAIN_TEXT - 1;
        }
        rt->plain = (char *)malloc(rt->plain_len + 1);
        memcpy(rt->plain, plain_text, rt->plain_len);
        rt->plain[rt->plain_len] = '\0';
    }

    return rt;
}

void aui_richtext_destroy(AUI_RichText *rt)
{
    if (!rt) return;
    free(rt->bbcode);
    free(rt->plain);
    free(rt);
}

/* ============================================================================
 * Rich Text Modification
 * ============================================================================ */

void aui_richtext_set_bbcode(AUI_RichText *rt, const char *bbcode)
{
    if (rt) {
        aui_richtext_parse_internal(rt, bbcode);
    }
}

const char *aui_richtext_get_bbcode(const AUI_RichText *rt)
{
    return rt ? rt->bbcode : "";
}

const char *aui_richtext_get_plain(const AUI_RichText *rt)
{
    return rt ? rt->plain : "";
}

int aui_richtext_get_length(const AUI_RichText *rt)
{
    return rt ? rt->plain_len : 0;
}

void aui_richtext_append(AUI_RichText *rt, const char *bbcode)
{
    if (!rt || !bbcode) return;

    /* Simple append - reparse everything */
    int old_len = rt->bbcode_len;
    int add_len = (int)strlen(bbcode);

    if (old_len + add_len >= MAX_BBCODE_TEXT) {
        add_len = MAX_BBCODE_TEXT - old_len - 1;
    }

    if (add_len > 0) {
        memcpy(rt->bbcode + old_len, bbcode, add_len);
        rt->bbcode[old_len + add_len] = '\0';
        aui_richtext_parse_internal(rt, rt->bbcode);
    }
}

void aui_richtext_clear(AUI_RichText *rt)
{
    if (!rt) return;
    if (rt->bbcode) rt->bbcode[0] = '\0';
    if (rt->plain) rt->plain[0] = '\0';
    rt->bbcode_len = 0;
    rt->plain_len = 0;
    rt->span_count = 0;
    rt->line_count = 0;
    rt->layout_valid = false;
}

/* ============================================================================
 * Rich Text Layout
 * ============================================================================ */

void aui_richtext_layout(AUI_RichText *rt, float max_width)
{
    if (!rt) return;

    rt->config.max_width = max_width;
    rt->line_count = 0;
    rt->total_width = 0;
    rt->total_height = 0;
    rt->hotspot_count = 0;

    if (rt->plain_len == 0) {
        rt->layout_valid = true;
        return;
    }

    /* Simple line-breaking layout */
    /* TODO: Use actual font metrics from AUI_Context */
    float char_width = 8;   /* Placeholder */
    float line_height = rt->config.default_size * rt->config.line_height_factor;

    int line_start = 0;
    float line_width = 0;
    float y = 0;

    for (int i = 0; i <= rt->plain_len && rt->line_count < MAX_LINES; i++) {
        bool end_line = false;
        bool at_end = (i == rt->plain_len);

        if (at_end) {
            end_line = true;
        } else if (rt->plain[i] == '\n') {
            end_line = true;
        } else if (max_width > 0 && line_width + char_width > max_width) {
            /* Word wrap - find last space */
            int wrap_at = i;
            for (int j = i - 1; j > line_start; j--) {
                if (rt->plain[j] == ' ') {
                    wrap_at = j;
                    break;
                }
            }
            if (wrap_at > line_start) {
                i = wrap_at;
            }
            end_line = true;
        }

        if (end_line) {
            AUI_RichLine *line = &rt->lines[rt->line_count++];
            line->start_char = line_start;
            line->end_char = i;
            line->width = line_width;
            line->height = line_height;
            line->baseline = rt->config.default_size;
            line->y_offset = y;

            if (line_width > rt->total_width) {
                rt->total_width = line_width;
            }

            y += line_height;
            line_start = (at_end) ? i : i + 1;
            line_width = 0;
        } else {
            line_width += char_width;
        }
    }

    rt->total_height = y;

    /* Build hotspots for URLs */
    for (int i = 0; i < rt->span_count && rt->hotspot_count < MAX_HOTSPOTS; i++) {
        AUI_RichSpan *span = &rt->spans[i];
        if (span->type != AUI_RTAG_URL) continue;

        /* Find lines that contain this span */
        for (int j = 0; j < rt->line_count; j++) {
            AUI_RichLine *line = &rt->lines[j];
            if (line->end_char <= span->start) continue;
            if (line->start_char >= span->end) break;

            /* Calculate hotspot rect */
            int start = (span->start > line->start_char) ? span->start : line->start_char;
            int end = (span->end < line->end_char) ? span->end : line->end_char;

            AUI_RichHotspot *hs = &rt->hotspots[rt->hotspot_count++];
            hs->x = (start - line->start_char) * char_width;
            hs->y = line->y_offset;
            hs->w = (end - start) * char_width;
            hs->h = line->height;
            strncpy(hs->url, span->link.url, sizeof(hs->url) - 1);
            hs->span_index = i;
        }
    }

    rt->layout_valid = true;
}

void aui_richtext_get_size(const AUI_RichText *rt, float *width, float *height)
{
    if (width) *width = rt ? rt->total_width : 0;
    if (height) *height = rt ? rt->total_height : 0;
}

int aui_richtext_get_line_count(const AUI_RichText *rt)
{
    return rt ? rt->line_count : 0;
}

const AUI_RichLine *aui_richtext_get_line(const AUI_RichText *rt, int index)
{
    if (!rt || index < 0 || index >= rt->line_count) return NULL;
    return &rt->lines[index];
}

/* ============================================================================
 * Rich Text Rendering
 * ============================================================================ */

void aui_richtext_draw(AUI_Context *ctx, AUI_RichText *rt, float x, float y)
{
    aui_richtext_draw_ex(ctx, rt, x, y, NULL);
}

void aui_richtext_draw_ex(AUI_Context *ctx, AUI_RichText *rt, float x, float y,
                           const AUI_RichTextConfig *config)
{
    if (!ctx || !rt || rt->plain_len == 0) return;

    /* Ensure layout is valid */
    if (!rt->layout_valid) {
        aui_richtext_layout(rt, rt->config.max_width);
    }

    AUI_RichTextConfig cfg = config ? *config : rt->config;

    /* For each line */
    for (int li = 0; li < rt->line_count; li++) {
        AUI_RichLine *line = &rt->lines[li];
        float lx = x;
        float ly = y + line->y_offset;

        /* Apply alignment */
        if (cfg.alignment == AUI_RICH_ALIGN_CENTER) {
            lx += (rt->total_width - line->width) / 2;
        } else if (cfg.alignment == AUI_RICH_ALIGN_RIGHT) {
            lx += rt->total_width - line->width;
        }

        /* Get active spans for this line */
        uint32_t color = cfg.default_color;
        bool bold = false, italic = false, underline = false, strikethrough = false;
        bool wave = false, shake = false, rainbow = false;

        for (int si = 0; si < rt->span_count; si++) {
            AUI_RichSpan *span = &rt->spans[si];
            if (span->end <= line->start_char) continue;
            if (span->start >= line->end_char) break;

            /* Apply span effects */
            switch (span->type) {
                case AUI_RTAG_BOLD: bold = true; break;
                case AUI_RTAG_ITALIC: italic = true; break;
                case AUI_RTAG_UNDERLINE: underline = true; break;
                case AUI_RTAG_STRIKETHROUGH: strikethrough = true; break;
                case AUI_RTAG_COLOR: color = span->color; break;
                case AUI_RTAG_WAVE: wave = true; break;
                case AUI_RTAG_SHAKE: shake = true; break;
                case AUI_RTAG_RAINBOW: rainbow = true; break;
                case AUI_RTAG_URL:
                    if (cfg.meta_underlines) underline = true;
                    color = ctx->theme.accent;
                    break;
                default: break;
            }
        }

        /* Draw characters */
        float char_w = 8;  /* Placeholder - use actual font metrics */
        float cx = lx;

        for (int ci = line->start_char; ci < line->end_char; ci++) {
            char ch = rt->plain[ci];
            float cy = ly;

            /* Apply animations */
            if (wave) {
                cy += sinf(rt->anim_time * 5 + ci * 0.5f) * 3;
            }
            if (shake) {
                cx += ((rand() % 100) / 100.0f - 0.5f) * 2;
                cy += ((rand() % 100) / 100.0f - 0.5f) * 2;
            }
            if (rainbow) {
                float hue = fmodf(rt->anim_time + ci * 0.1f, 1.0f);
                /* HSV to RGB conversion - simplified */
                int hi = (int)(hue * 6) % 6;
                float f = hue * 6 - hi;
                uint8_t v = 255;
                uint8_t p = 0;
                uint8_t q = (uint8_t)(255 * (1 - f));
                uint8_t t = (uint8_t)(255 * f);
                uint8_t r, g, b;
                switch (hi) {
                    case 0: r = v; g = t; b = p; break;
                    case 1: r = q; g = v; b = p; break;
                    case 2: r = p; g = v; b = t; break;
                    case 3: r = p; g = q; b = v; break;
                    case 4: r = t; g = p; b = v; break;
                    default: r = v; g = p; b = q; break;
                }
                color = 0xFF000000 | (b << 16) | (g << 8) | r;
            }

            /* Draw character */
            char str[2] = {ch, '\0'};
            aui_draw_text(ctx, str, cx, cy, color);

            /* Draw underline */
            if (underline) {
                aui_draw_rect(ctx, cx, cy + cfg.default_size + 2, char_w, 1, color);
            }

            /* Draw strikethrough */
            if (strikethrough) {
                aui_draw_rect(ctx, cx, cy + cfg.default_size / 2, char_w, 1, color);
            }

            cx += char_w;
        }
    }

    /* Draw selection */
    if (rt->selection_start != rt->selection_end) {
        int sel_start = rt->selection_start < rt->selection_end ?
                        rt->selection_start : rt->selection_end;
        int sel_end = rt->selection_start > rt->selection_end ?
                      rt->selection_start : rt->selection_end;

        float char_w = 8;  /* Placeholder */

        for (int li = 0; li < rt->line_count; li++) {
            AUI_RichLine *line = &rt->lines[li];
            if (line->end_char <= sel_start) continue;
            if (line->start_char >= sel_end) break;

            int start = (sel_start > line->start_char) ? sel_start : line->start_char;
            int end = (sel_end < line->end_char) ? sel_end : line->end_char;

            float sx = x + (start - line->start_char) * char_w;
            float sy = y + line->y_offset;
            float sw = (end - start) * char_w;

            aui_draw_rect(ctx, sx, sy, sw, line->height, ctx->theme.selection);
        }
    }
}

void aui_richtext_update(AUI_RichText *rt, float delta_time)
{
    if (rt) {
        rt->anim_time += delta_time;
    }
}

/* ============================================================================
 * Rich Text Interaction
 * ============================================================================ */

const char *aui_richtext_get_link_at(const AUI_RichText *rt, float x, float y)
{
    if (!rt) return NULL;

    for (int i = 0; i < rt->hotspot_count; i++) {
        const AUI_RichHotspot *hs = &rt->hotspots[i];
        if (x >= hs->x && x < hs->x + hs->w &&
            y >= hs->y && y < hs->y + hs->h) {
            return hs->url;
        }
    }
    return NULL;
}

int aui_richtext_get_char_at(const AUI_RichText *rt, float x, float y)
{
    if (!rt) return -1;

    float char_w = 8;  /* Placeholder */

    for (int li = 0; li < rt->line_count; li++) {
        const AUI_RichLine *line = &rt->lines[li];
        if (y < line->y_offset) continue;
        if (y >= line->y_offset + line->height) continue;

        int char_offset = (int)(x / char_w);
        int char_index = line->start_char + char_offset;
        if (char_index > line->end_char) char_index = line->end_char;
        return char_index;
    }

    return -1;
}

void aui_richtext_get_char_pos(const AUI_RichText *rt, int char_index,
                                float *x, float *y)
{
    if (!rt || x == NULL || y == NULL) return;

    float char_w = 8;  /* Placeholder */

    for (int li = 0; li < rt->line_count; li++) {
        const AUI_RichLine *line = &rt->lines[li];
        if (char_index >= line->start_char && char_index <= line->end_char) {
            *x = (char_index - line->start_char) * char_w;
            *y = line->y_offset;
            return;
        }
    }

    *x = 0;
    *y = 0;
}

bool aui_richtext_hit_test(const AUI_RichText *rt, float x, float y)
{
    if (!rt) return false;
    return x >= 0 && x < rt->total_width && y >= 0 && y < rt->total_height;
}

/* ============================================================================
 * Selection
 * ============================================================================ */

void aui_richtext_set_selection(AUI_RichText *rt, int start, int end)
{
    if (rt) {
        rt->selection_start = start;
        rt->selection_end = end;
    }
}

void aui_richtext_get_selection(const AUI_RichText *rt, int *start, int *end)
{
    if (start) *start = rt ? rt->selection_start : 0;
    if (end) *end = rt ? rt->selection_end : 0;
}

void aui_richtext_clear_selection(AUI_RichText *rt)
{
    if (rt) {
        rt->selection_start = 0;
        rt->selection_end = 0;
    }
}

const char *aui_richtext_get_selected_text(const AUI_RichText *rt)
{
    static char buffer[1024];
    if (!rt || rt->selection_start == rt->selection_end) {
        buffer[0] = '\0';
        return buffer;
    }

    int start = rt->selection_start < rt->selection_end ?
                rt->selection_start : rt->selection_end;
    int end = rt->selection_start > rt->selection_end ?
              rt->selection_start : rt->selection_end;

    int len = end - start;
    if (len > (int)sizeof(buffer) - 1) len = sizeof(buffer) - 1;

    memcpy(buffer, rt->plain + start, len);
    buffer[len] = '\0';
    return buffer;
}

/* ============================================================================
 * Rich Text Node Widget
 * ============================================================================ */

AUI_Node *aui_richtext_node_create(AUI_Context *ctx, const char *name,
                                    const char *bbcode)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_RICHTEXT, name);
    if (node && bbcode) {
        /* Store rich text in custom_data */
        node->custom_data = aui_richtext_parse(bbcode);
    }
    return node;
}

void aui_richtext_node_set_text(AUI_Node *node, const char *bbcode)
{
    if (!node || node->type != AUI_NODE_RICHTEXT) return;

    AUI_RichText *rt = (AUI_RichText *)node->custom_data;
    if (!rt) {
        rt = aui_richtext_parse(bbcode);
        node->custom_data = rt;
    } else {
        aui_richtext_set_bbcode(rt, bbcode);
    }
}

const char *aui_richtext_node_get_text(AUI_Node *node)
{
    if (!node || node->type != AUI_NODE_RICHTEXT) return "";
    AUI_RichText *rt = (AUI_RichText *)node->custom_data;
    return rt ? aui_richtext_get_bbcode(rt) : "";
}

void aui_richtext_node_set_link_callback(AUI_Node *node,
                                          AUI_RichLinkCallback callback,
                                          void *userdata)
{
    if (!node || node->type != AUI_NODE_RICHTEXT) return;
    AUI_RichText *rt = (AUI_RichText *)node->custom_data;
    if (rt) {
        rt->config.on_link_click = callback;
        rt->config.link_userdata = userdata;
    }
}

void aui_richtext_node_set_alignment(AUI_Node *node, AUI_RichTextAlign alignment)
{
    if (!node || node->type != AUI_NODE_RICHTEXT) return;
    AUI_RichText *rt = (AUI_RichText *)node->custom_data;
    if (rt) {
        rt->config.alignment = alignment;
        rt->layout_valid = false;
    }
}

/* ============================================================================
 * Immediate Mode
 * ============================================================================ */

void aui_rich_label(AUI_Context *ctx, const char *bbcode)
{
    aui_rich_label_ex(ctx, bbcode, NULL, NULL);
}

bool aui_rich_label_ex(AUI_Context *ctx, const char *bbcode,
                        AUI_RichLinkCallback on_link, void *userdata)
{
    if (!ctx || !bbcode) return false;

    /* Parse and render (not efficient for per-frame, but convenient) */
    AUI_RichText *rt = aui_richtext_parse(bbcode);
    if (!rt) return false;

    rt->config.on_link_click = on_link;
    rt->config.link_userdata = userdata;

    /* Get current layout position from context */
    /* TODO: Integrate with immediate mode layout system */
    float x = 10, y = 10;  /* Placeholder */

    aui_richtext_layout(rt, 0);  /* No wrapping */
    aui_richtext_draw(ctx, rt, x, y);

    /* Check for link clicks */
    bool clicked = false;
    if (ctx->input.mouse_pressed[0]) {
        const char *link = aui_richtext_get_link_at(rt,
            ctx->input.mouse_x - x, ctx->input.mouse_y - y);
        if (link && on_link) {
            on_link(link, userdata);
            clicked = true;
        }
    }

    aui_richtext_destroy(rt);
    return clicked;
}
