/*
 * Carbon UI - Retained Mode Node System Implementation
 */

#include "carbon/ui_node.h"
#include "carbon/ui.h"
#include "carbon/ui_style.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal State
 * ============================================================================ */

static uint32_t s_next_node_id = 1;
static CUI_Node *s_focused_node = NULL;

/* ============================================================================
 * Anchor Preset Values
 * ============================================================================ */

typedef struct {
    CUI_Anchors anchors;
    bool use_offset_as_size;  /* If true, offsets define size from anchor point */
} CUI_AnchorPresetData;

static const CUI_AnchorPresetData s_anchor_presets[CUI_ANCHOR_PRESET_COUNT] = {
    /* TOP_LEFT */      {{0.0f, 0.0f, 0.0f, 0.0f}, true},
    /* TOP_CENTER */    {{0.5f, 0.0f, 0.5f, 0.0f}, true},
    /* TOP_RIGHT */     {{1.0f, 0.0f, 1.0f, 0.0f}, true},
    /* CENTER_LEFT */   {{0.0f, 0.5f, 0.0f, 0.5f}, true},
    /* CENTER */        {{0.5f, 0.5f, 0.5f, 0.5f}, true},
    /* CENTER_RIGHT */  {{1.0f, 0.5f, 1.0f, 0.5f}, true},
    /* BOTTOM_LEFT */   {{0.0f, 1.0f, 0.0f, 1.0f}, true},
    /* BOTTOM_CENTER */ {{0.5f, 1.0f, 0.5f, 1.0f}, true},
    /* BOTTOM_RIGHT */  {{1.0f, 1.0f, 1.0f, 1.0f}, true},
    /* TOP_WIDE */      {{0.0f, 0.0f, 1.0f, 0.0f}, true},
    /* BOTTOM_WIDE */   {{0.0f, 1.0f, 1.0f, 1.0f}, true},
    /* LEFT_WIDE */     {{0.0f, 0.0f, 0.0f, 1.0f}, true},
    /* RIGHT_WIDE */    {{1.0f, 0.0f, 1.0f, 1.0f}, true},
    /* VCENTER_WIDE */  {{0.0f, 0.5f, 1.0f, 0.5f}, true},
    /* HCENTER_WIDE */  {{0.5f, 0.0f, 0.5f, 1.0f}, true},
    /* FULL_RECT */     {{0.0f, 0.0f, 1.0f, 1.0f}, false},
};

/* ============================================================================
 * Node Lifecycle
 * ============================================================================ */

CUI_Node *cui_node_create(CUI_Context *ctx, CUI_NodeType type, const char *name)
{
    (void)ctx;

    CUI_Node *node = (CUI_Node *)calloc(1, sizeof(CUI_Node));
    if (!node) return NULL;

    node->id = s_next_node_id++;
    node->type = type;
    if (name) {
        strncpy(node->name, name, sizeof(node->name) - 1);
    }

    /* Default state */
    node->visible = true;
    node->enabled = true;
    node->opacity = 1.0f;
    node->scale_x = 1.0f;
    node->scale_y = 1.0f;
    node->pivot_x = 0.5f;
    node->pivot_y = 0.5f;
    node->size_flags_stretch_ratio = 1.0f;

    /* Default anchors (top-left) */
    node->anchors = (CUI_Anchors){0, 0, 0, 0};

    /* Default style */
    node->style = cui_style_default();

    /* Type-specific initialization */
    switch (type) {
        case CUI_NODE_VBOX:
        case CUI_NODE_HBOX:
            node->box.separation = 4.0f;
            break;

        case CUI_NODE_GRID:
            node->grid.columns = 2;
            node->grid.h_separation = 4.0f;
            node->grid.v_separation = 4.0f;
            break;

        case CUI_NODE_SCROLL:
            node->scroll.h_scroll_enabled = false;
            node->scroll.v_scroll_enabled = true;
            node->clip_contents = true;
            break;

        case CUI_NODE_SLIDER:
            node->slider.min_value = 0.0f;
            node->slider.max_value = 100.0f;
            node->slider.step = 1.0f;
            break;

        case CUI_NODE_PROGRESS_BAR:
            node->progress.min_value = 0.0f;
            node->progress.max_value = 1.0f;
            break;

        case CUI_NODE_BUTTON:
        case CUI_NODE_CHECKBOX:
        case CUI_NODE_TEXTBOX:
            node->focus_mode_click = true;
            break;

        default:
            break;
    }

    node->layout_dirty = true;
    return node;
}

void cui_node_destroy(CUI_Node *node)
{
    if (!node) return;

    /* Call destroy callback */
    if (node->on_destroy) {
        node->on_destroy(node);
    }

    /* Emit tree exit signal */
    cui_node_emit_simple(node, CUI_SIGNAL_TREE_EXITED);

    /* Remove from parent */
    cui_node_remove(node);

    /* Destroy all children */
    CUI_Node *child = node->first_child;
    while (child) {
        CUI_Node *next = child->next_sibling;
        child->parent = NULL;  /* Prevent double-remove */
        cui_node_destroy(child);
        child = next;
    }

    /* Clear focus if this was focused */
    if (s_focused_node == node) {
        s_focused_node = NULL;
    }

    /* Free textbox buffer if allocated */
    if (node->type == CUI_NODE_TEXTBOX && node->textbox.buffer) {
        /* Buffer is user-provided, don't free */
    }

    free(node);
}

CUI_Node *cui_node_duplicate(CUI_Node *node)
{
    if (!node) return NULL;

    CUI_Node *copy = (CUI_Node *)malloc(sizeof(CUI_Node));
    if (!copy) return NULL;

    *copy = *node;
    copy->id = s_next_node_id++;
    copy->parent = NULL;
    copy->first_child = NULL;
    copy->last_child = NULL;
    copy->next_sibling = NULL;
    copy->prev_sibling = NULL;
    copy->child_count = 0;
    copy->connection_count = 0;  /* Don't copy connections */

    /* Duplicate children */
    for (CUI_Node *child = node->first_child; child; child = child->next_sibling) {
        CUI_Node *child_copy = cui_node_duplicate(child);
        if (child_copy) {
            cui_node_add_child(copy, child_copy);
        }
    }

    return copy;
}

/* ============================================================================
 * Hierarchy Management
 * ============================================================================ */

