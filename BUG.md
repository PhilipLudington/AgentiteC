# HiDPI Rendering Bug

## Status: RESOLVED (2026-01-05)

## Summary

All HiDPI rendering issues have been fixed, including the transitions example.

## What Was Fixed

### 1. Swapchain Viewport (Core Fix)
**File:** `src/core/engine.cpp`

Changed viewport from logical to physical dimensions:
```cpp
// Before (broken):
viewport.w = (float)engine->logical_width;   // 1280
viewport.h = (float)engine->logical_height;  // 720

// After (fixed):
viewport.w = (float)engine->physical_width;  // 2560
viewport.h = (float)engine->physical_height; // 1440
```
Also updated scissor rect to match.

### 2. Lighting Shader Vertex Transformations
**File:** `src/graphics/lighting_shaders.h`

Fixed all 5 lighting shaders (point_light, point_light_shadow, spot_light, composite, ambient). The fullscreen quad vertex transformation was wrong:
```metal
// Before (only covered upper-right quadrant):
out.position = float4(in.position, 0.0, 1.0);

// After (covers full screen):
float2 clip_pos = in.position * 2.0 - 1.0;
clip_pos.y = -clip_pos.y;
out.position = float4(clip_pos, 0.0, 1.0);
```

### 3. Sprite Positioning for Centered Origin
Multiple examples had incorrect positioning formulas. Sprites with default origin (0.5, 0.5) are CENTERED on the draw position, not drawn from top-left.

**Files fixed:**
- `examples/hidpi/main.cpp` - Corner positioning
- `examples/transitions/main.cpp` - Sprite centering
- `examples/lighting/main.cpp` - Sprite centering

```cpp
// Old (wrong for centered origin):
float px = (WINDOW_WIDTH - 512) / 2.0f;   // 384

// New (correct for centered origin):
float px = WINDOW_WIDTH / 2.0f;           // 640
```

**Note:** `examples/shaders/main.cpp` explicitly sets `origin_x = 0.0f; origin_y = 0.0f;` so it correctly uses the old formula.

## Verified Working

| Example | Status | Notes |
|---------|--------|-------|
| hidpi | WORKING | Red borders at edges, corners work with 1-4 keys |
| shaders | WORKING | Sprite centered, effects work |
| lighting | WORKING | Floor centered, lighting/shadows work |
| transitions | WORKING | Sprite centered, transitions work |

## Previously Remaining Issues (Now Fixed)

### Transitions Example - FIXED

Two issues were identified and fixed:

1. **Vertical centering** - The `+ 50.0f` offset was unnecessary and pushed the sprite below center
   - **Fix:** Removed the offset in `examples/transitions/main.cpp`

2. **Transitions not working** - `agentite_shader_draw_fullscreen()` was missing the projection matrix push
   - **Fix:** Added `glm_ortho()` projection matrix push in `src/graphics/shader.cpp`
   - The builtin shaders (brightness, pixelate, etc.) require a projection matrix in the vertex shader uniform buffer

## Environment

```
Window: 1280x720 logical, 2560x1440 physical
DPI scale: 2.00
GPU: Metal (macOS)
```

## Key Learnings

1. **Swapchain viewport must use physical dimensions** - The GPU renders to physical pixels
2. **Render-to-texture viewports use logical dimensions** - Match the texture size
3. **Sprite origin affects positioning math** - Default (0.5, 0.5) = centered, (0, 0) = top-left
4. **Fullscreen quad shaders need proper clip space transform** - Unit coords (0-1) must map to clip space (-1 to +1) with Y flip

## Files Modified

- `src/core/engine.cpp` - Viewport fix
- `src/graphics/lighting_shaders.h` - All 5 shader vertex transforms
- `src/graphics/shader.cpp` - Added projection matrix to `agentite_shader_draw_fullscreen()`
- `examples/hidpi/main.cpp` - Corner positioning
- `examples/transitions/main.cpp` - Sprite centering (removed +50 offset)
- `examples/lighting/main.cpp` - Sprite centering
- `Makefile` - Renamed hidpi-test to hidpi
