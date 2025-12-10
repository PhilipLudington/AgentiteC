/*
 * Carbon UI - Immediate Mode UI System
 *
 * Usage:
 *   CUI_Context *ui = cui_init(gpu, width, height, "font.ttf", 16);
 *
 *   // Each frame:
 *   cui_begin_frame(ui, delta_time);
 *   cui_process_event(ui, &event);  // for each SDL event
 *
 *   if (cui_begin_panel(ui, "Menu", 10, 10, 200, 300, 0)) {
 *       cui_label(ui, "Hello!");
 *       if (cui_button(ui, "Click Me")) { ... }
 *       cui_end_panel(ui);
 *   }
 *
 *   cui_end_frame(ui);
 *   cui_render(ui, render_pass);
 */

#ifndef CARBON_UI_H
#define CARBON_UI_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

typedef uint32_t CUI_Id;
#define CUI_ID_NONE ((CUI_Id)0)

typedef struct CUI_Rect {
    float x, y, w, h;
} CUI_Rect;

typedef struct CUI_Color {
    float r, g, b, a;
} CUI_Color;

/* Vertex format for batched rendering */
typedef struct CUI_Vertex {
    float pos[2];       /* Screen position (x, y) */
    float uv[2];        /* Texture coordinates */
    uint32_t color;     /* Packed RGBA (0xAABBGGRR) */
} CUI_Vertex;

/* Persistent widget state (survives across frames) */
typedef struct CUI_WidgetState {
    CUI_Id id;
    float scroll_x, scroll_y;       /* For scrollable regions */
    int cursor_pos;                 /* For text input */
    int selection_start, selection_end;
    bool expanded;                  /* For collapsible headers */
    uint64_t last_frame;            /* For garbage collection */
} CUI_WidgetState;

/* State hash table entry */
typedef struct CUI_StateEntry {
    CUI_WidgetState state;
    struct CUI_StateEntry *next;
} CUI_StateEntry;

/* Layout frame (stackable) */
typedef struct CUI_LayoutFrame {
    CUI_Rect bounds;                /* Available area */
    float cursor_x, cursor_y;       /* Current position */
    float row_height;               /* For horizontal layouts */
    float spacing;
    float padding;
    bool horizontal;                /* true = row, false = column */
    CUI_Rect clip;                  /* Clipping rectangle */
    bool has_clip;
} CUI_LayoutFrame;

/* Theme colors and metrics */
typedef struct CUI_Theme {
    /* Background colors */
    uint32_t bg_panel;
    uint32_t bg_widget;
    uint32_t bg_widget_hover;
    uint32_t bg_widget_active;
    uint32_t bg_widget_disabled;

    /* Border */
    uint32_t border;

    /* Text colors */
    uint32_t text;
    uint32_t text_dim;
    uint32_t text_highlight;
    uint32_t text_disabled;

    /* Accent color (primary interactive color) */
    uint32_t accent;
    uint32_t accent_hover;
    uint32_t accent_active;

    /* Semantic colors */
    uint32_t success;           /* Green - positive actions, confirmations */
    uint32_t success_hover;
    uint32_t warning;           /* Yellow/Orange - caution, attention */
    uint32_t warning_hover;
    uint32_t danger;            /* Red - destructive actions, errors */
    uint32_t danger_hover;
    uint32_t info;              /* Blue - informational, neutral highlights */
    uint32_t info_hover;

    /* Widget-specific colors */
    uint32_t checkbox_check;
    uint32_t slider_track;
    uint32_t slider_grab;
    uint32_t scrollbar;
    uint32_t scrollbar_grab;
    uint32_t progress_fill;     /* Progress bar fill color */
    uint32_t selection;         /* Text selection background */

    /* Metrics */
    float corner_radius;
    float border_width;
    float widget_height;
    float spacing;
    float padding;
    float scrollbar_width;
} CUI_Theme;

