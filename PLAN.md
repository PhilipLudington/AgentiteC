# UI Node Styling Enhancement Plan

## Completed

### Text Styling (Done)
- Text alignment (left, center, right, justify) and vertical alignment
- Text overflow modes (visible, clip, ellipsis, wrap)
- Text shadow with blur approximation
- Line height and letter spacing controls
- Word wrapping with max lines limit

### Style Transitions (Done)
- Transition duration configurable per-style
- Easing functions for smooth transitions (linear, quad, cubic variants)
- Auto-animate background and text color on state change (hover, pressed)
- Supports mid-transition interruption (smoothly transitions from current interpolated color)

Usage example:
```c
CUI_Style style = cui_style_default();
style.background = cui_bg_solid(0x3A3A5AFF);
style.background_hover = cui_bg_solid(0x4A4A7AFF);
style.background_active = cui_bg_solid(0x2A2A4AFF);
style.transition = cui_transition(0.15f, CUI_TRANS_EASE_OUT_QUAD);
```

## Pending Enhancements

### 1. Pseudo-selectors/States
- Focus state styling (currently only has hover/active/disabled)
- Selected state (for items in lists/tabs)
- First-child/last-child styling

### 2. Outline
- Separate from border, for focus indication
- Outline offset and style options

### 3. Transform Properties
- Transform origin support (already has pivot_x/pivot_y but not fully integrated)
- Skew transforms
- Better integration of scale/rotation with rendering

### 4. Additional Visual Effects
- Blur backdrop filter
- Box glow effects
- Pattern/tiled backgrounds
- Gradient borders

### 5. Layout-Affecting Styles
- Gap property (like CSS flexbox gap)
- Aspect ratio constraints
- Better flex/grow controls

## Priority Recommendations

1. **Focus/Selected States** - Important for accessibility and selection UIs
2. **Outline** - Standard focus indicator separate from border
3. **Transform Properties** - Enable more complex animations