void cui_node_add_child(CUI_Node *parent, CUI_Node *child)
{
    if (!parent || !child) return;
    if (child->parent == parent) return;

    /* Remove from current parent */
    cui_node_remove(child);

    /* Add to new parent */
    child->parent = parent;

    if (parent->last_child) {
        parent->last_child->next_sibling = child;
        child->prev_sibling = parent->last_child;
    } else {
        parent->first_child = child;
    }
    parent->last_child = child;
    parent->child_count++;

    /* Mark for layout */
    parent->layout_dirty = true;

    /* Emit signals */
    CUI_Signal sig;
    memset(&sig, 0, sizeof(sig));
    sig.type = CUI_SIGNAL_CHILD_ADDED;
    sig.source = parent;
    sig.child.child = child;
    cui_node_emit(parent, CUI_SIGNAL_CHILD_ADDED, &sig);

    cui_node_emit_simple(child, CUI_SIGNAL_TREE_ENTERED);

    if (child->on_enter_tree) {
        child->on_enter_tree(child);
    }
}

void cui_node_remove_child(CUI_Node *parent, CUI_Node *child)
{
    if (!parent || !child || child->parent != parent) return;

    /* Unlink from siblings */
    if (child->prev_sibling) {
        child->prev_sibling->next_sibling = child->next_sibling;
    } else {
        parent->first_child = child->next_sibling;
    }

    if (child->next_sibling) {
        child->next_sibling->prev_sibling = child->prev_sibling;
    } else {
        parent->last_child = child->prev_sibling;
    }

    child->parent = NULL;
    child->prev_sibling = NULL;
    child->next_sibling = NULL;
    parent->child_count--;

    /* Mark for layout */
    parent->layout_dirty = true;

    /* Emit signals */
    CUI_Signal sig;
    memset(&sig, 0, sizeof(sig));
    sig.type = CUI_SIGNAL_CHILD_REMOVED;
    sig.source = parent;
    sig.child.child = child;
    cui_node_emit(parent, CUI_SIGNAL_CHILD_REMOVED, &sig);

    if (child->on_exit_tree) {
        child->on_exit_tree(child);
    }
}

void cui_node_remove(CUI_Node *node)
{
    if (!node || !node->parent) return;
    cui_node_remove_child(node->parent, node);
}

void cui_node_reparent(CUI_Node *node, CUI_Node *new_parent)
{
    if (!node) return;
    cui_node_remove(node);
    if (new_parent) {
        cui_node_add_child(new_parent, node);
    }
}

CUI_Node *cui_node_get_child(CUI_Node *node, int index)
{
    if (!node || index < 0 || index >= node->child_count) return NULL;

    CUI_Node *child = node->first_child;
    for (int i = 0; i < index && child; i++) {
        child = child->next_sibling;
    }
    return child;
}

CUI_Node *cui_node_get_child_by_name(CUI_Node *node, const char *name)
{
    if (!node || !name) return NULL;

    for (CUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
    }
    return NULL;
}

CUI_Node *cui_node_find(CUI_Node *root, const char *path)
{
    if (!root || !path) return NULL;

    /* Make a copy to tokenize */
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    CUI_Node *current = root;
    char *token = strtok(path_copy, "/");

    while (token && current) {
        current = cui_node_get_child_by_name(current, token);
        token = strtok(NULL, "/");
    }

    return current;
}

CUI_Node *cui_node_get_root(CUI_Node *node)
{
    if (!node) return NULL;
    while (node->parent) {
        node = node->parent;
    }
    return node;
}

bool cui_node_is_ancestor_of(CUI_Node *node, CUI_Node *descendant)
{
    if (!node || !descendant) return false;

    CUI_Node *current = descendant->parent;
    while (current) {
        if (current == node) return true;
        current = current->parent;
    }
    return false;
}

int cui_node_get_index(CUI_Node *node)
{
    if (!node || !node->parent) return -1;

    int index = 0;
    for (CUI_Node *child = node->parent->first_child; child; child = child->next_sibling) {
        if (child == node) return index;
        index++;
    }
    return -1;
}

void cui_node_move_child(CUI_Node *parent, CUI_Node *child, int new_index)
{
    if (!parent || !child || child->parent != parent) return;
    if (new_index < 0) new_index = 0;
    if (new_index >= parent->child_count) new_index = parent->child_count - 1;

    /* Remove from current position */
    if (child->prev_sibling) {
        child->prev_sibling->next_sibling = child->next_sibling;
    } else {
        parent->first_child = child->next_sibling;
    }
    if (child->next_sibling) {
        child->next_sibling->prev_sibling = child->prev_sibling;
    } else {
        parent->last_child = child->prev_sibling;
    }

    /* Insert at new position */
    if (new_index == 0) {
        child->prev_sibling = NULL;
        child->next_sibling = parent->first_child;
        if (parent->first_child) {
            parent->first_child->prev_sibling = child;
        }
        parent->first_child = child;
        if (!parent->last_child) {
            parent->last_child = child;
        }
    } else {
        CUI_Node *prev = cui_node_get_child(parent, new_index - 1);
        if (prev) {
            child->prev_sibling = prev;
            child->next_sibling = prev->next_sibling;
            if (prev->next_sibling) {
                prev->next_sibling->prev_sibling = child;
            } else {
                parent->last_child = child;
            }
            prev->next_sibling = child;
        }
    }

    parent->layout_dirty = true;
}

void cui_node_move_to_front(CUI_Node *node)
{
    if (!node || !node->parent) return;
    cui_node_move_child(node->parent, node, node->parent->child_count - 1);
}

void cui_node_move_to_back(CUI_Node *node)
{
    if (!node || !node->parent) return;
    cui_node_move_child(node->parent, node, 0);
}

/* ============================================================================
 * Layout
 * ============================================================================ */

void cui_node_set_anchor_preset(CUI_Node *node, CUI_AnchorPreset preset)
{
    if (!node || preset < 0 || preset >= CUI_ANCHOR_PRESET_COUNT) return;

    const CUI_AnchorPresetData *data = &s_anchor_presets[preset];
    node->anchors = data->anchors;
    node->layout_dirty = true;
}

void cui_node_set_anchors(CUI_Node *node, float left, float top,
                           float right, float bottom)
{
    if (!node) return;
    node->anchors.left = left;
    node->anchors.top = top;
    node->anchors.right = right;
    node->anchors.bottom = bottom;
    node->layout_dirty = true;
}

