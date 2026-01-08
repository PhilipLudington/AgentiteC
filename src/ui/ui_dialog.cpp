/*
 * Agentite UI - Dialog and Popup System Implementation
 */

#include "agentite/ui_dialog.h"
#include "agentite/ui.h"
#include "agentite/ui_tween.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cstdio>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_DIALOGS 8
#define MAX_CONTEXT_MENU_ITEMS 32
#define MAX_NOTIFICATIONS 8
#define MAX_TOOLTIP_TEXT 512

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

typedef struct AUI_DialogEntry {
    AUI_Node *node;
    AUI_DialogConfig config;
    bool active;
    bool closing;
    float close_timer;
} AUI_DialogEntry;

typedef struct AUI_ContextMenuState {
    AUI_MenuItem items[MAX_CONTEXT_MENU_ITEMS];
    int item_count;
    float x, y;
    bool active;
    int hovered_index;
    int submenu_index;
    AUI_Rect bounds;
} AUI_ContextMenuState;

typedef struct AUI_TooltipState {
    char text[MAX_TOOLTIP_TEXT];
    AUI_TooltipConfig config;
    float x, y;
    bool active;
    float hover_timer;
    AUI_Node *hover_node;
} AUI_TooltipState;

typedef struct AUI_Notification {
    char title[64];
    char message[256];
    AUI_NotificationType type;
    float duration;
    float elapsed;
    bool active;
    float y_offset;  /* For animation */
} AUI_Notification;

struct AUI_DialogManager {
    AUI_Node *dialog_root;  /* Root node for all dialogs - enables proper layout */
    AUI_DialogEntry dialogs[MAX_DIALOGS];
    int dialog_count;

    AUI_ContextMenuState context_menu;
    AUI_TooltipState tooltip;

    AUI_Notification notifications[MAX_NOTIFICATIONS];
    int notification_count;
    AUI_NotifyPosition notify_position;

    AUI_TweenManager *tweens;
};

/* ============================================================================
 * Dialog Manager Lifecycle
 * ============================================================================ */

AUI_DialogManager *aui_dialog_manager_create(void)
{
    AUI_DialogManager *dm = (AUI_DialogManager *)calloc(1, sizeof(AUI_DialogManager));
    if (!dm) return NULL;

    dm->notify_position = AUI_NOTIFY_TOP_RIGHT;
    dm->tweens = aui_tween_manager_create();

    return dm;
}

