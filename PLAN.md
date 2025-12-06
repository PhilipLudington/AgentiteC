# Carbon UI System Expansion Plan

## Overview

Expand Carbon's immediate-mode UI to a **hybrid system** combining the existing `cui_*` API with a new retained-mode scene tree, inspired by Godot's GUI system.

**Architecture Decision**: Hybrid approach
- **Layer 1**: Enhanced immediate-mode for HUDs, debug UI, simple elements
- **Layer 2**: Retained-mode node tree for complex screens (tech trees, dialogs, menus)
- Both layers share the same GPU batch renderer and theme system

---

## New File Structure

### Headers (`include/carbon/`)
```
ui_style.h        - Rich styling (gradients, 9-slice, shadows, borders)
ui_node.h         - Retained-mode node hierarchy
ui_containers.h   - Layout containers (VBox, HBox, Grid, Margin, Scroll)
ui_tween.h        - Animation/tween engine with easing
ui_dialog.h       - Modal dialogs, popups, context menus
ui_richtext.h     - BBCode-style rich text
ui_charts.h       - Line, bar, pie charts
```

### Sources (`src/ui/`)
```
ui_style.cpp      - Style system implementation
ui_node.cpp       - Node tree, hierarchy, layout engine
ui_containers.cpp - Container implementations
ui_tween.cpp      - Tween engine, 25 easing functions
ui_signals.cpp    - Signal/callback system
ui_dialog.cpp     - Dialog/popup implementation
ui_richtext.cpp   - BBCode parser and renderer
ui_charts.cpp     - Chart widgets
```

---

## Implementation Phases

### Phase 1: Foundation
**Rich Styling System**
- `CUI_Style` struct: padding, margin, borders, corners, backgrounds, shadows, opacity
- `CUI_Background` types: solid, gradient (linear/radial), texture, 9-slice
- `CUI_Shadow` for drop shadows and inner shadows
- Style class registry with inheritance
- `cui_push_style()` / `cui_pop_style()` for immediate mode

**Enhanced Drawing** (extend `ui_draw.cpp`)
- 9-slice rendering for scalable panels
- Linear/radial gradient rendering
- Shadow rendering
- Per-corner rounded rectangles

**Easing Functions**
- 25 standard easing curves (sine, quad, cubic, expo, back, elastic, bounce)

### Phase 2: Node System
**Node Hierarchy** (`ui_node.cpp`)
- `CUI_Node` base struct with type, name, parent/child pointers
- Tree traversal, path-based lookup (`"panel/content/button"`)
- Node creation/destruction lifecycle

**Anchor-Based Layout**
- Godot-style anchors (0-1 relative to parent) + pixel offsets
- Anchor presets (top-left, center, full-rect, etc.)
- Size flags: fill, expand, shrink, center
- Min/max size constraints
- Dirty flagging and layout caching

**Signal System**
- `cui_node_connect()` / `cui_node_disconnect()`
- Signal types: pressed, clicked, hovered, focused, value_changed, resized
- Callback with userdata

### Phase 3: Containers
- **VBoxContainer / HBoxContainer**: separation, alignment, reverse
- **GridContainer**: column count, h/v separation
- **MarginContainer**: configurable margins
- **ScrollContainer**: h/v scroll, content size tracking, scroll-to

### Phase 4: Animation System
**Property Tweens**
- Target node + property (position, size, opacity, color, offsets)
- Start/end values, duration, delay, easing
- Completion callbacks

**Tween Manager**
- `cui_tween_property()` - animate any property
- Convenience: `cui_tween_fade_in()`, `cui_tween_slide_in()`, `cui_tween_scale_pop()`
- Pause/resume/stop control

**Sequences**
- Chain tweens sequentially or run in parallel
- Loop support

### Phase 5: Priority Widgets

#### Dialogs & Popups (`ui_dialog.cpp`)
- `cui_dialog_message()` - title, message, OK/Cancel/Yes/No buttons
- `cui_dialog_confirm()` - yes/no with callback
- `cui_dialog_input()` - text input dialog
- `cui_dialog_custom()` - fully custom content
- Modal overlay with focus trapping
- Animated entry/exit via tween system
- **Context Menus**: `cui_context_menu_show()` with nested submenus
- **Popup Panels**: anchor to widgets, auto-positioning

#### Rich Text (`ui_richtext.cpp`)
BBCode tags:
- `[b]`, `[i]`, `[u]`, `[s]` - bold, italic, underline, strikethrough
- `[color=#RRGGBB]`, `[size=N]` - text formatting
- `[url=...]` - clickable links with callback
- `[img]path[/img]`, `[icon=name]` - inline images/icons
- `[wave]`, `[shake]`, `[rainbow]` - animated text effects