void cui_node_set_offsets(CUI_Node *node, float left, float top,
                           float right, float bottom)
{
    if (!node) return;
    node->offsets.left = left;
    node->offsets.top = top;
    node->offsets.right = right;
    node->offsets.bottom = bottom;
    node->layout_dirty = true;
}

void cui_node_set_size(CUI_Node *node, float width, float height)
{
    if (!node) return;

    /* Adjust offsets to achieve desired size */
    /* For point anchors, offset defines position + size */
    if (node->anchors.left == node->anchors.right) {
        node->offsets.left = -width / 2;
        node->offsets.right = width / 2;
    } else {
        /* Spanning anchors - offsets are edge distances */
        /* Size is determined by anchor span minus offsets */
    }

    if (node->anchors.top == node->anchors.bottom) {
        node->offsets.top = -height / 2;
        node->offsets.bottom = height / 2;
    }

    node->layout_dirty = true;
}

void cui_node_set_position(CUI_Node *node, float x, float y)
{
    if (!node) return;

    float w = node->rect.w;
    float h = node->rect.h;

    node->offsets.left = x;
    node->offsets.top = y;
    node->offsets.right = x + w;
    node->offsets.bottom = y + h;

    node->layout_dirty = true;
}

void cui_node_get_size(CUI_Node *node, float *width, float *height)
{
    if (!node) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    if (width) *width = node->rect.w;
    if (height) *height = node->rect.h;
}

void cui_node_get_position(CUI_Node *node, float *x, float *y)
{
    if (!node) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    if (x) *x = node->rect.x;
    if (y) *y = node->rect.y;
}

void cui_node_get_global_position(CUI_Node *node, float *x, float *y)
{
    if (!node) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    if (x) *x = node->global_rect.x;
    if (y) *y = node->global_rect.y;
}

void cui_node_set_h_size_flags(CUI_Node *node, uint8_t flags)
{
    if (!node) return;
    node->h_size_flags = flags;
    node->layout_dirty = true;
}

void cui_node_set_v_size_flags(CUI_Node *node, uint8_t flags)
{
    if (!node) return;
    node->v_size_flags = flags;
    node->layout_dirty = true;
}

void cui_node_set_stretch_ratio(CUI_Node *node, float ratio)
{
    if (!node) return;
    node->size_flags_stretch_ratio = ratio;
    node->layout_dirty = true;
}

void cui_node_set_custom_min_size(CUI_Node *node, float width, float height)
{
    if (!node) return;
    node->custom_min_size_x = width;
    node->custom_min_size_y = height;
    node->layout_dirty = true;
}

void cui_node_get_min_size(CUI_Node *node, float *width, float *height)
{
    if (!node) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    float min_w = node->custom_min_size_x;
    float min_h = node->custom_min_size_y;

    /* Add padding */
    min_w += node->style.padding.left + node->style.padding.right;
    min_h += node->style.padding.top + node->style.padding.bottom;

    if (width) *width = fmaxf(min_w, node->min_size_x);
    if (height) *height = fmaxf(min_h, node->min_size_y);
}

void cui_node_queue_layout(CUI_Node *node)
{
    if (!node) return;
    node->layout_dirty = true;

    /* Also mark parent */
    if (node->parent) {
        node->parent->layout_dirty = true;
    }
}

/* ============================================================================
 * Styling
 * ============================================================================ */

void cui_node_set_style(CUI_Node *node, const CUI_Style *style)
{
    if (!node || !style) return;
    node->style = *style;
}

void cui_node_set_style_class(CUI_Node *node, const char *class_name)
{
    if (!node) return;
    node->style_class_name = class_name;
}

CUI_Style cui_node_get_effective_style(CUI_Node *node)
{
    CUI_Style style = cui_style_default();
    if (!node) return style;

    /* Start with style class if set */
    if (node->style_class_name) {
        CUI_StyleClass *sc = cui_get_style_class(NULL, node->style_class_name);
        if (sc) {
            style = cui_resolve_style_class(sc);
        }
    }

    /* Merge node's direct style */
    cui_style_merge(&style, &node->style);

    /* Merge runtime override */
    if (node->style_override) {
        cui_style_merge(&style, node->style_override);
    }

    return style;
}

/* ============================================================================
 * State
 * ============================================================================ */

void cui_node_set_visible(CUI_Node *node, bool visible)
{
    if (!node || node->visible == visible) return;

    bool old = node->visible;
    node->visible = visible;

    CUI_Signal sig;
    memset(&sig, 0, sizeof(sig));
    sig.type = CUI_SIGNAL_VISIBILITY_CHANGED;
    sig.source = node;
    sig.bool_change.old_value = old;
    sig.bool_change.new_value = visible;
    cui_node_emit(node, CUI_SIGNAL_VISIBILITY_CHANGED, &sig);

    if (node->parent) {
        node->parent->layout_dirty = true;
    }
}

bool cui_node_is_visible(CUI_Node *node)
{
    return node ? node->visible : false;
}

bool cui_node_is_visible_in_tree(CUI_Node *node)
{
    while (node) {
        if (!node->visible) return false;
        node = node->parent;
    }
    return true;
}

void cui_node_set_enabled(CUI_Node *node, bool enabled)
{
    if (node) node->enabled = enabled;
}

bool cui_node_is_enabled(CUI_Node *node)
{
    return node ? node->enabled : false;
}

void cui_node_grab_focus(CUI_Node *node)
{
    if (!node) return;

    if (s_focused_node && s_focused_node != node) {
        CUI_Node *old = s_focused_node;
        old->focused = false;
        cui_node_emit_simple(old, CUI_SIGNAL_UNFOCUSED);
    }

    s_focused_node = node;
    node->focused = true;
    cui_node_emit_simple(node, CUI_SIGNAL_FOCUSED);
}

void cui_node_release_focus(CUI_Node *node)
{
    if (!node || s_focused_node != node) return;

    node->focused = false;
    s_focused_node = NULL;
    cui_node_emit_simple(node, CUI_SIGNAL_UNFOCUSED);
}

bool cui_node_has_focus(CUI_Node *node)
{
    return node && s_focused_node == node;
}

CUI_Node *cui_get_focused_node(CUI_Context *ctx)
{
    (void)ctx;
    return s_focused_node;
}

