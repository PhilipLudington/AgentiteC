# Bug: Modal Dialog Buttons Not Receiving Click Events

## Status
**Fixed** - Both issues resolved (lambda issue + layout issue)

## Symptoms
1. Open a scene in the editor
2. Make a change (e.g., drag an asset to hierarchy to make scene dirty)
3. File > Open to open another scene
4. "Unsaved Changes" confirm dialog appears
5. **Bug:** Clicking Yes/No buttons does nothing - app appears frozen
6. All input is blocked by the modal dialog that cannot be dismissed

## Root Cause Analysis

### Issue 1: Lambda Lifetime (FIXED)
The `aui_dialog_confirm()` function used a C++ lambda with automatic storage duration as a callback:

```cpp
// BROKEN - lambda is destroyed when function returns
auto confirm_handler = [](AUI_DialogResult result, void *ud) {
    // ...
};
aui_dialog_message(ctx, title, message, AUI_BUTTONS_YES_NO,
                    confirm_handler, wrapper);  // Stores invalid pointer
```

**Fix applied:** Replaced with static function `confirm_handler_static()`.

### Issue 2: Dialog Layout Not Computed (OPEN)
Modal dialogs are created as orphan nodes not connected to the main UI tree:

```cpp
AUI_Node *panel = aui_panel_create(ctx, "dialog", config->title);
// panel has no parent - it's a standalone node
entry->node = panel;
```

The dialog uses anchor-based positioning:
```cpp
aui_node_set_anchor_preset(panel, AUI_ANCHOR_CENTER);
aui_node_set_offsets(panel, -dialog_w / 2, -dialog_h / 2, ...);
```

**Problem:** Anchors require a parent node with computed `global_rect` to resolve positions. Without a parent:
- `panel->global_rect` is never computed
- Child nodes (buttons) also have invalid `global_rect`
- Hit testing in `aui_scene_process_event()` fails
- Button clicks are never detected

## Event Flow (Current - Broken)

```
User clicks on dialog button
    |
    v
aui_dialog_manager_process_event()
    |
    v
Modal dialog found (active=1, modal=1)
    |
    v
aui_scene_process_event(ctx, entry->node, event)
    |
    v
Hit test against entry->node->global_rect
    |
    v
FAIL: global_rect is {0,0,0,0} because layout never computed
    |
    v
Event not handled, but still blocked by modal
    |
    v
User stuck - can't click buttons, can't click elsewhere
```

## Files Involved

- `lib/agentite/src/ui/ui_dialog.cpp`
  - `aui_dialog_create()` - creates orphan panel node (line ~658)
  - `aui_dialog_manager_process_event()` - modal blocking (line ~461)
  - `aui_dialog_button_clicked()` - never called due to layout issue

## Proposed Fix

Option A: **Compute layout manually for dialogs**
```cpp
// In aui_dialog_create() or aui_dialog_manager_update()
void compute_dialog_layout(AUI_Context *ctx, AUI_Node *dialog) {
    // Use screen size as parent rect
    AUI_Rect parent_rect = {0, 0, ctx->width, ctx->height};
    aui_node_compute_layout(dialog, &parent_rect);
}
```

Option B: **Use absolute positioning instead of anchors**
```cpp
// In aui_dialog_create()
float dialog_x = (ctx->width - dialog_w) / 2;
float dialog_y = (ctx->height - dialog_h) / 2;
panel->global_rect = {dialog_x, dialog_y, dialog_w, dialog_h};
// Also compute child rects manually
```

Option C: **Attach dialogs to a root node temporarily**
```cpp
// Create a screen-sized root for dialog layout
AUI_Node *dialog_root = get_or_create_dialog_root(ctx);
aui_node_add_child(dialog_root, panel);
aui_scene_update(ctx, dialog_root, 0);  // Computes layouts
```

## Fix Applied (Option C)

Added a `dialog_root` node to `AUI_DialogManager` that serves as the parent for all dialogs:

1. **Added `dialog_root` field** to `AUI_DialogManager` struct
2. **Added `ensure_dialog_root()`** helper for lazy creation
3. **Layout computed in `update()`** - `aui_scene_layout()` called on dialog_root each frame
4. **Dialogs attached to root** on creation via `aui_node_add_child()`
5. **Dialogs detached** before destruction via `aui_node_remove_child()`
6. **Cleanup** of dialog_root in manager destroy

This ensures `global_rect` is always computed before event processing, fixing hit testing.

## Related

- Scene loading crash was also fixed in this investigation (commit 4b0abb4)
- Lambda fix applied but not yet committed to agentite submodule
