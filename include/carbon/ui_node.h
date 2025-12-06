/*
 * Carbon UI - Retained Mode Node System
 *
 * Provides a scene tree of UI nodes with Godot-style anchors and layout.
 *
 * Usage:
 *   // Create a scene tree
 *   CUI_Node *root = cui_node_create(ctx, CUI_NODE_CONTROL, "root");
 *   cui_node_set_anchor_preset(root, CUI_ANCHOR_FULL_RECT);
 *
 *   CUI_Node *panel = cui_node_create(ctx, CUI_NODE_PANEL, "settings");
 *   cui_node_set_anchor_preset(panel, CUI_ANCHOR_CENTER);
 *   cui_node_set_offsets(panel, -200, -150, 200, 150);
 *   cui_node_add_child(root, panel);
 *
 *   // Each frame
 *   cui_scene_update(ctx, root, delta_time);
 *   cui_scene_process_event(ctx, root, &event);
 *   cui_scene_render(ctx, root);
 */

#ifndef CARBON_UI_NODE_H
#define CARBON_UI_NODE_H

#include "carbon/ui.h"
#include "carbon/ui_style.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct CUI_Context CUI_Context;
typedef struct CUI_Node CUI_Node;
typedef struct CUI_Signal CUI_Signal;

/* ============================================================================
 * Node Types
 * ============================================================================ */

typedef enum CUI_NodeType {
    /* Base types */
    CUI_NODE_CONTROL,          /* Base control node (no rendering) */
    CUI_NODE_CONTAINER,        /* Generic container */

    /* Layout containers */
    CUI_NODE_VBOX,             /* Vertical box container */
    CUI_NODE_HBOX,             /* Horizontal box container */
    CUI_NODE_GRID,             /* Grid container */
    CUI_NODE_MARGIN,           /* Margin container */
    CUI_NODE_SCROLL,           /* Scrollable container */
    CUI_NODE_CENTER,           /* Centers child */

    /* Display widgets */
    CUI_NODE_LABEL,
    CUI_NODE_ICON,
    CUI_NODE_TEXTURE_RECT,
    CUI_NODE_PROGRESS_BAR,
    CUI_NODE_SEPARATOR,

    /* Interactive widgets */
    CUI_NODE_BUTTON,
    CUI_NODE_CHECKBOX,
    CUI_NODE_RADIO,
    CUI_NODE_SLIDER,
    CUI_NODE_TEXTBOX,
    CUI_NODE_DROPDOWN,
    CUI_NODE_SPINBOX,

    /* Containers/Windows */
    CUI_NODE_PANEL,
    CUI_NODE_DIALOG,
    CUI_NODE_POPUP,
    CUI_NODE_TAB_CONTAINER,

    /* Advanced widgets */
    CUI_NODE_TREE,
    CUI_NODE_RICHTEXT,
    CUI_NODE_CHART,

    /* Custom */
    CUI_NODE_CUSTOM,

    CUI_NODE_TYPE_COUNT
} CUI_NodeType;

/* ============================================================================
 * Anchor Presets
 * ============================================================================ */

typedef enum CUI_AnchorPreset {
    CUI_ANCHOR_TOP_LEFT,
    CUI_ANCHOR_TOP_CENTER,
    CUI_ANCHOR_TOP_RIGHT,
    CUI_ANCHOR_CENTER_LEFT,
    CUI_ANCHOR_CENTER,
    CUI_ANCHOR_CENTER_RIGHT,
    CUI_ANCHOR_BOTTOM_LEFT,
    CUI_ANCHOR_BOTTOM_CENTER,
    CUI_ANCHOR_BOTTOM_RIGHT,

    /* Wide presets (span full width/height on one axis) */
    CUI_ANCHOR_TOP_WIDE,
    CUI_ANCHOR_BOTTOM_WIDE,
    CUI_ANCHOR_LEFT_WIDE,
    CUI_ANCHOR_RIGHT_WIDE,
    CUI_ANCHOR_VCENTER_WIDE,
    CUI_ANCHOR_HCENTER_WIDE,

    /* Full rect (fills parent) */
    CUI_ANCHOR_FULL_RECT,

    CUI_ANCHOR_PRESET_COUNT
} CUI_AnchorPreset;

/* ============================================================================
 * Size Flags
 * ============================================================================ */

typedef enum CUI_SizeFlags {
    CUI_SIZE_NONE        = 0,
    CUI_SIZE_FILL        = 1 << 0,   /* Fill available space */
    CUI_SIZE_EXPAND      = 1 << 1,   /* Expand to take extra space */
    CUI_SIZE_SHRINK_CENTER = 1 << 2, /* Shrink and center */
    CUI_SIZE_SHRINK_END  = 1 << 3,   /* Shrink and align to end */
} CUI_SizeFlags;

