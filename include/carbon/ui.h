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
    uint32_t bg_panel;
    uint32_t bg_widget;
    uint32_t bg_widget_hover;
    uint32_t bg_widget_active;
    uint32_t bg_widget_disabled;
    uint32_t border;
    uint32_t text;
    uint32_t text_dim;
    uint32_t accent;
    uint32_t checkbox_check;
    uint32_t slider_track;
    uint32_t slider_grab;
    uint32_t scrollbar;
    uint32_t scrollbar_grab;
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

/* Glyph data is stored as opaque pointer, actual type is stbtt_bakedchar* */

/* Main UI context */
typedef struct CUI_Context {
    /* GPU resources */
    SDL_GPUDevice *gpu;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    SDL_GPUTexture *font_atlas;
    SDL_GPUSampler *sampler;

    /* Draw list (per-frame) */
    CUI_Vertex *vertices;
    uint16_t *indices;
    uint32_t vertex_count, index_count;
    uint32_t vertex_capacity, index_capacity;

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

    /* Font data (opaque, actual type is stbtt_bakedchar*) */
    void *glyphs; /* ASCII 32-127 (96 chars) */
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
} CUI_Context;

/* Panel flags */
#define CUI_PANEL_MOVABLE       (1 << 0)
#define CUI_PANEL_RESIZABLE     (1 << 1)
#define CUI_PANEL_CLOSABLE      (1 << 2)
#define CUI_PANEL_TITLE_BAR     (1 << 3)
#define CUI_PANEL_NO_SCROLLBAR  (1 << 4)
#define CUI_PANEL_BORDER        (1 << 5)

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

/* Collapsible sections */
bool cui_collapsing_header(CUI_Context *ctx, const char *label);

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

/* Text */
float cui_draw_text(CUI_Context *ctx, const char *text, float x, float y,
                    uint32_t color);
void cui_draw_text_clipped(CUI_Context *ctx, const char *text,
                           CUI_Rect bounds, uint32_t color);

/* Scissor/clipping */
void cui_push_scissor(CUI_Context *ctx, float x, float y, float w, float h);
void cui_pop_scissor(CUI_Context *ctx);

/* ============================================================================
 * Text Measurement
 * ============================================================================ */

float cui_text_width(CUI_Context *ctx, const char *text);
float cui_text_height(CUI_Context *ctx);
void cui_text_size(CUI_Context *ctx, const char *text, float *out_w, float *out_h);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* Color helpers */
uint32_t cui_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
uint32_t cui_rgb(uint8_t r, uint8_t g, uint8_t b);
uint32_t cui_color_lerp(uint32_t a, uint32_t b, float t);
uint32_t cui_color_alpha(uint32_t color, float alpha);

/* Rect helpers */
bool cui_rect_contains(CUI_Rect rect, float x, float y);
CUI_Rect cui_rect_intersect(CUI_Rect a, CUI_Rect b);

/* Get persistent widget state */
CUI_WidgetState *cui_get_state(CUI_Context *ctx, CUI_Id id);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_UI_H */
