/*
 * Agentite UI - Dialog and Popup System
 *
 * Provides modal dialogs, context menus, and popup panels.
 *
 * Usage:
 *   // Message dialog
 *   aui_dialog_message(ctx, "Error", "File not found!",
 *                      AUI_BUTTONS_OK, on_dialog_closed, userdata);
 *
 *   // Confirmation dialog
 *   aui_dialog_confirm(ctx, "Delete", "Are you sure?",
 *                      on_confirm_result, userdata);
 *
 *   // Context menu
 *   AUI_MenuItem items[] = {
 *       {"Cut", "Ctrl+X", on_cut, NULL},
 *       {"Copy", "Ctrl+C", on_copy, NULL},
 *       {NULL}, // separator
 *       {"Paste", "Ctrl+V", on_paste, NULL},
 *   };
 *   aui_context_menu_show(ctx, mouse_x, mouse_y, items, 4);
 */

#ifndef AGENTITE_UI_DIALOG_H
#define AGENTITE_UI_DIALOG_H

#include "agentite/ui_node.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Dialog Result
 * ============================================================================ */

typedef enum AUI_DialogResult {
    AUI_DIALOG_NONE = 0,
    AUI_DIALOG_OK,
    AUI_DIALOG_CANCEL,
    AUI_DIALOG_YES,
    AUI_DIALOG_NO,
    AUI_DIALOG_ABORT,
    AUI_DIALOG_RETRY,
    AUI_DIALOG_IGNORE,
    AUI_DIALOG_CLOSE,          /* Closed via X button */
    AUI_DIALOG_CUSTOM_1,
    AUI_DIALOG_CUSTOM_2,
    AUI_DIALOG_CUSTOM_3,
} AUI_DialogResult;

/* ============================================================================
 * Dialog Button Presets
 * ============================================================================ */

typedef enum AUI_DialogButtons {
    AUI_BUTTONS_NONE,
    AUI_BUTTONS_OK,
    AUI_BUTTONS_OK_CANCEL,
    AUI_BUTTONS_YES_NO,
    AUI_BUTTONS_YES_NO_CANCEL,
    AUI_BUTTONS_ABORT_RETRY_IGNORE,
    AUI_BUTTONS_RETRY_CANCEL,
    AUI_BUTTONS_CUSTOM,
} AUI_DialogButtons;

/* ============================================================================
 * Dialog Callbacks
 * ============================================================================ */

/* Called when dialog closes */
typedef void (*AUI_DialogCallback)(AUI_DialogResult result, void *userdata);

/* Called when confirmation dialog closes */
typedef void (*AUI_ConfirmCallback)(bool confirmed, void *userdata);

/* Called when input dialog closes */
typedef void (*AUI_InputCallback)(bool confirmed, const char *text, void *userdata);

/* ============================================================================
 * Dialog Configuration
 * ============================================================================ */

typedef struct AUI_DialogConfig {
    /* Content */
    const char *title;
    const char *message;
    const char *icon;          /* Icon name (optional) */

    /* Buttons */
    AUI_DialogButtons buttons;
    const char **custom_button_labels;  /* For BUTTONS_CUSTOM */
    int custom_button_count;
    int default_button;        /* Index of default button (Enter key) */
    int cancel_button;         /* Index of cancel button (Escape key) */

    /* Appearance */
    float width;               /* 0 = auto */
    float min_width;
    float max_width;
    bool show_close_button;
    bool modal;                /* Block input to other UI */
    bool center_on_screen;
    bool draggable;

    /* Callbacks */
    AUI_DialogCallback on_result;
    void *userdata;

    /* Animation */
    bool animate;
    float animation_duration;
} AUI_DialogConfig;

/* ============================================================================
 * Input Dialog Configuration
 * ============================================================================ */