void cui_node_set_opacity(CUI_Node *node, float opacity)
{
    if (node) {
        node->opacity = fmaxf(0.0f, fminf(1.0f, opacity));
    }
}

float cui_node_get_opacity(CUI_Node *node)
{
    return node ? node->opacity : 1.0f;
}

/* ============================================================================
 * Signals
 * ============================================================================ */

static uint32_t s_next_connection_id = 1;

uint32_t cui_node_connect(CUI_Node *node, CUI_SignalType signal,
                           CUI_SignalCallback callback, void *userdata)
{
    if (!node || !callback) return 0;
    if (node->connection_count >= CUI_MAX_CONNECTIONS) return 0;

    CUI_Connection *conn = &node->connections[node->connection_count++];
    conn->id = s_next_connection_id++;
    conn->signal_type = signal;
    conn->callback = callback;
    conn->userdata = userdata;
    conn->active = true;
    conn->oneshot = false;

    return conn->id;
}

uint32_t cui_node_connect_oneshot(CUI_Node *node, CUI_SignalType signal,
                                   CUI_SignalCallback callback, void *userdata)
{
    uint32_t id = cui_node_connect(node, signal, callback, userdata);
    if (id && node->connection_count > 0) {
        node->connections[node->connection_count - 1].oneshot = true;
    }
    return id;
}

void cui_node_disconnect(CUI_Node *node, uint32_t connection_id)
{
    if (!node || connection_id == 0) return;

    for (int i = 0; i < node->connection_count; i++) {
        if (node->connections[i].id == connection_id) {
            node->connections[i].active = false;
            return;
        }
    }
}

void cui_node_disconnect_all(CUI_Node *node, CUI_SignalType signal)
{
    if (!node) return;

    for (int i = 0; i < node->connection_count; i++) {
        if (node->connections[i].signal_type == signal) {
            node->connections[i].active = false;
        }
    }
}

void cui_node_emit(CUI_Node *node, CUI_SignalType signal, const CUI_Signal *data)
{
    if (!node) return;

    for (int i = 0; i < node->connection_count; i++) {
        CUI_Connection *conn = &node->connections[i];
        if (conn->active && conn->signal_type == signal) {
            conn->callback(node, data, conn->userdata);
            if (conn->oneshot) {
                conn->active = false;
            }
        }
    }
}

void cui_node_emit_simple(CUI_Node *node, CUI_SignalType signal)
{
    CUI_Signal sig;
    memset(&sig, 0, sizeof(sig));
    sig.type = signal;
    sig.source = node;
    cui_node_emit(node, signal, &sig);
}

/* ============================================================================
 * Layout Calculation
 * ============================================================================ */

static void cui_node_calculate_rect(CUI_Node *node, CUI_Rect parent_rect)
{
    if (!node) return;

    /* Calculate position from anchors */
    float parent_w = parent_rect.w;
    float parent_h = parent_rect.h;

    float anchor_left = parent_rect.x + node->anchors.left * parent_w;
    float anchor_top = parent_rect.y + node->anchors.top * parent_h;
    float anchor_right = parent_rect.x + node->anchors.right * parent_w;
    float anchor_bottom = parent_rect.y + node->anchors.bottom * parent_h;

    /* Apply offsets */
    float left = anchor_left + node->offsets.left;
    float top = anchor_top + node->offsets.top;
    float right = anchor_right + node->offsets.right;
    float bottom = anchor_bottom + node->offsets.bottom;

    /* Calculate rect */
    node->rect.x = left - parent_rect.x;
    node->rect.y = top - parent_rect.y;
    node->rect.w = right - left;
    node->rect.h = bottom - top;

    /* Apply minimum size */
    float min_w, min_h;
    cui_node_get_min_size(node, &min_w, &min_h);
    if (node->rect.w < min_w) node->rect.w = min_w;
    if (node->rect.h < min_h) node->rect.h = min_h;

    /* Calculate global rect */
    node->global_rect.x = parent_rect.x + node->rect.x;
    node->global_rect.y = parent_rect.y + node->rect.y;
    node->global_rect.w = node->rect.w;
    node->global_rect.h = node->rect.h;
}

/* Get minimum size for a node, using text metrics for labels/buttons */
static void cui_node_get_content_min_size(CUI_Context *ctx, CUI_Node *node,
                                           float *out_w, float *out_h)
{
    float min_w = node->custom_min_size_x;
    float min_h = node->custom_min_size_y;

    /* Calculate content-based minimum size for certain node types */
    if (ctx) {
        switch (node->type) {
            case CUI_NODE_LABEL:
                if (node->label.text[0]) {
                    float tw = cui_text_width(ctx, node->label.text);
                    float th = cui_text_height(ctx);
                    if (tw > min_w) min_w = tw;
                    if (th > min_h) min_h = th;
                }
                break;
            case CUI_NODE_BUTTON:
                if (node->button.text[0]) {
                    float tw = cui_text_width(ctx, node->button.text);
                    float th = cui_text_height(ctx);
                    /* Add some button padding */
                    if (tw + 20 > min_w) min_w = tw + 20;
                    if (th + 10 > min_h) min_h = th + 10;
                }
                break;
            default:
                break;
        }
    }

    /* Add style padding */
    min_w += node->style.padding.left + node->style.padding.right;
    min_h += node->style.padding.top + node->style.padding.bottom;

    /* Compare with node->min_size */
    if (out_w) *out_w = fmaxf(min_w, node->min_size_x);
    if (out_h) *out_h = fmaxf(min_h, node->min_size_y);
}