/* ============================================================================
 * Anchors and Offsets
 * ============================================================================ */

/* Anchors are 0-1 values relative to parent rect */
typedef struct CUI_Anchors {
    float left;
    float top;
    float right;
    float bottom;
} CUI_Anchors;

/* ============================================================================
 * Signal Types
 * ============================================================================ */

typedef enum CUI_SignalType {
    /* Input signals */
    CUI_SIGNAL_PRESSED,
    CUI_SIGNAL_RELEASED,
    CUI_SIGNAL_CLICKED,
    CUI_SIGNAL_DOUBLE_CLICKED,
    CUI_SIGNAL_RIGHT_CLICKED,

    /* Focus signals */
    CUI_SIGNAL_FOCUSED,
    CUI_SIGNAL_UNFOCUSED,

    /* Hover signals */
    CUI_SIGNAL_MOUSE_ENTERED,
    CUI_SIGNAL_MOUSE_EXITED,

    /* Value signals */
    CUI_SIGNAL_VALUE_CHANGED,
    CUI_SIGNAL_TEXT_CHANGED,
    CUI_SIGNAL_SELECTION_CHANGED,
    CUI_SIGNAL_TOGGLED,

    /* Layout signals */
    CUI_SIGNAL_RESIZED,
    CUI_SIGNAL_VISIBILITY_CHANGED,
    CUI_SIGNAL_MINIMUM_SIZE_CHANGED,

    /* Tree signals */
    CUI_SIGNAL_CHILD_ADDED,
    CUI_SIGNAL_CHILD_REMOVED,
    CUI_SIGNAL_TREE_ENTERED,
    CUI_SIGNAL_TREE_EXITED,

    /* Custom signals (100+) */
    CUI_SIGNAL_CUSTOM = 100,

    CUI_SIGNAL_TYPE_COUNT = 200
} CUI_SignalType;

/* ============================================================================
 * Signal Data
 * ============================================================================ */

struct CUI_Signal {
    CUI_SignalType type;
    CUI_Node *source;

    union {
        /* Value changes */
        struct { int old_value, new_value; } int_change;
        struct { float old_value, new_value; } float_change;
        struct { bool old_value, new_value; } bool_change;
        struct { const char *old_text, *new_text; } text_change;

        /* Mouse events */
        struct { float x, y; int button; } mouse;

        /* Child changes */
        struct { CUI_Node *child; } child;

        /* Custom data */
        void *custom_data;
    };
};

/* ============================================================================
 * Signal Callback
 * ============================================================================ */

typedef void (*CUI_SignalCallback)(CUI_Node *node, const CUI_Signal *signal,
                                    void *userdata);

/* ============================================================================
 * Signal Connection
 * ============================================================================ */

#define CUI_MAX_CONNECTIONS 16

typedef struct CUI_Connection {
    uint32_t id;
    CUI_SignalType signal_type;
    CUI_SignalCallback callback;
    void *userdata;
    bool active;
    bool oneshot;              /* Disconnect after first call */
} CUI_Connection;

/* ============================================================================
 * Node-Specific Data
 * ============================================================================ */

/* Label data */
typedef struct CUI_LabelData {
    char text[256];
    uint32_t color;
    bool autowrap;
    int max_lines;
} CUI_LabelData;

/* Button data */
typedef struct CUI_ButtonData {
    char text[256];
    bool disabled;
    bool toggle_mode;
    bool toggled;
} CUI_ButtonData;

/* Checkbox data */
typedef struct CUI_CheckboxData {
    char text[256];
    bool checked;
    bool disabled;
} CUI_CheckboxData;

/* Slider data */
typedef struct CUI_SliderData {
    float value;
    float min_value;
    float max_value;
    float step;
    bool show_value;
    bool dragging;
} CUI_SliderData;

/* Textbox data */
typedef struct CUI_TextboxData {
    char *buffer;
    int buffer_size;
    int cursor_pos;
    int selection_start;
    int selection_end;
    bool password_mode;
    char placeholder[128];
} CUI_TextboxData;

/* Dropdown data */
typedef struct CUI_DropdownData {
    int selected;
    const char **items;
    int item_count;
    bool open;
} CUI_DropdownData;

/* Panel data */
typedef struct CUI_PanelData {
    char title[128];
    uint32_t flags;
    bool dragging;
    float drag_offset_x, drag_offset_y;
    bool closed;
} CUI_PanelData;

