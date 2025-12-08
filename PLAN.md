# UI Node Demo - Bug Fixes

## Summary
Fixed multiple rendering, layout, and interaction bugs in the retained-mode UI node system.

## Bugs Fixed

### 1. Layout Container Recursion Bug
**Problem**: VBox/HBox children all had the same position because the recursive layout would recalculate their positions from anchors, overwriting positions set by the layout container.

**Fix**: Added `parent_is_layout_container` parameter to `cui_node_layout_recursive_internal()` to skip anchor-based position calculation for children of VBox/HBox/Grid containers.

### 2. Button Press State Not Resetting
**Problem**: `s_pressed_node` static variable was declared inside the BUTTON_UP handler block and never assigned in BUTTON_DOWN handler.

**Fix**: Moved `s_pressed_node` declaration outside the if blocks and properly assign it when a button is pressed.

### 3. Overlapping Text in Info Panel
**Problem**: Labels in VBox had zero height because min size wasn't being calculated from text content.

**Fix**: Added `cui_node_get_content_min_size()` function that calculates minimum size based on text content for labels and buttons.

### 4. Rounded Corner Rendering Artifacts
**Problem**: Original implementation had color inconsistencies and edge artifacts in rounded corners.

**Fix**: Rewrote `cui_draw_corner_filled()` to use triangle fan rendering instead of scanlines for clean corners. Added `cui_draw_triangle()` function.

### 5. Corner Outline Arcs Missing
**Problem**: `cui_draw_rect_rounded_outline()` had `/* TODO: Draw corner arcs */` - corners weren't being drawn.

**Fix**: Implemented corner arc drawing using triangle segments in the outline function.

### 6. Settings Panel Not Appearing
**Problem**: Tween functions `cui_tween_get_property_value()` and `cui_tween_set_property_value()` were stubs that did nothing.

**Fix**: Implemented both functions to properly get/set node properties (opacity, position, scale, rotation, offsets).

### 7. Slider Not Rendering
**Problem**: `CUI_NODE_SLIDER` fell through to `default: break;` in render switch - no rendering code existed.

**Fix**: Added slider rendering code: track background, filled portion, grab handle, and optional value text.

### 8. Checkbox Not Rendering
**Problem**: `CUI_NODE_CHECKBOX` fell through to `default: break;` in render switch.

**Fix**: Added checkbox rendering: box with border, checkmark when checked, label text.

### 9. Slider Not Interactive
**Problem**: No event handling for slider mouse interaction.

**Fix**: Added mouse click and drag handling for sliders in `cui_scene_process_event()`.

### 10. Checkbox Not Interactive
**Problem**: Checkboxes didn't toggle when clicked.

**Fix**: Added checkbox toggle handling on MOUSE_BUTTON_UP in event processing.

### 11. Checkbox/Slider Too Small
**Problem**: No default minimum sizes set, causing widgets to be tiny in layouts.

**Fix**: Added default min sizes - Checkbox: 200x24, Slider: 100x24. Set slider step to 0 for smooth dragging.

## Files Modified
- `src/ui/ui_node.cpp` - Widget rendering, event handling, default sizes
- `src/ui/ui_style.cpp` - Rounded corner rendering with triangle fans
- `src/ui/ui_draw.cpp` - Added `cui_draw_triangle()` function
- `src/ui/ui_tween.cpp` - Implemented property get/set functions
- `include/carbon/ui.h` - Added `cui_draw_triangle()` declaration
- `examples/ui_node/main.cpp` - Button alignment fix (VBox padding)

## Known Issues
- Checkbox text labels not visible (may need h_size_flags = CUI_SIZE_FILL)

## Running the Demo
```bash
make DEBUG=1 example-ui-node
```

## Features Working
- Godot-style anchor presets (CENTER, FULL_RECT, TOP_WIDE, etc.)
- VBox/HBox automatic layout with proper child positioning
- Signal-based event handling (button clicks, hover, slider changes, checkbox toggles)
- Button press/hover visual feedback
- Tween animations (fade in/out, slide, shake, scale pop)
- Rich styling (solid backgrounds, borders, rounded corners)
- Slider interaction (click and drag)
- Checkbox toggle interaction
- Panel fade-in animation
- FPS counter in status bar
- Keyboard shortcuts (ESC to quit, F1 toggle menu, F2 animate)
