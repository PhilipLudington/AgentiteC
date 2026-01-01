/*
 * Agentite UI - Retained Mode Node System
 *
 * Provides a scene tree of UI nodes with Godot-style anchors and layout.
 *
 * Usage:
 *   // Create a scene tree
 *   AUI_Node *root = aui_node_create(ctx, AUI_NODE_CONTROL, "root");
 *   aui_node_set_anchor_preset(root, AUI_ANCHOR_FULL_RECT);
 *
 *   AUI_Node *panel = aui_node_create(ctx, AUI_NODE_PANEL, "settings");
 *   aui_node_set_anchor_preset(panel, AUI_ANCHOR_CENTER);
 *   aui_node_set_offsets(panel, -200, -150, 200, 150);
 *   aui_node_add_child(root, panel);
 *
 *   // Each frame
 *   aui_scene_update(ctx, root, delta_time);
 *   aui_scene_process_event(ctx, root, &event);
 *   aui_scene_render(ctx, root);
 */

#ifndef AGENTITE_UI_NODE_H
#define AGENTITE_UI_NODE_H

#include "agentite/ui.h"
#include "agentite/ui_style.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct AUI_Context AUI_Context;
typedef struct AUI_Node AUI_Node;
typedef struct AUI_Signal AUI_Signal;

/* ============================================================================
 * Node Types
 * ============================================================================ */

typedef enum AUI_NodeType {
    /* Base types */
    AUI_NODE_CONTROL,          /* Base control node (no rendering) */
    AUI_NODE_CONTAINER,        /* Generic container */

    /* Layout containers */
    AUI_NODE_VBOX,             /* Vertical box container */
    AUI_NODE_HBOX,             /* Horizontal box container */
    AUI_NODE_GRID,             /* Grid container */
    AUI_NODE_MARGIN,           /* Margin container */
    AUI_NODE_SCROLL,           /* Scrollable container */
    AUI_NODE_CENTER,           /* Centers child */

    /* Display widgets */
    AUI_NODE_LABEL,
    AUI_NODE_ICON,
    AUI_NODE_TEXTURE_RECT,
    AUI_NODE_PROGRESS_BAR,
    AUI_NODE_SEPARATOR,

    /* Interactive widgets */
    AUI_NODE_BUTTON,
    AUI_NODE_CHECKBOX,
    AUI_NODE_RADIO,
    AUI_NODE_SLIDER,
    AUI_NODE_TEXTBOX,
    AUI_NODE_DROPDOWN,
    AUI_NODE_SPINBOX,

    /* Containers/Windows */
    AUI_NODE_PANEL,
    AUI_NODE_DIALOG,
    AUI_NODE_POPUP,
    AUI_NODE_TAB_CONTAINER,

    /* Advanced widgets */
    AUI_NODE_TREE,
    AUI_NODE_RICHTEXT,
    AUI_NODE_CHART,
    AUI_NODE_COLLAPSING_HEADER,
    AUI_NODE_SPLITTER,

    /* Custom */
    AUI_NODE_CUSTOM,

    AUI_NODE_TYPE_COUNT
} AUI_NodeType;

/* ============================================================================
 * Anchor Presets
 * ============================================================================ */

typedef enum AUI_AnchorPreset {
    AUI_ANCHOR_TOP_LEFT,
    AUI_ANCHOR_TOP_CENTER,
    AUI_ANCHOR_TOP_RIGHT,
    AUI_ANCHOR_CENTER_LEFT,
    AUI_ANCHOR_CENTER,
    AUI_ANCHOR_CENTER_RIGHT,
    AUI_ANCHOR_BOTTOM_LEFT,
    AUI_ANCHOR_BOTTOM_CENTER,
    AUI_ANCHOR_BOTTOM_RIGHT,

    /* Wide presets (span full width/height on one axis) */
    AUI_ANCHOR_TOP_WIDE,
    AUI_ANCHOR_BOTTOM_WIDE,
    AUI_ANCHOR_LEFT_WIDE,
    AUI_ANCHOR_RIGHT_WIDE,
    AUI_ANCHOR_VCENTER_WIDE,
    AUI_ANCHOR_HCENTER_WIDE,

    /* Full rect (fills parent) */
    AUI_ANCHOR_FULL_RECT,

    AUI_ANCHOR_PRESET_COUNT
} AUI_AnchorPreset;

