/*
 * Agentite UI - Retained Mode Node System Implementation
 */

#include "agentite/ui_node.h"
#include "agentite/ui.h"
#include "agentite/ui_charts.h"
#include "agentite/ui_richtext.h"
#include "agentite/ui_style.h"
#include "agentite/ui_tween.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cstdio>
#include <functional>

/* ============================================================================
 * Internal State
 * ============================================================================ */

static uint32_t s_next_node_id = 1;
static AUI_Node *s_focused_node = NULL;

/* Drag threshold in pixels before drag starts */
static const float TREE_DRAG_THRESHOLD = 5.0f;

/* Helper: Find tree item at Y position (returns item and its depth via out param) */
static AUI_TreeItem *tree_find_item_at_y(AUI_TreeItem *items, float item_h, float indent,
                                          float target_y, float *current_y_out, int *depth_out)
{
    float current_y = current_y_out ? *current_y_out : 0;
    int depth = depth_out ? *depth_out : 0;

    AUI_TreeItem *item = items;
    while (item) {
        if (target_y >= current_y && target_y < current_y + item_h) {
            if (current_y_out) *current_y_out = current_y;
            if (depth_out) *depth_out = depth;
            return item;
        }
        current_y += item_h;

        if (item->expanded && item->first_child) {
            float child_y = current_y;
            int child_depth = depth + 1;
            AUI_TreeItem *found = tree_find_item_at_y(item->first_child, item_h, indent,
                                                       target_y, &child_y, &child_depth);
            if (found) {
                if (current_y_out) *current_y_out = child_y;
                if (depth_out) *depth_out = child_depth;
                return found;
            }
            current_y = child_y;
        }
        item = item->next_sibling;
    }

    if (current_y_out) *current_y_out = current_y;
    return NULL;
}

/* Helper: Check if item is a descendant of potential_ancestor */
static bool tree_is_descendant(AUI_TreeItem *item, AUI_TreeItem *potential_ancestor)
{
    AUI_TreeItem *parent = item->parent;
    while (parent) {
        if (parent == potential_ancestor) return true;
        parent = parent->parent;
    }
    return false;
}

/* Helper: Unlink item from its current location in tree */
static void tree_unlink_item(AUI_Node *tree, AUI_TreeItem *item)
{
    /* Remove from sibling list */
    if (item->prev_sibling) {
        item->prev_sibling->next_sibling = item->next_sibling;
    }
    if (item->next_sibling) {
        item->next_sibling->prev_sibling = item->prev_sibling;
    }

    /* Update parent's child pointers */
    if (item->parent) {
        if (item->parent->first_child == item) {
            item->parent->first_child = item->next_sibling;
        }
        if (item->parent->last_child == item) {
            item->parent->last_child = item->prev_sibling;
        }
    } else {
        /* Root item */
        if (tree->tree.root_items == item) {
            tree->tree.root_items = item->next_sibling;
        }
    }

    item->parent = NULL;
    item->prev_sibling = NULL;
    item->next_sibling = NULL;
}

/* Helper: Insert item as sibling before target */
static void tree_insert_before(AUI_Node *tree, AUI_TreeItem *item, AUI_TreeItem *target)
{
    item->parent = target->parent;
    item->next_sibling = target;
    item->prev_sibling = target->prev_sibling;

    if (target->prev_sibling) {
        target->prev_sibling->next_sibling = item;
    } else if (target->parent) {
        target->parent->first_child = item;
    } else {
        tree->tree.root_items = item;
    }
    target->prev_sibling = item;
}

/* Helper: Insert item as sibling after target */
static void tree_insert_after(AUI_Node * /*tree*/, AUI_TreeItem *item, AUI_TreeItem *target)
{
    item->parent = target->parent;
    item->prev_sibling = target;
    item->next_sibling = target->next_sibling;

    if (target->next_sibling) {
        target->next_sibling->prev_sibling = item;
    } else if (target->parent) {
        target->parent->last_child = item;
    }
    target->next_sibling = item;
}

/* Helper: Insert item as child of target (at end) */
static void tree_insert_as_child(AUI_TreeItem *item, AUI_TreeItem *target)
{
    item->parent = target;
    item->prev_sibling = target->last_child;
    item->next_sibling = NULL;

    if (target->last_child) {
        target->last_child->next_sibling = item;
    } else {
        target->first_child = item;
    }
    target->last_child = item;
}

/* Helper to apply opacity to a color (format: 0xAABBGGRR - alpha in high byte) */
static inline uint32_t aui_apply_opacity(uint32_t color, float opacity) {
    if (opacity >= 1.0f) return color;
    if (opacity <= 0.0f) return color & 0x00FFFFFF;
    uint8_t a = (uint8_t)((color >> 24) & 0xFF);
    a = (uint8_t)(a * opacity);
    return (color & 0x00FFFFFF) | ((uint32_t)a << 24);
}

/* Get target background color based on node state */
static uint32_t aui_node_get_target_bg_color(AUI_Node *node, const AUI_Style *style) {
    if (!node->enabled && style->background_disabled.type == AUI_BG_SOLID) {
        return style->background_disabled.solid_color;
    }
    if (node->pressed && style->background_active.type == AUI_BG_SOLID) {
        return style->background_active.solid_color;
    }
    if (node->hovered && style->background_hover.type == AUI_BG_SOLID) {
        return style->background_hover.solid_color;
    }
    if (style->background.type == AUI_BG_SOLID) {
        return style->background.solid_color;
    }
    return 0;  /* Transparent/no background */
}

/* Get target text color based on node state */
static uint32_t aui_node_get_target_text_color(AUI_Node *node, const AUI_Style *style) {
    if (!node->enabled) return style->text_color_disabled;
    if (node->pressed && style->text_color_active != 0) return style->text_color_active;
    if (node->hovered && style->text_color_hover != 0) return style->text_color_hover;
    return style->text_color;
}

/* Update style transitions for a node */
static void aui_node_update_transitions(AUI_Node *node, float delta_time) {
    if (!node) return;

    AUI_Style style = aui_node_get_effective_style(node);
    float duration = style.transition.duration;

    /* Check if state changed */
    bool state_changed = (node->hovered != node->transition_state.prev_hovered) ||
                         (node->pressed != node->transition_state.prev_pressed);

    /* Get target colors for current state */
    uint32_t target_bg = aui_node_get_target_bg_color(node, &style);
    uint32_t target_text = aui_node_get_target_text_color(node, &style);
    uint32_t target_border = style.border.color;

    if (state_changed && duration > 0) {
        /* Start a new transition */
        /* Use current interpolated color as start (handles mid-transition changes) */
        node->transition_state.from_bg_color = node->transition_state.current_bg_color;
        node->transition_state.from_text_color = node->transition_state.current_text_color;
        node->transition_state.from_border_color = node->transition_state.current_border_color;

        node->transition_state.to_bg_color = target_bg;
        node->transition_state.to_text_color = target_text;
        node->transition_state.to_border_color = target_border;
        node->transition_state.progress = 0.0f;
        node->transition_state.active = true;
    }

    /* Update transition progress */
    if (node->transition_state.active && duration > 0) {
        node->transition_state.progress += delta_time / duration;

        if (node->transition_state.progress >= 1.0f) {
            node->transition_state.progress = 1.0f;
            node->transition_state.active = false;
            node->transition_state.current_bg_color = node->transition_state.to_bg_color;
            node->transition_state.current_text_color = node->transition_state.to_text_color;
            node->transition_state.current_border_color = node->transition_state.to_border_color;
        } else {
            /* Apply easing */
            float t = aui_ease((AUI_EaseType)style.transition.ease, node->transition_state.progress);

            /* Interpolate colors */
            node->transition_state.current_bg_color = aui_color_lerp(
                node->transition_state.from_bg_color,
                node->transition_state.to_bg_color, t);
            node->transition_state.current_text_color = aui_color_lerp(
                node->transition_state.from_text_color,
                node->transition_state.to_text_color, t);
            node->transition_state.current_border_color = aui_color_lerp(
                node->transition_state.from_border_color,
                node->transition_state.to_border_color, t);
        }
    } else if (!node->transition_state.active) {
        /* No active transition - set current colors directly */
        node->transition_state.current_bg_color = target_bg;
        node->transition_state.current_text_color = target_text;
        node->transition_state.current_border_color = target_border;
    }

    /* Store previous state for next frame */
    node->transition_state.prev_hovered = node->hovered;
    node->transition_state.prev_pressed = node->pressed;
}

/* ============================================================================
 * Anchor Preset Values
 * ============================================================================ */

typedef struct {
    AUI_Anchors anchors;
    bool use_offset_as_size;  /* If true, offsets define size from anchor point */
} AUI_AnchorPresetData;

static const AUI_AnchorPresetData s_anchor_presets[AUI_ANCHOR_PRESET_COUNT] = {
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

AUI_Node *aui_node_create(AUI_Context *ctx, AUI_NodeType type, const char *name)
{
    (void)ctx;

    AUI_Node *node = (AUI_Node *)calloc(1, sizeof(AUI_Node));
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
    node->anchors = (AUI_Anchors){0, 0, 0, 0};

    /* Default style */
    node->style = aui_style_default();

    /* Type-specific initialization */
    switch (type) {
        case AUI_NODE_VBOX:
        case AUI_NODE_HBOX:
            node->box.separation = 4.0f;
            break;

        case AUI_NODE_GRID:
            node->grid.columns = 2;
            node->grid.h_separation = 4.0f;
            node->grid.v_separation = 4.0f;
            break;

        case AUI_NODE_SCROLL:
            node->scroll.h_scroll_enabled = false;
            node->scroll.v_scroll_enabled = true;
            node->clip_contents = true;
            break;

        case AUI_NODE_SLIDER:
            node->slider.min_value = 0.0f;
            node->slider.max_value = 100.0f;
            node->slider.step = 0.0f;  /* No stepping by default for smooth dragging */
            node->custom_min_size_x = 100;
            node->custom_min_size_y = 24;
            break;

        case AUI_NODE_PROGRESS_BAR:
            node->progress.min_value = 0.0f;
            node->progress.max_value = 1.0f;
            break;

        case AUI_NODE_BUTTON:
        case AUI_NODE_TEXTBOX:
            node->focus_mode_click = true;
            break;

        case AUI_NODE_CHECKBOX:
            node->focus_mode_click = true;
            /* Default min size: 18px box + 8px spacing + ~150px for text */
            node->custom_min_size_x = 200;
            node->custom_min_size_y = 24;
            break;

        case AUI_NODE_COLLAPSING_HEADER:
            node->collapsing_header.expanded = true;
            node->collapsing_header.show_arrow = true;
            node->custom_min_size_y = 28;
            node->focus_mode_click = true;
            break;

        case AUI_NODE_SPLITTER:
            node->splitter.horizontal = true;
            node->splitter.split_ratio = 0.5f;
            node->splitter.min_size_first = 50.0f;
            node->splitter.min_size_second = 50.0f;
            node->splitter.splitter_width = 6.0f;
            node->splitter.dragging = false;
            break;

        case AUI_NODE_TREE:
            node->tree.root_items = NULL;
            node->tree.selected_item = NULL;
            node->tree.anchor_item = NULL;
            node->tree.indent_width = 20.0f;
            node->tree.item_height = 24.0f;
            node->tree.scroll_offset = 0.0f;
            node->tree.multi_select = false;
            node->tree.hide_root = false;
            node->tree.allow_reorder = false;
            node->tree.next_item_id = 1;
            node->tree.dragging_item = NULL;
            node->tree.drop_target = NULL;
            node->tree.drop_pos = AUI_TREE_DROP_NONE;
            node->tree.drag_start_x = 0;
            node->tree.drag_start_y = 0;
            node->tree.drag_started = false;
            node->clip_contents = true;
            node->focus_mode_click = true;
            break;

        default:
            break;
    }

    node->layout_dirty = true;
    return node;
}

void aui_node_destroy(AUI_Node *node)
{
    if (!node) return;

    /* Call destroy callback */
    if (node->on_destroy) {
        node->on_destroy(node);
    }

    /* Emit tree exit signal */
    aui_node_emit_simple(node, AUI_SIGNAL_TREE_EXITED);

    /* Remove from parent */
    aui_node_remove(node);

    /* Destroy all children */
    AUI_Node *child = node->first_child;
    while (child) {
        AUI_Node *next = child->next_sibling;
        child->parent = NULL;  /* Prevent double-remove */
        aui_node_destroy(child);
        child = next;
    }

    /* Clear focus if this was focused */
    if (s_focused_node == node) {
        s_focused_node = NULL;
    }

    /* Free textbox buffer if allocated */
    if (node->type == AUI_NODE_TEXTBOX && node->textbox.buffer) {
        /* Buffer is user-provided, don't free */
    }

    /* Free tree items if this is a tree */
    if (node->type == AUI_NODE_TREE) {
        aui_tree_clear(node);
    }

    /* Free rich text data if this is a richtext node */
    if (node->type == AUI_NODE_RICHTEXT && node->custom_data) {
        aui_richtext_destroy((AUI_RichText *)node->custom_data);
        node->custom_data = NULL;
    }

    /* Free chart data if this is a chart node */
    if (node->type == AUI_NODE_CHART && node->custom_data) {
        AUI_ChartNodeData *chart_data = (AUI_ChartNodeData *)node->custom_data;
        free(chart_data->series_storage);
        free(chart_data->slice_storage);
        free(chart_data->value_storage);
        free(chart_data);
        node->custom_data = NULL;
    }

    free(node);
}

AUI_Node *aui_node_duplicate(AUI_Node *node)
{
    if (!node) return NULL;

    AUI_Node *copy = (AUI_Node *)malloc(sizeof(AUI_Node));
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
    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
        AUI_Node *child_copy = aui_node_duplicate(child);
        if (child_copy) {
            aui_node_add_child(copy, child_copy);
        }
    }

    return copy;
}

/* ============================================================================
 * Hierarchy Management
 * ============================================================================ */

void aui_node_add_child(AUI_Node *parent, AUI_Node *child)
{
    if (!parent || !child) return;
    if (child->parent == parent) return;

    /* Remove from current parent */
    aui_node_remove(child);

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
    AUI_Signal sig;
    memset(&sig, 0, sizeof(sig));
    sig.type = AUI_SIGNAL_CHILD_ADDED;
    sig.source = parent;
    sig.child.child = child;
    aui_node_emit(parent, AUI_SIGNAL_CHILD_ADDED, &sig);

    aui_node_emit_simple(child, AUI_SIGNAL_TREE_ENTERED);

    if (child->on_enter_tree) {
        child->on_enter_tree(child);
    }
}

