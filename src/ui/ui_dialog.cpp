/*
 * Carbon UI - Dialog and Popup System Implementation
 */

#include "carbon/ui_dialog.h"
#include "carbon/ui.h"
#include "carbon/ui_tween.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

typedef struct CUI_DialogEntry {
    CUI_Node *node;
    CUI_DialogConfig config;
    bool active;
    bool closing;
    float close_timer;
} CUI_DialogEntry;

typedef struct CUI_ContextMenuState {
    CUI_MenuItem items[MAX_CONTEXT_MENU_ITEMS];
    int item_count;
    float x, y;
    bool active;
    int hovered_index;
    int submenu_index;
    CUI_Rect bounds;
} CUI_ContextMenuState;

typedef struct CUI_TooltipState {
    char text[MAX_TOOLTIP_TEXT];
    CUI_TooltipConfig config;
    float x, y;
    bool active;
    float hover_timer;
    CUI_Node *hover_node;
} CUI_TooltipState;

typedef struct CUI_Notification {
    char title[64];
    char message[256];
    CUI_NotificationType type;
    float duration;
    float elapsed;
    bool active;
    float y_offset;  /* For animation */
} CUI_Notification;

struct CUI_DialogManager {
    CUI_DialogEntry dialogs[MAX_DIALOGS];
    int dialog_count;

    CUI_ContextMenuState context_menu;
    CUI_TooltipState tooltip;

    CUI_Notification notifications[MAX_NOTIFICATIONS];
    int notification_count;
    CUI_NotifyPosition notify_position;

    CUI_TweenManager *tweens;
};

/* ============================================================================
 * Dialog Manager Lifecycle
 * ============================================================================ */

CUI_DialogManager *cui_dialog_manager_create(void)
{
    CUI_DialogManager *dm = (CUI_DialogManager *)calloc(1, sizeof(CUI_DialogManager));
    if (!dm) return NULL;

    dm->notify_position = CUI_NOTIFY_TOP_RIGHT;
    dm->tweens = cui_tween_manager_create();

    return dm;
}