/* ============================================================================
 * Size Flags
 * ============================================================================ */

typedef enum AUI_SizeFlags {
    AUI_SIZE_NONE        = 0,
    AUI_SIZE_FILL        = 1 << 0,   /* Fill available space */
    AUI_SIZE_EXPAND      = 1 << 1,   /* Expand to take extra space */
    AUI_SIZE_SHRINK_CENTER = 1 << 2, /* Shrink and center */
    AUI_SIZE_SHRINK_END  = 1 << 3,   /* Shrink and align to end */
} AUI_SizeFlags;

/* ============================================================================
 * Anchors and Offsets
 * ============================================================================ */

/* Anchors are 0-1 values relative to parent rect */
typedef struct AUI_Anchors {
    float left;
    float top;
    float right;
    float bottom;
} AUI_Anchors;

/* ============================================================================
 * Signal Types
 * ============================================================================ */

typedef enum AUI_SignalType {
    /* Input signals */
    AUI_SIGNAL_PRESSED,
    AUI_SIGNAL_RELEASED,
    AUI_SIGNAL_CLICKED,
    AUI_SIGNAL_DOUBLE_CLICKED,
    AUI_SIGNAL_RIGHT_CLICKED,

    /* Focus signals */
    AUI_SIGNAL_FOCUSED,
    AUI_SIGNAL_UNFOCUSED,

    /* Hover signals */
    AUI_SIGNAL_MOUSE_ENTERED,
    AUI_SIGNAL_MOUSE_EXITED,

    /* Value signals */
    AUI_SIGNAL_VALUE_CHANGED,
    AUI_SIGNAL_TEXT_CHANGED,
    AUI_SIGNAL_SELECTION_CHANGED,
    AUI_SIGNAL_TOGGLED,

    /* Layout signals */
    AUI_SIGNAL_RESIZED,
    AUI_SIGNAL_VISIBILITY_CHANGED,
    AUI_SIGNAL_MINIMUM_SIZE_CHANGED,

    /* Tree hierarchy signals */
    AUI_SIGNAL_CHILD_ADDED,
    AUI_SIGNAL_CHILD_REMOVED,
    AUI_SIGNAL_TREE_ENTERED,
    AUI_SIGNAL_TREE_EXITED,

    /* Tree widget signals */
    AUI_SIGNAL_ITEM_SELECTED,
    AUI_SIGNAL_ITEM_ACTIVATED,
    AUI_SIGNAL_ITEM_EXPANDED,
    AUI_SIGNAL_ITEM_COLLAPSED,

    /* Custom signals (100+) */
    AUI_SIGNAL_CUSTOM = 100,

    AUI_SIGNAL_TYPE_COUNT = 200
} AUI_SignalType;

/* ============================================================================
 * Signal Data
 * ============================================================================ */

struct AUI_Signal {
    AUI_SignalType type;
    AUI_Node *source;

    union {
        /* Value changes */
        struct { int old_value, new_value; } int_change;
        struct { float old_value, new_value; } float_change;
        struct { bool old_value, new_value; } bool_change;
        struct { const char *old_text, *new_text; } text_change;

        /* Mouse events */
        struct { float x, y; int button; } mouse;

        /* Child changes */
        struct { AUI_Node *child; } child;

        /* Custom data */
        void *custom_data;
    };
};

/* ============================================================================
 * Signal Callback
 * ============================================================================ */

typedef void (*AUI_SignalCallback)(AUI_Node *node, const AUI_Signal *signal,
                                    void *userdata);

/* ============================================================================
 * Signal Connection
 * ============================================================================ */

#define AUI_MAX_CONNECTIONS 16

typedef struct AUI_Connection {
    uint32_t id;
    AUI_SignalType signal_type;
    AUI_SignalCallback callback;
    void *userdata;
    bool active;
    bool oneshot;              /* Disconnect after first call */
} AUI_Connection;

/* ============================================================================
 * Node-Specific Data
 * ============================================================================ */