void aui_node_remove_child(AUI_Node *parent, AUI_Node *child)
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
    AUI_Signal sig;
    memset(&sig, 0, sizeof(sig));
    sig.type = AUI_SIGNAL_CHILD_REMOVED;
    sig.source = parent;
    sig.child.child = child;
    aui_node_emit(parent, AUI_SIGNAL_CHILD_REMOVED, &sig);

    if (child->on_exit_tree) {
        child->on_exit_tree(child);
    }
}

void aui_node_remove(AUI_Node *node)
{
    if (!node || !node->parent) return;
    aui_node_remove_child(node->parent, node);
}

void aui_node_reparent(AUI_Node *node, AUI_Node *new_parent)
{
    if (!node) return;
    aui_node_remove(node);
    if (new_parent) {
        aui_node_add_child(new_parent, node);
    }
}

AUI_Node *aui_node_get_child(AUI_Node *node, int index)
{
    if (!node || index < 0 || index >= node->child_count) return NULL;

    AUI_Node *child = node->first_child;
    for (int i = 0; i < index && child; i++) {
        child = child->next_sibling;
    }
    return child;
}

AUI_Node *aui_node_get_child_by_name(AUI_Node *node, const char *name)
{
    if (!node || !name) return NULL;

    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
    }
    return NULL;
}

/* Recursive helper to find node by name anywhere in tree */
static AUI_Node *aui_node_find_recursive(AUI_Node *node, const char *name)
{
    if (!node) return NULL;

    /* Check this node */
    if (strcmp(node->name, name) == 0) {
        return node;
    }

    /* Search children */
    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
        AUI_Node *found = aui_node_find_recursive(child, name);
        if (found) return found;
    }

    return NULL;
}

AUI_Node *aui_node_find(AUI_Node *root, const char *path)
{
    if (!root || !path) return NULL;

    /* If path contains '/', use path-based lookup */
    if (strchr(path, '/')) {
        /* Make a copy to tokenize */
        char path_copy[256];
        strncpy(path_copy, path, sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';

        AUI_Node *current = root;
        char *token = strtok(path_copy, "/");

        while (token && current) {
            current = aui_node_get_child_by_name(current, token);
            token = strtok(NULL, "/");
        }

        return current;
    }

    /* Otherwise, search recursively by name */
    return aui_node_find_recursive(root, path);
}

AUI_Node *aui_node_get_root(AUI_Node *node)
{
    if (!node) return NULL;
    while (node->parent) {
        node = node->parent;
    }
    return node;
}

bool aui_node_is_ancestor_of(AUI_Node *node, AUI_Node *descendant)
{
    if (!node || !descendant) return false;

    AUI_Node *current = descendant->parent;
    while (current) {
        if (current == node) return true;
        current = current->parent;
    }
    return false;
}

int aui_node_get_index(AUI_Node *node)
{
    if (!node || !node->parent) return -1;

    int index = 0;
    for (AUI_Node *child = node->parent->first_child; child; child = child->next_sibling) {
        if (child == node) return index;
        index++;
    }
    return -1;
}

void aui_node_move_child(AUI_Node *parent, AUI_Node *child, int new_index)
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
        AUI_Node *prev = aui_node_get_child(parent, new_index - 1);
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

void aui_node_move_to_front(AUI_Node *node)
{
    if (!node || !node->parent) return;
    aui_node_move_child(node->parent, node, node->parent->child_count - 1);
}

void aui_node_move_to_back(AUI_Node *node)
{
    if (!node || !node->parent) return;
    aui_node_move_child(node->parent, node, 0);
}

/* ============================================================================
 * Layout
 * ============================================================================ */

void aui_node_set_anchor_preset(AUI_Node *node, AUI_AnchorPreset preset)
{
    if (!node || preset < 0 || preset >= AUI_ANCHOR_PRESET_COUNT) return;

    const AUI_AnchorPresetData *data = &s_anchor_presets[preset];
    node->anchors = data->anchors;
    node->layout_dirty = true;
}

void aui_node_set_anchors(AUI_Node *node, float left, float top,
                           float right, float bottom)
{
    if (!node) return;
    node->anchors.left = left;
    node->anchors.top = top;
    node->anchors.right = right;
    node->anchors.bottom = bottom;
    node->layout_dirty = true;
}

void aui_node_set_offsets(AUI_Node *node, float left, float top,
                           float right, float bottom)
{
    if (!node) return;
    node->offsets.left = left;
    node->offsets.top = top;
    node->offsets.right = right;
    node->offsets.bottom = bottom;
    node->layout_dirty = true;
}

void aui_node_set_size(AUI_Node *node, float width, float height)
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

void aui_node_set_position(AUI_Node *node, float x, float y)
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

void aui_node_get_size(AUI_Node *node, float *width, float *height)
{
    if (!node) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    if (width) *width = node->rect.w;
    if (height) *height = node->rect.h;
}

void aui_node_get_position(AUI_Node *node, float *x, float *y)
{
    if (!node) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    if (x) *x = node->rect.x;
    if (y) *y = node->rect.y;
}

void aui_node_get_global_position(AUI_Node *node, float *x, float *y)
{
    if (!node) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    if (x) *x = node->global_rect.x;
    if (y) *y = node->global_rect.y;
}

void aui_node_set_h_size_flags(AUI_Node *node, uint8_t flags)
{
    if (!node) return;
    node->h_size_flags = flags;
    node->layout_dirty = true;
}

void aui_node_set_v_size_flags(AUI_Node *node, uint8_t flags)
{
    if (!node) return;
    node->v_size_flags = flags;
    node->layout_dirty = true;
}

void aui_node_set_stretch_ratio(AUI_Node *node, float ratio)
{
    if (!node) return;
    node->size_flags_stretch_ratio = ratio;
    node->layout_dirty = true;
}

void aui_node_set_custom_min_size(AUI_Node *node, float width, float height)
{
    if (!node) return;
    node->custom_min_size_x = width;
    node->custom_min_size_y = height;
    node->layout_dirty = true;
}

void aui_node_get_min_size(AUI_Node *node, float *width, float *height)
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

void aui_node_queue_layout(AUI_Node *node)
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

void aui_node_set_style(AUI_Node *node, const AUI_Style *style)
{
    if (!node || !style) return;
    node->style = *style;
}

void aui_node_set_style_class(AUI_Node *node, const char *class_name)
{
    if (!node) return;
    node->style_class_name = class_name;
}

AUI_Style aui_node_get_effective_style(AUI_Node *node)
{
    AUI_Style style = aui_style_default();
    if (!node) return style;

    /* Start with style class if set */
    if (node->style_class_name) {
        AUI_StyleClass *sc = aui_get_style_class(NULL, node->style_class_name);
        if (sc) {
            style = aui_resolve_style_class(sc);
        }
    }

    /* Merge node's direct style */
    aui_style_merge(&style, &node->style);

    /* Merge runtime override */
    if (node->style_override) {
        aui_style_merge(&style, node->style_override);
    }

    return style;
}

/* ============================================================================
 * State
 * ============================================================================ */

void aui_node_set_visible(AUI_Node *node, bool visible)
{
    if (!node || node->visible == visible) return;

    bool old = node->visible;
    node->visible = visible;

    AUI_Signal sig;
    memset(&sig, 0, sizeof(sig));
    sig.type = AUI_SIGNAL_VISIBILITY_CHANGED;
    sig.source = node;
    sig.bool_change.old_value = old;
    sig.bool_change.new_value = visible;
    aui_node_emit(node, AUI_SIGNAL_VISIBILITY_CHANGED, &sig);

    if (node->parent) {
        node->parent->layout_dirty = true;
    }
}

bool aui_node_is_visible(AUI_Node *node)
{
    return node ? node->visible : false;
}

bool aui_node_is_visible_in_tree(AUI_Node *node)
{
    while (node) {
        if (!node->visible) return false;
        node = node->parent;
    }
    return true;
}

void aui_node_set_enabled(AUI_Node *node, bool enabled)
{
    if (node) node->enabled = enabled;
}

bool aui_node_is_enabled(AUI_Node *node)
{
    return node ? node->enabled : false;
}

void aui_node_grab_focus(AUI_Node *node)
{
    if (!node) return;

    if (s_focused_node && s_focused_node != node) {
        AUI_Node *old = s_focused_node;
        old->focused = false;
        aui_node_emit_simple(old, AUI_SIGNAL_UNFOCUSED);
    }

    s_focused_node = node;
    node->focused = true;
    aui_node_emit_simple(node, AUI_SIGNAL_FOCUSED);
}

void aui_node_release_focus(AUI_Node *node)
{
    if (!node || s_focused_node != node) return;

    node->focused = false;
    s_focused_node = NULL;
    aui_node_emit_simple(node, AUI_SIGNAL_UNFOCUSED);
}

bool aui_node_has_focus(AUI_Node *node)
{
    return node && s_focused_node == node;
}

AUI_Node *aui_get_focused_node(AUI_Context *ctx)
{
    (void)ctx;
    return s_focused_node;
}

void aui_node_set_opacity(AUI_Node *node, float opacity)
{
    if (node) {
        node->opacity = fmaxf(0.0f, fminf(1.0f, opacity));
    }
}

float aui_node_get_opacity(AUI_Node *node)
{
    return node ? node->opacity : 1.0f;
}

/* ============================================================================
 * Signals
 * ============================================================================ */

static uint32_t s_next_connection_id = 1;

uint32_t aui_node_connect(AUI_Node *node, AUI_SignalType signal,
                           AUI_SignalCallback callback, void *userdata)
{
    if (!node || !callback) return 0;
    if (node->connection_count >= AUI_MAX_CONNECTIONS) return 0;

    AUI_Connection *conn = &node->connections[node->connection_count++];
    conn->id = s_next_connection_id++;
    conn->signal_type = signal;
    conn->callback = callback;
    conn->userdata = userdata;
    conn->active = true;
    conn->oneshot = false;

    return conn->id;
}

uint32_t aui_node_connect_oneshot(AUI_Node *node, AUI_SignalType signal,
                                   AUI_SignalCallback callback, void *userdata)
{
    uint32_t id = aui_node_connect(node, signal, callback, userdata);
    if (id && node->connection_count > 0) {
        node->connections[node->connection_count - 1].oneshot = true;
    }
    return id;
}

void aui_node_disconnect(AUI_Node *node, uint32_t connection_id)
{
    if (!node || connection_id == 0) return;

    for (int i = 0; i < node->connection_count; i++) {
        if (node->connections[i].id == connection_id) {
            node->connections[i].active = false;
            return;
        }
    }
}

void aui_node_disconnect_all(AUI_Node *node, AUI_SignalType signal)
{
    if (!node) return;

    for (int i = 0; i < node->connection_count; i++) {
        if (node->connections[i].signal_type == signal) {
            node->connections[i].active = false;
        }
    }
}

void aui_node_emit(AUI_Node *node, AUI_SignalType signal, const AUI_Signal *data)
{
    if (!node) return;

    for (int i = 0; i < node->connection_count; i++) {
        AUI_Connection *conn = &node->connections[i];
        if (conn->active && conn->signal_type == signal) {
            conn->callback(node, data, conn->userdata);
            if (conn->oneshot) {
                conn->active = false;
            }
        }
    }
}

void aui_node_emit_simple(AUI_Node *node, AUI_SignalType signal)
{
    AUI_Signal sig;
    memset(&sig, 0, sizeof(sig));
    sig.type = signal;
    sig.source = node;
    aui_node_emit(node, signal, &sig);
}

/* ============================================================================
 * Layout Calculation
 * ============================================================================ */

static void aui_node_calculate_rect(AUI_Node *node, AUI_Rect parent_rect)
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
    aui_node_get_min_size(node, &min_w, &min_h);
    if (node->rect.w < min_w) node->rect.w = min_w;
    if (node->rect.h < min_h) node->rect.h = min_h;

    /* Calculate global rect */
    node->global_rect.x = parent_rect.x + node->rect.x;
    node->global_rect.y = parent_rect.y + node->rect.y;
    node->global_rect.w = node->rect.w;
    node->global_rect.h = node->rect.h;
}