/* Input state */
typedef struct CUI_Input {
    float mouse_x, mouse_y;
    float mouse_prev_x, mouse_prev_y;
    bool mouse_down[3];             /* Left, Right, Middle */
    bool mouse_pressed[3];          /* Just pressed this frame */
    bool mouse_released[3];         /* Just released this frame */
    float scroll_x, scroll_y;
    bool keys_down[512];            /* SDL scancode indexed */
    bool keys_pressed[512];
    char text_input[64];            /* UTF-8 text input this frame */
    int text_input_len;
    bool shift, ctrl, alt;
} CUI_Input;

/* ============================================================================
 * Font System (Multi-Font Support with SDF/MSDF)
 * ============================================================================ */

#define CUI_MAX_FONTS 16
#define CUI_FONT_ATLAS_SIZE 512

/* Font type enumeration */
typedef enum CUI_FontType {
    CUI_FONT_BITMAP,    /* Standard bitmap font (stb_truetype) */
    CUI_FONT_SDF,       /* Single-channel signed distance field */
    CUI_FONT_MSDF       /* Multi-channel signed distance field */
} CUI_FontType;

/* Font handle - wraps either bitmap or SDF font (opaque pointer) */
typedef struct CUI_Font CUI_Font;

/* Draw command types */
typedef enum CUI_DrawCmdType {
    CUI_DRAW_CMD_SOLID,         /* Solid color primitives (rects, lines) */
    CUI_DRAW_CMD_BITMAP_TEXT,   /* Bitmap font text */
    CUI_DRAW_CMD_SDF_TEXT,      /* SDF font text */
    CUI_DRAW_CMD_MSDF_TEXT      /* MSDF font text */
} CUI_DrawCmdType;

/* Draw command - represents a batch of primitives sharing texture and layer */
typedef struct CUI_DrawCmd {
    CUI_DrawCmdType type;
    SDL_GPUTexture *texture;    /* Font atlas texture */
    int layer;                  /* Layer for z-ordering (lower = back) */
    uint32_t vertex_offset;     /* Start index in vertex buffer */
    uint32_t index_offset;      /* Start index in index buffer */
    uint32_t vertex_count;      /* Number of vertices */
    uint32_t index_count;       /* Number of indices */
    float sdf_scale;            /* Scale for SDF rendering */
    float sdf_distance_range;   /* Distance range for SDF font */
} CUI_DrawCmd;

#define CUI_MAX_DRAW_CMDS 256
#define CUI_DEFAULT_LAYER 0

/* Table sort specification (forward declared for use in context) */
typedef struct CUI_TableSortSpec {
    int column_index;
    bool descending;
} CUI_TableSortSpec;

/* Multi-select state (forward declared for use in context) */
typedef struct CUI_MultiSelectState {
    int *selected_indices;      /* Array of selected indices */
    int selected_count;         /* Number of selected items */
    int capacity;               /* Capacity of selected_indices array */
    int anchor_index;           /* Anchor for shift-click range selection */
    int last_clicked;           /* Last clicked index */
} CUI_MultiSelectState;