void aui_dialog_manager_destroy(AUI_DialogManager *dm)
{
    if (!dm) return;

    /* Destroy all dialog nodes */
    for (int i = 0; i < dm->dialog_count; i++) {
        if (dm->dialogs[i].node) {
            aui_node_destroy(dm->dialogs[i].node);
        }
    }

    /* Destroy dialog root */
    if (dm->dialog_root) {
        aui_node_destroy(dm->dialog_root);
    }

    if (dm->tweens) {
        aui_tween_manager_destroy(dm->tweens);
    }

    free(dm);
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint32_t aui_notification_color(AUI_NotificationType type)
{
    switch (type) {
        case AUI_NOTIFY_INFO:    return 0xFF8B4513;  /* Brown-ish blue */
        case AUI_NOTIFY_SUCCESS: return 0xFF228B22;  /* Forest green */
        case AUI_NOTIFY_WARNING: return 0xFF00A5FF;  /* Orange */
        case AUI_NOTIFY_ERROR:   return 0xFF0000CD;  /* Red */
        default:                 return 0xFF808080;
    }
}

static AUI_Node *ensure_dialog_root(AUI_Context *ctx, AUI_DialogManager *dm)
{
    if (!dm->dialog_root) {
        dm->dialog_root = aui_node_create(ctx, AUI_NODE_CONTROL, "dialog_root");
        aui_node_set_anchor_preset(dm->dialog_root, AUI_ANCHOR_FULL_RECT);
    }
    return dm->dialog_root;
}

/* ============================================================================
 * Dialog Update and Render
 * ============================================================================ */

void aui_dialog_manager_update(AUI_DialogManager *dm, AUI_Context *ctx, float dt)
{
    if (!dm || !ctx) return;

    /* Update dialog root layout - ensures all dialog global_rects are computed */
    if (dm->dialog_root) {
        aui_scene_layout(ctx, dm->dialog_root);
    }

    /* Update tweens */
    if (dm->tweens) {
        aui_tween_manager_update(dm->tweens, dt);
    }

    /* Update tooltip hover timer */
    if (dm->tooltip.hover_node && !dm->tooltip.active) {
        dm->tooltip.hover_timer += dt;
        if (dm->tooltip.hover_timer >= dm->tooltip.config.delay) {
            dm->tooltip.active = true;
        }
    }

    /* Update notifications */
    for (int i = 0; i < dm->notification_count; i++) {
        AUI_Notification *n = &dm->notifications[i];
        if (!n->active) continue;

        n->elapsed += dt;
        if (n->elapsed >= n->duration) {
            n->active = false;
        }
    }

    /* Compact inactive notifications */
    int write = 0;
    for (int i = 0; i < dm->notification_count; i++) {
        if (dm->notifications[i].active) {
            if (write != i) {
                dm->notifications[write] = dm->notifications[i];
            }
            write++;
        }
    }
    dm->notification_count = write;

    /* Update closing dialogs */
    for (int i = 0; i < dm->dialog_count; i++) {
        AUI_DialogEntry *entry = &dm->dialogs[i];
        if (entry->closing) {
            entry->close_timer += dt;
            if (entry->close_timer >= 0.2f) {
                /* Destroy and remove */
                if (entry->node) {
                    /* Detach from root before destroy */
                    if (entry->node->parent) {
                        aui_node_remove_child(entry->node->parent, entry->node);
                    }
                    aui_node_destroy(entry->node);
                }
                entry->active = false;
            }
        }
    }

    /* Compact inactive dialogs */
    write = 0;
    for (int i = 0; i < dm->dialog_count; i++) {
        if (dm->dialogs[i].active) {
            if (write != i) {
                dm->dialogs[write] = dm->dialogs[i];
            }
            write++;
        }
    }
    dm->dialog_count = write;
}

void aui_dialog_manager_render(AUI_DialogManager *dm, AUI_Context *ctx)
{
    if (!dm || !ctx) return;

    /* Draw modal overlay if any modal dialog is open */
    bool has_modal = false;
    for (int i = 0; i < dm->dialog_count; i++) {
        if (dm->dialogs[i].active && dm->dialogs[i].config.modal) {
            has_modal = true;
            break;
        }
    }

    if (has_modal) {
        aui_draw_rect(ctx, 0, 0, (float)ctx->width, (float)ctx->height, 0x80000000);
    }

    /* Layout and render dialogs */
    for (int i = 0; i < dm->dialog_count; i++) {
        AUI_DialogEntry *entry = &dm->dialogs[i];
        if (!entry->active || !entry->node) continue;

        /* Layout already computed in update - just render */
        aui_scene_render(ctx, entry->node);
    }

    /* Render context menu */
    if (dm->context_menu.active) {
        AUI_ContextMenuState *cm = &dm->context_menu;

        /* Background - use a lighter, opaque color for better readability */
        uint32_t menu_bg = 0xFF2A2A3A;  /* Slightly lighter than bg_panel, fully opaque */
        uint32_t menu_border = 0xFF4A4A5A;  /* Visible border */
        aui_draw_rect_rounded(ctx, cm->bounds.x, cm->bounds.y,
                               cm->bounds.w, cm->bounds.h,
                               menu_bg, ctx->theme.corner_radius);
        aui_draw_rect_outline(ctx, cm->bounds.x, cm->bounds.y,
                               cm->bounds.w, cm->bounds.h,
                               menu_border, 1);

        /* Items */
        float y = cm->bounds.y + 4;
        float item_h = ctx->theme.widget_height;

        for (int i = 0; i < cm->item_count; i++) {
            AUI_MenuItem *item = &cm->items[i];

            if (!item->label) {
                /* Separator */
                aui_draw_rect(ctx, cm->bounds.x + 8, y + item_h / 2 - 0.5f,
                               cm->bounds.w - 16, 1, ctx->theme.border);
                y += item_h / 2;
                continue;
            }

            /* Hover highlight */
            if (i == cm->hovered_index && item->enabled) {
                aui_draw_rect(ctx, cm->bounds.x + 2, y,
                               cm->bounds.w - 4, item_h, ctx->theme.accent);
            }

            /* Checkmark */
            if (item->checked) {
                aui_draw_text(ctx, "v", cm->bounds.x + 8, y + 4, ctx->theme.text);
            }

            /* Label */
            uint32_t text_color = item->enabled ? ctx->theme.text : ctx->theme.text_disabled;
            aui_draw_text(ctx, item->label, cm->bounds.x + 28, y + 4, text_color);

            /* Shortcut */
            if (item->shortcut) {
                float sw = aui_text_width(ctx, item->shortcut);
                aui_draw_text(ctx, item->shortcut,
                               cm->bounds.x + cm->bounds.w - sw - 12,
                               y + 4, ctx->theme.text_dim);
            }

            /* Submenu arrow */
            if (item->submenu) {
                aui_draw_text(ctx, ">",
                               cm->bounds.x + cm->bounds.w - 16,
                               y + 4, ctx->theme.text);
            }

            y += item_h;
        }
    }

    /* Render tooltip */
    if (dm->tooltip.active) {
        float tw = aui_text_width(ctx, dm->tooltip.text);
        float th = aui_text_height(ctx);
        float padding = 6;
        float tx = dm->tooltip.x;
        float ty = dm->tooltip.y + 20;  /* Below cursor */

        /* Keep on screen */
        if (tx + tw + padding * 2 > ctx->width) {
            tx = ctx->width - tw - padding * 2;
        }
        if (ty + th + padding * 2 > ctx->height) {
            ty = dm->tooltip.y - th - padding * 2 - 5;  /* Above cursor */
        }

        aui_draw_rect_rounded(ctx, tx, ty, tw + padding * 2, th + padding * 2,
                               0xF0202020, 4);
        aui_draw_text(ctx, dm->tooltip.text, tx + padding, ty + padding,
                       0xFFFFFFFF);
    }

    /* Render notifications */
    float notify_x, notify_y;
    float notify_spacing = 8;

    switch (dm->notify_position) {
        case AUI_NOTIFY_TOP_LEFT:
            notify_x = 16;
            notify_y = 16;
            break;
        case AUI_NOTIFY_TOP_CENTER:
            notify_x = ctx->width / 2;
            notify_y = 16;
            break;
        case AUI_NOTIFY_TOP_RIGHT:
            notify_x = ctx->width - 16;
            notify_y = 16;
            break;
        case AUI_NOTIFY_BOTTOM_LEFT:
            notify_x = 16;
            notify_y = ctx->height - 16;
            break;
        case AUI_NOTIFY_BOTTOM_CENTER:
            notify_x = ctx->width / 2;
            notify_y = ctx->height - 16;
            break;
        case AUI_NOTIFY_BOTTOM_RIGHT:
        default:
            notify_x = ctx->width - 16;
            notify_y = ctx->height - 16;
            break;
    }

    for (int i = 0; i < dm->notification_count; i++) {
        AUI_Notification *n = &dm->notifications[i];
        if (!n->active) continue;

        float nw = 280;
        float nh = 60;
        float nx, ny;

        /* Calculate position based on notify_position */
        bool from_top = dm->notify_position <= AUI_NOTIFY_TOP_RIGHT;
        bool from_right = dm->notify_position == AUI_NOTIFY_TOP_RIGHT ||
                          dm->notify_position == AUI_NOTIFY_BOTTOM_RIGHT;
        bool centered = dm->notify_position == AUI_NOTIFY_TOP_CENTER ||
                        dm->notify_position == AUI_NOTIFY_BOTTOM_CENTER;

        if (centered) {
            nx = notify_x - nw / 2;
        } else if (from_right) {
            nx = notify_x - nw;
        } else {
            nx = notify_x;
        }

        if (from_top) {
            ny = notify_y + i * (nh + notify_spacing);
        } else {
            ny = notify_y - nh - i * (nh + notify_spacing);
        }

        /* Fade out animation */
        float fade = 1.0f;
        if (n->elapsed > n->duration - 0.3f) {
            fade = (n->duration - n->elapsed) / 0.3f;
        } else if (n->elapsed < 0.2f) {
            fade = n->elapsed / 0.2f;
        }

        uint32_t bg_color = aui_notification_color(n->type);
        bg_color = (bg_color & 0x00FFFFFF) | ((uint32_t)(fade * 240) << 24);

        aui_draw_rect_rounded(ctx, nx, ny, nw, nh, bg_color, 6);

        uint32_t text_color = 0xFFFFFFFF;
        text_color = (text_color & 0x00FFFFFF) | ((uint32_t)(fade * 255) << 24);

        if (n->title[0]) {
            aui_draw_text(ctx, n->title, nx + 12, ny + 8, text_color);
            aui_draw_text(ctx, n->message, nx + 12, ny + 28, text_color);
        } else {
            aui_draw_text(ctx, n->message, nx + 12, ny + (nh - 16) / 2, text_color);
        }
    }
}

bool aui_dialog_manager_process_event(AUI_DialogManager *dm, AUI_Context *ctx,
                                       const SDL_Event *event)
{
    if (!dm || !ctx || !event) return false;

    /* Context menu takes priority */
    if (dm->context_menu.active) {
        if (event->type == SDL_EVENT_MOUSE_MOTION) {
            float mx = event->motion.x;
            float my = event->motion.y;

            /* Check if inside menu */
            AUI_ContextMenuState *cm = &dm->context_menu;
            if (mx >= cm->bounds.x && mx < cm->bounds.x + cm->bounds.w &&
                my >= cm->bounds.y && my < cm->bounds.y + cm->bounds.h) {

                /* Calculate hovered item */
                float y = cm->bounds.y + 4;
                float item_h = ctx->theme.widget_height;
                cm->hovered_index = -1;

                for (int i = 0; i < cm->item_count; i++) {
                    AUI_MenuItem *item = &cm->items[i];
                    float ih = item->label ? item_h : item_h / 2;

                    if (my >= y && my < y + ih && item->label) {
                        cm->hovered_index = i;
                        break;
                    }
                    y += ih;
                }
            } else {
                cm->hovered_index = -1;
            }
            return true;
        }

        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            AUI_ContextMenuState *cm = &dm->context_menu;
            float mx = event->button.x;
            float my = event->button.y;

            if (mx >= cm->bounds.x && mx < cm->bounds.x + cm->bounds.w &&
                my >= cm->bounds.y && my < cm->bounds.y + cm->bounds.h) {

                /* Click on item */
                if (cm->hovered_index >= 0) {
                    AUI_MenuItem *item = &cm->items[cm->hovered_index];
                    if (item->enabled && item->on_select && !item->submenu) {
                        item->on_select(item->userdata);
                    }
                }
            }

            /* Close menu on any click */
            cm->active = false;
            return true;
        }

        if (event->type == SDL_EVENT_KEY_DOWN &&
            event->key.scancode == SDL_SCANCODE_ESCAPE) {
            dm->context_menu.active = false;
            return true;
        }
    }

    /* Modal dialogs block other input */
    for (int i = dm->dialog_count - 1; i >= 0; i--) {
        AUI_DialogEntry *entry = &dm->dialogs[i];
        if (!entry->active || !entry->config.modal) continue;

        /* Process event through dialog's node tree */
        if (entry->node) {
            if (aui_scene_process_event(ctx, entry->node, event)) {
                return true;
            }
        }

        /* Escape key closes dialog if it has a cancel button */
        if (event->type == SDL_EVENT_KEY_DOWN &&
            event->key.scancode == SDL_SCANCODE_ESCAPE) {
            if (entry->config.show_close_button) {
                aui_dialog_close(entry->node, AUI_DIALOG_CANCEL);
                return true;
            }
        }

        /* Block event from reaching other UI */
        return true;
    }

    /* Reset tooltip on mouse move */
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        dm->tooltip.active = false;
        dm->tooltip.hover_timer = 0;
        dm->tooltip.x = event->motion.x;
        dm->tooltip.y = event->motion.y;
    }

    return false;
}