/* Get minimum size for a node, using text metrics for labels/buttons */
static void aui_node_get_content_min_size(AUI_Context *ctx, AUI_Node *node,
                                           float *out_w, float *out_h)
{
    float min_w = node->custom_min_size_x;
    float min_h = node->custom_min_size_y;

    /* Calculate content-based minimum size for certain node types */
    if (ctx) {
        switch (node->type) {
            case AUI_NODE_LABEL:
                if (node->label.text[0]) {
                    float tw = aui_text_width(ctx, node->label.text);
                    float th = aui_text_height(ctx);
                    if (tw > min_w) min_w = tw;
                    if (th > min_h) min_h = th;
                }
                break;
            case AUI_NODE_BUTTON:
                if (node->button.text[0]) {
                    float tw = aui_text_width(ctx, node->button.text);
                    float th = aui_text_height(ctx);
                    /* Add some button padding */
                    if (tw + 20 > min_w) min_w = tw + 20;
                    if (th + 10 > min_h) min_h = th + 10;
                }
                break;
            case AUI_NODE_RICHTEXT:
                {
                    AUI_RichText *rt = (AUI_RichText *)node->custom_data;
                    if (rt) {
                        /* Layout with current node width if available, else 0 (no wrapping) */
                        float layout_w = node->global_rect.w > 0 ?
                            node->global_rect.w - node->style.padding.left - node->style.padding.right : 0;
                        aui_richtext_layout_ctx(ctx, rt, layout_w);
                        float rw, rh;
                        aui_richtext_get_size(rt, &rw, &rh);
                        if (rw > min_w) min_w = rw;
                        if (rh > min_h) min_h = rh;
                    }
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

static void aui_node_layout_vbox_ctx(AUI_Context *ctx, AUI_Node *node)
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
    float total_stretch = 0;

    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (!child->visible) continue;

        float child_min_w, child_min_h;
        aui_node_get_content_min_size(ctx, child, &child_min_w, &child_min_h);
        total_min_h += child_min_h;

        if (child->v_size_flags & AUI_SIZE_EXPAND) {
            total_stretch += child->size_flags_stretch_ratio;
        }
    }

    /* Add separations */
    int visible_count = 0;
    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
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
    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (!child->visible) continue;

        float child_min_w, child_min_h;
        aui_node_get_content_min_size(ctx, child, &child_min_w, &child_min_h);

        /* Calculate child width - FILL takes full available width */
        float child_w = child_min_w;
        if (child->h_size_flags & AUI_SIZE_FILL) {
            child_w = available_w;
        }

        /* Calculate child height */
        float child_h = child_min_h;
        if ((child->v_size_flags & AUI_SIZE_EXPAND) && total_stretch > 0) {
            float ratio = child->size_flags_stretch_ratio / total_stretch;
            child_h += extra_space * ratio;
        }

        /* Calculate x position based on alignment */
        float child_x = padding_left;
        if (child->h_size_flags & AUI_SIZE_SHRINK_CENTER) {
            child_x = padding_left + (available_w - child_w) / 2;
        } else if (child->h_size_flags & AUI_SIZE_SHRINK_END) {
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


static void aui_node_layout_hbox(AUI_Node *node)
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

    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (!child->visible) continue;

        float child_min_w, child_min_h;
        aui_node_get_min_size(child, &child_min_w, &child_min_h);
        total_min_w += child_min_w;

        if (child->h_size_flags & AUI_SIZE_EXPAND) {
            total_stretch += child->size_flags_stretch_ratio;
        }
    }

    /* Add separations */
    int visible_count = 0;
    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
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
    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (!child->visible) continue;

        float child_min_w, child_min_h;
        aui_node_get_min_size(child, &child_min_w, &child_min_h);

        /* Calculate child width */
        float child_w = child_min_w;
        if ((child->h_size_flags & AUI_SIZE_EXPAND) && total_stretch > 0) {
            float ratio = child->size_flags_stretch_ratio / total_stretch;
            child_w += extra_space * ratio;
        }

        /* Calculate child height */
        float child_h = child_min_h;
        if (child->v_size_flags & AUI_SIZE_FILL) {
            child_h = available_h;
        }

        /* Calculate y position based on alignment */
        float child_y = padding_top;
        if (child->v_size_flags & AUI_SIZE_SHRINK_CENTER) {
            child_y = padding_top + (available_h - child_h) / 2;
        } else if (child->v_size_flags & AUI_SIZE_SHRINK_END) {
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
static bool aui_node_is_layout_container(AUI_Node *node)
{
    if (!node) return false;
    switch (node->type) {
        case AUI_NODE_VBOX:
        case AUI_NODE_HBOX:
        case AUI_NODE_GRID:
        case AUI_NODE_CENTER:
        case AUI_NODE_PANEL:  /* Panels offset children for title bar */
            return true;
        default:
            return false;
    }
}

static void aui_node_layout_children(AUI_Context *ctx, AUI_Node *node)
{
    if (!node) return;

    /* Container-specific layout - these directly set child positions */
    switch (node->type) {
        case AUI_NODE_VBOX:
            aui_node_layout_vbox_ctx(ctx, node);
            return;

        case AUI_NODE_HBOX:
            aui_node_layout_hbox(node);  /* TODO: add ctx version */
            return;

        case AUI_NODE_GRID:
            /* TODO: Implement grid layout */
            break;

        case AUI_NODE_CENTER:
            {
                /* Center single child within this container */
                AUI_Node *child = node->first_child;
                if (!child || !child->visible) break;

                /* Calculate child's minimum size */
                float child_w = child->custom_min_size_x > 0 ? child->custom_min_size_x :
                                child->min_size_x > 0 ? child->min_size_x : 100.0f;
                float child_h = child->custom_min_size_y > 0 ? child->custom_min_size_y :
                                child->min_size_y > 0 ? child->min_size_y : 100.0f;

                /* Center the child */
                float x = node->global_rect.x + (node->global_rect.w - child_w) * 0.5f;
                float y = node->global_rect.y + (node->global_rect.h - child_h) * 0.5f;

                child->rect.x = x - node->global_rect.x;
                child->rect.y = y - node->global_rect.y;
                child->rect.w = child_w;
                child->rect.h = child_h;
                child->global_rect.x = x;
                child->global_rect.y = y;
                child->global_rect.w = child_w;
                child->global_rect.h = child_h;
            }
            return;

        case AUI_NODE_PANEL:
            {
                /* Offset children by title bar height if title is set */
                float title_offset = 0;
                if (node->panel.title[0] != '\0' && ctx) {
                    title_offset = ctx->theme.widget_height;
                }

                /* If collapsed, hide all children */
                if (node->panel.collapsed) {
                    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
                        child->visible = false;
                    }
                    return;
                }

                /* Calculate content area (after title bar and padding) */
                AUI_Rect content_rect = node->global_rect;
                content_rect.x += node->style.padding.left;
                content_rect.y += title_offset + node->style.padding.top;
                content_rect.w -= node->style.padding.left + node->style.padding.right;
                content_rect.h -= title_offset + node->style.padding.top + node->style.padding.bottom;

                for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
                    if (!child->visible) continue;
                    aui_node_calculate_rect(child, content_rect);
                }
            }
            return;

        case AUI_NODE_COLLAPSING_HEADER:
            {
                /* Header height */
                float header_h = ctx ? ctx->theme.widget_height : 28.0f;

                /* If collapsed, hide all children */
                if (!node->collapsing_header.expanded) {
                    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
                        child->visible = false;
                    }
                    return;
                }

                /* Content area (after header) */
                AUI_Rect content_rect = node->global_rect;
                content_rect.y += header_h;
                content_rect.h -= header_h;

                for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
                    child->visible = true;
                    aui_node_calculate_rect(child, content_rect);
                }
            }
            return;

        case AUI_NODE_SPLITTER:
            {
                /* Splitter expects exactly 2 children */
                AUI_Node *first = node->first_child;
                AUI_Node *second = first ? first->next_sibling : NULL;
                if (!first || !second) return;

                float total_size;
                float splitter_w = node->splitter.splitter_width;
                float ratio = node->splitter.split_ratio;

                if (node->splitter.horizontal) {
                    /* Left/right split */
                    total_size = node->global_rect.w - splitter_w;
                    float first_w = total_size * ratio;
                    float second_w = total_size - first_w;

                    /* Apply min sizes */
                    if (first_w < node->splitter.min_size_first) {
                        first_w = node->splitter.min_size_first;
                        second_w = total_size - first_w;
                    }
                    if (second_w < node->splitter.min_size_second) {
                        second_w = node->splitter.min_size_second;
                        first_w = total_size - second_w;
                    }

                    AUI_Rect first_rect = {
                        node->global_rect.x,
                        node->global_rect.y,
                        first_w,
                        node->global_rect.h
                    };
                    AUI_Rect second_rect = {
                        node->global_rect.x + first_w + splitter_w,
                        node->global_rect.y,
                        second_w,
                        node->global_rect.h
                    };

                    first->global_rect = first_rect;
                    first->rect = first_rect;
                    second->global_rect = second_rect;
                    second->rect = second_rect;
                } else {
                    /* Top/bottom split */
                    total_size = node->global_rect.h - splitter_w;
                    float first_h = total_size * ratio;
                    float second_h = total_size - first_h;

                    /* Apply min sizes */
                    if (first_h < node->splitter.min_size_first) {
                        first_h = node->splitter.min_size_first;
                        second_h = total_size - first_h;
                    }
                    if (second_h < node->splitter.min_size_second) {
                        second_h = node->splitter.min_size_second;
                        first_h = total_size - second_h;
                    }

                    AUI_Rect first_rect = {
                        node->global_rect.x,
                        node->global_rect.y,
                        node->global_rect.w,
                        first_h
                    };
                    AUI_Rect second_rect = {
                        node->global_rect.x,
                        node->global_rect.y + first_h + splitter_w,
                        node->global_rect.w,
                        second_h
                    };

                    first->global_rect = first_rect;
                    first->rect = first_rect;
                    second->global_rect = second_rect;
                    second->rect = second_rect;
                }
            }
            return;

        default:
            break;
    }

    /* Default: calculate each child's rect from anchors */
    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (!child->visible) continue;
        aui_node_calculate_rect(child, node->global_rect);
    }
}

static void aui_node_layout_recursive_internal(AUI_Context *ctx, AUI_Node *node,
                                                 AUI_Rect parent_rect,
                                                 bool parent_is_layout_container)
{
    if (!node || !node->visible) return;

    /* Only calculate rect from anchors if parent didn't already position us.
     * Layout containers (VBox, HBox, Grid) set child positions directly,
     * so we skip the anchor-based calculation for their children. */
    if (!parent_is_layout_container) {
        aui_node_calculate_rect(node, parent_rect);
    }

    /* Layout children (this may directly set child positions for VBox/HBox/Grid) */
    aui_node_layout_children(ctx, node);

    /* Call custom layout */
    if (node->on_layout) {
        node->on_layout(node);
    }

    /* Check if this node is a layout container for children */
    bool this_is_layout_container = aui_node_is_layout_container(node);

    /* Recurse to children */
    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
        if (child->visible) {
            aui_node_layout_recursive_internal(ctx, child, node->global_rect, this_is_layout_container);
        }
    }

    node->layout_dirty = false;
}

static void aui_node_layout_recursive(AUI_Context *ctx, AUI_Node *node, AUI_Rect parent_rect)
{
    /* Root node is never inside a layout container */
    aui_node_layout_recursive_internal(ctx, node, parent_rect, false);
}

/* ============================================================================
 * Scene Tree Processing
 * ============================================================================ */

void aui_scene_update(AUI_Context *ctx, AUI_Node *root, float delta_time)
{
    if (!ctx || !root) return;

    /* Update style transitions */
    aui_node_update_transitions(root, delta_time);

    /* Process nodes recursively */
    if (root->on_process) {
        root->on_process(root, delta_time);
    }

    for (AUI_Node *child = root->first_child; child; child = child->next_sibling) {
        aui_scene_update(ctx, child, delta_time);
    }

    /* Update tooltip hover time for retained-mode nodes */
    if (ctx->hovered_node && ctx->hovered_node->tooltip_text[0] != '\0') {
        ctx->tooltip_hover_time += delta_time;

        /* Show tooltip after delay */
        float delay = ctx->hovered_node->tooltip_delay > 0 ?
                      ctx->hovered_node->tooltip_delay : 0.5f;
        if (ctx->tooltip_hover_time >= delay && !ctx->pending_tooltip_active) {
            strncpy(ctx->pending_tooltip, ctx->hovered_node->tooltip_text,
                    sizeof(ctx->pending_tooltip) - 1);
            ctx->pending_tooltip[sizeof(ctx->pending_tooltip) - 1] = '\0';
            ctx->pending_tooltip_active = true;
            ctx->pending_tooltip_x = ctx->input.mouse_x;
            ctx->pending_tooltip_y = ctx->input.mouse_y;
        }
    }
}