static void cui_node_layout_vbox_ctx(CUI_Context *ctx, CUI_Node *node)
{
    if (!node || node->child_count == 0) return;

    float sep = node->box.separation;
    float padding_top = node->style.padding.top;
    float padding_left = node->style.padding.left;
    float padding_right = node->style.padding.right;
    float padding_bottom = node->style.padding.bottom;

    float available_w = node->rect.w - padding_left - padding_right;
    float available_h = node->rect.h - padding_top - padding_bottom;

    /* First pass: calculate total minimum height and expanding children */
    float total_min_h = 0;
    int expand_count = 0;
    float total_stretch = 0;

    for (CUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (!child->visible) continue;

        float child_min_w, child_min_h;
        cui_node_get_content_min_size(ctx, child, &child_min_w, &child_min_h);
        total_min_h += child_min_h;

        if (child->v_size_flags & CUI_SIZE_EXPAND) {
            expand_count++;
            total_stretch += child->size_flags_stretch_ratio;
        }
    }

    /* Add separations */
    int visible_count = 0;
    for (CUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (child->visible) visible_count++;
    }
    if (visible_count > 1) {
        total_min_h += sep * (visible_count - 1);
    }

    /* Calculate extra space for expanding children */
    float extra_space = available_h - total_min_h;
    if (extra_space < 0) extra_space = 0;

    /* Second pass: position children */
    float y = padding_top;
    for (CUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (!child->visible) continue;

        float child_min_w, child_min_h;
        cui_node_get_content_min_size(ctx, child, &child_min_w, &child_min_h);

        /* Calculate child width */
        float child_w = child_min_w;
        if (child->h_size_flags & CUI_SIZE_FILL) {
            child_w = available_w;
        }

        /* Calculate child height */
        float child_h = child_min_h;
        if ((child->v_size_flags & CUI_SIZE_EXPAND) && total_stretch > 0) {
            float ratio = child->size_flags_stretch_ratio / total_stretch;
            child_h += extra_space * ratio;
        }

        /* Calculate x position based on alignment */
        float child_x = padding_left;
        if (child->h_size_flags & CUI_SIZE_SHRINK_CENTER) {
            child_x = padding_left + (available_w - child_w) / 2;
        } else if (child->h_size_flags & CUI_SIZE_SHRINK_END) {
            child_x = padding_left + available_w - child_w;
        }

        /* Set child rect */
        child->rect.x = child_x;
        child->rect.y = y;
        child->rect.w = child_w;
        child->rect.h = child_h;

        child->global_rect.x = node->global_rect.x + child_x;
        child->global_rect.y = node->global_rect.y + y;
        child->global_rect.w = child_w;
        child->global_rect.h = child_h;

        y += child_h + sep;
    }
}


static void cui_node_layout_hbox(CUI_Node *node)
{
    if (!node || node->child_count == 0) return;

    float sep = node->box.separation;
    float padding_top = node->style.padding.top;
    float padding_left = node->style.padding.left;
    float padding_right = node->style.padding.right;
    float padding_bottom = node->style.padding.bottom;

    float available_w = node->rect.w - padding_left - padding_right;
    float available_h = node->rect.h - padding_top - padding_bottom;

    /* First pass: calculate total minimum width and expanding children */
    float total_min_w = 0;
    float total_stretch = 0;

    for (CUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (!child->visible) continue;

        float child_min_w, child_min_h;
        cui_node_get_min_size(child, &child_min_w, &child_min_h);
        total_min_w += child_min_w;

        if (child->h_size_flags & CUI_SIZE_EXPAND) {
            total_stretch += child->size_flags_stretch_ratio;
        }
    }

    /* Add separations */
    int visible_count = 0;
    for (CUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (child->visible) visible_count++;
    }
    if (visible_count > 1) {
        total_min_w += sep * (visible_count - 1);
    }

    /* Calculate extra space for expanding children */
    float extra_space = available_w - total_min_w;
    if (extra_space < 0) extra_space = 0;

    /* Second pass: position children */
    float x = padding_left;
    for (CUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (!child->visible) continue;

        float child_min_w, child_min_h;
        cui_node_get_min_size(child, &child_min_w, &child_min_h);

        /* Calculate child width */
        float child_w = child_min_w;
        if ((child->h_size_flags & CUI_SIZE_EXPAND) && total_stretch > 0) {
            float ratio = child->size_flags_stretch_ratio / total_stretch;
            child_w += extra_space * ratio;
        }

        /* Calculate child height */
        float child_h = child_min_h;
        if (child->v_size_flags & CUI_SIZE_FILL) {
            child_h = available_h;
        }

        /* Calculate y position based on alignment */
        float child_y = padding_top;
        if (child->v_size_flags & CUI_SIZE_SHRINK_CENTER) {
            child_y = padding_top + (available_h - child_h) / 2;
        } else if (child->v_size_flags & CUI_SIZE_SHRINK_END) {
            child_y = padding_top + available_h - child_h;
        }

        /* Set child rect */
        child->rect.x = x;
        child->rect.y = child_y;
        child->rect.w = child_w;
        child->rect.h = child_h;

        child->global_rect.x = node->global_rect.x + x;
        child->global_rect.y = node->global_rect.y + child_y;
        child->global_rect.w = child_w;
        child->global_rect.h = child_h;

        x += child_w + sep;
    }
}

/* Returns true if this container type manages child layout directly */
static bool cui_node_is_layout_container(CUI_Node *node)
{
    if (!node) return false;
    switch (node->type) {
        case CUI_NODE_VBOX:
        case CUI_NODE_HBOX:
        case CUI_NODE_GRID:
            return true;
        default:
            return false;
    }
}

static void cui_node_layout_children(CUI_Context *ctx, CUI_Node *node)
{
    if (!node) return;

    /* Container-specific layout - these directly set child positions */
    switch (node->type) {
        case CUI_NODE_VBOX:
            cui_node_layout_vbox_ctx(ctx, node);
            return;

        case CUI_NODE_HBOX:
            cui_node_layout_hbox(node);  /* TODO: add ctx version */
            return;

        case CUI_NODE_GRID:
            /* TODO: Implement grid layout */
            break;

        default:
            break;
    }

    /* Default: calculate each child's rect from anchors */
    for (CUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (!child->visible) continue;
        cui_node_calculate_rect(child, node->global_rect);
    }
}

static void cui_node_layout_recursive_internal(CUI_Context *ctx, CUI_Node *node,
                                                 CUI_Rect parent_rect,
                                                 bool parent_is_layout_container)
{
    if (!node || !node->visible) return;

    /* Only calculate rect from anchors if parent didn't already position us.
     * Layout containers (VBox, HBox, Grid) set child positions directly,
     * so we skip the anchor-based calculation for their children. */
    if (!parent_is_layout_container) {
        cui_node_calculate_rect(node, parent_rect);
    }

    /* Layout children (this may directly set child positions for VBox/HBox/Grid) */
    cui_node_layout_children(ctx, node);

    /* Call custom layout */
    if (node->on_layout) {
        node->on_layout(node);
    }

    /* Check if this node is a layout container for children */
    bool this_is_layout_container = cui_node_is_layout_container(node);

    /* Recurse to children */
    for (CUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (child->visible) {
            cui_node_layout_recursive_internal(ctx, child, node->global_rect, this_is_layout_container);
        }
    }

    node->layout_dirty = false;
}

