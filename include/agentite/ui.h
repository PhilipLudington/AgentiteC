/*
 * Agentite UI - Immediate Mode UI System
 *
 * Usage:
 *   AUI_Context *ui = aui_init(gpu, width, height, "font.ttf", 16);
 *
 *   // Each frame:
 *   aui_begin_frame(ui, delta_time);
 *   aui_process_event(ui, &event);  // for each SDL event
 *
 *   if (aui_begin_panel(ui, "Menu", 10, 10, 200, 300, 0)) {
 *       aui_label(ui, "Hello!");
 *       if (aui_button(ui, "Click Me")) { ... }
 *       aui_end_panel(ui);
 *   }
 *
 *   aui_end_frame(ui);
 *   aui_render(ui, render_pass);
 */

#ifndef AGENTITE_UI_H
#define AGENTITE_UI_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

/* Forward declarations */
typedef struct AUI_Context AUI_Context;

typedef uint32_t AUI_Id;
#define AUI_ID_NONE ((AUI_Id)0)

typedef struct AUI_Rect {
    float x, y, w, h;
} AUI_Rect;

typedef struct AUI_Color {
    float r, g, b, a;
} AUI_Color;

/* Vertex format for batched rendering */
typedef struct AUI_Vertex {
    float pos[2];       /* Screen position (x, y) */
    float uv[2];        /* Texture coordinates */
    uint32_t color;     /* Packed RGBA (0xAABBGGRR) */
} AUI_Vertex;

/* Undo/redo history for textbox */
#define AUI_UNDO_HISTORY_SIZE 8
#define AUI_UNDO_TEXT_SIZE 256

typedef struct AUI_UndoEntry {
    char text[AUI_UNDO_TEXT_SIZE];
    int cursor_pos;
    int text_len;
} AUI_UndoEntry;

/* Persistent widget state (survives across frames) */
typedef struct AUI_WidgetState {
    AUI_Id id;
    float scroll_x, scroll_y;       /* For scrollable regions */
    int cursor_pos;                 /* For text input */
    int selection_start, selection_end;
    bool expanded;                  /* For collapsible headers */
    uint64_t last_frame;            /* For garbage collection */

    /* Undo/redo history for textbox */
    AUI_UndoEntry undo_history[AUI_UNDO_HISTORY_SIZE];
    int undo_pos;       /* Current position in undo history (0 = oldest) */
    int undo_count;     /* Number of valid entries in history */
    int redo_count;     /* Number of redo entries available */
} AUI_WidgetState;

/* State hash table entry */
typedef struct AUI_StateEntry {
    AUI_WidgetState state;
    struct AUI_StateEntry *next;
} AUI_StateEntry;

/* Layout frame (stackable) */
typedef struct AUI_LayoutFrame {
    AUI_Rect bounds;                /* Available area */
    float cursor_x, cursor_y;       /* Current position */
    float row_height;               /* For horizontal layouts */
    float spacing;
    float padding;
    bool horizontal;                /* true = row, false = column */
    AUI_Rect clip;                  /* Clipping rectangle */
    bool has_clip;
} AUI_LayoutFrame;

/* Theme colors and metrics */
typedef struct AUI_Theme {
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
} AUI_Theme;

/* Gamepad button indices (matches SDL_GamepadButton) */
#define AUI_GAMEPAD_BUTTON_COUNT 21

/* Input state */
typedef struct AUI_Input {
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

    /* Gamepad state */
    bool gamepad_button_down[AUI_GAMEPAD_BUTTON_COUNT];
    bool gamepad_button_pressed[AUI_GAMEPAD_BUTTON_COUNT];
    bool gamepad_button_released[AUI_GAMEPAD_BUTTON_COUNT];
    float gamepad_axis_left_x;      /* Left stick X (-1 to 1) */
    float gamepad_axis_left_y;      /* Left stick Y (-1 to 1) */
    float gamepad_axis_right_x;     /* Right stick X (-1 to 1) */
    float gamepad_axis_right_y;     /* Right stick Y (-1 to 1) */
} AUI_Input;

/* ============================================================================
 * Font System (Multi-Font Support with SDF/MSDF)
 * ============================================================================ */

#define AUI_MAX_FONTS 16
#define AUI_FONT_ATLAS_SIZE 512

