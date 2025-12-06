/*
 * Carbon UI - Dialog and Popup System
 *
 * Provides modal dialogs, context menus, and popup panels.
 *
 * Usage:
 *   // Message dialog
 *   cui_dialog_message(ctx, "Error", "File not found!",
 *                      CUI_BUTTONS_OK, on_dialog_closed, userdata);
 *
 *   // Confirmation dialog
 *   cui_dialog_confirm(ctx, "Delete", "Are you sure?",
 *                      on_confirm_result, userdata);
 *
 *   // Context menu
 *   CUI_MenuItem items[] = {
 *       {"Cut", "Ctrl+X", on_cut, NULL},
 *       {"Copy", "Ctrl+C", on_copy, NULL},
 *       {NULL}, // separator
 *       {"Paste", "Ctrl+V", on_paste, NULL},
 *   };
 *   cui_context_menu_show(ctx, mouse_x, mouse_y, items, 4);
 */

#ifndef CARBON_UI_DIALOG_H
#define CARBON_UI_DIALOG_H

#include "carbon/ui_node.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Dialog Result
 * ============================================================================ */

typedef enum CUI_DialogResult {
    CUI_DIALOG_NONE = 0,
    CUI_DIALOG_OK,
    CUI_DIALOG_CANCEL,
    CUI_DIALOG_YES,
    CUI_DIALOG_NO,
    CUI_DIALOG_ABORT,
    CUI_DIALOG_RETRY,
    CUI_DIALOG_IGNORE,
    CUI_DIALOG_CLOSE,          /* Closed via X button */
    CUI_DIALOG_CUSTOM_1,
    CUI_DIALOG_CUSTOM_2,
    CUI_DIALOG_CUSTOM_3,
} CUI_DialogResult;

/* ============================================================================
 * Dialog Button Presets
 * ============================================================================ */

typedef enum CUI_DialogButtons {
    CUI_BUTTONS_NONE,
    CUI_BUTTONS_OK,
    CUI_BUTTONS_OK_CANCEL,
    CUI_BUTTONS_YES_NO,
    CUI_BUTTONS_YES_NO_CANCEL,
    CUI_BUTTONS_ABORT_RETRY_IGNORE,
    CUI_BUTTONS_RETRY_CANCEL,
    CUI_BUTTONS_CUSTOM,
} CUI_DialogButtons;

/* ============================================================================
 * Dialog Callbacks
 * ============================================================================ */

/* Called when dialog closes */
typedef void (*CUI_DialogCallback)(CUI_DialogResult result, void *userdata);

/* Called when confirmation dialog closes */
typedef void (*CUI_ConfirmCallback)(bool confirmed, void *userdata);

/* Called when input dialog closes */
typedef void (*CUI_InputCallback)(bool confirmed, const char *text, void *userdata);

/* ============================================================================
 * Dialog Configuration
 * ============================================================================ */

typedef struct CUI_DialogConfig {
    /* Content */
    const char *title;
    const char *message;
    const char *icon;          /* Icon name (optional) */

    /* Buttons */
    CUI_DialogButtons buttons;
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
    CUI_DialogCallback on_result;
    void *userdata;

    /* Animation */
    bool animate;
    float animation_duration;
} CUI_DialogConfig;

/* ============================================================================
 * Input Dialog Configuration
 * ============================================================================ */

typedef struct CUI_InputDialogConfig {
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

    CUI_InputCallback on_result;
    void *userdata;
} CUI_InputDialogConfig;

/* ============================================================================
 * File Dialog (placeholder for future implementation)
 * ============================================================================ */

typedef enum CUI_FileDialogType {
    CUI_FILE_DIALOG_OPEN,
    CUI_FILE_DIALOG_SAVE,
    CUI_FILE_DIALOG_SELECT_FOLDER,
} CUI_FileDialogType;

typedef struct CUI_FileDialogConfig {
    CUI_FileDialogType type;
    const char *title;
    const char *default_path;
    const char **filters;      /* e.g., {"*.png", "*.jpg"} */
    int filter_count;
    const char *filter_description;
    bool allow_multiple;

    void (*on_result)(bool success, const char **paths, int count, void *userdata);
    void *userdata;
} CUI_FileDialogConfig;

/* ============================================================================
 * Context Menu Item
 * ============================================================================ */

typedef struct CUI_MenuItem {
    const char *label;         /* NULL = separator */
    const char *shortcut;      /* Display text only (e.g., "Ctrl+C") */
    const char *icon;          /* Icon name (optional) */
    bool enabled;
    bool checked;              /* Show checkmark */
    bool radio;                /* Radio button style */

    /* Submenu (if present, on_select is ignored) */
    struct CUI_MenuItem *submenu;
    int submenu_count;

    /* Action */
    void (*on_select)(void *userdata);
    void *userdata;
} CUI_MenuItem;

/* ============================================================================
 * Popup Position
 * ============================================================================ */

typedef enum CUI_PopupPosition {
    CUI_POPUP_BELOW,           /* Below anchor, aligned left */
    CUI_POPUP_ABOVE,           /* Above anchor, aligned left */
    CUI_POPUP_LEFT,            /* Left of anchor, aligned top */
    CUI_POPUP_RIGHT,           /* Right of anchor, aligned top */
    CUI_POPUP_BELOW_CENTER,    /* Below anchor, centered */
    CUI_POPUP_ABOVE_CENTER,    /* Above anchor, centered */
} CUI_PopupPosition;

/* ============================================================================
 * Tooltip Configuration
 * ============================================================================ */