bool aui_dialog_manager_has_modal(AUI_DialogManager *dm)
{
    if (!dm) return false;

    for (int i = 0; i < dm->dialog_count; i++) {
        if (dm->dialogs[i].active && dm->dialogs[i].config.modal) {
            return true;
        }
    }
    return false;
}

/* ============================================================================
 * Dialog Manager Access (temporary - will be in AUI_Context)
 * ============================================================================ */

static AUI_DialogManager *s_dialog_manager = NULL;

static AUI_DialogManager *aui_get_dialog_manager(AUI_Context *ctx)
{
    (void)ctx;
    if (!s_dialog_manager) {
        s_dialog_manager = aui_dialog_manager_create();
    }
    return s_dialog_manager;
}

/* ============================================================================
 * Standard Dialogs
 * ============================================================================ */

static void aui_dialog_button_clicked(AUI_Node *node, const AUI_Signal *sig, void *userdata)
{
    (void)sig;
    AUI_DialogEntry *entry = (AUI_DialogEntry *)userdata;
    if (!entry) return;

    /* Determine which button was clicked based on node name */
    AUI_DialogResult result = AUI_DIALOG_NONE;

    if (strcmp(node->name, "btn_ok") == 0) result = AUI_DIALOG_OK;
    else if (strcmp(node->name, "btn_cancel") == 0) result = AUI_DIALOG_CANCEL;
    else if (strcmp(node->name, "btn_yes") == 0) result = AUI_DIALOG_YES;
    else if (strcmp(node->name, "btn_no") == 0) result = AUI_DIALOG_NO;
    else if (strcmp(node->name, "btn_abort") == 0) result = AUI_DIALOG_ABORT;
    else if (strcmp(node->name, "btn_retry") == 0) result = AUI_DIALOG_RETRY;
    else if (strcmp(node->name, "btn_ignore") == 0) result = AUI_DIALOG_IGNORE;

    /* Call callback */
    if (entry->config.on_result) {
        entry->config.on_result(result, entry->config.userdata);
    }

    /* Start close animation */
    entry->closing = true;
    entry->close_timer = 0;
}