/* Label data */
typedef struct AUI_LabelData {
    char text[256];
    uint32_t color;
    bool autowrap;
    int max_lines;
} AUI_LabelData;

/* Button data */
typedef struct AUI_ButtonData {
    char text[256];
    bool disabled;
    bool toggle_mode;
    bool toggled;
} AUI_ButtonData;

/* Checkbox data */
typedef struct AUI_CheckboxData {
    char text[256];
    bool checked;
    bool disabled;
} AUI_CheckboxData;

/* Slider data */
typedef struct AUI_SliderData {
    float value;
    float min_value;
    float max_value;
    float step;
    bool show_value;
    bool dragging;
} AUI_SliderData;

/* Textbox data */
typedef struct AUI_TextboxData {
    char *buffer;
    int buffer_size;
    int cursor_pos;
    int selection_start;
    int selection_end;
    bool password_mode;
    char placeholder[128];
} AUI_TextboxData;

/* Dropdown data */
typedef struct AUI_DropdownData {
    int selected;
    const char **items;
    int item_count;
    bool open;
} AUI_DropdownData;

/* Panel data */
typedef struct AUI_PanelData {
    char title[128];
    uint32_t flags;
    bool dragging;
    float drag_offset_x, drag_offset_y;
    bool closed;
    bool collapsed;      /* Is content collapsed (only title bar shown) */
    bool closable;       /* Show close button in title bar */
    bool collapsible;    /* Show collapse button in title bar */
} AUI_PanelData;

/* Collapsing header data */
typedef struct AUI_CollapsingHeaderData {
    char text[256];
    bool expanded;
    bool show_arrow;
} AUI_CollapsingHeaderData;

/* Splitter data */
typedef struct AUI_SplitterData {
    bool horizontal;           /* true = left/right split */
    float split_ratio;         /* 0.0-1.0 position of splitter */
    float min_size_first;      /* min pixels for first child */
    float min_size_second;     /* min pixels for second child */
    float splitter_width;      /* drag bar width */
    bool dragging;
    float drag_start_ratio;
} AUI_SplitterData;

/* Tree item (forward declaration) */
typedef struct AUI_TreeItem AUI_TreeItem;

/* Tree item structure */
struct AUI_TreeItem {
    uint32_t id;                /* Unique item ID */
    char text[256];             /* Display text */
    bool expanded;              /* Is this item expanded */
    bool selected;              /* Is this item selected */
    void *user_data;            /* User data pointer */
    void *icon;                 /* Optional icon (texture) */

    AUI_TreeItem *parent;       /* Parent item (NULL for root) */
    AUI_TreeItem *first_child;
    AUI_TreeItem *last_child;
    AUI_TreeItem *next_sibling;
    AUI_TreeItem *prev_sibling;
};

/* Tree drag drop position */
typedef enum AUI_TreeDropPosition {
    AUI_TREE_DROP_NONE,             /* No drop target */
    AUI_TREE_DROP_BEFORE,           /* Drop as sibling before target */
    AUI_TREE_DROP_AFTER,            /* Drop as sibling after target */
    AUI_TREE_DROP_INTO              /* Drop as child of target */
} AUI_TreeDropPosition;

/* Tree widget data */
typedef struct AUI_TreeData {
    AUI_TreeItem *root_items;       /* First root item (linked list) */
    AUI_TreeItem *selected_item;    /* Currently selected item */
    AUI_TreeItem *anchor_item;      /* For shift-click range selection */
    float indent_width;             /* Pixels per indentation level */
    float item_height;              /* Height of each item row */
    float scroll_offset;            /* Vertical scroll offset */
    bool multi_select;              /* Allow multiple selection */
    bool hide_root;                 /* Hide root level items */
    bool allow_reorder;             /* Allow drag to reorder */
    uint32_t next_item_id;          /* Counter for unique IDs */

    /* Drag-to-reorder state */
    AUI_TreeItem *dragging_item;    /* Item being dragged (NULL if not dragging) */
    AUI_TreeItem *drop_target;      /* Potential drop target item */
    AUI_TreeDropPosition drop_pos;  /* Where to drop relative to target */
    float drag_start_x, drag_start_y; /* Mouse position when drag started */
    bool drag_started;              /* Has drag threshold been exceeded */
} AUI_TreeData;