bool aui_scene_process_event(AUI_Context *ctx, AUI_Node *root, const SDL_Event *event)
{
    if (!ctx || !root || !event) return false;

    /* Update input state for key events (needed for shortcuts) */
    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.scancode < 512) {
            ctx->input.keys_down[event->key.scancode] = true;
            ctx->input.keys_pressed[event->key.scancode] = true;
        }
        ctx->input.shift = (event->key.mod & SDL_KMOD_SHIFT) != 0;
        /* Treat Cmd (GUI) as Ctrl for shortcuts on macOS */
        ctx->input.ctrl = (event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
        ctx->input.alt = (event->key.mod & SDL_KMOD_ALT) != 0;
    } else if (event->type == SDL_EVENT_KEY_UP) {
        if (event->key.scancode < 512) {
            ctx->input.keys_down[event->key.scancode] = false;
        }
    }

    /* Ensure layout is up-to-date before hit testing */
    aui_scene_layout(ctx, root);

    /* Find node at mouse position for mouse events */
    if (event->type == SDL_EVENT_MOUSE_MOTION ||
        event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event->type == SDL_EVENT_MOUSE_BUTTON_UP) {

        float mx = event->motion.x;
        float my = event->motion.y;

        AUI_Node *hit = aui_node_hit_test(root, mx, my);

        /* Handle hover state changes */
        static AUI_Node *s_last_hovered = NULL;
        if (hit != s_last_hovered) {
            if (s_last_hovered) {
                s_last_hovered->hovered = false;
                aui_node_emit_simple(s_last_hovered, AUI_SIGNAL_MOUSE_EXITED);
            }
            if (hit) {
                hit->hovered = true;
                aui_node_emit_simple(hit, AUI_SIGNAL_MOUSE_ENTERED);
            }
            s_last_hovered = hit;

            /* Update context for tooltip tracking */
            ctx->hovered_node = hit;
            ctx->tooltip_hover_time = 0;
            ctx->pending_tooltip_active = false;
        }

        /* Track which node is currently pressed (must be outside block scope) */
        static AUI_Node *s_pressed_node = NULL;

        /* Handle click */
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && hit) {
            if (hit->focus_mode_click) {
                AUI_Node *old_focused = s_focused_node;
                aui_node_grab_focus(hit);

                /* Start/stop text input for textbox focus changes */
                if (ctx && ctx->window) {
                    bool old_is_textbox = old_focused && old_focused->type == AUI_NODE_TEXTBOX;
                    bool new_is_textbox = hit->type == AUI_NODE_TEXTBOX;
                    if (new_is_textbox && !old_is_textbox) {
                        SDL_StartTextInput(ctx->window);
                    } else if (!new_is_textbox && old_is_textbox) {
                        SDL_StopTextInput(ctx->window);
                    }
                }
            }

            hit->pressed = true;
            s_pressed_node = hit;  /* Store the pressed node */

            AUI_Signal sig;
            memset(&sig, 0, sizeof(sig));
            sig.type = AUI_SIGNAL_PRESSED;
            sig.source = hit;
            sig.mouse.x = mx;
            sig.mouse.y = my;
            sig.mouse.button = event->button.button;
            aui_node_emit(hit, AUI_SIGNAL_PRESSED, &sig);

            if (hit->on_gui_input) {
                return hit->on_gui_input(hit, ctx, event);
            }
            return !hit->mouse_filter_ignore;
        }

        if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
            /* Release pressed node and emit released/clicked */
            if (s_pressed_node) {
                s_pressed_node->pressed = false;
                aui_node_emit_simple(s_pressed_node, AUI_SIGNAL_RELEASED);

                if (hit == s_pressed_node) {
                    aui_node_emit_simple(hit, AUI_SIGNAL_CLICKED);

                    /* Handle checkbox toggle */
                    if (hit->type == AUI_NODE_CHECKBOX) {
                        bool old_val = hit->checkbox.checked;
                        hit->checkbox.checked = !hit->checkbox.checked;
                        AUI_Signal sig;
                        memset(&sig, 0, sizeof(sig));
                        sig.type = AUI_SIGNAL_TOGGLED;
                        sig.source = hit;
                        sig.bool_change.old_value = old_val;
                        sig.bool_change.new_value = hit->checkbox.checked;
                        aui_node_emit(hit, AUI_SIGNAL_TOGGLED, &sig);
                    }

                    /* Handle collapsing header toggle */
                    if (hit->type == AUI_NODE_COLLAPSING_HEADER) {
                        bool old_expanded = hit->collapsing_header.expanded;
                        hit->collapsing_header.expanded = !hit->collapsing_header.expanded;
                        hit->layout_dirty = true;

                        AUI_Signal sig;
                        memset(&sig, 0, sizeof(sig));
                        sig.type = AUI_SIGNAL_TOGGLED;
                        sig.source = hit;
                        sig.bool_change.old_value = old_expanded;
                        sig.bool_change.new_value = hit->collapsing_header.expanded;
                        aui_node_emit(hit, AUI_SIGNAL_TOGGLED, &sig);
                    }

                    /* Handle richtext link click */
                    if (hit->type == AUI_NODE_RICHTEXT) {
                        AUI_RichText *rt = (AUI_RichText *)hit->custom_data;
                        if (rt) {
                            AUI_Style style = aui_node_get_effective_style(hit);
                            float content_x = hit->global_rect.x + style.padding.left;
                            float content_y = hit->global_rect.y + style.padding.top;
                            float rel_x = mx - content_x;
                            float rel_y = my - content_y;

                            const char *url = aui_richtext_get_link_at(rt, rel_x, rel_y);
                            if (url) {
                                /* Emit signal with URL for external handling */
                                AUI_Signal sig;
                                memset(&sig, 0, sizeof(sig));
                                sig.type = AUI_SIGNAL_CLICKED;
                                sig.source = hit;
                                sig.text_change.new_text = url;
                                aui_node_emit(hit, AUI_SIGNAL_CLICKED, &sig);
                            }
                        }
                    }

                    /* Handle tree item click */
                    if (hit->type == AUI_NODE_TREE) {
                        float item_h = hit->tree.item_height;
                        float indent = hit->tree.indent_width;
                        float tree_y = hit->global_rect.y;
                        float click_y = my - tree_y + hit->tree.scroll_offset;
                        float click_x = mx - hit->global_rect.x;

                        /* Find clicked item */
                        float current_y = 0;
                        std::function<AUI_TreeItem*(AUI_TreeItem*, int)> find_item_at_y;
                        find_item_at_y = [&](AUI_TreeItem *item, int depth) -> AUI_TreeItem* {
                            while (item) {
                                if (click_y >= current_y && click_y < current_y + item_h) {
                                    /* Check if click is on expand arrow */
                                    float item_x = depth * indent;
                                    bool has_children = item->first_child != NULL;

                                    if (has_children && click_x >= item_x && click_x < item_x + 24.0f) {
                                        /* Toggle expand/collapse */
                                        item->expanded = !item->expanded;
                                        if (item->expanded) {
                                            aui_node_emit_simple(hit, AUI_SIGNAL_ITEM_EXPANDED);
                                        } else {
                                            aui_node_emit_simple(hit, AUI_SIGNAL_ITEM_COLLAPSED);
                                        }
                                        return NULL;  /* Don't select */
                                    }
                                    return item;  /* Select this item */
                                }
                                current_y += item_h;

                                if (item->expanded && item->first_child) {
                                    AUI_TreeItem *found = find_item_at_y(item->first_child, depth + 1);
                                    if (found) return found;
                                }
                                item = item->next_sibling;
                            }
                            return NULL;
                        };

                        AUI_TreeItem *clicked_item = find_item_at_y(hit->tree.root_items, 0);
                        if (clicked_item) {
                            /* Deselect previous */
                            if (hit->tree.selected_item && !hit->tree.multi_select) {
                                hit->tree.selected_item->selected = false;
                            }

                            /* Select new item */
                            clicked_item->selected = true;
                            hit->tree.selected_item = clicked_item;

                            aui_node_emit_simple(hit, AUI_SIGNAL_ITEM_SELECTED);
                        }
                    }

                    /* Handle panel title bar buttons */
                    if (hit->type == AUI_NODE_PANEL && hit->panel.title[0] != '\0' && ctx) {
                        float title_h = ctx->theme.widget_height;
                        float btn_size = title_h - 8;
                        float btn_padding = 4;
                        float x = hit->global_rect.x;
                        float y = hit->global_rect.y;
                        float w = hit->global_rect.w;

                        /* Check if click is in title bar area */
                        if (my >= y && my < y + title_h) {
                            float btn_x = x + w - btn_padding - btn_size;
                            float btn_y = y + (title_h - btn_size) / 2;

                            /* Check close button */
                            if (hit->panel.closable &&
                                mx >= btn_x && mx < btn_x + btn_size &&
                                my >= btn_y && my < btn_y + btn_size) {
                                hit->panel.closed = true;
                                hit->visible = false;
                                aui_node_emit_simple(hit, AUI_SIGNAL_VISIBILITY_CHANGED);
                            } else {
                                btn_x -= btn_size + btn_padding;

                                /* Check collapse button */
                                if (hit->panel.collapsible &&
                                    mx >= btn_x && mx < btn_x + btn_size &&
                                    my >= btn_y && my < btn_y + btn_size) {
                                    hit->panel.collapsed = !hit->panel.collapsed;
                                    hit->layout_dirty = true;
                                    AUI_Signal sig;
                                    memset(&sig, 0, sizeof(sig));
                                    sig.type = AUI_SIGNAL_TOGGLED;
                                    sig.source = hit;
                                    sig.bool_change.old_value = !hit->panel.collapsed;
                                    sig.bool_change.new_value = hit->panel.collapsed;
                                    aui_node_emit(hit, AUI_SIGNAL_TOGGLED, &sig);
                                }
                            }
                        }
                    }
                }
                s_pressed_node = NULL;
            }
        }

        /* Handle slider dragging */
        if (event->type == SDL_EVENT_MOUSE_MOTION && s_pressed_node &&
            s_pressed_node->type == AUI_NODE_SLIDER) {
            AUI_Node *slider = s_pressed_node;
            float rel_x = mx - slider->global_rect.x;
            float ratio = rel_x / slider->global_rect.w;
            ratio = fmaxf(0.0f, fminf(1.0f, ratio));

            float range = slider->slider.max_value - slider->slider.min_value;
            float old_val = slider->slider.value;
            float new_val = slider->slider.min_value + ratio * range;

            /* Apply step if set */
            if (slider->slider.step > 0) {
                new_val = roundf(new_val / slider->slider.step) * slider->slider.step;
            }
            new_val = fmaxf(slider->slider.min_value,
                           fminf(slider->slider.max_value, new_val));

            if (new_val != old_val) {
                slider->slider.value = new_val;
                AUI_Signal sig;
                memset(&sig, 0, sizeof(sig));
                sig.type = AUI_SIGNAL_VALUE_CHANGED;
                sig.source = slider;
                sig.float_change.old_value = old_val;
                sig.float_change.new_value = new_val;
                aui_node_emit(slider, AUI_SIGNAL_VALUE_CHANGED, &sig);
            }
        }

        /* Handle slider click to set value directly */
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && hit &&
            hit->type == AUI_NODE_SLIDER) {
            float rel_x = mx - hit->global_rect.x;
            float ratio = rel_x / hit->global_rect.w;
            ratio = fmaxf(0.0f, fminf(1.0f, ratio));

            float range = hit->slider.max_value - hit->slider.min_value;
            float old_val = hit->slider.value;
            float new_val = hit->slider.min_value + ratio * range;

            if (hit->slider.step > 0) {
                new_val = roundf(new_val / hit->slider.step) * hit->slider.step;
            }
            new_val = fmaxf(hit->slider.min_value,
                           fminf(hit->slider.max_value, new_val));

            if (new_val != old_val) {
                hit->slider.value = new_val;
                AUI_Signal sig;
                memset(&sig, 0, sizeof(sig));
                sig.type = AUI_SIGNAL_VALUE_CHANGED;
                sig.source = hit;
                sig.float_change.old_value = old_val;
                sig.float_change.new_value = new_val;
                aui_node_emit(hit, AUI_SIGNAL_VALUE_CHANGED, &sig);
            }
        }

        /* Handle splitter dragging */
        if (event->type == SDL_EVENT_MOUSE_MOTION && s_pressed_node &&
            s_pressed_node->type == AUI_NODE_SPLITTER) {
            AUI_Node *splitter = s_pressed_node;
            splitter->splitter.dragging = true;

            float total_size;
            float rel_pos;

            if (splitter->splitter.horizontal) {
                total_size = splitter->global_rect.w - splitter->splitter.splitter_width;
                rel_pos = mx - splitter->global_rect.x;
            } else {
                total_size = splitter->global_rect.h - splitter->splitter.splitter_width;
                rel_pos = my - splitter->global_rect.y;
            }

            float new_ratio = rel_pos / total_size;
            new_ratio = fmaxf(0.0f, fminf(1.0f, new_ratio));

            /* Enforce min sizes */
            float min_first_ratio = splitter->splitter.min_size_first / total_size;
            float min_second_ratio = splitter->splitter.min_size_second / total_size;
            new_ratio = fmaxf(min_first_ratio, fminf(1.0f - min_second_ratio, new_ratio));

            if (new_ratio != splitter->splitter.split_ratio) {
                float old_ratio = splitter->splitter.split_ratio;
                splitter->splitter.split_ratio = new_ratio;
                splitter->layout_dirty = true;

                AUI_Signal sig;
                memset(&sig, 0, sizeof(sig));
                sig.type = AUI_SIGNAL_VALUE_CHANGED;
                sig.source = splitter;
                sig.float_change.old_value = old_ratio;
                sig.float_change.new_value = new_ratio;
                aui_node_emit(splitter, AUI_SIGNAL_VALUE_CHANGED, &sig);
            }
        }

        /* Stop splitter dragging on mouse up */
        if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && s_pressed_node &&
            s_pressed_node->type == AUI_NODE_SPLITTER) {
            s_pressed_node->splitter.dragging = false;
        }

        /* Tree drag-to-reorder: Start potential drag on mouse down */
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && hit &&
            hit->type == AUI_NODE_TREE && hit->tree.allow_reorder) {
            float item_h = hit->tree.item_height;
            float tree_y = hit->global_rect.y;
            float local_y = my - tree_y + hit->tree.scroll_offset;

            float current_y = 0;
            int depth = 0;
            AUI_TreeItem *clicked_item = tree_find_item_at_y(hit->tree.root_items, item_h,
                                                              hit->tree.indent_width,
                                                              local_y, &current_y, &depth);
            if (clicked_item) {
                /* Store drag start info (but don't start drag yet - wait for threshold) */
                hit->tree.dragging_item = clicked_item;
                hit->tree.drag_start_x = mx;
                hit->tree.drag_start_y = my;
                hit->tree.drag_started = false;
                hit->tree.drop_target = NULL;
                hit->tree.drop_pos = AUI_TREE_DROP_NONE;
            }
        }

        /* Tree drag-to-reorder: Update drag state on mouse motion */
        if (event->type == SDL_EVENT_MOUSE_MOTION && s_pressed_node &&
            s_pressed_node->type == AUI_NODE_TREE && s_pressed_node->tree.dragging_item) {
            AUI_Node *tree = s_pressed_node;

            /* Check if drag threshold exceeded */
            if (!tree->tree.drag_started) {
                float dx = mx - tree->tree.drag_start_x;
                float dy = my - tree->tree.drag_start_y;
                if (dx * dx + dy * dy > TREE_DRAG_THRESHOLD * TREE_DRAG_THRESHOLD) {
                    tree->tree.drag_started = true;
                }
            }

            if (tree->tree.drag_started) {
                float item_h = tree->tree.item_height;
                float tree_y = tree->global_rect.y;
                float local_y = my - tree_y + tree->tree.scroll_offset;

                float current_y = 0;
                int depth = 0;
                AUI_TreeItem *target = tree_find_item_at_y(tree->tree.root_items, item_h,
                                                            tree->tree.indent_width,
                                                            local_y, &current_y, &depth);

                /* Can't drop on self or descendant */
                if (target && (target == tree->tree.dragging_item ||
                    tree_is_descendant(target, tree->tree.dragging_item))) {
                    target = NULL;
                }

                tree->tree.drop_target = target;

                if (target) {
                    /* Determine drop position based on mouse Y within item */
                    float item_y = tree_y + current_y - tree->tree.scroll_offset;
                    float rel_y = my - item_y;
                    float third = item_h / 3.0f;

                    if (rel_y < third) {
                        tree->tree.drop_pos = AUI_TREE_DROP_BEFORE;
                    } else if (rel_y > item_h - third) {
                        tree->tree.drop_pos = AUI_TREE_DROP_AFTER;
                    } else {
                        tree->tree.drop_pos = AUI_TREE_DROP_INTO;
                    }
                } else {
                    tree->tree.drop_pos = AUI_TREE_DROP_NONE;
                }
            }
        }

        /* Tree drag-to-reorder: Perform drop on mouse up */
        if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && s_pressed_node &&
            s_pressed_node->type == AUI_NODE_TREE && s_pressed_node->tree.dragging_item) {
            AUI_Node *tree = s_pressed_node;

            if (tree->tree.drag_started && tree->tree.drop_target &&
                tree->tree.drop_pos != AUI_TREE_DROP_NONE) {
                AUI_TreeItem *item = tree->tree.dragging_item;
                AUI_TreeItem *target = tree->tree.drop_target;

                /* Remove item from current position */
                tree_unlink_item(tree, item);

                /* Insert at new position */
                switch (tree->tree.drop_pos) {
                    case AUI_TREE_DROP_BEFORE:
                        tree_insert_before(tree, item, target);
                        break;
                    case AUI_TREE_DROP_AFTER:
                        tree_insert_after(tree, item, target);
                        break;
                    case AUI_TREE_DROP_INTO:
                        tree_insert_as_child(item, target);
                        target->expanded = true;  /* Expand to show dropped item */
                        break;
                    default:
                        break;
                }

                /* Emit signal for reorder */
                aui_node_emit_simple(tree, AUI_SIGNAL_VALUE_CHANGED);
            }

            /* Clear drag state */
            tree->tree.dragging_item = NULL;
            tree->tree.drop_target = NULL;
            tree->tree.drop_pos = AUI_TREE_DROP_NONE;
            tree->tree.drag_started = false;
        }

        return hit != NULL && !hit->mouse_filter_ignore;
    }

    /* Keyboard events go to focused node */
    if (s_focused_node) {
        /* Handle textbox input specially */
        if (s_focused_node->type == AUI_NODE_TEXTBOX && s_focused_node->textbox.buffer) {
            AUI_Node *tb = s_focused_node;

            /* Text input event */
            if (event->type == SDL_EVENT_TEXT_INPUT) {
                const char *input = event->text.text;
                int input_len = (int)strlen(input);
                int buf_len = (int)strlen(tb->textbox.buffer);
                int cursor = tb->textbox.cursor_pos;

                /* Check if there's room for the new text */
                if (buf_len + input_len < tb->textbox.buffer_size - 1) {
                    /* Shift text after cursor to make room */
                    memmove(tb->textbox.buffer + cursor + input_len,
                            tb->textbox.buffer + cursor,
                            buf_len - cursor + 1);
                    /* Insert new text */
                    memcpy(tb->textbox.buffer + cursor, input, input_len);
                    tb->textbox.cursor_pos += input_len;

                    aui_node_emit_simple(tb, AUI_SIGNAL_TEXT_CHANGED);
                }
                return true;
            }

            /* Key down events */
            if (event->type == SDL_EVENT_KEY_DOWN) {
                SDL_Keycode key = event->key.key;
                int cursor = tb->textbox.cursor_pos;
                int len = (int)strlen(tb->textbox.buffer);

                /* Backspace - delete character before cursor */
                if (key == SDLK_BACKSPACE && cursor > 0) {
                    memmove(tb->textbox.buffer + cursor - 1,
                            tb->textbox.buffer + cursor,
                            len - cursor + 1);
                    tb->textbox.cursor_pos--;
                    aui_node_emit_simple(tb, AUI_SIGNAL_TEXT_CHANGED);
                    return true;
                }

                /* Delete - delete character at cursor */
                if (key == SDLK_DELETE && cursor < len) {
                    memmove(tb->textbox.buffer + cursor,
                            tb->textbox.buffer + cursor + 1,
                            len - cursor);
                    aui_node_emit_simple(tb, AUI_SIGNAL_TEXT_CHANGED);
                    return true;
                }

                /* Left arrow - move cursor left */
                if (key == SDLK_LEFT && cursor > 0) {
                    tb->textbox.cursor_pos--;
                    return true;
                }

                /* Right arrow - move cursor right */
                if (key == SDLK_RIGHT && cursor < len) {
                    tb->textbox.cursor_pos++;
                    return true;
                }

                /* Home - move cursor to start */
                if (key == SDLK_HOME) {
                    tb->textbox.cursor_pos = 0;
                    return true;
                }

                /* End - move cursor to end */
                if (key == SDLK_END) {
                    tb->textbox.cursor_pos = len;
                    return true;
                }

                /* Enter - release focus */
                if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                    aui_node_release_focus(tb);
                    return true;
                }

                /* Escape - release focus */
                if (key == SDLK_ESCAPE) {
                    aui_node_release_focus(tb);
                    return true;
                }
            }
        }

        /* Custom input handler for other focused nodes */
        if (s_focused_node->on_gui_input) {
            return s_focused_node->on_gui_input(s_focused_node, ctx, event);
        }
    }

    /* Process keyboard shortcuts */
    if (event->type == SDL_EVENT_KEY_DOWN) {
        bool textbox_focused = s_focused_node && s_focused_node->type == AUI_NODE_TEXTBOX;

        /* Allow shortcuts with Ctrl/Cmd/Alt modifiers even when textbox focused */
        bool has_modifier = (event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI | SDL_KMOD_ALT)) != 0;

        if (!textbox_focused || has_modifier) {
            if (aui_shortcuts_process(ctx)) {
                return true;
            }
        }
    }

    return false;
}