/* VBox/HBox data */
typedef struct CUI_BoxData {
    float separation;
    bool reverse;
    CUI_SizeFlags alignment;
} CUI_BoxData;

/* Grid data */
typedef struct CUI_GridData {
    int columns;
    float h_separation;
    float v_separation;
} CUI_GridData;

/* Scroll data */
typedef struct CUI_ScrollData {
    float scroll_x, scroll_y;
    float content_width, content_height;
    bool h_scroll_enabled;
    bool v_scroll_enabled;
    bool dragging_h, dragging_v;
} CUI_ScrollData;

/* Progress bar data */
typedef struct CUI_ProgressData {
    float value;
    float min_value;
    float max_value;
    uint32_t fill_color;
} CUI_ProgressData;

/* ============================================================================
 * Main Node Structure
 * ============================================================================ */

struct CUI_Node {
    /* Identity */
    uint32_t id;
    CUI_NodeType type;
    char name[64];

    /* Hierarchy */
    CUI_Node *parent;
    CUI_Node *first_child;
    CUI_Node *last_child;
    CUI_Node *next_sibling;
    CUI_Node *prev_sibling;
    int child_count;

    /* Anchors and offsets (Godot-style layout) */
    CUI_Anchors anchors;
    CUI_Edges offsets;         /* Pixel offset from anchor position */

    /* Computed layout */
    CUI_Rect rect;             /* Local rect (relative to parent) */
    CUI_Rect global_rect;      /* Screen coordinates */
    bool layout_dirty;

    /* Size hints */
    float min_size_x, min_size_y;
    float custom_min_size_x, custom_min_size_y;  /* User-specified minimum */
    uint8_t h_size_flags;
    uint8_t v_size_flags;
    float size_flags_stretch_ratio;

    /* Styling */
    CUI_Style style;
    const char *style_class_name;
    CUI_Style *style_override;  /* Runtime style override */

    /* Transform (for animations) */
    float scale_x, scale_y;
    float rotation;
    float pivot_x, pivot_y;    /* 0-1 normalized pivot point */

    /* State */
    bool visible;
    bool enabled;
    bool focused;
    bool hovered;
    bool pressed;
    float opacity;             /* Combined with style opacity */
    bool clip_contents;        /* Clip children to this node's rect */
    bool mouse_filter_stop;    /* Stop mouse events from propagating */
    bool mouse_filter_ignore;  /* Let mouse events pass through */

    /* Focus */
    bool focus_mode_click;
    bool focus_mode_all;
    CUI_Node *focus_next;
    CUI_Node *focus_prev;

    /* Signals/callbacks */
    CUI_Connection connections[CUI_MAX_CONNECTIONS];
    int connection_count;

    /* Type-specific data */
    union {
        CUI_LabelData label;
        CUI_ButtonData button;
        CUI_CheckboxData checkbox;
        CUI_SliderData slider;
        CUI_TextboxData textbox;
        CUI_DropdownData dropdown;
        CUI_PanelData panel;
        CUI_BoxData box;
        CUI_GridData grid;
        CUI_ScrollData scroll;
        CUI_ProgressData progress;
        void *custom_data;
    };

    /* Virtual functions (for custom nodes) */
    void (*on_draw)(CUI_Node *node, CUI_Context *ctx);
    void (*on_input)(CUI_Node *node, CUI_Context *ctx, const SDL_Event *event);
    bool (*on_gui_input)(CUI_Node *node, CUI_Context *ctx, const SDL_Event *event);
    void (*on_layout)(CUI_Node *node);
    void (*on_enter_tree)(CUI_Node *node);
    void (*on_exit_tree)(CUI_Node *node);
    void (*on_ready)(CUI_Node *node);
    void (*on_process)(CUI_Node *node, float delta);
    void (*on_destroy)(CUI_Node *node);
    void (*on_notification)(CUI_Node *node, int what);
};

/* ============================================================================
 * Node Lifecycle
 * ============================================================================ */

/* Create a new node */
CUI_Node *cui_node_create(CUI_Context *ctx, CUI_NodeType type, const char *name);

/* Destroy a node and all its children */
void cui_node_destroy(CUI_Node *node);

/* Duplicate a node (deep copy) */
CUI_Node *cui_node_duplicate(CUI_Node *node);

/* ============================================================================
 * Hierarchy Management
 * ============================================================================ */

/* Add a child node */
void cui_node_add_child(CUI_Node *parent, CUI_Node *child);

/* Remove a child node (doesn't destroy it) */
void cui_node_remove_child(CUI_Node *parent, CUI_Node *child);