static void cui_node_layout_recursive(CUI_Context *ctx, CUI_Node *node, CUI_Rect parent_rect)
{
    /* Root node is never inside a layout container */
    cui_node_layout_recursive_internal(ctx, node, parent_rect, false);
}

/* ============================================================================
 * Scene Tree Processing
 * ============================================================================ */

void cui_scene_update(CUI_Context *ctx, CUI_Node *root, float delta_time)
{
    if (!ctx || !root) return;

    /* Process nodes recursively */
    if (root->on_process) {
        root->on_process(root, delta_time);
    }

    for (CUI_Node *child = root->first_child; child; child = child->next_sibling) {
        cui_scene_update(ctx, child, delta_time);
    }
}

bool cui_scene_process_event(CUI_Context *ctx, CUI_Node *root, const SDL_Event *event)
{
    if (!ctx || !root || !event) return false;

    /* Find node at mouse position for mouse events */
    if (event->type == SDL_EVENT_MOUSE_MOTION ||
        event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event->type == SDL_EVENT_MOUSE_BUTTON_UP) {

        float mx = event->motion.x;
        float my = event->motion.y;

        CUI_Node *hit = cui_node_hit_test(root, mx, my);

        /* Handle hover state changes */
        static CUI_Node *s_last_hovered = NULL;
        if (hit != s_last_hovered) {
            if (s_last_hovered) {
                s_last_hovered->hovered = false;
                cui_node_emit_simple(s_last_hovered, CUI_SIGNAL_MOUSE_EXITED);
            }
            if (hit) {
                hit->hovered = true;
                cui_node_emit_simple(hit, CUI_SIGNAL_MOUSE_ENTERED);
            }
            s_last_hovered = hit;
        }

        /* Track which node is currently pressed (must be outside block scope) */
        static CUI_Node *s_pressed_node = NULL;

        /* Handle click */
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && hit) {
            if (hit->focus_mode_click) {
                cui_node_grab_focus(hit);
            }

            hit->pressed = true;
            s_pressed_node = hit;  /* Store the pressed node */

            CUI_Signal sig;
            memset(&sig, 0, sizeof(sig));
            sig.type = CUI_SIGNAL_PRESSED;
            sig.source = hit;
            sig.mouse.x = mx;
            sig.mouse.y = my;
            sig.mouse.button = event->button.button;
            cui_node_emit(hit, CUI_SIGNAL_PRESSED, &sig);

            if (hit->on_gui_input) {
                return hit->on_gui_input(hit, ctx, event);
            }
            return !hit->mouse_filter_ignore;
        }

        if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
            /* Release pressed node and emit released/clicked */
            if (s_pressed_node) {
                s_pressed_node->pressed = false;
                cui_node_emit_simple(s_pressed_node, CUI_SIGNAL_RELEASED);

                if (hit == s_pressed_node) {
                    cui_node_emit_simple(hit, CUI_SIGNAL_CLICKED);
                }
                s_pressed_node = NULL;
            }
        }

        return hit != NULL && !hit->mouse_filter_ignore;
    }

    /* Keyboard events go to focused node */
    if (s_focused_node) {
        if (s_focused_node->on_gui_input) {
            return s_focused_node->on_gui_input(s_focused_node, ctx, event);
        }
    }

    return false;
}

static void cui_node_render_recursive(CUI_Context *ctx, CUI_Node *node);

void cui_scene_render(CUI_Context *ctx, CUI_Node *root)
{
    if (!ctx || !root) return;

    /* Layout pass */
    cui_scene_layout(ctx, root);

    /* Render the scene tree */
    cui_node_render_recursive(ctx, root);
}

void cui_scene_layout(CUI_Context *ctx, CUI_Node *root)
{
    if (!ctx || !root) return;

    /* Create root rect from screen size */
    CUI_Rect screen_rect = {0, 0, (float)ctx->width, (float)ctx->height};
    cui_node_layout_recursive(ctx, root, screen_rect);
}

static void cui_node_render_recursive(CUI_Context *ctx, CUI_Node *node)
{
    if (!node || !node->visible) return;

    /* Get effective style */
    CUI_Style style = cui_node_get_effective_style(node);

    /* Apply node opacity */
    style.opacity *= node->opacity;

    /* Draw styled background */
    if (style.background.type != CUI_BG_NONE) {
        cui_draw_styled_rect(ctx, node->global_rect.x, node->global_rect.y,
                              node->global_rect.w, node->global_rect.h, &style);
    }

    /* Type-specific rendering */
    switch (node->type) {
        case CUI_NODE_LABEL:
            {
                float text_w = cui_text_width(ctx, node->label.text);
                float text_h = cui_text_height(ctx);
                float avail_w = node->global_rect.w - style.padding.left - style.padding.right;
                float avail_h = node->global_rect.h - style.padding.top - style.padding.bottom;

                /* Horizontal alignment based on size flags */
                float text_x = node->global_rect.x + style.padding.left;
                if (node->h_size_flags & CUI_SIZE_SHRINK_CENTER) {
                    text_x = node->global_rect.x + style.padding.left +
                             (avail_w - text_w) / 2;
                } else if (node->h_size_flags & CUI_SIZE_SHRINK_END) {
                    text_x = node->global_rect.x + node->global_rect.w -
                             style.padding.right - text_w;
                }

                /* Center vertically */
                float text_y = node->global_rect.y + style.padding.top +
                               (avail_h - text_h) / 2;

                cui_draw_text(ctx, node->label.text, text_x, text_y,
                              node->label.color ? node->label.color : style.text_color);
            }
            break;

        case CUI_NODE_BUTTON:
            /* Draw button with state-appropriate background */
            {
                CUI_Background *bg = &style.background;
                if (!node->enabled) {
                    bg = &style.background_disabled;
                } else if (node->pressed) {
                    bg = &style.background_active;
                } else if (node->hovered) {
                    bg = &style.background_hover;
                }

                if (bg->type == CUI_BG_SOLID) {
                    cui_draw_rect_rounded(ctx, node->global_rect.x, node->global_rect.y,
                                          node->global_rect.w, node->global_rect.h,
                                          bg->solid_color, style.corner_radius.top_left);
                }

                /* Draw text centered */
                float text_w = cui_text_width(ctx, node->button.text);
                float text_x = node->global_rect.x +
                               (node->global_rect.w - text_w) / 2;
                float text_y = node->global_rect.y +
                               (node->global_rect.h - cui_text_height(ctx)) / 2;
                cui_draw_text(ctx, node->button.text, text_x, text_y,
                              node->enabled ? style.text_color : style.text_color_disabled);
            }
            break;

        case CUI_NODE_PROGRESS_BAR:
            {
                /* Background */
                cui_draw_rect(ctx, node->global_rect.x, node->global_rect.y,
                              node->global_rect.w, node->global_rect.h,
                              style.background.solid_color);

                /* Fill */
                float range = node->progress.max_value - node->progress.min_value;
                float fill_ratio = (range > 0) ?
                    (node->progress.value - node->progress.min_value) / range : 0;
                fill_ratio = fmaxf(0, fminf(1, fill_ratio));

                cui_draw_rect(ctx, node->global_rect.x, node->global_rect.y,
                              node->global_rect.w * fill_ratio, node->global_rect.h,
                              node->progress.fill_color ? node->progress.fill_color :
                              ctx->theme.accent);
            }
            break;

        default:
            break;
    }

    /* Custom draw */
    if (node->on_draw) {
        node->on_draw(node, ctx);
    }

    /* Render children */
    if (node->clip_contents) {
        cui_push_scissor(ctx, node->global_rect.x, node->global_rect.y,
                          node->global_rect.w, node->global_rect.h);
    }

    for (CUI_Node *child = node->first_child; child; child = child->next_sibling) {
        cui_node_render_recursive(ctx, child);
    }

    if (node->clip_contents) {
        cui_pop_scissor(ctx);
    }
}