/* Font type enumeration */
typedef enum AUI_FontType {
    AUI_FONT_BITMAP,    /* Standard bitmap font (stb_truetype) */
    AUI_FONT_SDF,       /* Single-channel signed distance field */
    AUI_FONT_MSDF       /* Multi-channel signed distance field */
} AUI_FontType;

/* Font handle - wraps either bitmap or SDF font (opaque pointer) */
typedef struct AUI_Font AUI_Font;

/* Draw command types */
typedef enum AUI_DrawCmdType {
    AUI_DRAW_CMD_SOLID,         /* Solid color primitives (rects, lines) */
    AUI_DRAW_CMD_BITMAP_TEXT,   /* Bitmap font text */
    AUI_DRAW_CMD_SDF_TEXT,      /* SDF font text */
    AUI_DRAW_CMD_MSDF_TEXT      /* MSDF font text */
} AUI_DrawCmdType;

/* Draw command - represents a batch of primitives sharing texture and layer */
typedef struct AUI_DrawCmd {
    AUI_DrawCmdType type;
    SDL_GPUTexture *texture;    /* Font atlas texture */
    int layer;                  /* Layer for z-ordering (lower = back) */
    uint32_t vertex_offset;     /* Start index in vertex buffer */
    uint32_t index_offset;      /* Start index in index buffer */
    uint32_t vertex_count;      /* Number of vertices */
    uint32_t index_count;       /* Number of indices */
    float sdf_scale;            /* Scale for SDF rendering */
    float sdf_distance_range;   /* Distance range for SDF font */
} AUI_DrawCmd;

#define AUI_MAX_DRAW_CMDS 256
#define AUI_DEFAULT_LAYER 0

/* Table sort specification (forward declared for use in context) */
typedef struct AUI_TableSortSpec {
    int column_index;
    bool descending;
} AUI_TableSortSpec;

/* Multi-select state (forward declared for use in context) */
typedef struct AUI_MultiSelectState {
    int *selected_indices;      /* Array of selected indices */
    int selected_count;         /* Number of selected items */
    int capacity;               /* Capacity of selected_indices array */
    int anchor_index;           /* Anchor for shift-click range selection */
    int last_clicked;           /* Last clicked index */
} AUI_MultiSelectState;

/* Keyboard shortcut modifiers */
#define AUI_MOD_NONE   0
#define AUI_MOD_CTRL   (1 << 0)
#define AUI_MOD_SHIFT  (1 << 1)
#define AUI_MOD_ALT    (1 << 2)

/* Maximum number of registered shortcuts */
#define AUI_MAX_SHORTCUTS 64

/* Shortcut callback type */
typedef void (*AUI_ShortcutCallback)(AUI_Context *ctx, void *userdata);

/* Main UI context */
struct AUI_Context {
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
    AUI_Vertex *vertices;
    uint16_t *indices;
    uint32_t vertex_count, index_count;
    uint32_t vertex_capacity, index_capacity;

    /* Draw command queue */
    AUI_DrawCmd *draw_cmds;
    uint32_t draw_cmd_count;
    uint32_t draw_cmd_capacity;

    /* Current draw state */
    SDL_GPUTexture *current_texture; /* Current texture being batched */
    int current_layer;               /* Current layer for new primitives */
    uint32_t cmd_vertex_start;       /* Start of current command's vertices */
    uint32_t cmd_index_start;        /* Start of current command's indices */

    /* Input state */
    AUI_Input input;

    /* Widget interaction state */
    AUI_Id hot;                     /* Hovered widget */
    AUI_Id active;                  /* Mouse-down widget */
    AUI_Id focused;                 /* Keyboard focus */
    AUI_Id last_widget_id;          /* ID of last widget processed (for tooltip association) */

    /* Focus navigation (Tab/Shift+Tab) */
    bool focus_next_requested;      /* Tab pressed - focus next widget */
    bool focus_prev_requested;      /* Shift+Tab pressed - focus previous widget */
    AUI_Id first_focusable;         /* First focusable widget this frame */
    AUI_Id last_focusable;          /* Last focusable widget this frame */
    AUI_Id prev_focusable;          /* Widget before currently focused one */
    bool focus_found_this_frame;    /* Whether focused widget was seen */

    /* Gamepad/spatial focus navigation */
    bool gamepad_mode;              /* True when using gamepad input */
    bool focus_up_requested;        /* D-pad up - focus widget above */
    bool focus_down_requested;      /* D-pad down - focus widget below */
    bool focus_left_requested;      /* D-pad left - focus widget left */
    bool focus_right_requested;     /* D-pad right - focus widget right */
    SDL_JoystickID gamepad_id;      /* Connected gamepad ID (0 = none) */