void cui_dialog_manager_destroy(CUI_DialogManager *dm)
{
    if (!dm) return;

    /* Destroy all dialog nodes */
    for (int i = 0; i < dm->dialog_count; i++) {
        if (dm->dialogs[i].node) {
            cui_node_destroy(dm->dialogs[i].node);
        }
    }

    if (dm->tweens) {
        cui_tween_manager_destroy(dm->tweens);
    }

    free(dm);
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint32_t cui_notification_color(CUI_NotificationType type)
{
    switch (type) {
        case CUI_NOTIFY_INFO:    return 0xFF8B4513;  /* Brown-ish blue */
        case CUI_NOTIFY_SUCCESS: return 0xFF228B22;  /* Forest green */
        case CUI_NOTIFY_WARNING: return 0xFF00A5FF;  /* Orange */
        case CUI_NOTIFY_ERROR:   return 0xFF0000CD;  /* Red */
        default:                 return 0xFF808080;
    }
}

static const char *cui_button_label(CUI_DialogResult result)
{
    switch (result) {
        case CUI_DIALOG_OK:     return "OK";
        case CUI_DIALOG_CANCEL: return "Cancel";
        case CUI_DIALOG_YES:    return "Yes";
        case CUI_DIALOG_NO:     return "No";
        case CUI_DIALOG_ABORT:  return "Abort";
        case CUI_DIALOG_RETRY:  return "Retry";
        case CUI_DIALOG_IGNORE: return "Ignore";
        default:                return "OK";
    }
}

/* ============================================================================
 * Dialog Update and Render
 * ============================================================================ */

void cui_dialog_manager_update(CUI_DialogManager *dm, CUI_Context *ctx, float dt)
{
    if (!dm || !ctx) return;

    /* Update tweens */
    if (dm->tweens) {
        cui_tween_manager_update(dm->tweens, dt);
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
        CUI_Notification *n = &dm->notifications[i];
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
        CUI_DialogEntry *entry = &dm->dialogs[i];
        if (entry->closing) {
            entry->close_timer += dt;
            if (entry->close_timer >= 0.2f) {
                /* Destroy and remove */
                if (entry->node) {
                    cui_node_destroy(entry->node);
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

void cui_dialog_manager_render(CUI_DialogManager *dm, CUI_Context *ctx)
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
        cui_draw_rect(ctx, 0, 0, (float)ctx->width, (float)ctx->height, 0x80000000);
    }

    /* Render dialogs */
    for (int i = 0; i < dm->dialog_count; i++) {
        CUI_DialogEntry *entry = &dm->dialogs[i];
        if (!entry->active || !entry->node) continue;

        cui_scene_render(ctx, entry->node);
    }

    /* Render context menu */
    if (dm->context_menu.active) {
        CUI_ContextMenuState *cm = &dm->context_menu;

        /* Background */
        cui_draw_rect_rounded(ctx, cm->bounds.x, cm->bounds.y,
                               cm->bounds.w, cm->bounds.h,
                               ctx->theme.bg_panel, ctx->theme.corner_radius);
        cui_draw_rect_outline(ctx, cm->bounds.x, cm->bounds.y,
                               cm->bounds.w, cm->bounds.h,
                               ctx->theme.border, 1);

        /* Items */
        float y = cm->bounds.y + 4;
        float item_h = ctx->theme.widget_height;

        for (int i = 0; i < cm->item_count; i++) {
            CUI_MenuItem *item = &cm->items[i];

            if (!item->label) {
                /* Separator */
                cui_draw_rect(ctx, cm->bounds.x + 8, y + item_h / 2 - 0.5f,
                               cm->bounds.w - 16, 1, ctx->theme.border);
                y += item_h / 2;
                continue;
            }

            /* Hover highlight */
            if (i == cm->hovered_index && item->enabled) {
                cui_draw_rect(ctx, cm->bounds.x + 2, y,
                               cm->bounds.w - 4, item_h, ctx->theme.accent);
            }

            /* Checkmark */
            if (item->checked) {
                cui_draw_text(ctx, "v", cm->bounds.x + 8, y + 4, ctx->theme.text);
            }

            /* Label */
            uint32_t text_color = item->enabled ? ctx->theme.text : ctx->theme.text_disabled;
            cui_draw_text(ctx, item->label, cm->bounds.x + 28, y + 4, text_color);

            /* Shortcut */
            if (item->shortcut) {
                float sw = cui_text_width(ctx, item->shortcut);
                cui_draw_text(ctx, item->shortcut,
                               cm->bounds.x + cm->bounds.w - sw - 12,
                               y + 4, ctx->theme.text_dim);
            }

            /* Submenu arrow */
            if (item->submenu) {
                cui_draw_text(ctx, ">",
                               cm->bounds.x + cm->bounds.w - 16,
                               y + 4, ctx->theme.text);
            }

            y += item_h;
        }
    }

    /* Render tooltip */
    if (dm->tooltip.active) {
        float tw = cui_text_width(ctx, dm->tooltip.text);
        float th = cui_text_height(ctx);
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

        cui_draw_rect_rounded(ctx, tx, ty, tw + padding * 2, th + padding * 2,
                               0xF0202020, 4);
        cui_draw_text(ctx, dm->tooltip.text, tx + padding, ty + padding,
                       0xFFFFFFFF);
    }

    /* Render notifications */
    float notify_x, notify_y;
    float notify_spacing = 8;

    switch (dm->notify_position) {
        case CUI_NOTIFY_TOP_LEFT:
            notify_x = 16;
            notify_y = 16;
            break;
        case CUI_NOTIFY_TOP_CENTER:
            notify_x = ctx->width / 2;
            notify_y = 16;
            break;
        case CUI_NOTIFY_TOP_RIGHT:
            notify_x = ctx->width - 16;
            notify_y = 16;
            break;
        case CUI_NOTIFY_BOTTOM_LEFT:
            notify_x = 16;
            notify_y = ctx->height - 16;
            break;
        case CUI_NOTIFY_BOTTOM_CENTER:
            notify_x = ctx->width / 2;
            notify_y = ctx->height - 16;
            break;
        case CUI_NOTIFY_BOTTOM_RIGHT:
        default:
            notify_x = ctx->width - 16;
            notify_y = ctx->height - 16;
            break;
    }

    for (int i = 0; i < dm->notification_count; i++) {
        CUI_Notification *n = &dm->notifications[i];
        if (!n->active) continue;

        float nw = 280;
        float nh = 60;
        float nx, ny;

        /* Calculate position based on notify_position */
        bool from_top = dm->notify_position <= CUI_NOTIFY_TOP_RIGHT;
        bool from_right = dm->notify_position == CUI_NOTIFY_TOP_RIGHT ||
                          dm->notify_position == CUI_NOTIFY_BOTTOM_RIGHT;
        bool centered = dm->notify_position == CUI_NOTIFY_TOP_CENTER ||
                        dm->notify_position == CUI_NOTIFY_BOTTOM_CENTER;

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

        uint32_t bg_color = cui_notification_color(n->type);
        bg_color = (bg_color & 0x00FFFFFF) | ((uint32_t)(fade * 240) << 24);

        cui_draw_rect_rounded(ctx, nx, ny, nw, nh, bg_color, 6);

        uint32_t text_color = 0xFFFFFFFF;
        text_color = (text_color & 0x00FFFFFF) | ((uint32_t)(fade * 255) << 24);

        if (n->title[0]) {
            cui_draw_text(ctx, n->title, nx + 12, ny + 8, text_color);
            cui_draw_text(ctx, n->message, nx + 12, ny + 28, text_color);
        } else {
            cui_draw_text(ctx, n->message, nx + 12, ny + (nh - 16) / 2, text_color);
        }
    }
}

bool cui_dialog_manager_process_event(CUI_DialogManager *dm, CUI_Context *ctx,
                                       const SDL_Event *event)
{
    if (!dm || !ctx || !event) return false;

    /* Context menu takes priority */
    if (dm->context_menu.active) {
        if (event->type == SDL_EVENT_MOUSE_MOTION) {
            float mx = event->motion.x;
            float my = event->motion.y;

            /* Check if inside menu */
            CUI_ContextMenuState *cm = &dm->context_menu;
            if (mx >= cm->bounds.x && mx < cm->bounds.x + cm->bounds.w &&
                my >= cm->bounds.y && my < cm->bounds.y + cm->bounds.h) {

                /* Calculate hovered item */
                float y = cm->bounds.y + 4;
                float item_h = ctx->theme.widget_height;
                cm->hovered_index = -1;

                for (int i = 0; i < cm->item_count; i++) {
                    CUI_MenuItem *item = &cm->items[i];
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
            CUI_ContextMenuState *cm = &dm->context_menu;
            float mx = event->button.x;
            float my = event->button.y;

            if (mx >= cm->bounds.x && mx < cm->bounds.x + cm->bounds.w &&
                my >= cm->bounds.y && my < cm->bounds.y + cm->bounds.h) {

                /* Click on item */
                if (cm->hovered_index >= 0) {
                    CUI_MenuItem *item = &cm->items[cm->hovered_index];
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
        CUI_DialogEntry *entry = &dm->dialogs[i];
        if (!entry->active || !entry->config.modal) continue;

        /* Process event through dialog's node tree */
        if (entry->node) {
            if (cui_scene_process_event(ctx, entry->node, event)) {
                return true;
            }
        }

        /* Escape key closes dialog if it has a cancel button */
        if (event->type == SDL_EVENT_KEY_DOWN &&
            event->key.scancode == SDL_SCANCODE_ESCAPE) {
            if (entry->config.show_close_button) {
                cui_dialog_close(entry->node, CUI_DIALOG_CANCEL);
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

bool cui_dialog_manager_has_modal(CUI_DialogManager *dm)
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
 * Dialog Manager Access (temporary - will be in CUI_Context)
 * ============================================================================ */

static CUI_DialogManager *s_dialog_manager = NULL;

static CUI_DialogManager *cui_get_dialog_manager(CUI_Context *ctx)
{
    (void)ctx;
    if (!s_dialog_manager) {
        s_dialog_manager = cui_dialog_manager_create();
    }
    return s_dialog_manager;
}

/* ============================================================================
 * Standard Dialogs
 * ============================================================================ */

static void cui_dialog_button_clicked(CUI_Node *node, const CUI_Signal *sig, void *userdata)
{
    (void)sig;
    CUI_DialogEntry *entry = (CUI_DialogEntry *)userdata;
    if (!entry) return;

    /* Determine which button was clicked based on node name */
    CUI_DialogResult result = CUI_DIALOG_NONE;

    if (strcmp(node->name, "btn_ok") == 0) result = CUI_DIALOG_OK;
    else if (strcmp(node->name, "btn_cancel") == 0) result = CUI_DIALOG_CANCEL;
    else if (strcmp(node->name, "btn_yes") == 0) result = CUI_DIALOG_YES;
    else if (strcmp(node->name, "btn_no") == 0) result = CUI_DIALOG_NO;
    else if (strcmp(node->name, "btn_abort") == 0) result = CUI_DIALOG_ABORT;
    else if (strcmp(node->name, "btn_retry") == 0) result = CUI_DIALOG_RETRY;
    else if (strcmp(node->name, "btn_ignore") == 0) result = CUI_DIALOG_IGNORE;

    /* Call callback */
    if (entry->config.on_result) {
        entry->config.on_result(result, entry->config.userdata);
    }

    /* Start close animation */
    entry->closing = true;
    entry->close_timer = 0;
}

static void cui_dialog_add_button(CUI_Context *ctx, CUI_Node *button_row,
                                   const char *name, const char *label,
                                   CUI_DialogEntry *entry)
{
    CUI_Node *btn = cui_button_create(ctx, name, label);
    if (btn) {
        cui_node_set_h_size_flags(btn, CUI_SIZE_EXPAND);
        cui_node_connect(btn, CUI_SIGNAL_CLICKED, cui_dialog_button_clicked, entry);
        cui_node_add_child(button_row, btn);
    }
}

void cui_dialog_message(CUI_Context *ctx, const char *title, const char *message,
                         CUI_DialogButtons buttons,
                         CUI_DialogCallback on_result, void *userdata)
{
    CUI_DialogConfig config = {0};
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

    cui_dialog_create(ctx, &config);
}

void cui_dialog_alert(CUI_Context *ctx, const char *title, const char *message)
{
    cui_dialog_message(ctx, title, message, CUI_BUTTONS_OK, NULL, NULL);
}

void cui_dialog_confirm(CUI_Context *ctx, const char *title, const char *message,
                         CUI_ConfirmCallback on_result, void *userdata)
{
    /* Wrap callback */
    typedef struct {
        CUI_ConfirmCallback callback;
        void *userdata;
    } ConfirmWrapper;

    ConfirmWrapper *wrapper = (ConfirmWrapper *)malloc(sizeof(ConfirmWrapper));
    wrapper->callback = on_result;
    wrapper->userdata = userdata;

    auto confirm_handler = [](CUI_DialogResult result, void *ud) {
        ConfirmWrapper *w = (ConfirmWrapper *)ud;
        if (w->callback) {
            w->callback(result == CUI_DIALOG_YES, w->userdata);
        }
        free(w);
    };

    cui_dialog_message(ctx, title, message, CUI_BUTTONS_YES_NO,
                        confirm_handler, wrapper);
}

void cui_dialog_input(CUI_Context *ctx, const char *title, const char *prompt,
                       const char *default_text,
                       CUI_InputCallback on_result, void *userdata)
{
    CUI_InputDialogConfig config = {0};
    config.title = title;
    config.prompt = prompt;
    config.default_text = default_text;
    config.max_length = 256;
    config.on_result = on_result;
    config.userdata = userdata;

    cui_dialog_input_ex(ctx, &config);
}

void cui_dialog_input_ex(CUI_Context *ctx, const CUI_InputDialogConfig *config)
{
    if (!ctx || !config) return;

    /* TODO: Implement input dialog with textbox */
    /* For now, just call callback with empty result */
    if (config->on_result) {
        config->on_result(false, "", config->userdata);
    }
}

CUI_Node *cui_dialog_create(CUI_Context *ctx, const CUI_DialogConfig *config)
{
    if (!ctx || !config) return NULL;

    CUI_DialogManager *dm = cui_get_dialog_manager(ctx);
    if (!dm || dm->dialog_count >= MAX_DIALOGS) return NULL;

    /* Create dialog entry */
    CUI_DialogEntry *entry = &dm->dialogs[dm->dialog_count++];
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

    float dialog_h = 120;  /* Base height */

    /* Create dialog panel */
    CUI_Node *panel = cui_panel_create(ctx, "dialog", config->title);
    if (!panel) {
        dm->dialog_count--;
        return NULL;
    }
    entry->node = panel;

    /* Position dialog */
    if (config->center_on_screen) {
        cui_node_set_anchor_preset(panel, CUI_ANCHOR_CENTER);
        cui_node_set_offsets(panel, -dialog_w / 2, -dialog_h / 2,
                              dialog_w / 2, dialog_h / 2);
    } else {
        cui_node_set_anchor_preset(panel, CUI_ANCHOR_TOP_LEFT);
        cui_node_set_offsets(panel, 100, 100, 100 + dialog_w, 100 + dialog_h);
    }

    /* Set style */
    panel->style.background = cui_bg_solid(ctx->theme.bg_panel);
    panel->style.corner_radius = cui_corners_uniform(8);
    panel->style.padding = cui_edges_uniform(16);

    /* Add shadow */
    panel->style.shadows[0] = cui_shadow(0, 4, 16, 0x60000000);
    panel->style.shadow_count = 1;

    /* Content layout */
    CUI_Node *vbox = cui_vbox_create(ctx, "content");
    cui_node_set_anchor_preset(vbox, CUI_ANCHOR_FULL_RECT);
    cui_box_set_separation(vbox, 12);
    cui_node_add_child(panel, vbox);

    /* Message label */
    if (config->message) {
        CUI_Node *label = cui_label_create(ctx, "message", config->message);
        cui_node_set_h_size_flags(label, CUI_SIZE_FILL);
        cui_node_add_child(vbox, label);
    }

    /* Button row */
    CUI_Node *button_row = cui_hbox_create(ctx, "buttons");
    cui_box_set_separation(button_row, 8);
    cui_node_set_v_size_flags(button_row, CUI_SIZE_SHRINK_END);
    cui_node_add_child(vbox, button_row);

    /* Add buttons based on preset */
    switch (config->buttons) {
        case CUI_BUTTONS_OK:
            cui_dialog_add_button(ctx, button_row, "btn_ok", "OK", entry);
            break;

        case CUI_BUTTONS_OK_CANCEL:
            cui_dialog_add_button(ctx, button_row, "btn_ok", "OK", entry);
            cui_dialog_add_button(ctx, button_row, "btn_cancel", "Cancel", entry);
            break;

        case CUI_BUTTONS_YES_NO:
            cui_dialog_add_button(ctx, button_row, "btn_yes", "Yes", entry);
            cui_dialog_add_button(ctx, button_row, "btn_no", "No", entry);
            break;

        case CUI_BUTTONS_YES_NO_CANCEL:
            cui_dialog_add_button(ctx, button_row, "btn_yes", "Yes", entry);
            cui_dialog_add_button(ctx, button_row, "btn_no", "No", entry);
            cui_dialog_add_button(ctx, button_row, "btn_cancel", "Cancel", entry);
            break;

        case CUI_BUTTONS_ABORT_RETRY_IGNORE:
            cui_dialog_add_button(ctx, button_row, "btn_abort", "Abort", entry);
            cui_dialog_add_button(ctx, button_row, "btn_retry", "Retry", entry);
            cui_dialog_add_button(ctx, button_row, "btn_ignore", "Ignore", entry);
            break;

        case CUI_BUTTONS_RETRY_CANCEL:
            cui_dialog_add_button(ctx, button_row, "btn_retry", "Retry", entry);
            cui_dialog_add_button(ctx, button_row, "btn_cancel", "Cancel", entry);
            break;

        case CUI_BUTTONS_CUSTOM:
            for (int i = 0; i < config->custom_button_count; i++) {
                char name[32];
                snprintf(name, sizeof(name), "btn_custom_%d", i);
                cui_dialog_add_button(ctx, button_row, name,
                                       config->custom_button_labels[i], entry);
            }
            break;

        case CUI_BUTTONS_NONE:
        default:
            break;
    }

    /* Animate entry */
    if (config->animate && dm->tweens) {
        cui_node_set_opacity(panel, 0);
        cui_tween_fade_in(dm->tweens, panel, config->animation_duration);
    }

    return panel;
}

void cui_dialog_close(CUI_Node *dialog, CUI_DialogResult result)
{
    if (!dialog) return;

    CUI_DialogManager *dm = s_dialog_manager;
    if (!dm) return;

    /* Find dialog entry */
    for (int i = 0; i < dm->dialog_count; i++) {
        if (dm->dialogs[i].node == dialog) {
            CUI_DialogEntry *entry = &dm->dialogs[i];

            /* Call callback */
            if (entry->config.on_result) {
                entry->config.on_result(result, entry->config.userdata);
            }

            /* Start close animation */
            entry->closing = true;
            entry->close_timer = 0;

            /* Fade out */
            if (entry->config.animate && dm->tweens) {
                cui_tween_fade_out(dm->tweens, dialog, 0.15f);
            }
            return;
        }
    }
}

/* ============================================================================
 * Context Menus
 * ============================================================================ */

void cui_context_menu_show(CUI_Context *ctx, float x, float y,
                            const CUI_MenuItem *items, int count)
{
    if (!ctx || !items || count <= 0) return;

    CUI_DialogManager *dm = cui_get_dialog_manager(ctx);
    if (!dm) return;

    CUI_ContextMenuState *cm = &dm->context_menu;

    /* Copy items */
    cm->item_count = count < MAX_CONTEXT_MENU_ITEMS ? count : MAX_CONTEXT_MENU_ITEMS;
    memcpy(cm->items, items, cm->item_count * sizeof(CUI_MenuItem));

    /* Calculate bounds */
    float max_label_w = 0;
    float max_shortcut_w = 0;
    float total_h = 8;  /* Padding */
    float item_h = ctx->theme.widget_height;

    for (int i = 0; i < cm->item_count; i++) {
        if (cm->items[i].label) {
            float lw = cui_text_width(ctx, cm->items[i].label);
            if (lw > max_label_w) max_label_w = lw;

            if (cm->items[i].shortcut) {
                float sw = cui_text_width(ctx, cm->items[i].shortcut);
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

void cui_context_menu_show_at_mouse(CUI_Context *ctx, const CUI_MenuItem *items, int count)
{
    if (!ctx) return;
    cui_context_menu_show(ctx, ctx->input.mouse_x, ctx->input.mouse_y, items, count);
}

void cui_context_menu_close(CUI_Context *ctx)
{
    CUI_DialogManager *dm = cui_get_dialog_manager(ctx);
    if (dm) {
        dm->context_menu.active = false;
    }
}

bool cui_context_menu_is_open(CUI_Context *ctx)
{
    CUI_DialogManager *dm = cui_get_dialog_manager(ctx);
    return dm && dm->context_menu.active;
}

/* ============================================================================
 * Popup Panels
 * ============================================================================ */

CUI_Node *cui_popup_create(CUI_Context *ctx, const char *name)
{
    CUI_Node *popup = cui_node_create(ctx, CUI_NODE_POPUP, name);
    if (popup) {
        popup->visible = false;
        popup->style.background = cui_bg_solid(ctx->theme.bg_panel);
        popup->style.corner_radius = cui_corners_uniform(4);
        popup->style.shadows[0] = cui_shadow(0, 2, 8, 0x40000000);
        popup->style.shadow_count = 1;
    }
    return popup;
}

void cui_popup_show(CUI_Node *popup, float x, float y)
{
    if (!popup) return;
    cui_node_set_position(popup, x, y);
    cui_node_set_visible(popup, true);
}

void cui_popup_show_at_node(CUI_Node *popup, CUI_Node *anchor, CUI_PopupPosition pos)
{
    if (!popup || !anchor) return;

    float ax = anchor->global_rect.x;
    float ay = anchor->global_rect.y;
    float aw = anchor->global_rect.w;
    float ah = anchor->global_rect.h;
    float pw, ph;
    cui_node_get_size(popup, &pw, &ph);

    float px, py;

    switch (pos) {
        case CUI_POPUP_BELOW:
            px = ax;
            py = ay + ah;
            break;
        case CUI_POPUP_ABOVE:
            px = ax;
            py = ay - ph;
            break;
        case CUI_POPUP_LEFT:
            px = ax - pw;
            py = ay;
            break;
        case CUI_POPUP_RIGHT:
            px = ax + aw;
            py = ay;
            break;
        case CUI_POPUP_BELOW_CENTER:
            px = ax + (aw - pw) / 2;
            py = ay + ah;
            break;
        case CUI_POPUP_ABOVE_CENTER:
            px = ax + (aw - pw) / 2;
            py = ay - ph;
            break;
        default:
            px = ax;
            py = ay + ah;
    }

    cui_popup_show(popup, px, py);
}

void cui_popup_hide(CUI_Node *popup)
{
    if (popup) {
        cui_node_set_visible(popup, false);
    }
}

bool cui_popup_is_visible(CUI_Node *popup)
{
    return popup && popup->visible;
}

/* ============================================================================
 * Tooltips
 * ============================================================================ */

void cui_node_set_tooltip(CUI_Node *node, const char *text)
{
    CUI_TooltipConfig config = {0};
    config.text = text;
    config.delay = 0.5f;
    cui_node_set_tooltip_ex(node, &config);
}

void cui_node_set_tooltip_ex(CUI_Node *node, const CUI_TooltipConfig *config)
{
    if (!node || !config) return;

    /* Store tooltip config in node (TODO: add to CUI_Node structure) */
    /* For now, use a simple approach with the global dialog manager */
    (void)node;
    (void)config;
}

void cui_tooltip_show(CUI_Context *ctx, float x, float y, const char *text)
{
    CUI_TooltipConfig config = {0};
    config.text = text;
    config.delay = 0;
    cui_tooltip_show_ex(ctx, x, y, &config);
}

void cui_tooltip_show_ex(CUI_Context *ctx, float x, float y,
                          const CUI_TooltipConfig *config)
{
    if (!ctx || !config || !config->text) return;

    CUI_DialogManager *dm = cui_get_dialog_manager(ctx);
    if (!dm) return;

    strncpy(dm->tooltip.text, config->text, MAX_TOOLTIP_TEXT - 1);
    dm->tooltip.config = *config;
    dm->tooltip.x = x;
    dm->tooltip.y = y;
    dm->tooltip.active = true;
}

void cui_tooltip_hide(CUI_Context *ctx)
{
    CUI_DialogManager *dm = cui_get_dialog_manager(ctx);
    if (dm) {
        dm->tooltip.active = false;
    }
}

/* ============================================================================
 * Notifications
 * ============================================================================ */

void cui_notify(CUI_Context *ctx, const char *message, CUI_NotificationType type)
{
    cui_notify_ex(ctx, NULL, message, type, 3.0f);
}

void cui_notify_ex(CUI_Context *ctx, const char *title, const char *message,
                    CUI_NotificationType type, float duration)
{
    if (!ctx || !message) return;

    CUI_DialogManager *dm = cui_get_dialog_manager(ctx);
    if (!dm || dm->notification_count >= MAX_NOTIFICATIONS) return;

    CUI_Notification *n = &dm->notifications[dm->notification_count++];
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

void cui_notify_set_position(CUI_Context *ctx, CUI_NotifyPosition position)
{
    CUI_DialogManager *dm = cui_get_dialog_manager(ctx);
    if (dm) {
        dm->notify_position = position;
    }
}

void cui_notify_clear_all(CUI_Context *ctx)
{
    CUI_DialogManager *dm = cui_get_dialog_manager(ctx);
    if (dm) {
        dm->notification_count = 0;
    }
}