/* Remove from parent */
void cui_node_remove(CUI_Node *node);

/* Move to a new parent */
void cui_node_reparent(CUI_Node *node, CUI_Node *new_parent);

/* Get child by index */
CUI_Node *cui_node_get_child(CUI_Node *node, int index);

/* Get child by name */
CUI_Node *cui_node_get_child_by_name(CUI_Node *node, const char *name);

/* Find node by path (e.g., "Panel/Content/Button") */
CUI_Node *cui_node_find(CUI_Node *root, const char *path);

/* Get the root node */
CUI_Node *cui_node_get_root(CUI_Node *node);

/* Check if node is ancestor of another */
bool cui_node_is_ancestor_of(CUI_Node *node, CUI_Node *descendant);

/* Get node index among siblings */
int cui_node_get_index(CUI_Node *node);

/* Move in sibling order */
void cui_node_move_child(CUI_Node *parent, CUI_Node *child, int new_index);
void cui_node_move_to_front(CUI_Node *node);
void cui_node_move_to_back(CUI_Node *node);

/* ============================================================================
 * Layout
 * ============================================================================ */

/* Set anchor preset */
void cui_node_set_anchor_preset(CUI_Node *node, CUI_AnchorPreset preset);

/* Set individual anchors (0-1 values) */
void cui_node_set_anchors(CUI_Node *node, float left, float top,
                           float right, float bottom);

/* Set offsets from anchors (in pixels) */
void cui_node_set_offsets(CUI_Node *node, float left, float top,
                           float right, float bottom);

/* Set size directly (adjusts offsets) */
void cui_node_set_size(CUI_Node *node, float width, float height);

/* Set position (relative to anchor position) */
void cui_node_set_position(CUI_Node *node, float x, float y);

/* Get computed size */
void cui_node_get_size(CUI_Node *node, float *width, float *height);

/* Get position in parent */
void cui_node_get_position(CUI_Node *node, float *x, float *y);

/* Get global position */
void cui_node_get_global_position(CUI_Node *node, float *x, float *y);

/* Size flags */
void cui_node_set_h_size_flags(CUI_Node *node, uint8_t flags);
void cui_node_set_v_size_flags(CUI_Node *node, uint8_t flags);
void cui_node_set_stretch_ratio(CUI_Node *node, float ratio);

/* Minimum size */
void cui_node_set_custom_min_size(CUI_Node *node, float width, float height);
void cui_node_get_min_size(CUI_Node *node, float *width, float *height);

/* Force layout recalculation */
void cui_node_queue_layout(CUI_Node *node);

/* ============================================================================
 * Styling
 * ============================================================================ */

/* Set the node's style directly */
void cui_node_set_style(CUI_Node *node, const CUI_Style *style);

/* Set style class by name */
void cui_node_set_style_class(CUI_Node *node, const char *class_name);

/* Get the effective style (resolved from class + overrides) */
CUI_Style cui_node_get_effective_style(CUI_Node *node);

/* ============================================================================
 * State
 * ============================================================================ */

/* Visibility */
void cui_node_set_visible(CUI_Node *node, bool visible);
bool cui_node_is_visible(CUI_Node *node);
bool cui_node_is_visible_in_tree(CUI_Node *node);

/* Enable/disable */
void cui_node_set_enabled(CUI_Node *node, bool enabled);
bool cui_node_is_enabled(CUI_Node *node);

/* Focus */
void cui_node_grab_focus(CUI_Node *node);
void cui_node_release_focus(CUI_Node *node);
bool cui_node_has_focus(CUI_Node *node);
CUI_Node *cui_get_focused_node(CUI_Context *ctx);

/* Opacity */
void cui_node_set_opacity(CUI_Node *node, float opacity);
float cui_node_get_opacity(CUI_Node *node);

/* ============================================================================
 * Signals
 * ============================================================================ */

/* Connect a callback to a signal */
uint32_t cui_node_connect(CUI_Node *node, CUI_SignalType signal,
                           CUI_SignalCallback callback, void *userdata);

/* Connect with oneshot (disconnect after first call) */
uint32_t cui_node_connect_oneshot(CUI_Node *node, CUI_SignalType signal,
                                   CUI_SignalCallback callback, void *userdata);

/* Disconnect a specific connection */
void cui_node_disconnect(CUI_Node *node, uint32_t connection_id);

/* Disconnect all connections of a signal type */
void cui_node_disconnect_all(CUI_Node *node, CUI_SignalType signal);