    /* Spatial focus tracking (positions of focusable widgets) */
    struct {
        AUI_Id id;
        float center_x, center_y;   /* Center position for distance calc */
    } focusable_widgets[128];       /* Track widget positions this frame */
    int focusable_widget_count;

    /* Persistent state hash table */
    AUI_StateEntry *state_table[256];

    /* Layout stack */
    AUI_LayoutFrame layout_stack[32];
    int layout_depth;

    /* Scissor stack */
    AUI_Rect scissor_stack[16];
    int scissor_depth;

    /* ID stack for scoping */
    AUI_Id id_stack[32];
    int id_stack_depth;

    /* Font registry (multi-font support) */
    AUI_Font *fonts[AUI_MAX_FONTS];
    int font_count;
    AUI_Font *default_font;         /* Default font for widgets */
    AUI_Font *current_font;         /* Currently active font for drawing */

    /* Legacy compatibility - points to default font data */
    void *glyphs; /* ASCII 32-127 (96 chars) - DEPRECATED, use fonts */
    SDL_GPUTexture *font_atlas;     /* DEPRECATED, use fonts */
    float font_size;
    float line_height;
    float ascent;
    int atlas_width, atlas_height;

    /* Theme */
    AUI_Theme theme;

    /* Screen dimensions */
    int width, height;
    float dpi_scale;        /* DPI scale factor (1.0 standard, 2.0 retina) */

    /* Frame timing */
    float delta_time;
    uint64_t frame_count;

    /* Dropdown/popup state */
    AUI_Id open_popup;
    AUI_Rect popup_rect;
    int *popup_selected;            /* Pointer to selection value */
    const char **popup_items;       /* Popup items array */
    int popup_count;                /* Number of popup items */
    bool popup_changed;             /* Whether selection changed */

    /* Text input tracking */
    AUI_Id prev_focused;            /* Previous frame's focused widget */
    SDL_Window *window;             /* Window for text input control */

    /* Path building state */
    float *path_points;             /* Array of (x, y) pairs */
    uint32_t path_count;            /* Number of points */
    uint32_t path_capacity;         /* Capacity of path_points array */

    /* Table state (active table during begin/end) */
    struct {
        AUI_Id id;                  /* Current table ID */
        int column_count;           /* Number of columns */
        int current_column;         /* Current column index */
        int current_row;            /* Current row index */
        uint32_t flags;             /* Table flags */
        AUI_Rect bounds;            /* Table bounds */
        float row_height;           /* Height of each row */
        float *column_widths;       /* Array of column widths */
        const char **column_labels; /* Array of column labels */
        uint32_t *column_flags;     /* Array of column flags */
        int columns_setup;          /* Number of columns set up */
        float scroll_x, scroll_y;   /* Scroll position */
        float content_width;        /* Total content width */
        float content_height;       /* Total content height */
        AUI_TableSortSpec sort_spec;/* Current sort specification */
        bool sort_specs_changed;    /* Whether sort changed this frame */
    } table;

    /* Active multi-select state pointer (set during begin/end) */
    AUI_MultiSelectState *multi_select;

    /* Tab bar state (active during begin/end) */
    struct {
        AUI_Id id;                  /* Tab bar ID */
        int active_tab;             /* Currently selected tab index */
        int tab_count;              /* Number of tabs processed */
        float tab_x;                /* Current X position for next tab */
        float bar_y;                /* Y position of tab bar */
        float bar_height;           /* Height of tab bar */
        AUI_Rect content_rect;      /* Rect for tab content area */
    } tab_bar;

    /* Scroll region state (active during begin/end) */
    struct {
        AUI_Id id;                  /* Scroll region ID */
        AUI_Rect outer_rect;        /* Full scroll region rect */
        float content_start_y;      /* Y position where content started */
    } scroll;

    /* Pending tooltip for deferred rendering (on top of everything) */
    char pending_tooltip[512];      /* Tooltip text buffer */
    bool pending_tooltip_active;    /* Whether a tooltip should be drawn */
    float pending_tooltip_x;        /* Tooltip position X */
    float pending_tooltip_y;        /* Tooltip position Y */