/* Main UI context */
typedef struct CUI_Context {
    /* GPU resources */
    SDL_GPUDevice *gpu;
    SDL_GPUGraphicsPipeline *pipeline;           /* Bitmap/solid pipeline */
    SDL_GPUGraphicsPipeline *sdf_pipeline;       /* SDF text pipeline */
    SDL_GPUGraphicsPipeline *msdf_pipeline;      /* MSDF text pipeline */
    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    SDL_GPUTexture *white_texture;  /* 1x1 white texture for solid primitives */
    SDL_GPUSampler *sampler;

    /* Draw list (per-frame) */
    CUI_Vertex *vertices;
    uint16_t *indices;
    uint32_t vertex_count, index_count;
    uint32_t vertex_capacity, index_capacity;

    /* Draw command queue */
    CUI_DrawCmd *draw_cmds;
    uint32_t draw_cmd_count;
    uint32_t draw_cmd_capacity;

    /* Current draw state */
    SDL_GPUTexture *current_texture; /* Current texture being batched */
    int current_layer;               /* Current layer for new primitives */
    uint32_t cmd_vertex_start;       /* Start of current command's vertices */
    uint32_t cmd_index_start;        /* Start of current command's indices */

    /* Input state */
    CUI_Input input;

    /* Widget interaction state */
    CUI_Id hot;                     /* Hovered widget */
    CUI_Id active;                  /* Mouse-down widget */
    CUI_Id focused;                 /* Keyboard focus */

    /* Persistent state hash table */
    CUI_StateEntry *state_table[256];

    /* Layout stack */
    CUI_LayoutFrame layout_stack[32];
    int layout_depth;

    /* Scissor stack */
    CUI_Rect scissor_stack[16];
    int scissor_depth;

    /* ID stack for scoping */
    CUI_Id id_stack[32];
    int id_stack_depth;

    /* Font registry (multi-font support) */
    CUI_Font *fonts[CUI_MAX_FONTS];
    int font_count;
    CUI_Font *default_font;         /* Default font for widgets */
    CUI_Font *current_font;         /* Currently active font for drawing */

    /* Legacy compatibility - points to default font data */
    void *glyphs; /* ASCII 32-127 (96 chars) - DEPRECATED, use fonts */
    SDL_GPUTexture *font_atlas;     /* DEPRECATED, use fonts */
    float font_size;
    float line_height;
    float ascent;
    int atlas_width, atlas_height;

    /* Theme */
    CUI_Theme theme;

    /* Screen dimensions */
    int width, height;

    /* Frame timing */
    float delta_time;
    uint64_t frame_count;

    /* Dropdown/popup state */
    CUI_Id open_popup;
    CUI_Rect popup_rect;
    int *popup_selected;            /* Pointer to selection value */
    const char **popup_items;       /* Popup items array */
    int popup_count;                /* Number of popup items */
    bool popup_changed;             /* Whether selection changed */

    /* Text input tracking */
    CUI_Id prev_focused;            /* Previous frame's focused widget */
    SDL_Window *window;             /* Window for text input control */

    /* Path building state */
    float *path_points;             /* Array of (x, y) pairs */
    uint32_t path_count;            /* Number of points */
    uint32_t path_capacity;         /* Capacity of path_points array */

    /* Table state (active table during begin/end) */
    struct {
        CUI_Id id;                  /* Current table ID */
        int column_count;           /* Number of columns */
        int current_column;         /* Current column index */
        int current_row;            /* Current row index */
        uint32_t flags;             /* Table flags */
        CUI_Rect bounds;            /* Table bounds */
        float row_height;           /* Height of each row */
        float *column_widths;       /* Array of column widths */
        const char **column_labels; /* Array of column labels */
        uint32_t *column_flags;     /* Array of column flags */
        int columns_setup;          /* Number of columns set up */
        float scroll_x, scroll_y;   /* Scroll position */
        float content_width;        /* Total content width */
        float content_height;       /* Total content height */
        CUI_TableSortSpec sort_spec;/* Current sort specification */
        bool sort_specs_changed;    /* Whether sort changed this frame */
    } table;

    /* Active multi-select state pointer (set during begin/end) */
    CUI_MultiSelectState *multi_select;

    /* Layer system for z-ordering */
    int layer_stack[16];            /* Stack of pushed layers */
    int layer_stack_depth;
} CUI_Context;

/* Panel flags */
#define CUI_PANEL_MOVABLE       (1 << 0)
#define CUI_PANEL_RESIZABLE     (1 << 1)
#define CUI_PANEL_CLOSABLE      (1 << 2)
#define CUI_PANEL_TITLE_BAR     (1 << 3)
#define CUI_PANEL_NO_SCROLLBAR  (1 << 4)
#define CUI_PANEL_BORDER        (1 << 5)