/* VBox/HBox data */
typedef struct AUI_BoxData {
    float separation;
    bool reverse;
    AUI_SizeFlags alignment;
} AUI_BoxData;

/* Grid data */
typedef struct AUI_GridData {
    int columns;
    float h_separation;
    float v_separation;
} AUI_GridData;

/* Scroll data */
typedef struct AUI_ScrollData {
    float scroll_x, scroll_y;
    float content_width, content_height;
    bool h_scroll_enabled;
    bool v_scroll_enabled;
    bool dragging_h, dragging_v;
} AUI_ScrollData;

/* Progress bar data */
typedef struct AUI_ProgressData {
    float value;
    float min_value;
    float max_value;
    uint32_t fill_color;
} AUI_ProgressData;

/* Texture rect data - for displaying images/textures */
typedef struct AUI_TextureRectData {
    SDL_GPUTexture *texture;
    float src_x, src_y, src_w, src_h;  /* Source rectangle (0 = use full texture) */
    uint32_t tint;                      /* Color tint (0xFFFFFFFF = no tint) */
    bool stretch;                       /* true = stretch to fill, false = maintain aspect */
    bool flip_h, flip_v;                /* Horizontal/vertical flip */
} AUI_TextureRectData;

/* Icon data - for displaying small icons */
typedef struct AUI_IconData {
    SDL_GPUTexture *texture;
    float icon_x, icon_y, icon_w, icon_h;  /* Icon region in atlas */
    uint32_t color;                         /* Icon color/tint */
    float size;                             /* Display size (0 = use icon_w/h) */
} AUI_IconData;

/* Separator data - horizontal or vertical line divider */
typedef struct AUI_SeparatorData {
    bool vertical;          /* true = vertical line, false = horizontal */
    uint32_t color;         /* Line color (0 = use theme border color) */
    float thickness;        /* Line thickness (0 = 1px default) */
} AUI_SeparatorData;

/* ============================================================================
 * Main Node Structure
 * ============================================================================ */

struct AUI_Node {
    /* Identity */
    uint32_t id;
    AUI_NodeType type;
    char name[64];

    /* Hierarchy */
    AUI_Node *parent;
    AUI_Node *first_child;
    AUI_Node *last_child;
    AUI_Node *next_sibling;
    AUI_Node *prev_sibling;
    int child_count;

    /* Anchors and offsets (Godot-style layout) */
    AUI_Anchors anchors;
    AUI_Edges offsets;         /* Pixel offset from anchor position */

    /* Computed layout */
    AUI_Rect rect;             /* Local rect (relative to parent) */
    AUI_Rect global_rect;      /* Screen coordinates */
    bool layout_dirty;

    /* Size hints */
    float min_size_x, min_size_y;
    float custom_min_size_x, custom_min_size_y;  /* User-specified minimum */
    uint8_t h_size_flags;
    uint8_t v_size_flags;
    float size_flags_stretch_ratio;

    /* Styling */
    AUI_Style style;
    const char *style_class_name;
    AUI_Style *style_override;  /* Runtime style override */

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

    /* Style transition state */
    struct {
        bool prev_hovered;             /* Was hovered last frame */
        bool prev_pressed;             /* Was pressed last frame */
        float progress;                /* Transition progress 0-1 */
        uint32_t from_bg_color;        /* Start background color */
        uint32_t to_bg_color;          /* Target background color */
        uint32_t current_bg_color;     /* Current interpolated color */
        uint32_t from_text_color;      /* Start text color */
        uint32_t to_text_color;        /* Target text color */
        uint32_t current_text_color;   /* Current interpolated color */
        uint32_t from_border_color;    /* Start border color */
        uint32_t to_border_color;      /* Target border color */
        uint32_t current_border_color; /* Current interpolated color */
        bool active;                   /* Is a transition in progress */
    } transition_state;

    /* Focus */
    bool focus_mode_click;
    bool focus_mode_all;
    AUI_Node *focus_next;
    AUI_Node *focus_prev;

    /* Tooltip */
    char tooltip_text[128];        /* Tooltip to show on hover */
    float tooltip_delay;           /* Delay before showing (default 0.5s) */