/* Emit a signal */
void cui_node_emit(CUI_Node *node, CUI_SignalType signal, const CUI_Signal *data);

/* Emit a simple signal (no extra data) */
void cui_node_emit_simple(CUI_Node *node, CUI_SignalType signal);

/* ============================================================================
 * Scene Tree Processing
 * ============================================================================ */

/* Update all nodes (call each frame) */
void cui_scene_update(CUI_Context *ctx, CUI_Node *root, float delta_time);

/* Process an SDL event through the tree */
bool cui_scene_process_event(CUI_Context *ctx, CUI_Node *root,
                              const SDL_Event *event);

/* Render the scene tree */
void cui_scene_render(CUI_Context *ctx, CUI_Node *root);

/* Layout pass (called automatically, but can force) */
void cui_scene_layout(CUI_Context *ctx, CUI_Node *root);

/* ============================================================================
 * Hit Testing
 * ============================================================================ */

/* Find node at screen position */
CUI_Node *cui_node_hit_test(CUI_Node *root, float x, float y);

/* Check if point is inside node */
bool cui_node_contains_point(CUI_Node *node, float x, float y);

/* ============================================================================
 * Convenience Creators
 * ============================================================================ */

/* Create a label */
CUI_Node *cui_label_create(CUI_Context *ctx, const char *name, const char *text);

/* Create a button */
CUI_Node *cui_button_create(CUI_Context *ctx, const char *name, const char *text);

/* Create VBox container */
CUI_Node *cui_vbox_create(CUI_Context *ctx, const char *name);

/* Create HBox container */
CUI_Node *cui_hbox_create(CUI_Context *ctx, const char *name);

/* Create grid container */
CUI_Node *cui_grid_create(CUI_Context *ctx, const char *name, int columns);

/* Create margin container */
CUI_Node *cui_margin_create(CUI_Context *ctx, const char *name);

/* Create scroll container */
CUI_Node *cui_scroll_create(CUI_Context *ctx, const char *name);

/* Create panel */
CUI_Node *cui_panel_create(CUI_Context *ctx, const char *name, const char *title);

/* ============================================================================
 * Container-Specific Functions
 * ============================================================================ */

/* VBox/HBox */
void cui_box_set_separation(CUI_Node *node, float separation);
void cui_box_set_alignment(CUI_Node *node, CUI_SizeFlags alignment);

/* Grid */
void cui_grid_set_columns(CUI_Node *node, int columns);
void cui_grid_set_h_separation(CUI_Node *node, float separation);
void cui_grid_set_v_separation(CUI_Node *node, float separation);

/* Margin container */
void cui_margin_set_margins(CUI_Node *node, float left, float top,
                             float right, float bottom);

/* Scroll container */
void cui_scroll_set_h_scroll_enabled(CUI_Node *node, bool enabled);
void cui_scroll_set_v_scroll_enabled(CUI_Node *node, bool enabled);
void cui_scroll_set_scroll(CUI_Node *node, float x, float y);
void cui_scroll_ensure_visible(CUI_Node *node, CUI_Rect rect);

/* ============================================================================
 * Widget-Specific Functions
 * ============================================================================ */

/* Label */
void cui_label_set_text(CUI_Node *node, const char *text);
const char *cui_label_get_text(CUI_Node *node);

/* Button */
void cui_button_set_text(CUI_Node *node, const char *text);
void cui_button_set_disabled(CUI_Node *node, bool disabled);
void cui_button_set_toggle_mode(CUI_Node *node, bool toggle);
bool cui_button_is_toggled(CUI_Node *node);

/* Checkbox */
void cui_checkbox_set_checked(CUI_Node *node, bool checked);
bool cui_checkbox_is_checked(CUI_Node *node);

/* Slider */
void cui_slider_set_value(CUI_Node *node, float value);
float cui_slider_get_value(CUI_Node *node);
void cui_slider_set_range(CUI_Node *node, float min, float max);
void cui_slider_set_step(CUI_Node *node, float step);

/* Textbox */
void cui_textbox_set_text(CUI_Node *node, const char *text);
const char *cui_textbox_get_text(CUI_Node *node);
void cui_textbox_set_placeholder(CUI_Node *node, const char *placeholder);

/* Dropdown */
void cui_dropdown_set_items(CUI_Node *node, const char **items, int count);
void cui_dropdown_set_selected(CUI_Node *node, int index);
int cui_dropdown_get_selected(CUI_Node *node);

/* Progress bar */
void cui_progress_set_value(CUI_Node *node, float value);
void cui_progress_set_range(CUI_Node *node, float min, float max);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_UI_NODE_H */