/* Table flags */
#define CUI_TABLE_RESIZABLE     (1 << 0)
#define CUI_TABLE_REORDERABLE   (1 << 1)
#define CUI_TABLE_SORTABLE      (1 << 2)
#define CUI_TABLE_HIDEABLE      (1 << 3)
#define CUI_TABLE_BORDERS       (1 << 4)
#define CUI_TABLE_ROW_HIGHLIGHT (1 << 5)
#define CUI_TABLE_SCROLL_X      (1 << 6)
#define CUI_TABLE_SCROLL_Y      (1 << 7)

/* Table column flags */
#define CUI_TABLE_COLUMN_DEFAULT_SORT   (1 << 0)
#define CUI_TABLE_COLUMN_NO_SORT        (1 << 1)
#define CUI_TABLE_COLUMN_NO_RESIZE      (1 << 2)
#define CUI_TABLE_COLUMN_NO_HIDE        (1 << 3)

/* Color picker flags */
#define CUI_COLORPICKER_NO_ALPHA     (1 << 0)
#define CUI_COLORPICKER_HDR          (1 << 1)
#define CUI_COLORPICKER_WHEEL        (1 << 2)  /* Use color wheel instead of square */
#define CUI_COLORPICKER_INPUT_RGB    (1 << 3)  /* Show RGB input fields */
#define CUI_COLORPICKER_INPUT_HSV    (1 << 4)  /* Show HSV input fields */
#define CUI_COLORPICKER_INPUT_HEX    (1 << 5)  /* Show hex input field */
#define CUI_COLORPICKER_PALETTE      (1 << 6)  /* Show saved color palette */

/* ============================================================================
 * Font Management
 * ============================================================================ */

/* Load a bitmap font (standard TTF rasterization). Returns NULL on failure. */
CUI_Font *cui_font_load(CUI_Context *ctx, const char *path, float size);

/* Load an SDF/MSDF font from pre-generated atlas files (msdf-atlas-gen format).
 * atlas_path: Path to PNG atlas image
 * metrics_path: Path to JSON metrics file
 * Returns NULL on failure. */
CUI_Font *cui_font_load_sdf(CUI_Context *ctx, const char *atlas_path,
                            const char *metrics_path);

/* Unload a font (removes from registry and frees resources) */
void cui_font_unload(CUI_Context *ctx, CUI_Font *font);

/* Get font type (bitmap, SDF, or MSDF) */
CUI_FontType cui_font_get_type(CUI_Font *font);

/* Set the default font for widgets */
void cui_set_default_font(CUI_Context *ctx, CUI_Font *font);

/* Get the default font */
CUI_Font *cui_get_default_font(CUI_Context *ctx);

/* Set the current font for subsequent text drawing */
void cui_set_font(CUI_Context *ctx, CUI_Font *font);

/* Get the current font */
CUI_Font *cui_get_font(CUI_Context *ctx);

/* Get font metrics */
float cui_font_size(CUI_Font *font);
float cui_font_line_height(CUI_Font *font);
float cui_font_ascent(CUI_Font *font);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/* Initialize UI system */
CUI_Context *cui_init(SDL_GPUDevice *gpu, SDL_Window *window, int width, int height,
                      const char *font_path, float font_size);

/* Shutdown UI system */
void cui_shutdown(CUI_Context *ctx);

/* Begin new UI frame (call before widgets) */
void cui_begin_frame(CUI_Context *ctx, float delta_time);

/* End UI frame (call after widgets) */
void cui_end_frame(CUI_Context *ctx);

/* Upload UI data to GPU (call BEFORE render pass begins) */
void cui_upload(CUI_Context *ctx, SDL_GPUCommandBuffer *cmd);

/* Render UI (call DURING render pass) */
void cui_render(CUI_Context *ctx, SDL_GPUCommandBuffer *cmd,
                SDL_GPURenderPass *pass);

/* Process SDL event (returns true if consumed) */
bool cui_process_event(CUI_Context *ctx, const SDL_Event *event);

/* Update screen size */
void cui_set_screen_size(CUI_Context *ctx, int width, int height);