    /* Signals/callbacks */
    AUI_Connection connections[AUI_MAX_CONNECTIONS];
    int connection_count;

    /* Type-specific data */
    union {
        AUI_LabelData label;
        AUI_ButtonData button;
        AUI_CheckboxData checkbox;
        AUI_SliderData slider;
        AUI_TextboxData textbox;
        AUI_DropdownData dropdown;
        AUI_PanelData panel;
        AUI_BoxData box;
        AUI_GridData grid;
        AUI_ScrollData scroll;
        AUI_ProgressData progress;
        AUI_CollapsingHeaderData collapsing_header;
        AUI_SplitterData splitter;
        AUI_TreeData tree;
        AUI_TextureRectData texture_rect;
        AUI_IconData icon;
        AUI_SeparatorData separator;
        void *custom_data;
    };

    /* Virtual functions (for custom nodes) */
    void (*on_draw)(AUI_Node *node, AUI_Context *ctx);
    void (*on_input)(AUI_Node *node, AUI_Context *ctx, const SDL_Event *event);
    bool (*on_gui_input)(AUI_Node *node, AUI_Context *ctx, const SDL_Event *event);
    void (*on_layout)(AUI_Node *node);
    void (*on_enter_tree)(AUI_Node *node);
    void (*on_exit_tree)(AUI_Node *node);
    void (*on_ready)(AUI_Node *node);
    void (*on_process)(AUI_Node *node, float delta);
    void (*on_destroy)(AUI_Node *node);
    void (*on_notification)(AUI_Node *node, int what);
};

/* ============================================================================
 * Node Lifecycle
 * ============================================================================ */

/* Create a new node */
AUI_Node *aui_node_create(AUI_Context *ctx, AUI_NodeType type, const char *name);

/* Destroy a node and all its children */
void aui_node_destroy(AUI_Node *node);

/* Duplicate a node (deep copy) */
AUI_Node *aui_node_duplicate(AUI_Node *node);

/* ============================================================================
 * Hierarchy Management
 * ============================================================================ */

/* Add a child node */
void aui_node_add_child(AUI_Node *parent, AUI_Node *child);

/* Remove a child node (doesn't destroy it) */
void aui_node_remove_child(AUI_Node *parent, AUI_Node *child);

/* Remove from parent */
void aui_node_remove(AUI_Node *node);

/* Move to a new parent */
void aui_node_reparent(AUI_Node *node, AUI_Node *new_parent);

/* Get child by index */
AUI_Node *aui_node_get_child(AUI_Node *node, int index);

/* Get child by name */
AUI_Node *aui_node_get_child_by_name(AUI_Node *node, const char *name);

/* Find node by path (e.g., "Panel/Content/Button") */
AUI_Node *aui_node_find(AUI_Node *root, const char *path);

/* Get the root node */
AUI_Node *aui_node_get_root(AUI_Node *node);

/* Check if node is ancestor of another */
bool aui_node_is_ancestor_of(AUI_Node *node, AUI_Node *descendant);

/* Get node index among siblings */
int aui_node_get_index(AUI_Node *node);

/* Move in sibling order */
void aui_node_move_child(AUI_Node *parent, AUI_Node *child, int new_index);
void aui_node_move_to_front(AUI_Node *node);
void aui_node_move_to_back(AUI_Node *node);

/* ============================================================================
 * Layout
 * ============================================================================ */

/* Set anchor preset */
void aui_node_set_anchor_preset(AUI_Node *node, AUI_AnchorPreset preset);

/* Set individual anchors (0-1 values) */
void aui_node_set_anchors(AUI_Node *node, float left, float top,
                           float right, float bottom);

/* Set offsets from anchors (in pixels) */
void aui_node_set_offsets(AUI_Node *node, float left, float top,
                           float right, float bottom);

/* Set size directly (adjusts offsets) */
void aui_node_set_size(AUI_Node *node, float width, float height);

/* Set position (relative to anchor position) */
void aui_node_set_position(AUI_Node *node, float x, float y);

/* Get computed size */
void aui_node_get_size(AUI_Node *node, float *width, float *height);