    /* Retained-mode node tooltip tracking */
    struct AUI_Node *hovered_node;  /* Currently hovered node (for tooltips) */
    float tooltip_hover_time;       /* Time hovering over current node */

    /* Layer system for z-ordering */
    int layer_stack[16];            /* Stack of pushed layers */
    int layer_stack_depth;

    /* Keyboard shortcut system */
    struct {
        SDL_Keycode key;            /* Key code (e.g., SDLK_S) */
        uint8_t modifiers;          /* Modifier flags (AUI_MOD_*) */
        AUI_ShortcutCallback callback;
        void *userdata;
        char name[32];              /* Optional name for display */
        bool active;                /* Whether this slot is in use */
    } shortcuts[AUI_MAX_SHORTCUTS];
    int shortcut_count;
};

/* Panel flags */
#define AUI_PANEL_MOVABLE       (1 << 0)
#define AUI_PANEL_RESIZABLE     (1 << 1)
#define AUI_PANEL_CLOSABLE      (1 << 2)
#define AUI_PANEL_TITLE_BAR     (1 << 3)
#define AUI_PANEL_NO_SCROLLBAR  (1 << 4)
#define AUI_PANEL_BORDER        (1 << 5)

/* Table flags */
#define AUI_TABLE_RESIZABLE     (1 << 0)
#define AUI_TABLE_REORDERABLE   (1 << 1)
#define AUI_TABLE_SORTABLE      (1 << 2)
#define AUI_TABLE_HIDEABLE      (1 << 3)
#define AUI_TABLE_BORDERS       (1 << 4)
#define AUI_TABLE_ROW_HIGHLIGHT (1 << 5)
#define AUI_TABLE_SCROLL_X      (1 << 6)
#define AUI_TABLE_SCROLL_Y      (1 << 7)

/* Table column flags */
#define AUI_TABLE_COLUMN_DEFAULT_SORT   (1 << 0)
#define AUI_TABLE_COLUMN_NO_SORT        (1 << 1)
#define AUI_TABLE_COLUMN_NO_RESIZE      (1 << 2)
#define AUI_TABLE_COLUMN_NO_HIDE        (1 << 3)

/* Color picker flags */
#define AUI_COLORPICKER_NO_ALPHA     (1 << 0)
#define AUI_COLORPICKER_HDR          (1 << 1)
#define AUI_COLORPICKER_WHEEL        (1 << 2)  /* Use color wheel instead of square */
#define AUI_COLORPICKER_INPUT_RGB    (1 << 3)  /* Show RGB input fields */
#define AUI_COLORPICKER_INPUT_HSV    (1 << 4)  /* Show HSV input fields */
#define AUI_COLORPICKER_INPUT_HEX    (1 << 5)  /* Show hex input field */
#define AUI_COLORPICKER_PALETTE      (1 << 6)  /* Show saved color palette */

/* ============================================================================
 * Font Management
 * ============================================================================ */

/* Load a bitmap font (standard TTF rasterization). Returns NULL on failure. */
AUI_Font *aui_font_load(AUI_Context *ctx, const char *path, float size);

/* Load an SDF/MSDF font from pre-generated atlas files (msdf-atlas-gen format).
 * atlas_path: Path to PNG atlas image
 * metrics_path: Path to JSON metrics file
 * Returns NULL on failure. */
AUI_Font *aui_font_load_sdf(AUI_Context *ctx, const char *atlas_path,
                            const char *metrics_path);

/* Unload a font (removes from registry and frees resources) */
void aui_font_unload(AUI_Context *ctx, AUI_Font *font);

/* Get font type (bitmap, SDF, or MSDF) */
AUI_FontType aui_font_get_type(AUI_Font *font);

/* Set the default font for widgets */
void aui_set_default_font(AUI_Context *ctx, AUI_Font *font);

/* Get the default font */
AUI_Font *aui_get_default_font(AUI_Context *ctx);

/* Set the current font for subsequent text drawing */
void aui_set_font(AUI_Context *ctx, AUI_Font *font);

/* Get the current font */
AUI_Font *aui_get_font(AUI_Context *ctx);

/* Get font metrics */
float aui_font_size(AUI_Font *font);
float aui_font_line_height(AUI_Font *font);
float aui_font_ascent(AUI_Font *font);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/* Initialize UI system */
AUI_Context *aui_init(SDL_GPUDevice *gpu, SDL_Window *window, int width, int height,
                      const char *font_path, float font_size);

/* Shutdown UI system */
void aui_shutdown(AUI_Context *ctx);