/* ============================================================================
 * ID System
 * ============================================================================ */

/* Generate ID from string */
CUI_Id cui_id(const char *str);

/* Generate ID from string + integer (for loops) */
CUI_Id cui_id_int(const char *str, int n);

/* Push/pop ID prefix for scoping */
void cui_push_id(CUI_Context *ctx, const char *str);
void cui_push_id_int(CUI_Context *ctx, int n);
void cui_pop_id(CUI_Context *ctx);

/* ============================================================================
 * Layout Functions
 * ============================================================================ */

/* Begin horizontal layout */
void cui_begin_row(CUI_Context *ctx);
void cui_begin_row_ex(CUI_Context *ctx, float height, float spacing);

/* Begin vertical layout */
void cui_begin_column(CUI_Context *ctx);
void cui_begin_column_ex(CUI_Context *ctx, float width, float spacing);

/* End layout */
void cui_end_row(CUI_Context *ctx);
void cui_end_column(CUI_Context *ctx);

/* Add spacing */
void cui_spacing(CUI_Context *ctx, float amount);
void cui_separator(CUI_Context *ctx);

/* Stay on same line (for horizontal arrangement) */
void cui_same_line(CUI_Context *ctx);

/* Scrollable region */
void cui_begin_scroll(CUI_Context *ctx, const char *id, float width, float height);
void cui_end_scroll(CUI_Context *ctx);

/* Get available rect from current layout */
CUI_Rect cui_get_available_rect(CUI_Context *ctx);

/* ============================================================================
 * Widgets
 * ============================================================================ */

/* Labels and text */
void cui_label(CUI_Context *ctx, const char *text);
void cui_label_colored(CUI_Context *ctx, const char *text, uint32_t color);

/* Buttons */
bool cui_button(CUI_Context *ctx, const char *label);
bool cui_button_ex(CUI_Context *ctx, const char *label, float width, float height);

/* Semantic button variants (colored by theme semantic colors) */
bool cui_button_primary(CUI_Context *ctx, const char *label);
bool cui_button_success(CUI_Context *ctx, const char *label);
bool cui_button_warning(CUI_Context *ctx, const char *label);
bool cui_button_danger(CUI_Context *ctx, const char *label);
bool cui_button_info(CUI_Context *ctx, const char *label);

/* Toggle widgets */
bool cui_checkbox(CUI_Context *ctx, const char *label, bool *value);
bool cui_radio(CUI_Context *ctx, const char *label, int *value, int option);

/* Sliders */
bool cui_slider_float(CUI_Context *ctx, const char *label, float *value,
                      float min, float max);
bool cui_slider_int(CUI_Context *ctx, const char *label, int *value,
                    int min, int max);

/* Text input */
bool cui_textbox(CUI_Context *ctx, const char *label, char *buffer, int buffer_size);
bool cui_textbox_ex(CUI_Context *ctx, const char *label, char *buffer,
                    int buffer_size, float width);

/* Selection widgets */
bool cui_dropdown(CUI_Context *ctx, const char *label, int *selected,
                  const char **items, int count);
bool cui_listbox(CUI_Context *ctx, const char *label, int *selected,
                 const char **items, int count, float height);

/* Progress display */
void cui_progress_bar(CUI_Context *ctx, float value, float min, float max);
void cui_progress_bar_colored(CUI_Context *ctx, float value, float min, float max,
                               uint32_t fill_color);

/* Collapsible sections */
bool cui_collapsing_header(CUI_Context *ctx, const char *label);

/* Tables */
bool cui_begin_table(CUI_Context *ctx, const char *id, int columns,
                     uint32_t flags, float width, float height);
void cui_table_setup_column(CUI_Context *ctx, const char *label,
                            uint32_t flags, float init_width);