/* Get position in parent */
void aui_node_get_position(AUI_Node *node, float *x, float *y);

/* Get global position */
void aui_node_get_global_position(AUI_Node *node, float *x, float *y);

/* Size flags */
void aui_node_set_h_size_flags(AUI_Node *node, uint8_t flags);
void aui_node_set_v_size_flags(AUI_Node *node, uint8_t flags);
void aui_node_set_stretch_ratio(AUI_Node *node, float ratio);

/* Minimum size */
void aui_node_set_custom_min_size(AUI_Node *node, float width, float height);
void aui_node_get_min_size(AUI_Node *node, float *width, float *height);

/* Force layout recalculation */
void aui_node_queue_layout(AUI_Node *node);

/* ============================================================================
 * Styling
 * ============================================================================ */

/* Set the node's style directly */
void aui_node_set_style(AUI_Node *node, const AUI_Style *style);

/* Set style class by name */
void aui_node_set_style_class(AUI_Node *node, const char *class_name);

/* Get the effective style (resolved from class + overrides) */
AUI_Style aui_node_get_effective_style(AUI_Node *node);

/* ============================================================================
 * State
 * ============================================================================ */

/* Visibility */
void aui_node_set_visible(AUI_Node *node, bool visible);
bool aui_node_is_visible(AUI_Node *node);
bool aui_node_is_visible_in_tree(AUI_Node *node);

/* Enable/disable */
void aui_node_set_enabled(AUI_Node *node, bool enabled);
bool aui_node_is_enabled(AUI_Node *node);

/* Focus */
void aui_node_grab_focus(AUI_Node *node);
void aui_node_release_focus(AUI_Node *node);
bool aui_node_has_focus(AUI_Node *node);
AUI_Node *aui_get_focused_node(AUI_Context *ctx);

/* Opacity */
void aui_node_set_opacity(AUI_Node *node, float opacity);
float aui_node_get_opacity(AUI_Node *node);

/* ============================================================================
 * Signals
 * ============================================================================ */

/* Connect a callback to a signal */
uint32_t aui_node_connect(AUI_Node *node, AUI_SignalType signal,
                           AUI_SignalCallback callback, void *userdata);

/* Connect with oneshot (disconnect after first call) */
uint32_t aui_node_connect_oneshot(AUI_Node *node, AUI_SignalType signal,
                                   AUI_SignalCallback callback, void *userdata);

/* Disconnect a specific connection */
void aui_node_disconnect(AUI_Node *node, uint32_t connection_id);

/* Disconnect all connections of a signal type */
void aui_node_disconnect_all(AUI_Node *node, AUI_SignalType signal);

/* Emit a signal */
void aui_node_emit(AUI_Node *node, AUI_SignalType signal, const AUI_Signal *data);

/* Emit a simple signal (no extra data) */
void aui_node_emit_simple(AUI_Node *node, AUI_SignalType signal);

/* ============================================================================
 * Scene Tree Processing
 * ============================================================================ */

/* Update all nodes (call each frame) */
void aui_scene_update(AUI_Context *ctx, AUI_Node *root, float delta_time);

/* Process an SDL event through the tree */
bool aui_scene_process_event(AUI_Context *ctx, AUI_Node *root,
                              const SDL_Event *event);

/* Render the scene tree */
void aui_scene_render(AUI_Context *ctx, AUI_Node *root);

/* Layout pass (called automatically, but can force) */
void aui_scene_layout(AUI_Context *ctx, AUI_Node *root);

/* ============================================================================
 * Hit Testing
 * ============================================================================ */

/* Find node at screen position */
AUI_Node *aui_node_hit_test(AUI_Node *root, float x, float y);

/* Check if point is inside node */
bool aui_node_contains_point(AUI_Node *node, float x, float y);

/* ============================================================================
 * Convenience Creators
 * ============================================================================ */

/* Create a label */
AUI_Node *aui_label_create(AUI_Context *ctx, const char *name, const char *text);

/* Create a button */
AUI_Node *aui_button_create(AUI_Context *ctx, const char *name, const char *text);

/* Create VBox container */
AUI_Node *aui_vbox_create(AUI_Context *ctx, const char *name);