typedef struct AUI_InputDialogConfig {
    const char *title;
    const char *prompt;
    const char *default_text;
    const char *placeholder;
    int max_length;
    bool password_mode;
    bool multiline;

    /* Validation */
    bool (*validate)(const char *text, void *userdata);
    const char *validation_error;
    void *validate_userdata;

    AUI_InputCallback on_result;
    void *userdata;
} AUI_InputDialogConfig;

/* ============================================================================
 * File Dialog (placeholder for future implementation)
 * ============================================================================ */

typedef enum AUI_FileDialogType {
    AUI_FILE_DIALOG_OPEN,
    AUI_FILE_DIALOG_SAVE,
    AUI_FILE_DIALOG_SELECT_FOLDER,
} AUI_FileDialogType;

typedef struct AUI_FileDialogConfig {
    AUI_FileDialogType type;
    const char *title;
    const char *default_path;
    const char **filters;      /* e.g., {"*.png", "*.jpg"} */
    int filter_count;
    const char *filter_description;
    bool allow_multiple;

    void (*on_result)(bool success, const char **paths, int count, void *userdata);
    void *userdata;
} AUI_FileDialogConfig;

/* ============================================================================
 * Context Menu Item
 * ============================================================================ */

typedef struct AUI_MenuItem {
    const char *label;         /* NULL = separator */
    const char *shortcut;      /* Display text only (e.g., "Ctrl+C") */
    const char *icon;          /* Icon name (optional) */
    bool enabled;
    bool checked;              /* Show checkmark */
    bool radio;                /* Radio button style */

    /* Submenu (if present, on_select is ignored) */
    struct AUI_MenuItem *submenu;
    int submenu_count;

    /* Action */
    void (*on_select)(void *userdata);
    void *userdata;
} AUI_MenuItem;

/* ============================================================================
 * Popup Position
 * ============================================================================ */

typedef enum AUI_PopupPosition {
    AUI_POPUP_BELOW,           /* Below anchor, aligned left */
    AUI_POPUP_ABOVE,           /* Above anchor, aligned left */
    AUI_POPUP_LEFT,            /* Left of anchor, aligned top */
    AUI_POPUP_RIGHT,           /* Right of anchor, aligned top */
    AUI_POPUP_BELOW_CENTER,    /* Below anchor, centered */
    AUI_POPUP_ABOVE_CENTER,    /* Above anchor, centered */
} AUI_PopupPosition;

/* ============================================================================
 * Tooltip Configuration
 * ============================================================================ */

typedef struct AUI_TooltipConfig {
    const char *text;
    float delay;               /* Seconds before showing */
    float duration;            /* 0 = until mouse moves */
    float max_width;           /* Word wrap width */
    bool rich_text;            /* Parse BBCode */
} AUI_TooltipConfig;

/* ============================================================================
 * Dialog Manager (opaque, stored in AUI_Context)
 * ============================================================================ */

typedef struct AUI_DialogManager AUI_DialogManager;

/* Create/destroy dialog manager */
AUI_DialogManager *aui_dialog_manager_create(void);
void aui_dialog_manager_destroy(AUI_DialogManager *dm);

/* Update and render dialogs (call each frame) */
void aui_dialog_manager_update(AUI_DialogManager *dm, AUI_Context *ctx, float dt);
void aui_dialog_manager_render(AUI_DialogManager *dm, AUI_Context *ctx);

/* Process events through dialog manager first */
bool aui_dialog_manager_process_event(AUI_DialogManager *dm, AUI_Context *ctx,
                                       const SDL_Event *event);

/* Check if a modal dialog is open */
bool aui_dialog_manager_has_modal(AUI_DialogManager *dm);

/* ============================================================================
 * Standard Dialogs
 * ============================================================================ */

/* Message dialog with preset buttons */
void aui_dialog_message(AUI_Context *ctx, const char *title, const char *message,
                         AUI_DialogButtons buttons,
                         AUI_DialogCallback on_result, void *userdata);

/* Simple OK message */
void aui_dialog_alert(AUI_Context *ctx, const char *title, const char *message);

