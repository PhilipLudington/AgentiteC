# Dialog System Example

Demonstrates the AUI dialog and popup system including modal dialogs, context menus, file dialogs, and toast notifications.

## Features

- **Message Dialogs**: Alert, OK/Cancel, Yes/No, Yes/No/Cancel
- **Input Dialogs**: Text input with validation support
- **File Dialogs**: Native OS open/save/folder dialogs
- **Context Menus**: Right-click popup menus with shortcuts
- **Toast Notifications**: Info, success, warning, error messages

## Controls

| Input | Action |
|-------|--------|
| Left-click | Interact with buttons |
| Right-click | Show context menu |
| ESC | Quit |

## Building

```bash
make example-dialogs
```

## API Overview

### Message Dialogs
```c
aui_dialog_alert(ctx, "Title", "Message");
aui_dialog_confirm(ctx, "Title", "Message", callback, userdata);
aui_dialog_message(ctx, "Title", "Message", AUI_BUTTONS_YES_NO, callback, userdata);
```

### Input Dialogs
```c
aui_dialog_input(ctx, "Title", "Prompt", "default", callback, userdata);
```

### File Dialogs
```c
AUI_FileFilter filters[] = {{"Images", "*.png;*.jpg"}};
aui_file_dialog_open(ctx, "Open", NULL, filters, 1, callback, userdata);
aui_file_dialog_save(ctx, "Save", "file.txt", filters, 1, callback, userdata);
aui_file_dialog_folder(ctx, "Select Folder", NULL, callback, userdata);
```

### Context Menus
```c
AUI_MenuItem items[] = {
    {.label = "Cut", .shortcut = "Ctrl+X", .on_select = on_cut},
    {.label = NULL},  // separator
    {.label = "Paste", .shortcut = "Ctrl+V", .on_select = on_paste},
};
aui_context_menu_show(ctx, x, y, items, 3);
```

### Notifications
```c
aui_notify(ctx, "Message", AUI_NOTIFY_SUCCESS);
aui_notify_set_position(ctx, AUI_NOTIFY_TOP_RIGHT);
```