static void aui_node_render_recursive(AUI_Context *ctx, AUI_Node *node, float inherited_opacity);

void aui_scene_render(AUI_Context *ctx, AUI_Node *root)
{
    if (!ctx || !root) return;

    /* Layout pass */
    aui_scene_layout(ctx, root);

    /* Render the scene tree */
    aui_node_render_recursive(ctx, root, 1.0f);
}

void aui_scene_layout(AUI_Context *ctx, AUI_Node *root)
{
    if (!ctx || !root) return;

    /* Create root rect from screen size */
    AUI_Rect screen_rect = {0, 0, (float)ctx->width, (float)ctx->height};
    aui_node_layout_recursive(ctx, root, screen_rect);
}

static void aui_node_render_recursive(AUI_Context *ctx, AUI_Node *node, float inherited_opacity)
{
    if (!node || !node->visible) return;

    /* Get effective style */
    AUI_Style style = aui_node_get_effective_style(node);

    /* Calculate effective opacity (node opacity * inherited from ancestors) */
    float effective_opacity = node->opacity * inherited_opacity;
    style.opacity *= effective_opacity;

    /* Draw styled background */
    if (style.background.type != AUI_BG_NONE) {
        aui_draw_styled_rect(ctx, node->global_rect.x, node->global_rect.y,
                              node->global_rect.w, node->global_rect.h, &style);
    }

    /* Type-specific rendering (apply effective_opacity to all colors) */
    switch (node->type) {
        case AUI_NODE_LABEL:
            {
                float avail_w = node->global_rect.w - style.padding.left - style.padding.right;
                float avail_h = node->global_rect.h - style.padding.top - style.padding.bottom;

                float text_x = node->global_rect.x + style.padding.left;
                float text_y = node->global_rect.y + style.padding.top;

                uint32_t text_color = node->label.color ? node->label.color : style.text_color;

                /* Use styled text drawing with alignment, overflow, shadow etc. */
                AUI_TextStyle text_style = style.text;

                /* Apply size flags as alignment overrides if not explicitly set */
                if (node->h_size_flags & AUI_SIZE_SHRINK_CENTER) {
                    text_style.align = AUI_TEXT_ALIGN_CENTER;
                } else if (node->h_size_flags & AUI_SIZE_SHRINK_END) {
                    text_style.align = AUI_TEXT_ALIGN_RIGHT;
                }

                /* Apply label-specific settings */
                if (node->label.autowrap) {
                    text_style.wrap = true;
                    text_style.overflow = AUI_TEXT_OVERFLOW_WRAP;
                }
                if (node->label.max_lines > 0) {
                    text_style.max_lines = node->label.max_lines;
                }

                aui_draw_styled_text(ctx, node->label.text,
                                     text_x, text_y, avail_w, avail_h,
                                     aui_apply_opacity(text_color, effective_opacity),
                                     &text_style);
            }
            break;

        case AUI_NODE_BUTTON:
            /* Draw button with transition-aware background */
            {
                uint32_t bg_color;
                uint32_t text_color;

                /* Use transition state colors if style has transitions enabled */
                if (style.transition.duration > 0 && node->transition_state.current_bg_color != 0) {
                    bg_color = node->transition_state.current_bg_color;
                    text_color = node->transition_state.current_text_color;
                } else {
                    /* Fallback to instant switching */
                    AUI_Background *bg = &style.background;
                    if (!node->enabled) {
                        bg = &style.background_disabled;
                    } else if (node->pressed) {
                        bg = &style.background_active;
                    } else if (node->hovered) {
                        bg = &style.background_hover;
                    }
                    bg_color = (bg->type == AUI_BG_SOLID) ? bg->solid_color : 0;
                    text_color = node->enabled ? style.text_color : style.text_color_disabled;
                }

                if (bg_color != 0) {
                    aui_draw_rect_rounded(ctx, node->global_rect.x, node->global_rect.y,
                                          node->global_rect.w, node->global_rect.h,
                                          aui_apply_opacity(bg_color, effective_opacity),
                                          style.corner_radius.top_left);
                }

                /* Draw text centered using styled text */
                AUI_TextStyle text_style = style.text;
                text_style.align = AUI_TEXT_ALIGN_CENTER;
                text_style.valign = AUI_TEXT_VALIGN_MIDDLE;

                aui_draw_styled_text(ctx, node->button.text,
                                     node->global_rect.x, node->global_rect.y,
                                     node->global_rect.w, node->global_rect.h,
                                     aui_apply_opacity(text_color, effective_opacity),
                                     &text_style);
            }
            break;

        case AUI_NODE_PROGRESS_BAR:
            {
                /* Background */
                aui_draw_rect(ctx, node->global_rect.x, node->global_rect.y,
                              node->global_rect.w, node->global_rect.h,
                              aui_apply_opacity(style.background.solid_color, effective_opacity));

                /* Fill */
                float range = node->progress.max_value - node->progress.min_value;
                float fill_ratio = (range > 0) ?
                    (node->progress.value - node->progress.min_value) / range : 0;
                fill_ratio = fmaxf(0, fminf(1, fill_ratio));

                uint32_t fill_color = node->progress.fill_color ? node->progress.fill_color : ctx->theme.accent;
                aui_draw_rect(ctx, node->global_rect.x, node->global_rect.y,
                              node->global_rect.w * fill_ratio, node->global_rect.h,
                              aui_apply_opacity(fill_color, effective_opacity));
            }
            break;

        case AUI_NODE_SLIDER:
            {
                float x = node->global_rect.x;
                float y = node->global_rect.y;
                float w = node->global_rect.w;
                float h = node->global_rect.h;

                /* Track background */
                float track_h = 6.0f;
                float track_y = y + (h - track_h) / 2;
                aui_draw_rect_rounded(ctx, x, track_y, w, track_h,
                                      aui_apply_opacity(ctx->theme.slider_track, effective_opacity), 3.0f);

                /* Calculate fill amount */
                float range = node->slider.max_value - node->slider.min_value;
                float fill_ratio = (range > 0) ?
                    (node->slider.value - node->slider.min_value) / range : 0;
                fill_ratio = fmaxf(0, fminf(1, fill_ratio));

                /* Filled portion */
                float fill_w = w * fill_ratio;
                if (fill_w > 0) {
                    aui_draw_rect_rounded(ctx, x, track_y, fill_w, track_h,
                                          aui_apply_opacity(ctx->theme.accent, effective_opacity), 3.0f);
                }

                /* Grab handle */
                float grab_r = 8.0f;
                float grab_x = x + fill_w;
                float grab_y = y + h / 2;
                uint32_t grab_color = node->hovered || node->pressed ?
                    ctx->theme.accent_hover : ctx->theme.slider_grab;
                aui_draw_rect_rounded(ctx, grab_x - grab_r, grab_y - grab_r,
                                      grab_r * 2, grab_r * 2,
                                      aui_apply_opacity(grab_color, effective_opacity), grab_r);

                /* Value text if enabled - draw inside the track, right-aligned */
                if (node->slider.show_value) {
                    char val_text[16];
                    snprintf(val_text, sizeof(val_text), "%.0f%%",
                             fill_ratio * 100);
                    float text_w = aui_text_width(ctx, val_text);
                    float text_x = x + w - text_w - 4;  /* Right-aligned with 4px padding */
                    aui_draw_text(ctx, val_text, text_x, y + (h - aui_text_height(ctx)) / 2,
                                  aui_apply_opacity(style.text_color, effective_opacity));
                }
            }
            break;

        case AUI_NODE_CHECKBOX:
            {
                float x = node->global_rect.x;
                float y = node->global_rect.y;
                float h = node->global_rect.h;

                /* Checkbox box */
                float box_size = 18.0f;
                float box_y = y + (h - box_size) / 2;
                uint32_t box_bg = node->hovered ? ctx->theme.bg_widget_hover :
                                  ctx->theme.bg_widget;
                aui_draw_rect_rounded(ctx, x, box_y, box_size, box_size,
                                      aui_apply_opacity(box_bg, effective_opacity), 3.0f);
                aui_draw_rect_outline(ctx, x, box_y, box_size, box_size,
                                      aui_apply_opacity(ctx->theme.border, effective_opacity), 1.0f);

                /* Checkmark if checked */
                if (node->checkbox.checked) {
                    /* Draw simple checkmark using lines */
                    float cx = x + box_size / 2;
                    float cy = box_y + box_size / 2;
                    /* Simple filled square for now */
                    float inner = box_size - 8;
                    aui_draw_rect(ctx, cx - inner/2, cy - inner/2, inner, inner,
                                  aui_apply_opacity(ctx->theme.checkbox_check, effective_opacity));
                }

                /* Label text */
                float text_x = x + box_size + 8;
                float text_y = y + (h - aui_text_height(ctx)) / 2;
                uint32_t cb_text_color = node->enabled ? style.text_color : style.text_color_disabled;
                aui_draw_text(ctx, node->checkbox.text, text_x, text_y,
                              aui_apply_opacity(cb_text_color, effective_opacity));
            }
            break;

        case AUI_NODE_TEXTBOX:
            {
                float x = node->global_rect.x;
                float y = node->global_rect.y;
                float w = node->global_rect.w;
                float h = node->global_rect.h;

                /* Background - highlight when focused */
                uint32_t bg = ctx->theme.bg_widget;
                if (node->focused) {
                    bg = ctx->theme.bg_widget_hover;
                } else if (node->hovered) {
                    bg = ctx->theme.bg_widget_hover;
                }
                aui_draw_rect_rounded(ctx, x, y, w, h,
                                      aui_apply_opacity(bg, effective_opacity),
                                      style.corner_radius.top_left);

                /* Border - accent color when focused */
                uint32_t border = node->focused ? ctx->theme.accent : ctx->theme.border;
                aui_draw_rect_outline(ctx, x, y, w, h,
                                      aui_apply_opacity(border, effective_opacity), 1.0f);

                /* Text content area */
                float padding = 6.0f;
                float text_x = x + padding;
                float text_y = y + (h - aui_text_height(ctx)) / 2;

                /* Determine display text */
                const char *display_text = node->textbox.buffer;
                uint32_t text_color = style.text_color;

                /* Show placeholder if empty and not focused */
                bool show_placeholder = (!display_text || display_text[0] == '\0') && !node->focused;
                if (show_placeholder && node->textbox.placeholder[0] != '\0') {
                    display_text = node->textbox.placeholder;
                    text_color = ctx->theme.text_disabled;
                }

                /* Clip text to widget bounds */
                aui_push_scissor(ctx, x + padding, y, w - padding * 2, h);

                if (display_text && display_text[0]) {
                    aui_draw_text(ctx, display_text, text_x, text_y,
                                  aui_apply_opacity(text_color, effective_opacity));
                }

                /* Draw cursor if focused */
                if (node->focused && node->textbox.buffer) {
                    int cursor_pos = node->textbox.cursor_pos;
                    float cursor_x = text_x;

                    if (cursor_pos > 0) {
                        /* Calculate cursor position by measuring text up to cursor */
                        char temp[256];
                        int copy_len = cursor_pos < 255 ? cursor_pos : 255;
                        strncpy(temp, node->textbox.buffer, copy_len);
                        temp[copy_len] = '\0';
                        cursor_x = text_x + aui_text_width(ctx, temp);
                    }

                    /* Draw cursor line */
                    aui_draw_rect(ctx, cursor_x, y + 4, 2, h - 8,
                                  aui_apply_opacity(style.text_color, effective_opacity));
                }

                aui_pop_scissor(ctx);
            }
            break;

        case AUI_NODE_PANEL:
            {
                float x = node->global_rect.x;
                float y = node->global_rect.y;
                float w = node->global_rect.w;
                float h = node->global_rect.h;
                float title_h = ctx->theme.widget_height;
                float btn_size = title_h - 8;
                float btn_padding = 4;
                bool has_title = node->panel.title[0] != '\0';

                /* Panel background */
                aui_draw_rect_rounded(ctx, x, y, w, h,
                                      aui_apply_opacity(ctx->theme.bg_panel, effective_opacity),
                                      style.corner_radius.top_left);

                /* Title bar if title is set */
                if (has_title) {
                    /* Title bar background (slightly darker) */
                    uint32_t title_bg = ctx->theme.bg_widget;
                    aui_draw_rect_rounded(ctx, x, y, w, title_h,
                                          aui_apply_opacity(title_bg, effective_opacity),
                                          style.corner_radius.top_left);

                    /* Title text */
                    float text_x = x + ctx->theme.padding;
                    float text_y = y + (title_h - aui_text_height(ctx)) / 2;
                    aui_draw_text(ctx, node->panel.title, text_x, text_y,
                                  aui_apply_opacity(ctx->theme.text, effective_opacity));

                    /* Buttons on right side */
                    float btn_x = x + w - btn_padding - btn_size;

                    /* Close button (X) */
                    if (node->panel.closable) {
                        float btn_y = y + (title_h - btn_size) / 2;
                        uint32_t btn_bg = ctx->theme.bg_widget_hover;
                        aui_draw_rect_rounded(ctx, btn_x, btn_y, btn_size, btn_size,
                                              aui_apply_opacity(btn_bg, effective_opacity), 3.0f);
                        /* Draw X */
                        float cx = btn_x + btn_size / 2;
                        float cy = btn_y + btn_size / 2;
                        float cross_size = btn_size * 0.3f;
                        aui_draw_line(ctx, cx - cross_size, cy - cross_size,
                                      cx + cross_size, cy + cross_size,
                                      aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                        aui_draw_line(ctx, cx + cross_size, cy - cross_size,
                                      cx - cross_size, cy + cross_size,
                                      aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                        btn_x -= btn_size + btn_padding;
                    }

                    /* Collapse button (v or >) */
                    if (node->panel.collapsible) {
                        float btn_y = y + (title_h - btn_size) / 2;
                        uint32_t btn_bg = ctx->theme.bg_widget_hover;
                        aui_draw_rect_rounded(ctx, btn_x, btn_y, btn_size, btn_size,
                                              aui_apply_opacity(btn_bg, effective_opacity), 3.0f);
                        /* Draw arrow (v when expanded, > when collapsed) */
                        float cx = btn_x + btn_size / 2;
                        float cy = btn_y + btn_size / 2;
                        float arrow_size = btn_size * 0.25f;
                        if (node->panel.collapsed) {
                            /* > arrow */
                            aui_draw_line(ctx, cx - arrow_size, cy - arrow_size,
                                          cx + arrow_size, cy,
                                          aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                            aui_draw_line(ctx, cx + arrow_size, cy,
                                          cx - arrow_size, cy + arrow_size,
                                          aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                        } else {
                            /* v arrow */
                            aui_draw_line(ctx, cx - arrow_size, cy - arrow_size / 2,
                                          cx, cy + arrow_size / 2,
                                          aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                            aui_draw_line(ctx, cx, cy + arrow_size / 2,
                                          cx + arrow_size, cy - arrow_size / 2,
                                          aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                        }
                    }

                    /* Separator line below title */
                    aui_draw_line(ctx, x, y + title_h, x + w, y + title_h,
                                  aui_apply_opacity(ctx->theme.border, effective_opacity), 1.0f);
                }

                /* Draw border */
                if (style.border.width.top > 0) {
                    aui_draw_rect_outline(ctx, x, y, w, h,
                                          aui_apply_opacity(style.border.color, effective_opacity),
                                          style.border.width.top);
                }
            }
            break;

        case AUI_NODE_COLLAPSING_HEADER:
            {
                float x = node->global_rect.x;
                float y = node->global_rect.y;
                float w = node->global_rect.w;
                float header_h = ctx->theme.widget_height;

                /* Header background */
                uint32_t bg = node->hovered ? ctx->theme.bg_widget_hover : ctx->theme.bg_widget;
                aui_draw_rect_rounded(ctx, x, y, w, header_h,
                                      aui_apply_opacity(bg, effective_opacity),
                                      style.corner_radius.top_left);

                /* Arrow indicator */
                if (node->collapsing_header.show_arrow) {
                    float arrow_size = 8.0f;
                    float arrow_x = x + 12.0f;
                    float arrow_y = y + header_h / 2;

                    if (node->collapsing_header.expanded) {
                        /* v arrow (expanded) */
                        aui_draw_line(ctx, arrow_x - arrow_size/2, arrow_y - arrow_size/4,
                                      arrow_x, arrow_y + arrow_size/4,
                                      aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                        aui_draw_line(ctx, arrow_x, arrow_y + arrow_size/4,
                                      arrow_x + arrow_size/2, arrow_y - arrow_size/4,
                                      aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                    } else {
                        /* > arrow (collapsed) */
                        aui_draw_line(ctx, arrow_x - arrow_size/4, arrow_y - arrow_size/2,
                                      arrow_x + arrow_size/4, arrow_y,
                                      aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                        aui_draw_line(ctx, arrow_x + arrow_size/4, arrow_y,
                                      arrow_x - arrow_size/4, arrow_y + arrow_size/2,
                                      aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                    }
                }

                /* Header text */
                float text_x = x + (node->collapsing_header.show_arrow ? 28.0f : ctx->theme.padding);
                float text_y = y + (header_h - aui_text_height(ctx)) / 2;
                aui_draw_text(ctx, node->collapsing_header.text, text_x, text_y,
                              aui_apply_opacity(ctx->theme.text, effective_opacity));
            }
            break;

        case AUI_NODE_SPLITTER:
            {
                /* Draw splitter bar between children */
                AUI_Node *first = node->first_child;
                if (!first) break;

                float splitter_w = node->splitter.splitter_width;
                uint32_t splitter_color = node->splitter.dragging ?
                    ctx->theme.accent : ctx->theme.border;

                if (node->splitter.horizontal) {
                    float bar_x = first->global_rect.x + first->global_rect.w;
                    float bar_y = node->global_rect.y;
                    float bar_h = node->global_rect.h;

                    aui_draw_rect(ctx, bar_x, bar_y, splitter_w, bar_h,
                                  aui_apply_opacity(splitter_color, effective_opacity));

                    /* Draw grab handle (3 dots) */
                    float cx = bar_x + splitter_w / 2;
                    float cy = bar_y + bar_h / 2;
                    for (int i = -1; i <= 1; i++) {
                        aui_draw_rect(ctx, cx - 1, cy + i * 8 - 1, 3, 3,
                                      aui_apply_opacity(ctx->theme.text_disabled, effective_opacity));
                    }
                } else {
                    float bar_x = node->global_rect.x;
                    float bar_y = first->global_rect.y + first->global_rect.h;
                    float bar_w = node->global_rect.w;

                    aui_draw_rect(ctx, bar_x, bar_y, bar_w, splitter_w,
                                  aui_apply_opacity(splitter_color, effective_opacity));

                    /* Draw grab handle (3 dots) */
                    float cx = bar_x + bar_w / 2;
                    float cy = bar_y + splitter_w / 2;
                    for (int i = -1; i <= 1; i++) {
                        aui_draw_rect(ctx, cx + i * 8 - 1, cy - 1, 3, 3,
                                      aui_apply_opacity(ctx->theme.text_disabled, effective_opacity));
                    }
                }
            }
            break;

        case AUI_NODE_TREE:
            {
                float x = node->global_rect.x;
                float y = node->global_rect.y;
                float w = node->global_rect.w;
                float h = node->global_rect.h;
                float item_h = node->tree.item_height;
                float indent = node->tree.indent_width;

                /* Background */
                aui_draw_rect_rounded(ctx, x, y, w, h,
                                      aui_apply_opacity(ctx->theme.bg_widget, effective_opacity),
                                      style.corner_radius.top_left);

                /* Border */
                aui_draw_rect_outline(ctx, x, y, w, h,
                                      aui_apply_opacity(ctx->theme.border, effective_opacity), 1.0f);

                /* Clip tree content */
                aui_push_scissor(ctx, x, y, w, h);

                /* Helper function to render tree items recursively */
                float current_y = y - node->tree.scroll_offset;

                /* Recursive lambda for rendering items */
                std::function<void(AUI_TreeItem*, int)> render_item;
                render_item = [&](AUI_TreeItem *item, int depth) {
                    while (item) {
                        /* Only render if visible */
                        if (current_y + item_h > y && current_y < y + h) {
                            float item_x = x + depth * indent;
                            float arrow_size = 8.0f;
                            float arrow_offset = 12.0f;

                            /* Selection highlight */
                            if (item->selected) {
                                aui_draw_rect(ctx, x, current_y, w, item_h,
                                              aui_apply_opacity(ctx->theme.accent, effective_opacity * 0.3f));
                            }

                            /* Draw expand/collapse arrow if has children */
                            bool has_children = item->first_child != NULL;
                            if (has_children) {
                                float ax = item_x + arrow_offset;
                                float ay = current_y + item_h / 2;

                                if (item->expanded) {
                                    /* v arrow */
                                    aui_draw_line(ctx, ax - arrow_size/2, ay - arrow_size/4,
                                                  ax, ay + arrow_size/4,
                                                  aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                                    aui_draw_line(ctx, ax, ay + arrow_size/4,
                                                  ax + arrow_size/2, ay - arrow_size/4,
                                                  aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                                } else {
                                    /* > arrow */
                                    aui_draw_line(ctx, ax - arrow_size/4, ay - arrow_size/2,
                                                  ax + arrow_size/4, ay,
                                                  aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                                    aui_draw_line(ctx, ax + arrow_size/4, ay,
                                                  ax - arrow_size/4, ay + arrow_size/2,
                                                  aui_apply_opacity(ctx->theme.text, effective_opacity), 2.0f);
                                }
                            }

                            /* Draw item text */
                            float text_x = item_x + (has_children ? 28.0f : 8.0f);
                            float text_y = current_y + (item_h - aui_text_height(ctx)) / 2;

                            /* Dim the item being dragged */
                            float text_opacity = effective_opacity;
                            if (node->tree.drag_started && item == node->tree.dragging_item) {
                                text_opacity *= 0.5f;
                            }
                            aui_draw_text(ctx, item->text, text_x, text_y,
                                          aui_apply_opacity(ctx->theme.text, text_opacity));

                            /* Draw drop indicator for reorder */
                            if (node->tree.drag_started && item == node->tree.drop_target) {
                                uint32_t indicator_color = ctx->theme.accent;
                                float indicator_thickness = 2.0f;

                                switch (node->tree.drop_pos) {
                                    case AUI_TREE_DROP_BEFORE:
                                        /* Line above item */
                                        aui_draw_rect(ctx, x + depth * indent, current_y,
                                                      w - depth * indent, indicator_thickness,
                                                      aui_apply_opacity(indicator_color, effective_opacity));
                                        break;
                                    case AUI_TREE_DROP_AFTER:
                                        /* Line below item */
                                        aui_draw_rect(ctx, x + depth * indent, current_y + item_h - indicator_thickness,
                                                      w - depth * indent, indicator_thickness,
                                                      aui_apply_opacity(indicator_color, effective_opacity));
                                        break;
                                    case AUI_TREE_DROP_INTO:
                                        /* Highlight entire item as drop target */
                                        aui_draw_rect_outline(ctx, x + depth * indent, current_y,
                                                              w - depth * indent, item_h,
                                                              aui_apply_opacity(indicator_color, effective_opacity),
                                                              indicator_thickness);
                                        break;
                                    default:
                                        break;
                                }
                            }
                        }

                        current_y += item_h;

                        /* Render children if expanded */
                        if (item->expanded && item->first_child) {
                            render_item(item->first_child, depth + 1);
                        }

                        item = item->next_sibling;
                    }
                };

                /* Start rendering from root items */
                render_item(node->tree.root_items, 0);

                aui_pop_scissor(ctx);
            }
            break;

        case AUI_NODE_TEXTURE_RECT:
            {
                if (!node->texture_rect.texture) break;

                float x = node->global_rect.x;
                float y = node->global_rect.y;
                float w = node->global_rect.w;
                float h = node->global_rect.h;

                /* Source region */
                float src_x = node->texture_rect.src_x;
                float src_y = node->texture_rect.src_y;
                float src_w = node->texture_rect.src_w;
                float src_h = node->texture_rect.src_h;

                /* Maintain aspect ratio if not stretching */
                if (!node->texture_rect.stretch && src_w > 0 && src_h > 0) {
                    float src_aspect = src_w / src_h;
                    float dst_aspect = w / h;
                    if (src_aspect > dst_aspect) {
                        float new_h = w / src_aspect;
                        y += (h - new_h) * 0.5f;
                        h = new_h;
                    } else {
                        float new_w = h * src_aspect;
                        x += (w - new_w) * 0.5f;
                        w = new_w;
                    }
                }

                uint32_t tint = aui_apply_opacity(node->texture_rect.tint, effective_opacity);
                aui_draw_textured_rect(ctx, node->texture_rect.texture,
                                       x, y, w, h,
                                       src_x, src_y, src_w, src_h,
                                       tint,
                                       node->texture_rect.flip_h,
                                       node->texture_rect.flip_v);
            }
            break;

        case AUI_NODE_ICON:
            {
                if (!node->icon.texture) break;

                float size = node->icon.size > 0 ? node->icon.size : node->icon.icon_w;
                float x = node->global_rect.x + (node->global_rect.w - size) * 0.5f;
                float y = node->global_rect.y + (node->global_rect.h - size) * 0.5f;

                uint32_t color = aui_apply_opacity(node->icon.color, effective_opacity);
                aui_draw_textured_rect(ctx, node->icon.texture,
                                       x, y, size, size,
                                       node->icon.icon_x, node->icon.icon_y,
                                       node->icon.icon_w, node->icon.icon_h,
                                       color, false, false);
            }
            break;

        case AUI_NODE_SEPARATOR:
            {
                uint32_t color = node->separator.color;
                if (color == 0) {
                    color = ctx->theme.border;
                }
                color = aui_apply_opacity(color, effective_opacity);

                float thickness = node->separator.thickness > 0 ?
                                  node->separator.thickness : 1.0f;

                if (node->separator.vertical) {
                    float x = node->global_rect.x + (node->global_rect.w - thickness) * 0.5f;
                    aui_draw_rect(ctx, x, node->global_rect.y,
                                  thickness, node->global_rect.h, color);
                } else {
                    float y = node->global_rect.y + (node->global_rect.h - thickness) * 0.5f;
                    aui_draw_rect(ctx, node->global_rect.x, y,
                                  node->global_rect.w, thickness, color);
                }
            }
            break;

        case AUI_NODE_RICHTEXT:
            {
                AUI_RichText *rt = (AUI_RichText *)node->custom_data;
                if (rt) {
                    float x = node->global_rect.x + style.padding.left;
                    float y = node->global_rect.y + style.padding.top;
                    float max_w = node->global_rect.w - style.padding.left - style.padding.right;

                    /* Ensure layout is calculated with current width and font metrics */
                    aui_richtext_layout_ctx(ctx, rt, max_w);

                    /* Update animation if any animated tags */
                    aui_richtext_update(rt, ctx->delta_time);

                    /* Draw the rich text */
                    aui_richtext_draw(ctx, rt, x, y);
                }
            }
            break;

        case AUI_NODE_CHART:
            {
                AUI_ChartNodeData *chart_data = (AUI_ChartNodeData *)node->custom_data;
                if (chart_data) {
                    /* Update animation progress */
                    if (chart_data->config.animated && chart_data->state.anim_progress < 1.0f) {
                        float duration = chart_data->config.animation_duration > 0 ?
                                         chart_data->config.animation_duration : 0.5f;
                        chart_data->state.anim_progress += ctx->delta_time / duration;
                        if (chart_data->state.anim_progress > 1.0f) {
                            chart_data->state.anim_progress = 1.0f;
                        }
                    }

                    /* Update hover state */
                    if (node->hovered) {
                        chart_data->state.hover_x = ctx->input.mouse_x;
                        chart_data->state.hover_y = ctx->input.mouse_y;
                        chart_data->state.tooltip_visible = true;
                        /* TODO: Hit test to find hovered_series and hovered_index */
                    } else {
                        chart_data->state.tooltip_visible = false;
                        chart_data->state.hovered_series = -1;
                        chart_data->state.hovered_index = -1;
                    }

                    /* Draw the chart */
                    AUI_Rect bounds = {
                        node->global_rect.x,
                        node->global_rect.y,
                        node->global_rect.w,
                        node->global_rect.h
                    };
                    aui_draw_chart_ex(ctx, bounds, &chart_data->config, &chart_data->state);
                }
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
        aui_push_scissor(ctx, node->global_rect.x, node->global_rect.y,
                          node->global_rect.w, node->global_rect.h);
    }

    for (AUI_Node *child = node->first_child; child; child = child->next_sibling) {
        aui_node_render_recursive(ctx, child, effective_opacity);
    }

    if (node->clip_contents) {
        aui_pop_scissor(ctx);
    }
}

/* ============================================================================
 * Hit Testing
 * ============================================================================ */

AUI_Node *aui_node_hit_test(AUI_Node *root, float x, float y)
{
    if (!root || !root->visible) return NULL;

    /* Check children in reverse order (front to back) */
    for (AUI_Node *child = root->last_child; child; child = child->prev_sibling) {
        AUI_Node *hit = aui_node_hit_test(child, x, y);
        if (hit) return hit;
    }

    /* Check this node */
    if (!root->mouse_filter_ignore && aui_node_contains_point(root, x, y)) {
        return root;
    }

    return NULL;
}

bool aui_node_contains_point(AUI_Node *node, float x, float y)
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

AUI_Node *aui_label_create(AUI_Context *ctx, const char *name, const char *text)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_LABEL, name);
    if (node && text) {
        strncpy(node->label.text, text, sizeof(node->label.text) - 1);
    }
    return node;
}

AUI_Node *aui_button_create(AUI_Context *ctx, const char *name, const char *text)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_BUTTON, name);
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

AUI_Node *aui_vbox_create(AUI_Context *ctx, const char *name)
{
    return aui_node_create(ctx, AUI_NODE_VBOX, name);
}

AUI_Node *aui_hbox_create(AUI_Context *ctx, const char *name)
{
    return aui_node_create(ctx, AUI_NODE_HBOX, name);
}

AUI_Node *aui_grid_create(AUI_Context *ctx, const char *name, int columns)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_GRID, name);
    if (node) {
        node->grid.columns = columns > 0 ? columns : 2;
    }
    return node;
}

AUI_Node *aui_margin_create(AUI_Context *ctx, const char *name)
{
    return aui_node_create(ctx, AUI_NODE_MARGIN, name);
}

AUI_Node *aui_center_create(AUI_Context *ctx, const char *name)
{
    return aui_node_create(ctx, AUI_NODE_CENTER, name);
}

AUI_Node *aui_scroll_create(AUI_Context *ctx, const char *name)
{
    return aui_node_create(ctx, AUI_NODE_SCROLL, name);
}

AUI_Node *aui_panel_create(AUI_Context *ctx, const char *name, const char *title)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_PANEL, name);
    if (node) {
        node->clip_contents = true;  /* Panels clip their contents by default */
        if (title) {
            strncpy(node->panel.title, title, sizeof(node->panel.title) - 1);
        }
    }
    return node;
}

AUI_Node *aui_textbox_create(AUI_Context *ctx, const char *name,
                              char *buffer, int buffer_size)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_TEXTBOX, name);
    if (node) {
        node->textbox.buffer = buffer;
        node->textbox.buffer_size = buffer_size;
        node->textbox.cursor_pos = buffer ? (int)strlen(buffer) : 0;
        node->custom_min_size_x = 100;
        node->custom_min_size_y = 28;
    }
    return node;
}

AUI_Node *aui_checkbox_create(AUI_Context *ctx, const char *name,
                               const char *text, bool *value)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_CHECKBOX, name);
    if (node) {
        if (text) {
            strncpy(node->checkbox.text, text, sizeof(node->checkbox.text) - 1);
        }
        if (value) {
            node->checkbox.checked = *value;
        }
    }
    return node;
}

AUI_Node *aui_slider_create(AUI_Context *ctx, const char *name,
                             float min_val, float max_val, float *value)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_SLIDER, name);
    if (node) {
        node->slider.min_value = min_val;
        node->slider.max_value = max_val;
        if (value) {
            node->slider.value = *value;
        }
    }
    return node;
}

AUI_Node *aui_collapsing_header_create(AUI_Context *ctx, const char *name,
                                        const char *text)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_COLLAPSING_HEADER, name);
    if (node && text) {
        strncpy(node->collapsing_header.text, text,
                sizeof(node->collapsing_header.text) - 1);
        node->collapsing_header.text[sizeof(node->collapsing_header.text) - 1] = '\0';
    }
    return node;
}