static void aui_dialog_add_button(AUI_Context *ctx, AUI_Node *button_row,
                                   const char *name, const char *label,
                                   AUI_DialogEntry *entry)
{
    AUI_Node *btn = aui_button_create(ctx, name, label);
    if (btn) {
        /* Apply button style from theme */
        btn->style.background = aui_bg_solid(ctx->theme.bg_widget);
        btn->style.background_hover = aui_bg_solid(ctx->theme.bg_widget_hover);
        btn->style.background_active = aui_bg_solid(ctx->theme.bg_widget_active);
        btn->style.text_color = ctx->theme.text;
        btn->style.corner_radius = aui_corners_uniform(ctx->theme.corner_radius);
        btn->style.padding = aui_edges(4, 12, 4, 12);  /* Minimal vertical padding */

        aui_node_set_h_size_flags(btn, AUI_SIZE_EXPAND);
        aui_node_connect(btn, AUI_SIGNAL_CLICKED, aui_dialog_button_clicked, entry);
        aui_node_add_child(button_row, btn);
    }
}

void aui_dialog_message(AUI_Context *ctx, const char *title, const char *message,
                         AUI_DialogButtons buttons,
                         AUI_DialogCallback on_result, void *userdata)
{
    AUI_DialogConfig config = {0};
    config.title = title;
    config.message = message;
    config.buttons = buttons;
    config.modal = true;
    config.show_close_button = true;
    config.center_on_screen = true;
    config.draggable = true;
    config.on_result = on_result;
    config.userdata = userdata;
    config.min_width = 300;
    config.animate = true;
    config.animation_duration = 0.2f;

    aui_dialog_create(ctx, &config);
}

void aui_dialog_alert(AUI_Context *ctx, const char *title, const char *message)
{
    aui_dialog_message(ctx, title, message, AUI_BUTTONS_OK, NULL, NULL);
}

