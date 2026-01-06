# Localization Demo - Font Issues

## Status: ALL ISSUES FIXED

---

## Issue 3: Font Switching Breaks MSDF Languages (FIXED)

### Root Causes
1. **`s_current_font_key` tracking bug**: The `update_font_for_language()` function used a static variable to track the current font key and skip updates. When manually switching to bitmap font for Controls panel, this tracking wasn't updated, so subsequent frames would skip setting the MSDF font.

2. **`aui_set_font()` didn't flush batches**: When switching between fonts (especially between MSDF and bitmap), the current draw batch wasn't being flushed, which could cause rendering issues with mixed font types.

### Fixes Applied
1. **Removed `s_current_font_key` tracking** - Renamed `update_font_for_language()` to `set_font_for_language()` and removed the caching. The function now always sets the font unconditionally.

2. **`aui_set_font()` now flushes on font change** - Added batch flushing in `src/ui/ui_text.cpp` when the font actually changes, ensuring clean transitions between font types.

3. **Proper font switching in main.cpp**:
   - Set language font at start of main panel rendering
   - Switch to default bitmap font before Controls panel
   - Key handlers no longer call font update (font is set per-frame)

3. **Arabic font lacks Latin characters** - Al Nile.ttc doesn't have A-Z letters. Fixed by using default bitmap font for English text (locale info, player name, labels) and `aui_same_line()` to keep mixed-font content on same line.

4. **Updated charsets** - Japanese charset was missing some hiragana/katakana (と、を、コ) and punctuation (、！). Arabic charset simplified to exact 30 characters needed.

### Files Modified
- `src/ui/ui_text.cpp` - `aui_set_font()` now flushes batch on font change
- `src/ui/ui_draw.cpp` - Made `aui_flush_draw_cmd()` non-static for cross-file access
- `examples/localization/main.cpp`:
  - Proper per-frame font switching
  - Use default font for English text with Arabic font for Arabic content
  - `aui_same_line()` for mixed-font labels
  - Updated CHARSET_CJK and CHARSET_ARABIC

---

## Issue 2: MSDF Text Not Rendering (FIXED)

### Symptoms
- Japanese and Arabic text not visible when switching languages in demo
- Fonts load successfully (181 CJK glyphs, 124 Arabic glyphs)
- MSDF atlas data is valid (verified via dump file analysis)
- No error messages

### Resolution
The root cause was **incorrect UV coordinates** for MSDF atlas sampling. The atlas stores glyphs with Y-down (row 0 is top), same as SDL_GPU/Metal textures. But `msdf_atlas_get_glyph()` was flipping Y to match OpenGL's `yOrigin=bottom` convention, causing UVs to sample from v≈1.0 (bottom) instead of v≈0 (top) where glyphs actually are.

### Root Causes Found & Fixed

#### Fix 3: MSDF Atlas UV Y-Flip (THE ACTUAL BUG)
**File:** `src/graphics/msdf_atlas.cpp`

**Before (wrong - flipped Y for OpenGL):**
```cpp
out_info->atlas_bottom = (atlas->atlas_height - (g->atlas_y + g->atlas_h)) * inv_h;
out_info->atlas_top = (atlas->atlas_height - g->atlas_y) * inv_h;
```

**After (correct - Y-down same as texture):**
```cpp
out_info->atlas_bottom = g->atlas_y * inv_h;  /* Top edge in texture */
out_info->atlas_top = (g->atlas_y + g->atlas_h) * inv_h;  /* Bottom edge */
```

#### Fix 4: MSDF Font Size (glyph_scale vs requested size)
**File:** `src/ui/ui_text.cpp`

The generated MSDF font's `font_size` was being set to `glyph_scale` (used for high-quality generation) instead of the requested size. This made text render 2x too large.

**Fix:** After generating, override `sdf_font->font_size = size` (the requested size).

#### Fix 1: Fragment Shader Uniforms Not Pushed (prerequisite)
The MSDF fragment shader expected uniform data but only vertex uniforms were being pushed.