AUI_Node *aui_splitter_create(AUI_Context *ctx, const char *name, bool horizontal)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_SPLITTER, name);
    if (node) {
        node->splitter.horizontal = horizontal;
    }
    return node;
}

/* ============================================================================
 * Container-Specific Functions
 * ============================================================================ */

void aui_box_set_separation(AUI_Node *node, float separation)
{
    if (!node) return;
    if (node->type == AUI_NODE_VBOX || node->type == AUI_NODE_HBOX) {
        node->box.separation = separation;
        node->layout_dirty = true;
    }
}

void aui_box_set_alignment(AUI_Node *node, AUI_SizeFlags alignment)
{
    if (!node) return;
    if (node->type == AUI_NODE_VBOX || node->type == AUI_NODE_HBOX) {
        node->box.alignment = alignment;
        node->layout_dirty = true;
    }
}

void aui_grid_set_columns(AUI_Node *node, int columns)
{
    if (!node || node->type != AUI_NODE_GRID) return;
    node->grid.columns = columns > 0 ? columns : 1;
    node->layout_dirty = true;
}

void aui_grid_set_h_separation(AUI_Node *node, float separation)
{
    if (!node || node->type != AUI_NODE_GRID) return;
    node->grid.h_separation = separation;
    node->layout_dirty = true;
}