/* Confirmation dialog (Yes/No) */
void aui_dialog_confirm(AUI_Context *ctx, const char *title, const char *message,
                         AUI_ConfirmCallback on_result, void *userdata);

/* Text input dialog */
void aui_dialog_input(AUI_Context *ctx, const char *title, const char *prompt,
                       const char *default_text,
                       AUI_InputCallback on_result, void *userdata);

/* Input dialog with full config */
void aui_dialog_input_ex(AUI_Context *ctx, const AUI_InputDialogConfig *config);

/* Custom dialog with full config */
AUI_Node *aui_dialog_create(AUI_Context *ctx, const AUI_DialogConfig *config);

/* Close a dialog programmatically */
void aui_dialog_close(AUI_Node *dialog, AUI_DialogResult result);

/* ============================================================================
 * Context Menus
 * ============================================================================ */

/* Show context menu at position */
void aui_context_menu_show(AUI_Context *ctx, float x, float y,
                            const AUI_MenuItem *items, int count);

/* Show context menu at mouse position */
void aui_context_menu_show_at_mouse(AUI_Context *ctx,
                                     const AUI_MenuItem *items, int count);

/* Close any open context menu */
void aui_context_menu_close(AUI_Context *ctx);

/* Check if context menu is open */
bool aui_context_menu_is_open(AUI_Context *ctx);

/* ============================================================================
 * Popup Panels
 * ============================================================================ */

/* Create a popup panel (must be manually managed) */
AUI_Node *aui_popup_create(AUI_Context *ctx, const char *name);

/* Show popup at screen position */
void aui_popup_show(AUI_Node *popup, float x, float y);

/* Show popup relative to an anchor node */
void aui_popup_show_at_node(AUI_Node *popup, AUI_Node *anchor, AUI_PopupPosition pos);

/* Hide popup */
void aui_popup_hide(AUI_Node *popup);

/* Check if popup is visible */
bool aui_popup_is_visible(AUI_Node *popup);

/* ============================================================================
 * Tooltips
 * ============================================================================ */

/* Set tooltip for a node (shown on hover) */
void aui_node_set_tooltip(AUI_Node *node, const char *text);

/* Set tooltip with full config */
void aui_node_set_tooltip_ex(AUI_Node *node, const AUI_TooltipConfig *config);

/* Show tooltip immediately at position */
void aui_tooltip_show(AUI_Context *ctx, float x, float y, const char *text);

/* Show tooltip with config */
void aui_tooltip_show_ex(AUI_Context *ctx, float x, float y,
                          const AUI_TooltipConfig *config);

/* Hide tooltip */
void aui_tooltip_hide(AUI_Context *ctx);

/* ============================================================================
 * Notification Toasts (integrated from existing notification.cpp)
 * ============================================================================ */

typedef enum AUI_NotificationType {
    AUI_NOTIFY_INFO,
    AUI_NOTIFY_SUCCESS,
    AUI_NOTIFY_WARNING,
    AUI_NOTIFY_ERROR,
} AUI_NotificationType;

typedef enum AUI_NotifyPosition {
    AUI_NOTIFY_TOP_LEFT,
    AUI_NOTIFY_TOP_CENTER,
    AUI_NOTIFY_TOP_RIGHT,
    AUI_NOTIFY_BOTTOM_LEFT,
    AUI_NOTIFY_BOTTOM_CENTER,
    AUI_NOTIFY_BOTTOM_RIGHT,
} AUI_NotifyPosition;

/* Show a notification toast */
void aui_notify(AUI_Context *ctx, const char *message, AUI_NotificationType type);

/* Show notification with duration */
void aui_notify_ex(AUI_Context *ctx, const char *title, const char *message,
                    AUI_NotificationType type, float duration);

/* Set notification position */
void aui_notify_set_position(AUI_Context *ctx, AUI_NotifyPosition position);

/* Clear all notifications */
void aui_notify_clear_all(AUI_Context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_UI_DIALOG_H */