/* ============================================================================
 * Hit Testing
 * ============================================================================ */

CUI_Node *cui_node_hit_test(CUI_Node *root, float x, float y)
{
    if (!root || !root->visible) return NULL;

    /* Check children in reverse order (front to back) */
    for (CUI_Node *child = root->last_child; child; child = child->prev_sibling) {
        CUI_Node *hit = cui_node_hit_test(child, x, y);
        if (hit) return hit;
    }

    /* Check this node */
    if (!root->mouse_filter_ignore && cui_node_contains_point(root, x, y)) {
        return root;
    }

    return NULL;
}

bool cui_node_contains_point(CUI_Node *node, float x, float y)
{
    if (!node) return false;
    return x >= node->global_rect.x &&
           x < node->global_rect.x + node->global_rect.w &&
           y >= node->global_rect.y &&
           y < node->global_rect.y + node->global_rect.h;
}

/* ============================================================================
 * Convenience Creators
 * ============================================================================ */

CUI_Node *cui_label_create(CUI_Context *ctx, const char *name, const char *text)
{
    CUI_Node *node = cui_node_create(ctx, CUI_NODE_LABEL, name);
    if (node && text) {
        strncpy(node->label.text, text, sizeof(node->label.text) - 1);
    }
    return node;
}

CUI_Node *cui_button_create(CUI_Context *ctx, const char *name, const char *text)
{
    CUI_Node *node = cui_node_create(ctx, CUI_NODE_BUTTON, name);
    if (node) {
        if (text) {
            strncpy(node->button.text, text, sizeof(node->button.text) - 1);
        }
        /* Set default min size for button */
        node->custom_min_size_x = 80;
        node->custom_min_size_y = 28;
    }
    return node;
}

CUI_Node *cui_vbox_create(CUI_Context *ctx, const char *name)
{
    return cui_node_create(ctx, CUI_NODE_VBOX, name);
}

CUI_Node *cui_hbox_create(CUI_Context *ctx, const char *name)
{
    return cui_node_create(ctx, CUI_NODE_HBOX, name);
}

CUI_Node *cui_grid_create(CUI_Context *ctx, const char *name, int columns)
{
    CUI_Node *node = cui_node_create(ctx, CUI_NODE_GRID, name);
    if (node) {
        node->grid.columns = columns > 0 ? columns : 2;
    }
    return node;
}

CUI_Node *cui_margin_create(CUI_Context *ctx, const char *name)
{
    return cui_node_create(ctx, CUI_NODE_MARGIN, name);
}

CUI_Node *cui_scroll_create(CUI_Context *ctx, const char *name)
{
    return cui_node_create(ctx, CUI_NODE_SCROLL, name);
}

CUI_Node *cui_panel_create(CUI_Context *ctx, const char *name, const char *title)
{
    CUI_Node *node = cui_node_create(ctx, CUI_NODE_PANEL, name);
    if (node && title) {
        strncpy(node->panel.title, title, sizeof(node->panel.title) - 1);
    }
    return node;
}

/* ============================================================================
 * Container-Specific Functions
 * ============================================================================ */

void cui_box_set_separation(CUI_Node *node, float separation)
{
    if (!node) return;
    if (node->type == CUI_NODE_VBOX || node->type == CUI_NODE_HBOX) {
        node->box.separation = separation;
        node->layout_dirty = true;
    }
}

void cui_box_set_alignment(CUI_Node *node, CUI_SizeFlags alignment)
{
    if (!node) return;
    if (node->type == CUI_NODE_VBOX || node->type == CUI_NODE_HBOX) {
        node->box.alignment = alignment;
        node->layout_dirty = true;
    }
}

void cui_grid_set_columns(CUI_Node *node, int columns)
{
    if (!node || node->type != CUI_NODE_GRID) return;
    node->grid.columns = columns > 0 ? columns : 1;
    node->layout_dirty = true;
}

void cui_grid_set_h_separation(CUI_Node *node, float separation)
{
    if (!node || node->type != CUI_NODE_GRID) return;
    node->grid.h_separation = separation;
    node->layout_dirty = true;
}

void cui_grid_set_v_separation(CUI_Node *node, float separation)
{
    if (!node || node->type != CUI_NODE_GRID) return;
    node->grid.v_separation = separation;
    node->layout_dirty = true;
}

void cui_margin_set_margins(CUI_Node *node, float left, float top,
                             float right, float bottom)
{
    if (!node) return;
    node->style.padding = cui_edges(top, right, bottom, left);
    node->layout_dirty = true;
}

void cui_scroll_set_h_scroll_enabled(CUI_Node *node, bool enabled)
{
    if (!node || node->type != CUI_NODE_SCROLL) return;
    node->scroll.h_scroll_enabled = enabled;
}