void aui_dialog_confirm(AUI_Context *ctx, const char *title, const char *message,
                         AUI_ConfirmCallback on_result, void *userdata)
{
    /* Wrap callback */
    typedef struct {
        AUI_ConfirmCallback callback;
        void *userdata;
    } ConfirmWrapper;

    ConfirmWrapper *wrapper = (ConfirmWrapper *)malloc(sizeof(ConfirmWrapper));
    wrapper->callback = on_result;
    wrapper->userdata = userdata;

    auto confirm_handler = [](AUI_DialogResult result, void *ud) {
        ConfirmWrapper *w = (ConfirmWrapper *)ud;
        if (w->callback) {
            w->callback(result == AUI_DIALOG_YES, w->userdata);
        }
        free(w);
    };

    aui_dialog_message(ctx, title, message, AUI_BUTTONS_YES_NO,
                        confirm_handler, wrapper);
}

void aui_dialog_input(AUI_Context *ctx, const char *title, const char *prompt,
                       const char *default_text,
                       AUI_InputCallback on_result, void *userdata)
{
    AUI_InputDialogConfig config = {0};
    config.title = title;
    config.prompt = prompt;
    config.default_text = default_text;
    config.max_length = 256;
    config.on_result = on_result;
    config.userdata = userdata;

    aui_dialog_input_ex(ctx, &config);
}

void aui_dialog_input_ex(AUI_Context *ctx, const AUI_InputDialogConfig *config)
{
    if (!ctx || !config) return;

    /* TODO: Implement input dialog with textbox */
    /* For now, just call callback with empty result */
    if (config->on_result) {
        config->on_result(false, "", config->userdata);
    }
}

AUI_Node *aui_dialog_create(AUI_Context *ctx, const AUI_DialogConfig *config)
{
    if (!ctx || !config) return NULL;

    AUI_DialogManager *dm = aui_get_dialog_manager(ctx);
    if (!dm || dm->dialog_count >= MAX_DIALOGS) return NULL;

    /* Create dialog entry */
    AUI_DialogEntry *entry = &dm->dialogs[dm->dialog_count++];
    memset(entry, 0, sizeof(*entry));
    entry->config = *config;
    entry->active = true;

    /* Calculate dialog size */
    float dialog_w = config->width > 0 ? config->width : 350;
    if (config->min_width > 0 && dialog_w < config->min_width) {
        dialog_w = config->min_width;
    }
    if (config->max_width > 0 && dialog_w > config->max_width) {
        dialog_w = config->max_width;
    }

    float dialog_h = config->height > 0 ? config->height : 150;  /* Use height or default */

    /* Create dialog panel */
    AUI_Node *panel = aui_panel_create(ctx, "dialog", config->title);
    if (!panel) {
        dm->dialog_count--;
        return NULL;
    }
    entry->node = panel;

    /* Attach to dialog root for proper layout computation */
    ensure_dialog_root(ctx, dm);
    aui_node_add_child(dm->dialog_root, panel);

    /* Position dialog */
    if (config->center_on_screen) {
        aui_node_set_anchor_preset(panel, AUI_ANCHOR_CENTER);
        aui_node_set_offsets(panel, -dialog_w / 2, -dialog_h / 2,
                              dialog_w / 2, dialog_h / 2);
    } else {
        aui_node_set_anchor_preset(panel, AUI_ANCHOR_TOP_LEFT);
        aui_node_set_offsets(panel, 100, 100, 100 + dialog_w, 100 + dialog_h);
    }

    /* Set style */
    panel->style.background = aui_bg_solid(ctx->theme.bg_panel);
    panel->style.corner_radius = aui_corners_uniform(8);
    panel->style.padding = aui_edges_uniform(12);

    /* Add shadow */
    panel->style.shadows[0] = aui_shadow(0, 4, 16, 0x60000000);
    panel->style.shadow_count = 1;

    /* Message label - anchored to top */
    AUI_Node *label = NULL;
    if (config->message) {
        label = aui_label_create(ctx, "message", config->message);
        aui_node_set_anchor_preset(label, AUI_ANCHOR_TOP_WIDE);
        aui_node_set_offsets(label, 0, 0, 0, 40);  /* Top with 40px height */
        label->label.autowrap = true;
        aui_node_add_child(panel, label);
    }

    /* Button row - anchored to bottom */
    AUI_Node *button_row = aui_hbox_create(ctx, "buttons");
    aui_box_set_separation(button_row, 8);
    aui_node_set_anchor_preset(button_row, AUI_ANCHOR_BOTTOM_WIDE);
    aui_node_set_offsets(button_row, 0, -36, 0, 0);  /* Bottom with 36px height */
    aui_node_add_child(panel, button_row);

    /* Add buttons based on preset */
    switch (config->buttons) {
        case AUI_BUTTONS_OK:
            aui_dialog_add_button(ctx, button_row, "btn_ok", "OK", entry);
            break;

        case AUI_BUTTONS_OK_CANCEL:
            aui_dialog_add_button(ctx, button_row, "btn_ok", "OK", entry);
            aui_dialog_add_button(ctx, button_row, "btn_cancel", "Cancel", entry);
            break;

        case AUI_BUTTONS_YES_NO:
            aui_dialog_add_button(ctx, button_row, "btn_yes", "Yes", entry);
            aui_dialog_add_button(ctx, button_row, "btn_no", "No", entry);
            break;

        case AUI_BUTTONS_YES_NO_CANCEL:
            aui_dialog_add_button(ctx, button_row, "btn_yes", "Yes", entry);
            aui_dialog_add_button(ctx, button_row, "btn_no", "No", entry);
            aui_dialog_add_button(ctx, button_row, "btn_cancel", "Cancel", entry);
            break;

        case AUI_BUTTONS_ABORT_RETRY_IGNORE:
            aui_dialog_add_button(ctx, button_row, "btn_abort", "Abort", entry);
            aui_dialog_add_button(ctx, button_row, "btn_retry", "Retry", entry);
            aui_dialog_add_button(ctx, button_row, "btn_ignore", "Ignore", entry);
            break;

        case AUI_BUTTONS_RETRY_CANCEL:
            aui_dialog_add_button(ctx, button_row, "btn_retry", "Retry", entry);
            aui_dialog_add_button(ctx, button_row, "btn_cancel", "Cancel", entry);
            break;

        case AUI_BUTTONS_CUSTOM:
            for (int i = 0; i < config->custom_button_count; i++) {
                char name[32];
                snprintf(name, sizeof(name), "btn_custom_%d", i);
                aui_dialog_add_button(ctx, button_row, name,
                                       config->custom_button_labels[i], entry);
            }
            break;

        case AUI_BUTTONS_NONE:
        default:
            break;
    }

    /* Animate entry */
    if (config->animate && dm->tweens) {
        aui_node_set_opacity(panel, 0);
        aui_tween_fade_in(dm->tweens, panel, config->animation_duration);
    }

    return panel;
}