/* Begin new UI frame (call before widgets) */
void aui_begin_frame(AUI_Context *ctx, float delta_time);

/* End UI frame (call after widgets) */
void aui_end_frame(AUI_Context *ctx);

/* Upload UI data to GPU (call BEFORE render pass begins) */
void aui_upload(AUI_Context *ctx, SDL_GPUCommandBuffer *cmd);

/* Render UI (call DURING render pass) */
void aui_render(AUI_Context *ctx, SDL_GPUCommandBuffer *cmd,
                SDL_GPURenderPass *pass);

/* Process SDL event (returns true if consumed) */
bool aui_process_event(AUI_Context *ctx, const SDL_Event *event);

/* Update screen size */
void aui_set_screen_size(AUI_Context *ctx, int width, int height);

/* ============================================================================
 * ID System
 * ============================================================================ */

/* Generate ID from string */
AUI_Id aui_id(const char *str);

/* Generate ID from string + integer (for loops) */
AUI_Id aui_id_int(const char *str, int n);

/* Push/pop ID prefix for scoping */
void aui_push_id(AUI_Context *ctx, const char *str);
void aui_push_id_int(AUI_Context *ctx, int n);
void aui_pop_id(AUI_Context *ctx);

/* ============================================================================
 * Focus Navigation
 * ============================================================================ */

/* Register a widget as focusable (call from widget implementations).
 * Returns true if this widget should grab focus this frame. */
bool aui_focus_register(AUI_Context *ctx, AUI_Id id);

/* Register a widget as focusable with spatial position for gamepad navigation.
 * The rect is used for D-pad directional navigation.
 * Returns true if this widget should grab focus this frame. */
bool aui_focus_register_rect(AUI_Context *ctx, AUI_Id id, AUI_Rect rect);

/* Check if widget currently has focus */
bool aui_has_focus(AUI_Context *ctx, AUI_Id id);

/* Programmatically set focus to a widget */
void aui_set_focus(AUI_Context *ctx, AUI_Id id);

/* Clear focus (unfocus all widgets) */
void aui_clear_focus(AUI_Context *ctx);

/* ============================================================================
 * Gamepad Navigation
 * ============================================================================ */

/* Check if gamepad mode is active (vs keyboard/mouse mode) */
bool aui_is_gamepad_mode(AUI_Context *ctx);

/* Manually enable/disable gamepad mode.
 * Normally this is automatic based on last input type. */
void aui_set_gamepad_mode(AUI_Context *ctx, bool enabled);

/* Get currently connected gamepad ID (0 if none) */
SDL_JoystickID aui_get_gamepad_id(AUI_Context *ctx);

/* Check if a gamepad button is currently held */
bool aui_gamepad_button_down(AUI_Context *ctx, int button);

/* Check if a gamepad button was just pressed this frame */
bool aui_gamepad_button_pressed(AUI_Context *ctx, int button);

/* Check if a gamepad button was just released this frame */
bool aui_gamepad_button_released(AUI_Context *ctx, int button);

/* Get gamepad axis value (-1.0 to 1.0) */
float aui_gamepad_axis(AUI_Context *ctx, int axis);

/* ============================================================================
 * Keyboard Shortcuts
 * ============================================================================ */

/* Register a keyboard shortcut.
 * key: SDL keycode (e.g., SDLK_S)
 * modifiers: Combination of AUI_MOD_CTRL, AUI_MOD_SHIFT, AUI_MOD_ALT
 * name: Optional display name (can be NULL)
 * callback: Function called when shortcut is triggered
 * userdata: User data passed to callback
 * Returns: Shortcut ID (>= 0) on success, -1 on failure (table full) */
int aui_shortcut_register(AUI_Context *ctx, SDL_Keycode key, uint8_t modifiers,
                          const char *name, AUI_ShortcutCallback callback,
                          void *userdata);

/* Unregister a shortcut by ID */
void aui_shortcut_unregister(AUI_Context *ctx, int id);

/* Unregister all shortcuts */
void aui_shortcuts_clear(AUI_Context *ctx);

/* Process shortcuts (called automatically by aui_process_event, but can be
 * called manually if needed). Returns true if a shortcut was triggered. */
bool aui_shortcuts_process(AUI_Context *ctx);

/* Get shortcut display string (e.g., "Ctrl+S"). Buffer must be at least 32 chars.
 * Returns pointer to buffer on success, NULL on invalid ID. */