**File:** `src/ui/ui_draw.cpp`
```cpp
/* SDF/MSDF shaders also need fragment uniforms */
if (draw_cmd->type == AUI_DRAW_CMD_SDF_TEXT ||
    draw_cmd->type == AUI_DRAW_CMD_MSDF_TEXT) {
    SDL_PushGPUFragmentUniformData(cmd, 0, uniforms, sizeof(uniforms));
}
```

#### Fix 2: MSDF Command Type Being Overwritten (prerequisite)
When `aui_flush_draw_cmd()` was called, it always reset the command type to SOLID or BITMAP_TEXT, ignoring the MSDF type set during `aui_draw_sdf_quad()`.

**File:** `include/agentite/ui.h` - Added tracking fields:
```cpp
AUI_DrawCmdType current_cmd_type;     /* Type of command currently being batched */
float current_sdf_scale;              /* SDF scale for current batch */
float current_sdf_distance_range;     /* SDF distance range for current batch */
```

**File:** `src/ui/ui_draw.cpp` - Modified flush to use tracked type:
```cpp
if (ctx->current_cmd_type != AUI_DRAW_CMD_SOLID) {
    /* SDF/MSDF text - use tracked type and params */
    cmd->type = ctx->current_cmd_type;
    cmd->sdf_scale = ctx->current_sdf_scale;
    cmd->sdf_distance_range = ctx->current_sdf_distance_range;
}
```

### MSDF Font Sizing Notes
Different fonts have different metrics. To match Roboto 18pt visually:
- Geneva (German): use 14pt
- Hiragino (Japanese): use 14pt
- Al Nile (Arabic): use 14pt

---

## Issue 1: TTC Font Loading (FIXED)

## Root Cause

The issue was **NOT Unicode path handling** as originally suspected. SDL handles UTF-8 paths correctly on macOS.

The actual issue was **TTC (TrueType Collection) file support**. macOS system fonts like `ヒラギノ角ゴシック W3.ttc` and `Al Nile.ttc` are TTC files containing multiple fonts. The code was passing offset `0` to `stbtt_InitFont()` and `stbtt_BakeFontBitmap()`, which only works for single TTF files.

For TTC files, you must use `stbtt_GetFontOffsetForIndex()` to get the correct byte offset before initializing.

## Fix Applied

Updated `src/graphics/text_font.cpp` in `agentite_font_load_memory()`:

```cpp
/* Get font offset - handles TTC (TrueType Collection) files
 * For single TTF files, this returns 0
 * For TTC files, this returns the offset to the first font in the collection */
int font_offset = stbtt_GetFontOffsetForIndex(font_data, 0);
if (font_offset < 0) {
    agentite_set_error("Text: Invalid font data or unsupported format");
    free(font);
    return NULL;
}

/* Use font_offset instead of hardcoded 0 */
stbtt_InitFont(&font->stb_font, font_data, font_offset);
stbtt_BakeFontBitmap(font_data, font_offset, ...);
```

## Testing

```bash
make example-localization
```

All four fonts now load successfully:
- `assets/fonts/Roboto-Regular.ttf` (TTF)
- `/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc` (TTC - Japanese)
- `/System/Library/Fonts/Supplemental/Al Nile.ttc` (TTC - Arabic)

## Completed Work

### Localization System (DONE)
- `include/agentite/localization.h` - Full API header
- `src/core/localization.cpp` - Complete implementation
- `docs/api/localization.md` - API documentation

### Demo Example (DONE)
- `examples/localization/main.cpp` - Demo application
- `examples/localization/locales/en.toml` - English
- `examples/localization/locales/de.toml` - German
- `examples/localization/locales/ja.toml` - Japanese (native script)
- `examples/localization/locales/ar.toml` - Arabic (native script)
- `Makefile` - Added `example-localization` target

### TTC Font Support (DONE)
- `src/graphics/text_font.cpp` - Fixed to handle TTC files via proper offset calculation

### MSDF Rendering (DONE)
- `src/graphics/msdf_atlas.cpp` - Fixed UV coordinate Y-flip
- `src/ui/ui_text.cpp` - Fixed font_size to use requested size, not glyph_scale