Implementation:
- Single-pass BBCode parser with tag stack
- Glyph-level layout for mixed formatting
- Word-wrap with style spans
- Hit testing for link clicks
- Animation state updated each frame

#### Charts (`ui_charts.cpp`)
- **Line Chart**: multiple series, points, smooth lines
- **Bar Chart**: grouped and stacked variants
- **Pie Chart**: slices with labels, exploded segments
- Features:
  - Auto-scaling Y axis with nice numbers
  - Grid lines and axes
  - Legend (top/bottom/right)
  - Hover tooltips
  - Animated entry (bars grow, pie slices expand)

### Phase 6: Integration & Polish
- Retained nodes use existing `CUI_Theme`
- Immediate-mode widgets embeddable in retained nodes
- JSON serialization for node trees
- Hot-reload support for UI definitions

---

## Key Data Structures

### CUI_Style
```c
typedef struct CUI_Style {
    CUI_Edges padding, margin;
    CUI_Border border;
    CUI_CornerRadius corner_radius;
    CUI_Background background, background_hover, background_active;
    CUI_Shadow shadows[4]; int shadow_count;
    float opacity;
    float min_width, min_height, max_width, max_height;
    uint32_t text_color, text_color_hover;
} CUI_Style;
```

### CUI_Node
```c
typedef struct CUI_Node {
    uint32_t id;
    CUI_NodeType type;
    char name[64];
    CUI_Node *parent, *first_child, *next_sibling;
    CUI_Anchors anchors;      // 0-1 relative positions
    CUI_Edges offset;         // pixel offsets from anchors
    CUI_Rect rect, global_rect;
    CUI_Style style;
    bool visible, enabled, focused, hovered;
    CUI_Connection connections[16];
    union { /* type-specific data */ };
    void (*on_draw)(CUI_Node*, CUI_Context*);
    void (*on_input)(CUI_Node*, CUI_Context*, const SDL_Event*);
} CUI_Node;
```

### CUI_PropertyTween
```c
typedef struct CUI_PropertyTween {
    CUI_Node *target;
    CUI_TweenProperty property;
    float start_value, end_value;
    float duration, elapsed, delay;
    CUI_EaseType ease;
    void (*on_complete)(struct CUI_PropertyTween*, void*);
} CUI_PropertyTween;
```

---

## Critical Files to Modify

| File | Changes |
|------|---------|
| `include/carbon/ui.h` | Add includes for new headers, extend `CUI_Context` with node root, tween manager, dialog stack |
| `src/ui/ui_draw.cpp` | Add 9-slice, gradient, shadow rendering functions |
| `src/ui/ui.cpp` | Integrate scene tree update/render into frame lifecycle |

---

## API Examples

### Retained Mode Scene
```c
CUI_Node *panel = cui_node_create(ctx, CUI_NODE_PANEL, "settings");
cui_node_set_anchor_preset(panel, CUI_ANCHOR_CENTER);
cui_node_set_offsets(panel, -200, -150, 200, 150); // 400x300 centered

CUI_Node *vbox = cui_vbox_create(ctx, "content");
cui_box_set_separation(vbox, 8);
cui_node_add_child(panel, vbox);

CUI_Node *btn = cui_node_create(ctx, CUI_NODE_BUTTON, "apply");
cui_node_connect(btn, CUI_SIGNAL_CLICKED, on_apply_clicked, NULL);
cui_node_add_child(vbox, btn);
```

### Tween Animation
```c
cui_tween_fade_in(tweens, panel, 0.3f);
cui_tween_property(tweens, panel, CUI_TWEEN_OFFSET_TOP, -100, 0.4f, CUI_EASE_OUT_BACK);
```

### Dialog
```c
cui_dialog_confirm(ctx, "Quit Game", "Are you sure you want to quit?",
                   on_quit_confirmed, NULL);
```

### Rich Text
```c
cui_rich_label(ctx, "[color=#FFD700]Gold:[/color] 1,234 [icon=coin]");
```

### Chart
```c
CUI_ChartConfig cfg = {
    .type = CUI_CHART_LINE,
    .title = "Population",
    .series = &pop_series,
    .series_count = 1,
    .show_grid = true
};
cui_draw_line_chart(ctx, bounds, &cfg);
```

---

## Estimated Scope

- ~8-10 new source files
- ~3000-5000 lines of new code
- Backward compatible with existing `cui_*` API