/* Create HBox container */
AUI_Node *aui_hbox_create(AUI_Context *ctx, const char *name);

/* Create grid container */
AUI_Node *aui_grid_create(AUI_Context *ctx, const char *name, int columns);

/* Create margin container */
AUI_Node *aui_margin_create(AUI_Context *ctx, const char *name);

/* Create center container - centers its single child */
AUI_Node *aui_center_create(AUI_Context *ctx, const char *name);

/* Create scroll container */
AUI_Node *aui_scroll_create(AUI_Context *ctx, const char *name);

/* Create panel */
AUI_Node *aui_panel_create(AUI_Context *ctx, const char *name, const char *title);

/* Create textbox */
AUI_Node *aui_textbox_create(AUI_Context *ctx, const char *name,
                              char *buffer, int buffer_size);

/* Create checkbox */
AUI_Node *aui_checkbox_create(AUI_Context *ctx, const char *name,
                               const char *text, bool *value);

/* Create slider */
AUI_Node *aui_slider_create(AUI_Context *ctx, const char *name,
                             float min_val, float max_val, float *value);

/* Create collapsing header */
AUI_Node *aui_collapsing_header_create(AUI_Context *ctx, const char *name,
                                        const char *text);

/* Create splitter */
AUI_Node *aui_splitter_create(AUI_Context *ctx, const char *name, bool horizontal);

/* Create tree widget */
AUI_Node *aui_tree_create(AUI_Context *ctx, const char *name);

/* Create texture rect - displays a texture/image */
AUI_Node *aui_texture_rect_create(AUI_Context *ctx, const char *name,
                                   SDL_GPUTexture *texture);

/* Create icon - displays a small icon from an atlas */
AUI_Node *aui_icon_create(AUI_Context *ctx, const char *name,
                           SDL_GPUTexture *atlas, float x, float y, float w, float h);

/* Create separator - horizontal or vertical line divider */
AUI_Node *aui_separator_create(AUI_Context *ctx, const char *name, bool vertical);

/* ============================================================================
 * Texture Rect Functions
 * ============================================================================ */

/* Set texture rect source region */
void aui_texture_rect_set_region(AUI_Node *node, float x, float y, float w, float h);

/* Set texture rect tint color */
void aui_texture_rect_set_tint(AUI_Node *node, uint32_t color);

/* Set texture rect stretch mode */
void aui_texture_rect_set_stretch(AUI_Node *node, bool stretch);

/* Set texture rect flip */
void aui_texture_rect_set_flip(AUI_Node *node, bool flip_h, bool flip_v);

/* ============================================================================
 * Icon Functions
 * ============================================================================ */

/* Set icon color */
void aui_icon_set_color(AUI_Node *node, uint32_t color);

/* Set icon display size */
void aui_icon_set_size(AUI_Node *node, float size);

/* ============================================================================
 * Separator Functions
 * ============================================================================ */

/* Set separator color */
void aui_separator_set_color(AUI_Node *node, uint32_t color);

/* Set separator thickness */
void aui_separator_set_thickness(AUI_Node *node, float thickness);

/* ============================================================================
 * Tree Widget Functions
 * ============================================================================ */

/* Item management */
AUI_TreeItem *aui_tree_add_item(AUI_Node *tree, const char *text, void *user_data);
AUI_TreeItem *aui_tree_add_child(AUI_Node *tree, AUI_TreeItem *parent,
                                  const char *text, void *user_data);
void aui_tree_remove_item(AUI_Node *tree, AUI_TreeItem *item);
void aui_tree_clear(AUI_Node *tree);

/* Selection */
AUI_TreeItem *aui_tree_get_selected(AUI_Node *tree);
void aui_tree_set_selected(AUI_Node *tree, AUI_TreeItem *item);

/* Expand/collapse */
void aui_tree_set_expanded(AUI_Node *tree, AUI_TreeItem *item, bool expanded);
void aui_tree_expand_all(AUI_Node *tree);
void aui_tree_collapse_all(AUI_Node *tree);

/* Navigation */
void aui_tree_ensure_visible(AUI_Node *tree, AUI_TreeItem *item);
AUI_TreeItem *aui_tree_find_by_data(AUI_Node *tree, void *user_data);

