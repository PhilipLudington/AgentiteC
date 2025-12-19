/*
 * Agentite UI - Retained Mode Node System Implementation
 */

#include "agentite/ui_node.h"
#include "agentite/ui.h"
#include "agentite/ui_style.h"
#include "agentite/ui_tween.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cstdio>

/* ============================================================================
 * Internal State
 * ============================================================================ */

static uint32_t s_next_node_id = 1;
static AUI_Node *s_focused_node = NULL;

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
}

bool aui_scene_process_event(AUI_Context *ctx, AUI_Node *root, const SDL_Event *event)
{
    if (!ctx || !root || !event) return false;

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
        }

        /* Track which node is currently pressed (must be outside block scope) */
        static AUI_Node *s_pressed_node = NULL;

        /* Handle click */
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && hit) {
            if (hit->focus_mode_click) {
                aui_node_grab_focus(hit);
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