typedef struct CUI_TooltipConfig {
    const char *text;
    float delay;               /* Seconds before showing */
    float duration;            /* 0 = until mouse moves */
    float max_width;           /* Word wrap width */
    bool rich_text;            /* Parse BBCode */
} CUI_TooltipConfig;

/* ============================================================================
 * Dialog Manager (opaque, stored in CUI_Context)
 * ============================================================================ */

typedef struct CUI_DialogManager CUI_DialogManager;

/* Create/destroy dialog manager */
CUI_DialogManager *cui_dialog_manager_create(void);
void cui_dialog_manager_destroy(CUI_DialogManager *dm);

/* Update and render dialogs (call each frame) */
void cui_dialog_manager_update(CUI_DialogManager *dm, CUI_Context *ctx, float dt);
void cui_dialog_manager_render(CUI_DialogManager *dm, CUI_Context *ctx);

/* Process events through dialog manager first */
bool cui_dialog_manager_process_event(CUI_DialogManager *dm, CUI_Context *ctx,
                                       const SDL_Event *event);

/* Check if a modal dialog is open */
bool cui_dialog_manager_has_modal(CUI_DialogManager *dm);

/* ============================================================================
 * Standard Dialogs
 * ============================================================================ */

/* Message dialog with preset buttons */
void cui_dialog_message(CUI_Context *ctx, const char *title, const char *message,
                         CUI_DialogButtons buttons,
                         CUI_DialogCallback on_result, void *userdata);

/* Simple OK message */
void cui_dialog_alert(CUI_Context *ctx, const char *title, const char *message);

/* Confirmation dialog (Yes/No) */
void cui_dialog_confirm(CUI_Context *ctx, const char *title, const char *message,
                         CUI_ConfirmCallback on_result, void *userdata);

/* Text input dialog */
void cui_dialog_input(CUI_Context *ctx, const char *title, const char *prompt,
                       const char *default_text,
                       CUI_InputCallback on_result, void *userdata);

/* Input dialog with full config */
void cui_dialog_input_ex(CUI_Context *ctx, const CUI_InputDialogConfig *config);

/* Custom dialog with full config */
CUI_Node *cui_dialog_create(CUI_Context *ctx, const CUI_DialogConfig *config);

/* Close a dialog programmatically */
void cui_dialog_close(CUI_Node *dialog, CUI_DialogResult result);

/* ============================================================================
 * Context Menus
 * ============================================================================ */

/* Show context menu at position */
void cui_context_menu_show(CUI_Context *ctx, float x, float y,
                            const CUI_MenuItem *items, int count);

/* Show context menu at mouse position */
void cui_context_menu_show_at_mouse(CUI_Context *ctx,
                                     const CUI_MenuItem *items, int count);

/* Close any open context menu */
void cui_context_menu_close(CUI_Context *ctx);

/* Check if context menu is open */
bool cui_context_menu_is_open(CUI_Context *ctx);

/* ============================================================================
 * Popup Panels
 * ============================================================================ */

/* Create a popup panel (must be manually managed) */
CUI_Node *cui_popup_create(CUI_Context *ctx, const char *name);

/* Show popup at screen position */
void cui_popup_show(CUI_Node *popup, float x, float y);

/* Show popup relative to an anchor node */
void cui_popup_show_at_node(CUI_Node *popup, CUI_Node *anchor, CUI_PopupPosition pos);

/* Hide popup */
void cui_popup_hide(CUI_Node *popup);

/* Check if popup is visible */
bool cui_popup_is_visible(CUI_Node *popup);

/* ============================================================================
 * Tooltips
 * ============================================================================ */

/* Set tooltip for a node (shown on hover) */
void cui_node_set_tooltip(CUI_Node *node, const char *text);

/* Set tooltip with full config */
void cui_node_set_tooltip_ex(CUI_Node *node, const CUI_TooltipConfig *config);

/* Show tooltip immediately at position */
void cui_tooltip_show(CUI_Context *ctx, float x, float y, const char *text);

/* Show tooltip with config */
void cui_tooltip_show_ex(CUI_Context *ctx, float x, float y,
                          const CUI_TooltipConfig *config);

/* Hide tooltip */
void cui_tooltip_hide(CUI_Context *ctx);

/* ============================================================================
 * Notification Toasts (integrated from existing notification.cpp)
 * ============================================================================ */

typedef enum CUI_NotificationType {
    CUI_NOTIFY_INFO,
    CUI_NOTIFY_SUCCESS,
    CUI_NOTIFY_WARNING,
    CUI_NOTIFY_ERROR,
} CUI_NotificationType;

typedef enum CUI_NotifyPosition {
    CUI_NOTIFY_TOP_LEFT,
    CUI_NOTIFY_TOP_CENTER,
    CUI_NOTIFY_TOP_RIGHT,
    CUI_NOTIFY_BOTTOM_LEFT,
    CUI_NOTIFY_BOTTOM_CENTER,
    CUI_NOTIFY_BOTTOM_RIGHT,
} CUI_NotifyPosition;

/* Show a notification toast */
void cui_notify(CUI_Context *ctx, const char *message, CUI_NotificationType type);

/* Show notification with duration */
void cui_notify_ex(CUI_Context *ctx, const char *title, const char *message,
                    CUI_NotificationType type, float duration);

/* Set notification position */
void cui_notify_set_position(CUI_Context *ctx, CUI_NotifyPosition position);

/* Clear all notifications */
void cui_notify_clear_all(CUI_Context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_UI_DIALOG_H */