/* Properties */
void aui_tree_set_multi_select(AUI_Node *tree, bool multi);
void aui_tree_set_indent(AUI_Node *tree, float indent_width);
void aui_tree_set_item_height(AUI_Node *tree, float height);
void aui_tree_set_allow_reorder(AUI_Node *tree, bool allow);

/* Item properties */
void aui_tree_item_set_text(AUI_TreeItem *item, const char *text);
void aui_tree_item_set_icon(AUI_TreeItem *item, void *icon);
int aui_tree_item_get_depth(AUI_TreeItem *item);
bool aui_tree_item_has_children(AUI_TreeItem *item);

/* ============================================================================
 * Container-Specific Functions
 * ============================================================================ */

/* VBox/HBox */
void aui_box_set_separation(AUI_Node *node, float separation);
void aui_box_set_alignment(AUI_Node *node, AUI_SizeFlags alignment);

/* Grid */
void aui_grid_set_columns(AUI_Node *node, int columns);
void aui_grid_set_h_separation(AUI_Node *node, float separation);
void aui_grid_set_v_separation(AUI_Node *node, float separation);

/* Margin container */
void aui_margin_set_margins(AUI_Node *node, float left, float top,
                             float right, float bottom);

/* Scroll container */
void aui_scroll_set_h_scroll_enabled(AUI_Node *node, bool enabled);
void aui_scroll_set_v_scroll_enabled(AUI_Node *node, bool enabled);
void aui_scroll_set_scroll(AUI_Node *node, float x, float y);
void aui_scroll_ensure_visible(AUI_Node *node, AUI_Rect rect);

/* ============================================================================
 * Widget-Specific Functions
 * ============================================================================ */

/* Label */
void aui_label_set_text(AUI_Node *node, const char *text);
const char *aui_label_get_text(AUI_Node *node);

/* Button */
void aui_button_set_text(AUI_Node *node, const char *text);
void aui_button_set_disabled(AUI_Node *node, bool disabled);
void aui_button_set_toggle_mode(AUI_Node *node, bool toggle);
bool aui_button_is_toggled(AUI_Node *node);

/* Checkbox */
void aui_checkbox_set_checked(AUI_Node *node, bool checked);
bool aui_checkbox_is_checked(AUI_Node *node);

/* Slider */
void aui_slider_set_value(AUI_Node *node, float value);
float aui_slider_get_value(AUI_Node *node);
void aui_slider_set_range(AUI_Node *node, float min, float max);
void aui_slider_set_step(AUI_Node *node, float step);

/* Textbox */
void aui_textbox_set_text(AUI_Node *node, const char *text);
const char *aui_textbox_get_text(AUI_Node *node);
void aui_textbox_set_placeholder(AUI_Node *node, const char *placeholder);

/* Dropdown */
void aui_dropdown_set_items(AUI_Node *node, const char **items, int count);
void aui_dropdown_set_selected(AUI_Node *node, int index);
int aui_dropdown_get_selected(AUI_Node *node);

/* Progress bar */
void aui_progress_set_value(AUI_Node *node, float value);
void aui_progress_set_range(AUI_Node *node, float min, float max);

/* Panel */
void aui_panel_set_title(AUI_Node *node, const char *title);
void aui_panel_set_closable(AUI_Node *node, bool closable);
void aui_panel_set_collapsible(AUI_Node *node, bool collapsible);
bool aui_panel_is_collapsed(AUI_Node *node);
void aui_panel_set_collapsed(AUI_Node *node, bool collapsed);
bool aui_panel_is_closed(AUI_Node *node);

/* Collapsing Header */
void aui_collapsing_header_set_text(AUI_Node *node, const char *text);
void aui_collapsing_header_set_expanded(AUI_Node *node, bool expanded);
bool aui_collapsing_header_is_expanded(AUI_Node *node);

/* Splitter */
void aui_splitter_set_ratio(AUI_Node *node, float ratio);
float aui_splitter_get_ratio(AUI_Node *node);
void aui_splitter_set_min_sizes(AUI_Node *node, float first, float second);
void aui_splitter_set_width(AUI_Node *node, float width);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_UI_NODE_H */