void aui_dialog_close(AUI_Node *dialog, AUI_DialogResult result)
{
    if (!dialog) return;

    AUI_DialogManager *dm = s_dialog_manager;
    if (!dm) return;

    /* Find dialog entry */
    for (int i = 0; i < dm->dialog_count; i++) {
        if (dm->dialogs[i].node == dialog) {
            AUI_DialogEntry *entry = &dm->dialogs[i];

            /* Call callback */
            if (entry->config.on_result) {
                entry->config.on_result(result, entry->config.userdata);
            }

            /* Start close animation */
            entry->closing = true;
            entry->close_timer = 0;

            /* Fade out */
            if (entry->config.animate && dm->tweens) {
                aui_tween_fade_out(dm->tweens, dialog, 0.15f);
            }
            return;
        }
    }
}

/* ============================================================================
 * Context Menus
 * ============================================================================ */

void aui_context_menu_show(AUI_Context *ctx, float x, float y,
                            const AUI_MenuItem *items, int count)
{
    if (!ctx || !items || count <= 0) return;

    AUI_DialogManager *dm = aui_get_dialog_manager(ctx);
    if (!dm) return;

    AUI_ContextMenuState *cm = &dm->context_menu;

    /* Copy items */
    cm->item_count = count < MAX_CONTEXT_MENU_ITEMS ? count : MAX_CONTEXT_MENU_ITEMS;
    memcpy(cm->items, items, cm->item_count * sizeof(AUI_MenuItem));

    /* Calculate bounds */
    float max_label_w = 0;
    float max_shortcut_w = 0;
    float total_h = 8;  /* Padding */
    float item_h = ctx->theme.widget_height;

    for (int i = 0; i < cm->item_count; i++) {
        if (cm->items[i].label) {
            float lw = aui_text_width(ctx, cm->items[i].label);
            if (lw > max_label_w) max_label_w = lw;

            if (cm->items[i].shortcut) {
                float sw = aui_text_width(ctx, cm->items[i].shortcut);
                if (sw > max_shortcut_w) max_shortcut_w = sw;
            }

            total_h += item_h;
        } else {
            total_h += item_h / 2;  /* Separator */
        }
    }

    float menu_w = 28 + max_label_w + 20 + max_shortcut_w + 16;
    if (menu_w < 150) menu_w = 150;

    /* Position menu, keep on screen */
    cm->x = x;
    cm->y = y;
    if (x + menu_w > ctx->width) {
        cm->x = ctx->width - menu_w;
    }
    if (y + total_h > ctx->height) {
        cm->y = ctx->height - total_h;
    }

    cm->bounds.x = cm->x;
    cm->bounds.y = cm->y;
    cm->bounds.w = menu_w;
    cm->bounds.h = total_h;

    cm->active = true;
    cm->hovered_index = -1;
}

void aui_context_menu_show_at_mouse(AUI_Context *ctx, const AUI_MenuItem *items, int count)
{
    if (!ctx) return;
    aui_context_menu_show(ctx, ctx->input.mouse_x, ctx->input.mouse_y, items, count);
}

void aui_context_menu_close(AUI_Context *ctx)
{
    AUI_DialogManager *dm = aui_get_dialog_manager(ctx);
    if (dm) {
        dm->context_menu.active = false;
    }
}

bool aui_context_menu_is_open(AUI_Context *ctx)
{
    AUI_DialogManager *dm = aui_get_dialog_manager(ctx);
    return dm && dm->context_menu.active;
}

/* ============================================================================
 * Popup Panels
 * ============================================================================ */