const char *aui_shortcut_get_display(AUI_Context *ctx, int id, char *buffer,
                                      int buffer_size);

/* ============================================================================
 * Layout Functions
 * ============================================================================ */

/* Begin horizontal layout */
void aui_begin_row(AUI_Context *ctx);
void aui_begin_row_ex(AUI_Context *ctx, float height, float spacing);

/* Begin vertical layout */
void aui_begin_column(AUI_Context *ctx);
void aui_begin_column_ex(AUI_Context *ctx, float width, float spacing);

/* End layout */
void aui_end_row(AUI_Context *ctx);
void aui_end_column(AUI_Context *ctx);

/* Add spacing */
void aui_spacing(AUI_Context *ctx, float amount);
void aui_separator(AUI_Context *ctx);

/* Stay on same line (for horizontal arrangement) */
void aui_same_line(AUI_Context *ctx);

/* Scrollable region */
void aui_begin_scroll(AUI_Context *ctx, const char *id, float width, float height);
void aui_end_scroll(AUI_Context *ctx);

/* Get available rect from current layout */
AUI_Rect aui_get_available_rect(AUI_Context *ctx);

/* ============================================================================
 * Widgets
 * ============================================================================ */

/* Labels and text */
void aui_label(AUI_Context *ctx, const char *text);
void aui_label_colored(AUI_Context *ctx, const char *text, uint32_t color);

/* Buttons */
bool aui_button(AUI_Context *ctx, const char *label);
bool aui_button_ex(AUI_Context *ctx, const char *label, float width, float height);

/* Semantic button variants (colored by theme semantic colors) */
bool aui_button_primary(AUI_Context *ctx, const char *label);
bool aui_button_success(AUI_Context *ctx, const char *label);
bool aui_button_warning(AUI_Context *ctx, const char *label);
bool aui_button_danger(AUI_Context *ctx, const char *label);
bool aui_button_info(AUI_Context *ctx, const char *label);

/* Toggle widgets */
bool aui_checkbox(AUI_Context *ctx, const char *label, bool *value);
bool aui_radio(AUI_Context *ctx, const char *label, int *value, int option);

/* Sliders */
bool aui_slider_float(AUI_Context *ctx, const char *label, float *value,
                      float min, float max);
bool aui_slider_int(AUI_Context *ctx, const char *label, int *value,
                    int min, int max);

/* Spinbox (numeric input with +/- buttons) */
bool aui_spinbox_int(AUI_Context *ctx, const char *label, int *value,
                     int min, int max, int step);
bool aui_spinbox_float(AUI_Context *ctx, const char *label, float *value,
                       float min, float max, float step);

/* Text input */
bool aui_textbox(AUI_Context *ctx, const char *label, char *buffer, int buffer_size);
bool aui_textbox_ex(AUI_Context *ctx, const char *label, char *buffer,
                    int buffer_size, float width);

/* Selectable item (for lists, returns true if clicked) */
bool aui_selectable(AUI_Context *ctx, const char *label, bool selected);

/* Selection widgets */
bool aui_dropdown(AUI_Context *ctx, const char *label, int *selected,
                  const char **items, int count);
bool aui_listbox(AUI_Context *ctx, const char *label, int *selected,
                 const char **items, int count, float height);

/* Progress display */
void aui_progress_bar(AUI_Context *ctx, float value, float min, float max);
void aui_progress_bar_colored(AUI_Context *ctx, float value, float min, float max,
                               uint32_t fill_color);

/* Collapsible sections */
bool aui_collapsing_header(AUI_Context *ctx, const char *label);

/* Tables */
bool aui_begin_table(AUI_Context *ctx, const char *id, int columns,
                     uint32_t flags, float width, float height);
void aui_table_setup_column(AUI_Context *ctx, const char *label,
                            uint32_t flags, float init_width);
void aui_table_headers_row(AUI_Context *ctx);
void aui_table_next_row(AUI_Context *ctx);
bool aui_table_next_column(AUI_Context *ctx);
bool aui_table_set_column(AUI_Context *ctx, int column);
AUI_TableSortSpec *aui_table_get_sort_specs(AUI_Context *ctx, int *count);
bool aui_table_sort_specs_changed(AUI_Context *ctx);
void aui_end_table(AUI_Context *ctx);