void cui_table_headers_row(CUI_Context *ctx);
void cui_table_next_row(CUI_Context *ctx);
bool cui_table_next_column(CUI_Context *ctx);
bool cui_table_set_column(CUI_Context *ctx, int column);
CUI_TableSortSpec *cui_table_get_sort_specs(CUI_Context *ctx, int *count);
bool cui_table_sort_specs_changed(CUI_Context *ctx);
void cui_end_table(CUI_Context *ctx);

/* Multi-Select */
CUI_MultiSelectState cui_multi_select_create(int capacity);
void cui_multi_select_destroy(CUI_MultiSelectState *state);
void cui_multi_select_clear(CUI_MultiSelectState *state);
bool cui_multi_select_is_selected(CUI_MultiSelectState *state, int index);
void cui_multi_select_begin(CUI_Context *ctx, CUI_MultiSelectState *state);
bool cui_multi_select_item(CUI_Context *ctx, CUI_MultiSelectState *state,
                           int index, bool *is_selected);
void cui_multi_select_end(CUI_Context *ctx);

/* Color Picker */
bool cui_color_picker(CUI_Context *ctx, const char *label,
                      float *rgba, uint32_t flags);
bool cui_color_button(CUI_Context *ctx, const char *label,
                      float *rgba, float size);
bool cui_color_edit3(CUI_Context *ctx, const char *label, float *rgb);
bool cui_color_edit4(CUI_Context *ctx, const char *label, float *rgba);

/* Color conversion utilities */
void cui_rgb_to_hsv(float r, float g, float b, float *h, float *s, float *v);
void cui_hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b);

/* ============================================================================
 * Layer System (Z-Ordering)
 * ============================================================================ */

/* Set current layer for subsequent draw operations (lower = back, higher = front) */
void cui_set_layer(CUI_Context *ctx, int layer);

/* Get current layer */
int cui_get_layer(CUI_Context *ctx);

/* Push/pop layer (for nested layer changes) */
void cui_push_layer(CUI_Context *ctx, int layer);
void cui_pop_layer(CUI_Context *ctx);

/* Legacy API (deprecated - use cui_set_layer instead) */
void cui_draw_split_begin(CUI_Context *ctx, int channel_count);
void cui_draw_set_channel(CUI_Context *ctx, int channel);
void cui_draw_split_merge(CUI_Context *ctx);

/* Panels/Windows */
bool cui_begin_panel(CUI_Context *ctx, const char *name,
                     float x, float y, float w, float h, uint32_t flags);
void cui_end_panel(CUI_Context *ctx);

/* Tooltips */
void cui_tooltip(CUI_Context *ctx, const char *text);

/* ============================================================================
 * Drawing Primitives (Low-level)
 * ============================================================================ */

/* Filled rectangles */
void cui_draw_rect(CUI_Context *ctx, float x, float y, float w, float h,
                   uint32_t color);
void cui_draw_rect_rounded(CUI_Context *ctx, float x, float y, float w, float h,
                           uint32_t color, float radius);

/* Outlined rectangles */
void cui_draw_rect_outline(CUI_Context *ctx, float x, float y, float w, float h,
                           uint32_t color, float thickness);

/* Lines */
void cui_draw_line(CUI_Context *ctx, float x1, float y1, float x2, float y2,
                   uint32_t color, float thickness);

/* Bezier Curves */
void cui_draw_bezier_cubic(CUI_Context *ctx,
                           float x1, float y1,   /* Start point */
                           float cx1, float cy1, /* Control point 1 */
                           float cx2, float cy2, /* Control point 2 */
                           float x2, float y2,   /* End point */
                           uint32_t color, float thickness);

void cui_draw_bezier_quadratic(CUI_Context *ctx,
                               float x1, float y1,  /* Start point */
                               float cx, float cy,  /* Control point */
                               float x2, float y2,  /* End point */
                               uint32_t color, float thickness);

/* Path API for complex shapes */
void cui_path_begin(CUI_Context *ctx);
void cui_path_line_to(CUI_Context *ctx, float x, float y);
void cui_path_bezier_cubic_to(CUI_Context *ctx, float cx1, float cy1,
                               float cx2, float cy2, float x, float y);