AUI_Node *aui_popup_create(AUI_Context *ctx, const char *name)
{
    AUI_Node *popup = aui_node_create(ctx, AUI_NODE_POPUP, name);
    if (popup) {
        popup->visible = false;
        popup->style.background = aui_bg_solid(ctx->theme.bg_panel);
        popup->style.corner_radius = aui_corners_uniform(4);
        popup->style.shadows[0] = aui_shadow(0, 2, 8, 0x40000000);
        popup->style.shadow_count = 1;
    }
    return popup;
}

void aui_popup_show(AUI_Node *popup, float x, float y)
{
    if (!popup) return;
    aui_node_set_position(popup, x, y);
    aui_node_set_visible(popup, true);
}

void aui_popup_show_at_node(AUI_Node *popup, AUI_Node *anchor, AUI_PopupPosition pos)
{
    if (!popup || !anchor) return;

    float ax = anchor->global_rect.x;
    float ay = anchor->global_rect.y;
    float aw = anchor->global_rect.w;
    float ah = anchor->global_rect.h;
    float pw, ph;
    aui_node_get_size(popup, &pw, &ph);

    float px, py;

    switch (pos) {
        case AUI_POPUP_BELOW:
            px = ax;
            py = ay + ah;
            break;
        case AUI_POPUP_ABOVE:
            px = ax;
            py = ay - ph;
            break;
        case AUI_POPUP_LEFT:
            px = ax - pw;
            py = ay;
            break;
        case AUI_POPUP_RIGHT:
            px = ax + aw;
            py = ay;
            break;
        case AUI_POPUP_BELOW_CENTER:
            px = ax + (aw - pw) / 2;
            py = ay + ah;
            break;
        case AUI_POPUP_ABOVE_CENTER:
            px = ax + (aw - pw) / 2;
            py = ay - ph;
            break;
        default:
            px = ax;
            py = ay + ah;
    }

    aui_popup_show(popup, px, py);
}

void aui_popup_hide(AUI_Node *popup)
{
    if (popup) {
        aui_node_set_visible(popup, false);
    }
}

bool aui_popup_is_visible(AUI_Node *popup)
{
    return popup && popup->visible;
}

/* ============================================================================
 * Tooltips
 * ============================================================================ */

void aui_node_set_tooltip(AUI_Node *node, const char *text)
{
    if (!node) return;

    if (text) {
        strncpy(node->tooltip_text, text, sizeof(node->tooltip_text) - 1);
        node->tooltip_text[sizeof(node->tooltip_text) - 1] = '\0';
    } else {
        node->tooltip_text[0] = '\0';
    }

    if (node->tooltip_delay == 0) {
        node->tooltip_delay = 0.5f;  /* Default delay */
    }
}

void aui_node_set_tooltip_ex(AUI_Node *node, const AUI_TooltipConfig *config)
{
    if (!node || !config) return;

    if (config->text) {
        strncpy(node->tooltip_text, config->text, sizeof(node->tooltip_text) - 1);
        node->tooltip_text[sizeof(node->tooltip_text) - 1] = '\0';
    } else {
        node->tooltip_text[0] = '\0';
    }

    node->tooltip_delay = config->delay > 0 ? config->delay : 0.5f;
}

void aui_tooltip_show(AUI_Context *ctx, float x, float y, const char *text)
{
    AUI_TooltipConfig config = {0};
    config.text = text;
    config.delay = 0;
    aui_tooltip_show_ex(ctx, x, y, &config);
}

void aui_tooltip_show_ex(AUI_Context *ctx, float x, float y,
                          const AUI_TooltipConfig *config)
{
    if (!ctx || !config || !config->text) return;

    AUI_DialogManager *dm = aui_get_dialog_manager(ctx);
    if (!dm) return;

    strncpy(dm->tooltip.text, config->text, MAX_TOOLTIP_TEXT - 1);
    dm->tooltip.config = *config;
    dm->tooltip.x = x;
    dm->tooltip.y = y;
    dm->tooltip.active = true;
}

void aui_tooltip_hide(AUI_Context *ctx)
{
    AUI_DialogManager *dm = aui_get_dialog_manager(ctx);
    if (dm) {
        dm->tooltip.active = false;
    }
}

/* ============================================================================
 * Notifications
 * ============================================================================ */

void aui_notify(AUI_Context *ctx, const char *message, AUI_NotificationType type)
{
    aui_notify_ex(ctx, NULL, message, type, 3.0f);
}

void aui_notify_ex(AUI_Context *ctx, const char *title, const char *message,
                    AUI_NotificationType type, float duration)
{
    if (!ctx || !message) return;

    AUI_DialogManager *dm = aui_get_dialog_manager(ctx);
    if (!dm || dm->notification_count >= MAX_NOTIFICATIONS) return;

    AUI_Notification *n = &dm->notifications[dm->notification_count++];
    memset(n, 0, sizeof(*n));

    if (title) {
        strncpy(n->title, title, sizeof(n->title) - 1);
    }
    strncpy(n->message, message, sizeof(n->message) - 1);
    n->type = type;
    n->duration = duration;
    n->elapsed = 0;
    n->active = true;
}

void aui_notify_set_position(AUI_Context *ctx, AUI_NotifyPosition position)
{
    AUI_DialogManager *dm = aui_get_dialog_manager(ctx);
    if (dm) {
        dm->notify_position = position;
    }
}