/* Multi-Select */
AUI_MultiSelectState aui_multi_select_create(int capacity);
void aui_multi_select_destroy(AUI_MultiSelectState *state);
void aui_multi_select_clear(AUI_MultiSelectState *state);
bool aui_multi_select_is_selected(AUI_MultiSelectState *state, int index);
void aui_multi_select_begin(AUI_Context *ctx, AUI_MultiSelectState *state);
bool aui_multi_select_item(AUI_Context *ctx, AUI_MultiSelectState *state,
                           int index, bool *is_selected);
void aui_multi_select_end(AUI_Context *ctx);

/* Color Picker */
bool aui_color_picker(AUI_Context *ctx, const char *label,
                      float *rgba, uint32_t flags);
bool aui_color_button(AUI_Context *ctx, const char *label,
                      float *rgba, float size);
bool aui_color_edit3(AUI_Context *ctx, const char *label, float *rgb);
bool aui_color_edit4(AUI_Context *ctx, const char *label, float *rgba);

/* Color conversion utilities */
void aui_rgb_to_hsv(float r, float g, float b, float *h, float *s, float *v);
void aui_hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b);

/* Tab Container - tabbed content areas
 *
 * Usage:
 *   if (aui_begin_tab_bar(ctx, "mytabs")) {
 *       if (aui_tab(ctx, "Tab 1")) {
 *           // Tab 1 content
 *       }
 *       if (aui_tab(ctx, "Tab 2")) {
 *           // Tab 2 content
 *       }
 *       aui_end_tab_bar(ctx);
 *   }
 */
bool aui_begin_tab_bar(AUI_Context *ctx, const char *id);
bool aui_tab(AUI_Context *ctx, const char *label);
void aui_end_tab_bar(AUI_Context *ctx);

/* ============================================================================
 * Layer System (Z-Ordering)
 * ============================================================================ */

/* Set current layer for subsequent draw operations (lower = back, higher = front) */
void aui_set_layer(AUI_Context *ctx, int layer);

/* Get current layer */
int aui_get_layer(AUI_Context *ctx);

/* Push/pop layer (for nested layer changes) */
void aui_push_layer(AUI_Context *ctx, int layer);
void aui_pop_layer(AUI_Context *ctx);

/* Legacy API (deprecated - use aui_set_layer instead) */
void aui_draw_split_begin(AUI_Context *ctx, int channel_count);
void aui_draw_set_channel(AUI_Context *ctx, int channel);
void aui_draw_split_merge(AUI_Context *ctx);

/* Panels/Windows */
bool aui_begin_panel(AUI_Context *ctx, const char *name,
                     float x, float y, float w, float h, uint32_t flags);
void aui_end_panel(AUI_Context *ctx);

/* Tooltips */
void aui_tooltip(AUI_Context *ctx, const char *text);

/* ============================================================================
 * Drawing Primitives (Low-level)
 * ============================================================================ */

/* Filled rectangles */
void aui_draw_rect(AUI_Context *ctx, float x, float y, float w, float h,
                   uint32_t color);
void aui_draw_rect_rounded(AUI_Context *ctx, float x, float y, float w, float h,
                           uint32_t color, float radius);

/* Textured rectangle - for images/icons */
void aui_draw_textured_rect(AUI_Context *ctx, SDL_GPUTexture *texture,
                             float x, float y, float w, float h,
                             float src_x, float src_y, float src_w, float src_h,
                             uint32_t tint, bool flip_h, bool flip_v);

/* Outlined rectangles */
void aui_draw_rect_outline(AUI_Context *ctx, float x, float y, float w, float h,
                           uint32_t color, float thickness);

/* Lines */
void aui_draw_line(AUI_Context *ctx, float x1, float y1, float x2, float y2,
                   uint32_t color, float thickness);

/* Bezier Curves */
void aui_draw_bezier_cubic(AUI_Context *ctx,
                           float x1, float y1,   /* Start point */
                           float cx1, float cy1, /* Control point 1 */
                           float cx2, float cy2, /* Control point 2 */
                           float x2, float y2,   /* End point */
                           uint32_t color, float thickness);

void aui_draw_bezier_quadratic(AUI_Context *ctx,
                               float x1, float y1,  /* Start point */
                               float cx, float cy,  /* Control point */
                               float x2, float y2,  /* End point */
                               uint32_t color, float thickness);

