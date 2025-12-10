# Text Rendering System

TrueType font rendering with batched glyphs and SDF/MSDF support.

## Bitmap Text (Quick Start)

```c
#include "agentite/text.h"

// Initialize
Agentite_TextRenderer *text = agentite_text_init(gpu, window);

// Load font (each size creates a separate atlas)
Agentite_Font *font = agentite_font_load(text, "assets/fonts/Roboto.ttf", 24.0f);

// In game loop
agentite_text_begin(text);
agentite_text_draw(text, font, "Hello World!", 100.0f, 200.0f);
agentite_text_draw_colored(text, font, "Red text", x, y, 1.0f, 0.3f, 0.3f, 1.0f);
agentite_text_printf(text, font, x, y, "FPS: %.0f", fps);
agentite_text_end(text);

// Upload before render pass
agentite_text_upload(text, cmd);

// During render pass
agentite_text_render(text, cmd, pass);
```

## Draw Variants

```c
agentite_text_draw(text, font, "Text", x, y);
agentite_text_draw_colored(text, font, "Text", x, y, r, g, b, a);
agentite_text_draw_scaled(text, font, "Big", x, y, 2.0f);
agentite_text_draw_ex(text, font, "Centered", x, y, r, g, b, a, scale, AGENTITE_TEXT_ALIGN_CENTER);
agentite_text_printf(text, font, x, y, "Score: %d", score);
agentite_text_printf_colored(text, font, x, y, r, g, b, a, "Value: %.1f", val);
```

## Text Measurement

```c
float width = agentite_text_measure(font, "Hello");
float text_width, text_height;
agentite_text_measure_bounds(font, "Hello", &text_width, &text_height);
float line_height = agentite_font_get_line_height(font);
```

## SDF/MSDF Text (Sharp at Any Scale)

For text that needs to scale without blurring, use SDF or MSDF fonts.

### Generating SDF Fonts

Use [msdf-atlas-gen](https://github.com/Chlumsky/msdf-atlas-gen):
```bash
msdf-atlas-gen -font Roboto.ttf -type msdf -format png \
    -imageout Roboto-msdf.png -json Roboto-msdf.json \
    -size 48 -pxrange 4
```

### Using SDF Fonts

```c
// Load SDF font
Agentite_SDFFont *sdf = agentite_sdf_font_load(text,
    "assets/fonts/Roboto-msdf.png",
    "assets/fonts/Roboto-msdf.json");

// Draw with scale parameter (instead of font size)
agentite_text_begin(text);
agentite_sdf_text_draw(text, sdf, "Sharp text!", x, y, 1.5f);
agentite_sdf_text_draw_colored(text, sdf, "Blue", x, y, 2.0f, 0.2f, 0.5f, 1.0f, 1.0f);
agentite_text_end(text);
```

### SDF Effects

```c
// Outline
agentite_sdf_text_set_outline(text, 0.1f, 0.0f, 0.0f, 0.0f, 1.0f);  // Black outline
agentite_sdf_text_draw(text, sdf, "Outlined", x, y, 2.0f);

// Glow
agentite_sdf_text_set_glow(text, 0.15f, 1.0f, 0.8f, 0.0f, 0.8f);    // Golden glow
agentite_sdf_text_draw(text, sdf, "Glowing", x, y, 2.0f);

// Weight (bolder/lighter)
agentite_sdf_text_set_weight(text, 0.1f);  // Bolder (-0.5 to 0.5)

// Clear effects
agentite_sdf_text_clear_effects(text);
```

## Notes

- All text in a batch must use the same font
- Different font sizes require separate fonts
- SDF and bitmap fonts cannot be mixed in the same batch
- Newlines (`\n`) are supported