void aui_notify_clear_all(AUI_Context *ctx)
{
    AUI_DialogManager *dm = aui_get_dialog_manager(ctx);
    if (dm) {
        dm->notification_count = 0;
    }
}

void aui_dialogs_update(AUI_Context *ctx, float dt)
{
    AUI_DialogManager *dm = aui_get_dialog_manager(ctx);
    if (dm) {
        aui_dialog_manager_update(dm, ctx, dt);
    }
}

bool aui_dialogs_process_event(AUI_Context *ctx, const SDL_Event *event)
{
    AUI_DialogManager *dm = aui_get_dialog_manager(ctx);
    if (dm) {
        return aui_dialog_manager_process_event(dm, ctx, event);
    }
    return false;
}

void aui_dialogs_render(AUI_Context *ctx)
{
    AUI_DialogManager *dm = aui_get_dialog_manager(ctx);
    if (dm) {
        aui_dialog_manager_render(dm, ctx);
    }
}

/* ============================================================================
 * File Dialogs (SDL3 native dialogs)
 * ============================================================================ */

/**
 * Internal callback data for file dialog.
 */
typedef struct FileDialogCallbackData {
    AUI_FileDialogCallback callback;
    void *userdata;
} FileDialogCallbackData;

/**
 * SDL3 file dialog callback adapter.
 * Converts SDL's multi-file format to our single-file callback.
 */
static void sdl_file_dialog_callback(void *userdata, const char * const *filelist, int filter)
{
    (void)filter;
    FileDialogCallbackData *data = (FileDialogCallbackData *)userdata;
    if (!data) return;

    const char *path = NULL;
    if (filelist && filelist[0]) {
        path = filelist[0];
    }

    if (data->callback) {
        data->callback(path, data->userdata);
    }

    free(data);
}

/**
 * Convert AUI_FileFilter array to SDL_DialogFileFilter array.
 * Returns allocated array that must be freed by caller.
 */
static SDL_DialogFileFilter *convert_filters(const AUI_FileFilter *filters, int count)
{
    if (!filters || count <= 0) return NULL;

    SDL_DialogFileFilter *sdl_filters = (SDL_DialogFileFilter *)calloc(
        (size_t)count, sizeof(SDL_DialogFileFilter));
    if (!sdl_filters) return NULL;

    for (int i = 0; i < count; i++) {
        sdl_filters[i].name = filters[i].name;
        sdl_filters[i].pattern = filters[i].pattern;
    }

    return sdl_filters;
}

void aui_file_dialog_open(
    AUI_Context *ctx,
    const char *title,
    const char *default_path,
    const AUI_FileFilter *filters,
    int filter_count,
    AUI_FileDialogCallback callback,
    void *userdata)
{
    (void)title;  /* SDL3 doesn't support custom title for file dialogs */

    if (!ctx || !callback) return;

    /* Allocate callback data */
    FileDialogCallbackData *data = (FileDialogCallbackData *)malloc(sizeof(FileDialogCallbackData));
    if (!data) {
        callback(NULL, userdata);
        return;
    }
    data->callback = callback;
    data->userdata = userdata;

    /* Convert filters */
    SDL_DialogFileFilter *sdl_filters = convert_filters(filters, filter_count);

    /* Show native dialog (async - callback will be called later) */
    SDL_ShowOpenFileDialog(
        sdl_file_dialog_callback,
        data,
        ctx->window,
        sdl_filters,
        filter_count,
        default_path,
        false  /* allow_many = false for single file */
    );

    free(sdl_filters);
}

void aui_file_dialog_save(
    AUI_Context *ctx,
    const char *title,
    const char *default_path,
    const AUI_FileFilter *filters,
    int filter_count,
    AUI_FileDialogCallback callback,
    void *userdata)
{
    (void)title;  /* SDL3 doesn't support custom title for file dialogs */

    if (!ctx || !callback) return;

    /* Allocate callback data */
    FileDialogCallbackData *data = (FileDialogCallbackData *)malloc(sizeof(FileDialogCallbackData));
    if (!data) {
        callback(NULL, userdata);
        return;
    }
    data->callback = callback;
    data->userdata = userdata;

    /* Convert filters */
    SDL_DialogFileFilter *sdl_filters = convert_filters(filters, filter_count);

    /* Show native dialog */
    SDL_ShowSaveFileDialog(
        sdl_file_dialog_callback,
        data,
        ctx->window,
        sdl_filters,
        filter_count,
        default_path
    );

    free(sdl_filters);
}

void aui_file_dialog_folder(
    AUI_Context *ctx,
    const char *title,
    const char *default_path,
    AUI_FileDialogCallback callback,
    void *userdata)
{
    (void)title;  /* SDL3 doesn't support custom title for folder dialogs */

    if (!ctx || !callback) return;

    /* Allocate callback data */
    FileDialogCallbackData *data = (FileDialogCallbackData *)malloc(sizeof(FileDialogCallbackData));
    if (!data) {
        callback(NULL, userdata);
        return;
    }
    data->callback = callback;
    data->userdata = userdata;

    /* Show native folder dialog */
    SDL_ShowOpenFolderDialog(
        sdl_file_dialog_callback,
        data,
        ctx->window,
        default_path,
        false  /* allow_many = false for single folder */
    );
}