/* Path API for complex shapes */
void aui_path_begin(AUI_Context *ctx);
void aui_path_line_to(AUI_Context *ctx, float x, float y);
void aui_path_bezier_cubic_to(AUI_Context *ctx, float cx1, float cy1,
                               float cx2, float cy2, float x, float y);
void aui_path_bezier_quadratic_to(AUI_Context *ctx, float cx, float cy,
                                   float x, float y);
void aui_path_stroke(AUI_Context *ctx, uint32_t color, float thickness);
void aui_path_fill(AUI_Context *ctx, uint32_t color);

/* Triangles */
void aui_draw_triangle(AUI_Context *ctx,
                       float x0, float y0, float x1, float y1, float x2, float y2,
                       uint32_t color);

/* Text (uses current font set by aui_set_font, or default font) */
float aui_draw_text(AUI_Context *ctx, const char *text, float x, float y,
                    uint32_t color);
void aui_draw_text_clipped(AUI_Context *ctx, const char *text,
                           AUI_Rect bounds, uint32_t color);

/* Text with explicit font */
float aui_draw_text_font(AUI_Context *ctx, AUI_Font *font, const char *text,
                         float x, float y, uint32_t color);
void aui_draw_text_font_clipped(AUI_Context *ctx, AUI_Font *font, const char *text,
                                AUI_Rect bounds, uint32_t color);

/* Scaled text drawing (useful for SDF fonts) */
float aui_draw_text_scaled(AUI_Context *ctx, const char *text, float x, float y,
                           float scale, uint32_t color);
float aui_draw_text_font_scaled(AUI_Context *ctx, AUI_Font *font, const char *text,
                                float x, float y, float scale, uint32_t color);

/* Scissor/clipping */
void aui_push_scissor(AUI_Context *ctx, float x, float y, float w, float h);
void aui_pop_scissor(AUI_Context *ctx);

/* ============================================================================
 * Text Measurement
 * ============================================================================ */

/* Measure text with current/default font */
float aui_text_width(AUI_Context *ctx, const char *text);
float aui_text_height(AUI_Context *ctx);
void aui_text_size(AUI_Context *ctx, const char *text, float *out_w, float *out_h);

/* Measure text with explicit font */
float aui_text_width_font(AUI_Font *font, const char *text);
float aui_text_height_font(AUI_Font *font);
void aui_text_size_font(AUI_Font *font, const char *text, float *out_w, float *out_h);

/* Measure text with explicit font and scale (for SDF fonts) */
float aui_text_width_font_scaled(AUI_Font *font, const char *text, float scale);
float aui_text_height_font_scaled(AUI_Font *font, float scale);
void aui_text_size_font_scaled(AUI_Font *font, const char *text, float scale,
                               float *out_w, float *out_h);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* Color helpers */
uint32_t aui_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
uint32_t aui_rgb(uint8_t r, uint8_t g, uint8_t b);
uint32_t aui_color_lerp(uint32_t a, uint32_t b, float t);
uint32_t aui_color_alpha(uint32_t color, float alpha);
uint32_t aui_color_brighten(uint32_t color, float amount);
uint32_t aui_color_darken(uint32_t color, float amount);

/* Rect helpers */
bool aui_rect_contains(AUI_Rect rect, float x, float y);
AUI_Rect aui_rect_intersect(AUI_Rect a, AUI_Rect b);

/* Get persistent widget state */
AUI_WidgetState *aui_get_state(AUI_Context *ctx, AUI_Id id);

/* ============================================================================
 * Theme System
 * ============================================================================ */

/* Get predefined theme presets */
AUI_Theme aui_theme_dark(void);
AUI_Theme aui_theme_light(void);

/* Get/set current theme */
void aui_set_theme(AUI_Context *ctx, const AUI_Theme *theme);
const AUI_Theme *aui_get_theme(const AUI_Context *ctx);

/* Theme customization helpers */
void aui_theme_set_accent(AUI_Theme *theme, uint32_t color);
void aui_theme_set_semantic_colors(AUI_Theme *theme,
                                    uint32_t success, uint32_t warning,
                                    uint32_t danger, uint32_t info);

/* Scale theme metrics by DPI factor (corner_radius, border_width, etc.) */
void aui_theme_scale(AUI_Theme *theme, float dpi_scale);

/* Set DPI scale for the UI context (affects coordinate scaling) */
void aui_set_dpi_scale(AUI_Context *ctx, float dpi_scale);
float aui_get_dpi_scale(const AUI_Context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_UI_H */