void cui_scroll_set_v_scroll_enabled(CUI_Node *node, bool enabled)
{
    if (!node || node->type != CUI_NODE_SCROLL) return;
    node->scroll.v_scroll_enabled = enabled;
}

void cui_scroll_set_scroll(CUI_Node *node, float x, float y)
{
    if (!node || node->type != CUI_NODE_SCROLL) return;
    node->scroll.scroll_x = x;
    node->scroll.scroll_y = y;
}

void cui_scroll_ensure_visible(CUI_Node *node, CUI_Rect rect)
{
    if (!node || node->type != CUI_NODE_SCROLL) return;

    /* Adjust scroll to make rect visible */
    if (rect.y < node->scroll.scroll_y) {
        node->scroll.scroll_y = rect.y;
    }
    if (rect.y + rect.h > node->scroll.scroll_y + node->rect.h) {
        node->scroll.scroll_y = rect.y + rect.h - node->rect.h;
    }
    if (rect.x < node->scroll.scroll_x) {
        node->scroll.scroll_x = rect.x;
    }
    if (rect.x + rect.w > node->scroll.scroll_x + node->rect.w) {
        node->scroll.scroll_x = rect.x + rect.w - node->rect.w;
    }
}

/* ============================================================================
 * Widget-Specific Functions
 * ============================================================================ */

void cui_label_set_text(CUI_Node *node, const char *text)
{
    if (!node || node->type != CUI_NODE_LABEL) return;
    if (text) {
        strncpy(node->label.text, text, sizeof(node->label.text) - 1);
    } else {
        node->label.text[0] = '\0';
    }
}

const char *cui_label_get_text(CUI_Node *node)
{
    if (!node || node->type != CUI_NODE_LABEL) return "";
    return node->label.text;
}

void cui_button_set_text(CUI_Node *node, const char *text)
{
    if (!node || node->type != CUI_NODE_BUTTON) return;
    if (text) {
        strncpy(node->button.text, text, sizeof(node->button.text) - 1);
    } else {
        node->button.text[0] = '\0';
    }
}

void cui_button_set_disabled(CUI_Node *node, bool disabled)
{
    if (!node || node->type != CUI_NODE_BUTTON) return;
    node->button.disabled = disabled;
    node->enabled = !disabled;
}

void cui_button_set_toggle_mode(CUI_Node *node, bool toggle)
{
    if (!node || node->type != CUI_NODE_BUTTON) return;
    node->button.toggle_mode = toggle;
}

bool cui_button_is_toggled(CUI_Node *node)
{
    if (!node || node->type != CUI_NODE_BUTTON) return false;
    return node->button.toggled;
}

void cui_checkbox_set_checked(CUI_Node *node, bool checked)
{
    if (!node || node->type != CUI_NODE_CHECKBOX) return;
    node->checkbox.checked = checked;
}

bool cui_checkbox_is_checked(CUI_Node *node)
{
    if (!node || node->type != CUI_NODE_CHECKBOX) return false;
    return node->checkbox.checked;
}

void cui_slider_set_value(CUI_Node *node, float value)
{
    if (!node || node->type != CUI_NODE_SLIDER) return;
    node->slider.value = fmaxf(node->slider.min_value,
                                fminf(node->slider.max_value, value));
}

float cui_slider_get_value(CUI_Node *node)
{
    if (!node || node->type != CUI_NODE_SLIDER) return 0;
    return node->slider.value;
}

void cui_slider_set_range(CUI_Node *node, float min, float max)
{
    if (!node || node->type != CUI_NODE_SLIDER) return;
    node->slider.min_value = min;
    node->slider.max_value = max;
    node->slider.value = fmaxf(min, fminf(max, node->slider.value));
}

void cui_slider_set_step(CUI_Node *node, float step)
{
    if (!node || node->type != CUI_NODE_SLIDER) return;
    node->slider.step = step;
}

void cui_textbox_set_text(CUI_Node *node, const char *text)
{
    if (!node || node->type != CUI_NODE_TEXTBOX || !node->textbox.buffer) return;
    if (text) {
        strncpy(node->textbox.buffer, text, node->textbox.buffer_size - 1);
        node->textbox.buffer[node->textbox.buffer_size - 1] = '\0';
    } else {
        node->textbox.buffer[0] = '\0';
    }
    node->textbox.cursor_pos = (int)strlen(node->textbox.buffer);
}

const char *cui_textbox_get_text(CUI_Node *node)
{
    if (!node || node->type != CUI_NODE_TEXTBOX || !node->textbox.buffer) return "";
    return node->textbox.buffer;
}

void cui_textbox_set_placeholder(CUI_Node *node, const char *placeholder)
{
    if (!node || node->type != CUI_NODE_TEXTBOX) return;
    if (placeholder) {
        strncpy(node->textbox.placeholder, placeholder,
                sizeof(node->textbox.placeholder) - 1);
    } else {
        node->textbox.placeholder[0] = '\0';
    }
}

void cui_dropdown_set_items(CUI_Node *node, const char **items, int count)
{
    if (!node || node->type != CUI_NODE_DROPDOWN) return;
    node->dropdown.items = items;
    node->dropdown.item_count = count;
    if (node->dropdown.selected >= count) {
        node->dropdown.selected = count > 0 ? 0 : -1;
    }
}

void cui_dropdown_set_selected(CUI_Node *node, int index)
{
    if (!node || node->type != CUI_NODE_DROPDOWN) return;
    if (index >= 0 && index < node->dropdown.item_count) {
        node->dropdown.selected = index;
    }
}

int cui_dropdown_get_selected(CUI_Node *node)
{
    if (!node || node->type != CUI_NODE_DROPDOWN) return -1;
    return node->dropdown.selected;
}

void cui_progress_set_value(CUI_Node *node, float value)
{
    if (!node || node->type != CUI_NODE_PROGRESS_BAR) return;
    node->progress.value = fmaxf(node->progress.min_value,
                                  fminf(node->progress.max_value, value));
}

void cui_progress_set_range(CUI_Node *node, float min, float max)
{
    if (!node || node->type != CUI_NODE_PROGRESS_BAR) return;
    node->progress.min_value = min;
    node->progress.max_value = max;
    node->progress.value = fmaxf(min, fminf(max, node->progress.value));
}
