# UI Node Demo - All Issues Fixed

## Summary
Fixed multiple rendering and layout bugs in the retained-mode UI node system.

## Bugs Fixed

### 1. Layout Container Recursion Bug (Original Issue)
**Problem**: VBox/HBox children all had the same position because the recursive layout would recalculate their positions from anchors, overwriting positions set by the layout container.

**Fix**: Added `parent_is_layout_container` parameter to `cui_node_layout_recursive_internal()` to skip anchor-based position calculation for children of VBox/HBox/Grid containers.

### 2. Button Press State Not Resetting
**Problem**: `s_pressed_node` static variable was declared inside the BUTTON_UP handler block and never assigned in BUTTON_DOWN handler.

**Fix**: Moved `s_pressed_node` declaration outside the if blocks and properly assign it when a button is pressed.

### 3. Overlapping Text in Info Panel
**Problem**: Labels in VBox had zero height because min size wasn't being calculated from text content.

**Fix**: Added `cui_node_get_content_min_size()` function that calculates minimum size based on text content for labels and buttons. Modified `cui_node_layout_vbox_ctx()` to use this function with context access.

### 4. Rounded Corners Not Rendering
**Problem**: The rounded corner implementation only drew 1x1 pixel dots instead of filled quarter circles.

**Fix**: Rewrote `cui_draw_rect_rounded_ex()` with proper scanline-based filled quarter circle rendering using `cui_draw_corner_filled()`.

### 5. Title Text Not Centered
**Problem**: Label text was always left-aligned regardless of positioning.

**Fix**: Added horizontal text alignment support in label rendering that respects `CUI_SIZE_SHRINK_CENTER` and `CUI_SIZE_SHRINK_END` flags. Updated demo to use `SHRINK_CENTER` for the title.

## Files Modified
- `src/ui/ui_node.cpp` - Layout recursion fix, press state fix, content min size, label alignment
- `src/ui/ui_style.cpp` - Rounded corner rendering
- `examples/ui_node/main.cpp` - Title centering with size flags

## Running the Demo
```bash
make example-ui-node
```

## Features Working
- Godot-style anchor presets (CENTER, FULL_RECT, TOP_WIDE, etc.)
- VBox/HBox automatic layout with proper child positioning
- Signal-based event handling (button clicks, hover)
- Button press/hover visual feedback
- Tween animations (fade in/out, slide, shake, scale pop)
- Rich styling (solid backgrounds, borders, rounded corners)
- Text alignment in labels (left, center, right)
- FPS counter in status bar
- Keyboard shortcuts (ESC to quit, F1 toggle menu, F2 animate)