void aui_grid_set_v_separation(AUI_Node *node, float separation)
{
    if (!node || node->type != AUI_NODE_GRID) return;
    node->grid.v_separation = separation;
    node->layout_dirty = true;
}

void aui_margin_set_margins(AUI_Node *node, float left, float top,
                             float right, float bottom)
{
    if (!node) return;
    node->style.padding = aui_edges(top, right, bottom, left);
    node->layout_dirty = true;
}

void aui_scroll_set_h_scroll_enabled(AUI_Node *node, bool enabled)
{
    if (!node || node->type != AUI_NODE_SCROLL) return;
    node->scroll.h_scroll_enabled = enabled;
}

void aui_scroll_set_v_scroll_enabled(AUI_Node *node, bool enabled)
{
    if (!node || node->type != AUI_NODE_SCROLL) return;
    node->scroll.v_scroll_enabled = enabled;
}

void aui_scroll_set_scroll(AUI_Node *node, float x, float y)
{
    if (!node || node->type != AUI_NODE_SCROLL) return;
    node->scroll.scroll_x = x;
    node->scroll.scroll_y = y;
}

void aui_scroll_ensure_visible(AUI_Node *node, AUI_Rect rect)
{
    if (!node || node->type != AUI_NODE_SCROLL) return;

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

void aui_label_set_text(AUI_Node *node, const char *text)
{
    if (!node || node->type != AUI_NODE_LABEL) return;
    if (text) {
        strncpy(node->label.text, text, sizeof(node->label.text) - 1);
    } else {
        node->label.text[0] = '\0';
    }
}

const char *aui_label_get_text(AUI_Node *node)
{
    if (!node || node->type != AUI_NODE_LABEL) return "";
    return node->label.text;
}

void aui_button_set_text(AUI_Node *node, const char *text)
{
    if (!node || node->type != AUI_NODE_BUTTON) return;
    if (text) {
        strncpy(node->button.text, text, sizeof(node->button.text) - 1);
    } else {
        node->button.text[0] = '\0';
    }
}

void aui_button_set_disabled(AUI_Node *node, bool disabled)
{
    if (!node || node->type != AUI_NODE_BUTTON) return;
    node->button.disabled = disabled;
    node->enabled = !disabled;
}

void aui_button_set_toggle_mode(AUI_Node *node, bool toggle)
{
    if (!node || node->type != AUI_NODE_BUTTON) return;
    node->button.toggle_mode = toggle;
}

bool aui_button_is_toggled(AUI_Node *node)
{
    if (!node || node->type != AUI_NODE_BUTTON) return false;
    return node->button.toggled;
}

void aui_checkbox_set_checked(AUI_Node *node, bool checked)
{
    if (!node || node->type != AUI_NODE_CHECKBOX) return;
    node->checkbox.checked = checked;
}

bool aui_checkbox_is_checked(AUI_Node *node)
{
    if (!node || node->type != AUI_NODE_CHECKBOX) return false;
    return node->checkbox.checked;
}

void aui_slider_set_value(AUI_Node *node, float value)
{
    if (!node || node->type != AUI_NODE_SLIDER) return;
    node->slider.value = fmaxf(node->slider.min_value,
                                fminf(node->slider.max_value, value));
}

float aui_slider_get_value(AUI_Node *node)
{
    if (!node || node->type != AUI_NODE_SLIDER) return 0;
    return node->slider.value;
}

void aui_slider_set_range(AUI_Node *node, float min, float max)
{
    if (!node || node->type != AUI_NODE_SLIDER) return;
    node->slider.min_value = min;
    node->slider.max_value = max;
    node->slider.value = fmaxf(min, fminf(max, node->slider.value));
}

void aui_slider_set_step(AUI_Node *node, float step)
{
    if (!node || node->type != AUI_NODE_SLIDER) return;
    node->slider.step = step;
}

void aui_textbox_set_text(AUI_Node *node, const char *text)
{
    if (!node || node->type != AUI_NODE_TEXTBOX || !node->textbox.buffer) return;
    if (text) {
        strncpy(node->textbox.buffer, text, node->textbox.buffer_size - 1);
        node->textbox.buffer[node->textbox.buffer_size - 1] = '\0';
    } else {
        node->textbox.buffer[0] = '\0';
    }
    node->textbox.cursor_pos = (int)strlen(node->textbox.buffer);
}

const char *aui_textbox_get_text(AUI_Node *node)
{
    if (!node || node->type != AUI_NODE_TEXTBOX || !node->textbox.buffer) return "";
    return node->textbox.buffer;
}

void aui_textbox_set_placeholder(AUI_Node *node, const char *placeholder)
{
    if (!node || node->type != AUI_NODE_TEXTBOX) return;
    if (placeholder) {
        strncpy(node->textbox.placeholder, placeholder,
                sizeof(node->textbox.placeholder) - 1);
    } else {
        node->textbox.placeholder[0] = '\0';
    }
}

void aui_dropdown_set_items(AUI_Node *node, const char **items, int count)
{
    if (!node || node->type != AUI_NODE_DROPDOWN) return;
    node->dropdown.items = items;
    node->dropdown.item_count = count;
    if (node->dropdown.selected >= count) {
        node->dropdown.selected = count > 0 ? 0 : -1;
    }
}

void aui_dropdown_set_selected(AUI_Node *node, int index)
{
    if (!node || node->type != AUI_NODE_DROPDOWN) return;
    if (index >= 0 && index < node->dropdown.item_count) {
        node->dropdown.selected = index;
    }
}

int aui_dropdown_get_selected(AUI_Node *node)
{
    if (!node || node->type != AUI_NODE_DROPDOWN) return -1;
    return node->dropdown.selected;
}

void aui_progress_set_value(AUI_Node *node, float value)
{
    if (!node || node->type != AUI_NODE_PROGRESS_BAR) return;
    node->progress.value = fmaxf(node->progress.min_value,
                                  fminf(node->progress.max_value, value));
}

void aui_progress_set_range(AUI_Node *node, float min, float max)
{
    if (!node || node->type != AUI_NODE_PROGRESS_BAR) return;
    node->progress.min_value = min;
    node->progress.max_value = max;
    node->progress.value = fmaxf(min, fminf(max, node->progress.value));
}

/* ============================================================================
 * Panel Functions
 * ============================================================================ */

void aui_panel_set_title(AUI_Node *node, const char *title)
{
    if (!node || node->type != AUI_NODE_PANEL) return;
    if (title) {
        strncpy(node->panel.title, title, sizeof(node->panel.title) - 1);
        node->panel.title[sizeof(node->panel.title) - 1] = '\0';
    } else {
        node->panel.title[0] = '\0';
    }
    node->layout_dirty = true;
}

void aui_panel_set_closable(AUI_Node *node, bool closable)
{
    if (!node || node->type != AUI_NODE_PANEL) return;
    node->panel.closable = closable;
}

void aui_panel_set_collapsible(AUI_Node *node, bool collapsible)
{
    if (!node || node->type != AUI_NODE_PANEL) return;
    node->panel.collapsible = collapsible;
}

bool aui_panel_is_collapsed(AUI_Node *node)
{
    if (!node || node->type != AUI_NODE_PANEL) return false;
    return node->panel.collapsed;
}

void aui_panel_set_collapsed(AUI_Node *node, bool collapsed)
{
    if (!node || node->type != AUI_NODE_PANEL) return;
    node->panel.collapsed = collapsed;
    node->layout_dirty = true;
}

bool aui_panel_is_closed(AUI_Node *node)
{
    if (!node || node->type != AUI_NODE_PANEL) return false;
    return node->panel.closed;
}

/* ============================================================================
 * Collapsing Header Functions
 * ============================================================================ */

void aui_collapsing_header_set_text(AUI_Node *node, const char *text)
{
    if (!node || node->type != AUI_NODE_COLLAPSING_HEADER) return;
    if (text) {
        strncpy(node->collapsing_header.text, text,
                sizeof(node->collapsing_header.text) - 1);
        node->collapsing_header.text[sizeof(node->collapsing_header.text) - 1] = '\0';
    } else {
        node->collapsing_header.text[0] = '\0';
    }
}

void aui_collapsing_header_set_expanded(AUI_Node *node, bool expanded)
{
    if (!node || node->type != AUI_NODE_COLLAPSING_HEADER) return;
    node->collapsing_header.expanded = expanded;
    node->layout_dirty = true;
}

bool aui_collapsing_header_is_expanded(AUI_Node *node)
{
    if (!node || node->type != AUI_NODE_COLLAPSING_HEADER) return false;
    return node->collapsing_header.expanded;
}

/* ============================================================================
 * Splitter Functions
 * ============================================================================ */

void aui_splitter_set_ratio(AUI_Node *node, float ratio)
{
    if (!node || node->type != AUI_NODE_SPLITTER) return;
    node->splitter.split_ratio = fmaxf(0.0f, fminf(1.0f, ratio));
    node->layout_dirty = true;
}

float aui_splitter_get_ratio(AUI_Node *node)
{
    if (!node || node->type != AUI_NODE_SPLITTER) return 0.5f;
    return node->splitter.split_ratio;
}

void aui_splitter_set_min_sizes(AUI_Node *node, float first, float second)
{
    if (!node || node->type != AUI_NODE_SPLITTER) return;
    node->splitter.min_size_first = fmaxf(0.0f, first);
    node->splitter.min_size_second = fmaxf(0.0f, second);
}

void aui_splitter_set_width(AUI_Node *node, float width)
{
    if (!node || node->type != AUI_NODE_SPLITTER) return;
    node->splitter.splitter_width = fmaxf(2.0f, width);
    node->layout_dirty = true;
}

/* ============================================================================
 * Tree Widget Functions
 * ============================================================================ */

AUI_Node *aui_tree_create(AUI_Context *ctx, const char *name)
{
    return aui_node_create(ctx, AUI_NODE_TREE, name);
}

/* ============================================================================
 * Texture Rect Widget Functions
 * ============================================================================ */

AUI_Node *aui_texture_rect_create(AUI_Context *ctx, const char *name,
                                   SDL_GPUTexture *texture)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_TEXTURE_RECT, name);
    if (node) {
        node->texture_rect.texture = texture;
        node->texture_rect.src_x = 0;
        node->texture_rect.src_y = 0;
        node->texture_rect.src_w = 0;  /* 0 = use full texture */
        node->texture_rect.src_h = 0;
        node->texture_rect.tint = 0xFFFFFFFF;  /* No tint */
        node->texture_rect.stretch = true;
        node->texture_rect.flip_h = false;
        node->texture_rect.flip_v = false;
    }
    return node;
}