void cui_path_bezier_quadratic_to(CUI_Context *ctx, float cx, float cy,
                                   float x, float y);
void cui_path_stroke(CUI_Context *ctx, uint32_t color, float thickness);
void cui_path_fill(CUI_Context *ctx, uint32_t color);

/* Triangles */
void cui_draw_triangle(CUI_Context *ctx,
                       float x0, float y0, float x1, float y1, float x2, float y2,
                       uint32_t color);

/* Text (uses current font set by cui_set_font, or default font) */
float cui_draw_text(CUI_Context *ctx, const char *text, float x, float y,
                    uint32_t color);
void cui_draw_text_clipped(CUI_Context *ctx, const char *text,
                           CUI_Rect bounds, uint32_t color);

/* Text with explicit font */
float cui_draw_text_font(CUI_Context *ctx, CUI_Font *font, const char *text,
                         float x, float y, uint32_t color);
void cui_draw_text_font_clipped(CUI_Context *ctx, CUI_Font *font, const char *text,
                                CUI_Rect bounds, uint32_t color);

/* Scaled text drawing (useful for SDF fonts) */
float cui_draw_text_scaled(CUI_Context *ctx, const char *text, float x, float y,
                           float scale, uint32_t color);
float cui_draw_text_font_scaled(CUI_Context *ctx, CUI_Font *font, const char *text,
                                float x, float y, float scale, uint32_t color);

/* Scissor/clipping */
void cui_push_scissor(CUI_Context *ctx, float x, float y, float w, float h);
void cui_pop_scissor(CUI_Context *ctx);

/* ============================================================================
 * Text Measurement
 * ============================================================================ */

/* Measure text with current/default font */
float cui_text_width(CUI_Context *ctx, const char *text);
float cui_text_height(CUI_Context *ctx);
void cui_text_size(CUI_Context *ctx, const char *text, float *out_w, float *out_h);

/* Measure text with explicit font */
float cui_text_width_font(CUI_Font *font, const char *text);
float cui_text_height_font(CUI_Font *font);
void cui_text_size_font(CUI_Font *font, const char *text, float *out_w, float *out_h);

/* Measure text with explicit font and scale (for SDF fonts) */
float cui_text_width_font_scaled(CUI_Font *font, const char *text, float scale);
float cui_text_height_font_scaled(CUI_Font *font, float scale);
void cui_text_size_font_scaled(CUI_Font *font, const char *text, float scale,
                               float *out_w, float *out_h);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* Color helpers */
uint32_t cui_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
uint32_t cui_rgb(uint8_t r, uint8_t g, uint8_t b);
uint32_t cui_color_lerp(uint32_t a, uint32_t b, float t);
uint32_t cui_color_alpha(uint32_t color, float alpha);
uint32_t cui_color_brighten(uint32_t color, float amount);
uint32_t cui_color_darken(uint32_t color, float amount);

/* Rect helpers */
bool cui_rect_contains(CUI_Rect rect, float x, float y);
CUI_Rect cui_rect_intersect(CUI_Rect a, CUI_Rect b);

/* Get persistent widget state */
CUI_WidgetState *cui_get_state(CUI_Context *ctx, CUI_Id id);

/* ============================================================================
 * Theme System
 * ============================================================================ */

/* Get predefined theme presets */
CUI_Theme cui_theme_dark(void);
CUI_Theme cui_theme_light(void);

/* Get/set current theme */
void cui_set_theme(CUI_Context *ctx, const CUI_Theme *theme);
const CUI_Theme *cui_get_theme(const CUI_Context *ctx);

/* Theme customization helpers */
void cui_theme_set_accent(CUI_Theme *theme, uint32_t color);
void cui_theme_set_semantic_colors(CUI_Theme *theme,
                                    uint32_t success, uint32_t warning,
                                    uint32_t danger, uint32_t info);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_UI_H */