void aui_texture_rect_set_region(AUI_Node *node, float x, float y, float w, float h)
{
    if (!node || node->type != AUI_NODE_TEXTURE_RECT) return;
    node->texture_rect.src_x = x;
    node->texture_rect.src_y = y;
    node->texture_rect.src_w = w;
    node->texture_rect.src_h = h;
}

void aui_texture_rect_set_tint(AUI_Node *node, uint32_t color)
{
    if (!node || node->type != AUI_NODE_TEXTURE_RECT) return;
    node->texture_rect.tint = color;
}

void aui_texture_rect_set_stretch(AUI_Node *node, bool stretch)
{
    if (!node || node->type != AUI_NODE_TEXTURE_RECT) return;
    node->texture_rect.stretch = stretch;
}

void aui_texture_rect_set_flip(AUI_Node *node, bool flip_h, bool flip_v)
{
    if (!node || node->type != AUI_NODE_TEXTURE_RECT) return;
    node->texture_rect.flip_h = flip_h;
    node->texture_rect.flip_v = flip_v;
}

/* ============================================================================
 * Icon Widget Functions
 * ============================================================================ */

AUI_Node *aui_icon_create(AUI_Context *ctx, const char *name,
                           SDL_GPUTexture *atlas, float x, float y, float w, float h)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_ICON, name);
    if (node) {
        node->icon.texture = atlas;
        node->icon.icon_x = x;
        node->icon.icon_y = y;
        node->icon.icon_w = w;
        node->icon.icon_h = h;
        node->icon.color = 0xFFFFFFFF;  /* White/no tint */
        node->icon.size = 0;  /* Use original size */

        /* Set minimum size to icon size */
        node->custom_min_size_x = w;
        node->custom_min_size_y = h;
    }
    return node;
}

void aui_icon_set_color(AUI_Node *node, uint32_t color)
{
    if (!node || node->type != AUI_NODE_ICON) return;
    node->icon.color = color;
}

void aui_icon_set_size(AUI_Node *node, float size)
{
    if (!node || node->type != AUI_NODE_ICON) return;
    node->icon.size = size;
    if (size > 0) {
        node->custom_min_size_x = size;
        node->custom_min_size_y = size;
    }
}

/* ============================================================================
 * Separator Widget Functions
 * ============================================================================ */

AUI_Node *aui_separator_create(AUI_Context *ctx, const char *name, bool vertical)
{
    AUI_Node *node = aui_node_create(ctx, AUI_NODE_SEPARATOR, name);
    if (node) {
        node->separator.vertical = vertical;
        node->separator.color = 0;  /* 0 = use theme border color */
        node->separator.thickness = 1.0f;

        /* Set size flags for proper layout */
        if (vertical) {
            node->custom_min_size_x = 1.0f;
            node->h_size_flags = AUI_SIZE_SHRINK_CENTER;
            node->v_size_flags = AUI_SIZE_FILL;
        } else {
            node->custom_min_size_y = 1.0f;
            node->h_size_flags = AUI_SIZE_FILL;
            node->v_size_flags = AUI_SIZE_SHRINK_CENTER;
        }
    }
    return node;
}

void aui_separator_set_color(AUI_Node *node, uint32_t color)
{
    if (!node || node->type != AUI_NODE_SEPARATOR) return;
    node->separator.color = color;
}

void aui_separator_set_thickness(AUI_Node *node, float thickness)
{
    if (!node || node->type != AUI_NODE_SEPARATOR) return;
    node->separator.thickness = thickness;
    if (node->separator.vertical) {
        node->custom_min_size_x = thickness;
    } else {
        node->custom_min_size_y = thickness;
    }
}

/* ============================================================================
 * Tree Item Functions
 * ============================================================================ */

static void aui_tree_item_free_recursive(AUI_TreeItem *item)
{
    while (item) {
        AUI_TreeItem *next = item->next_sibling;
        if (item->first_child) {
            aui_tree_item_free_recursive(item->first_child);
        }
        free(item);
        item = next;
    }
}

AUI_TreeItem *aui_tree_add_item(AUI_Node *tree, const char *text, void *user_data)
{
    if (!tree || tree->type != AUI_NODE_TREE) return NULL;

    AUI_TreeItem *item = (AUI_TreeItem*)calloc(1, sizeof(AUI_TreeItem));
    if (!item) return NULL;

    item->id = tree->tree.next_item_id++;
    if (text) {
        strncpy(item->text, text, sizeof(item->text) - 1);
    }
    item->user_data = user_data;
    item->expanded = true;

    /* Add to end of root items list */
    if (!tree->tree.root_items) {
        tree->tree.root_items = item;
    } else {
        AUI_TreeItem *last = tree->tree.root_items;
        while (last->next_sibling) {
            last = last->next_sibling;
        }
        last->next_sibling = item;
        item->prev_sibling = last;
    }

    return item;
}

AUI_TreeItem *aui_tree_add_child(AUI_Node *tree, AUI_TreeItem *parent,
                                  const char *text, void *user_data)
{
    if (!tree || tree->type != AUI_NODE_TREE || !parent) return NULL;

    AUI_TreeItem *item = (AUI_TreeItem*)calloc(1, sizeof(AUI_TreeItem));
    if (!item) return NULL;

    item->id = tree->tree.next_item_id++;
    if (text) {
        strncpy(item->text, text, sizeof(item->text) - 1);
    }
    item->user_data = user_data;
    item->expanded = true;
    item->parent = parent;

    /* Add to end of parent's children list */
    if (!parent->first_child) {
        parent->first_child = item;
        parent->last_child = item;
    } else {
        parent->last_child->next_sibling = item;
        item->prev_sibling = parent->last_child;
        parent->last_child = item;
    }

    return item;
}

void aui_tree_remove_item(AUI_Node *tree, AUI_TreeItem *item)
{
    if (!tree || tree->type != AUI_NODE_TREE || !item) return;

    /* Clear selection if this item is selected */
    if (tree->tree.selected_item == item) {
        tree->tree.selected_item = NULL;
    }

    /* Remove from parent's children list */
    if (item->parent) {
        if (item->prev_sibling) {
            item->prev_sibling->next_sibling = item->next_sibling;
        } else {
            item->parent->first_child = item->next_sibling;
        }
        if (item->next_sibling) {
            item->next_sibling->prev_sibling = item->prev_sibling;
        } else {
            item->parent->last_child = item->prev_sibling;
        }
    } else {
        /* Root item */
        if (item->prev_sibling) {
            item->prev_sibling->next_sibling = item->next_sibling;
        } else {
            tree->tree.root_items = item->next_sibling;
        }
        if (item->next_sibling) {
            item->next_sibling->prev_sibling = item->prev_sibling;
        }
    }

    /* Free item and all children */
    if (item->first_child) {
        aui_tree_item_free_recursive(item->first_child);
    }
    free(item);
}

void aui_tree_clear(AUI_Node *tree)
{
    if (!tree || tree->type != AUI_NODE_TREE) return;

    aui_tree_item_free_recursive(tree->tree.root_items);
    tree->tree.root_items = NULL;
    tree->tree.selected_item = NULL;
    tree->tree.anchor_item = NULL;
}

AUI_TreeItem *aui_tree_get_selected(AUI_Node *tree)
{
    if (!tree || tree->type != AUI_NODE_TREE) return NULL;
    return tree->tree.selected_item;
}

void aui_tree_set_selected(AUI_Node *tree, AUI_TreeItem *item)
{
    if (!tree || tree->type != AUI_NODE_TREE) return;

    /* Deselect previous */
    if (tree->tree.selected_item && !tree->tree.multi_select) {
        tree->tree.selected_item->selected = false;
    }

    tree->tree.selected_item = item;
    if (item) {
        item->selected = true;
    }
}

void aui_tree_set_expanded(AUI_Node *tree, AUI_TreeItem *item, bool expanded)
{
    if (!tree || tree->type != AUI_NODE_TREE || !item) return;
    item->expanded = expanded;
}

static void aui_tree_set_expanded_recursive(AUI_TreeItem *item, bool expanded)
{
    while (item) {
        item->expanded = expanded;
        if (item->first_child) {
            aui_tree_set_expanded_recursive(item->first_child, expanded);
        }
        item = item->next_sibling;
    }
}

void aui_tree_expand_all(AUI_Node *tree)
{
    if (!tree || tree->type != AUI_NODE_TREE) return;
    aui_tree_set_expanded_recursive(tree->tree.root_items, true);
}

void aui_tree_collapse_all(AUI_Node *tree)
{
    if (!tree || tree->type != AUI_NODE_TREE) return;
    aui_tree_set_expanded_recursive(tree->tree.root_items, false);
}

void aui_tree_ensure_visible(AUI_Node *tree, AUI_TreeItem *item)
{
    if (!tree || tree->type != AUI_NODE_TREE || !item) return;

    /* Expand all ancestors */
    AUI_TreeItem *parent = item->parent;
    while (parent) {
        parent->expanded = true;
        parent = parent->parent;
    }

    /* TODO: Scroll to make item visible */
}

static AUI_TreeItem *aui_tree_find_by_data_recursive(AUI_TreeItem *item, void *user_data)
{
    while (item) {
        if (item->user_data == user_data) {
            return item;
        }
        if (item->first_child) {
            AUI_TreeItem *found = aui_tree_find_by_data_recursive(item->first_child, user_data);
            if (found) return found;
        }
        item = item->next_sibling;
    }
    return NULL;
}

AUI_TreeItem *aui_tree_find_by_data(AUI_Node *tree, void *user_data)
{
    if (!tree || tree->type != AUI_NODE_TREE) return NULL;
    return aui_tree_find_by_data_recursive(tree->tree.root_items, user_data);
}

void aui_tree_set_multi_select(AUI_Node *tree, bool multi)
{
    if (!tree || tree->type != AUI_NODE_TREE) return;
    tree->tree.multi_select = multi;
}

void aui_tree_set_indent(AUI_Node *tree, float indent_width)
{
    if (!tree || tree->type != AUI_NODE_TREE) return;
    tree->tree.indent_width = fmaxf(0.0f, indent_width);
}

void aui_tree_set_item_height(AUI_Node *tree, float height)
{
    if (!tree || tree->type != AUI_NODE_TREE) return;
    tree->tree.item_height = fmaxf(16.0f, height);
}

void aui_tree_set_allow_reorder(AUI_Node *tree, bool allow)
{
    if (!tree || tree->type != AUI_NODE_TREE) return;
    tree->tree.allow_reorder = allow;
}

void aui_tree_item_set_text(AUI_TreeItem *item, const char *text)
{
    if (!item) return;
    if (text) {
        strncpy(item->text, text, sizeof(item->text) - 1);
        item->text[sizeof(item->text) - 1] = '\0';
    } else {
        item->text[0] = '\0';
    }
}

void aui_tree_item_set_icon(AUI_TreeItem *item, void *icon)
{
    if (!item) return;
    item->icon = icon;
}

int aui_tree_item_get_depth(AUI_TreeItem *item)
{
    if (!item) return 0;
    int depth = 0;
    AUI_TreeItem *parent = item->parent;
    while (parent) {
        depth++;
        parent = parent->parent;
    }
    return depth;
}

bool aui_tree_item_has_children(AUI_TreeItem *item)
{
    if (!item) return false;
    return item->first_child != NULL;
}
