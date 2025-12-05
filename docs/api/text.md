# Text Rendering System

TrueType font rendering with batched glyphs and SDF/MSDF support.

## Bitmap Text (Quick Start)

```c
#include "carbon/text.h"

// Initialize
Carbon_TextRenderer *text = carbon_text_init(gpu, window);

// Load font (each size creates a separate atlas)
Carbon_Font *font = carbon_font_load(text, "assets/fonts/Roboto.ttf", 24.0f);

// In game loop
carbon_text_begin(text);
carbon_text_draw(text, font, "Hello World!", 100.0f, 200.0f);
carbon_text_draw_colored(text, font, "Red text", x, y, 1.0f, 0.3f, 0.3f, 1.0f);
carbon_text_printf(text, font, x, y, "FPS: %.0f", fps);
carbon_text_end(text);

// Upload before render pass
carbon_text_upload(text, cmd);

// During render pass
carbon_text_render(text, cmd, pass);
```

## Draw Variants

```c
carbon_text_draw(text, font, "Text", x, y);
carbon_text_draw_colored(text, font, "Text", x, y, r, g, b, a);
carbon_text_draw_scaled(text, font, "Big", x, y, 2.0f);
carbon_text_draw_ex(text, font, "Centered", x, y, r, g, b, a, scale, CARBON_TEXT_ALIGN_CENTER);
carbon_text_printf(text, font, x, y, "Score: %d", score);
carbon_text_printf_colored(text, font, x, y, r, g, b, a, "Value: %.1f", val);
```

## Text Measurement

```c
float width = carbon_text_measure(font, "Hello");
float text_width, text_height;
carbon_text_measure_bounds(font, "Hello", &text_width, &text_height);
float line_height = carbon_font_get_line_height(font);
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
Carbon_SDFFont *sdf = carbon_sdf_font_load(text,
    "assets/fonts/Roboto-msdf.png",
    "assets/fonts/Roboto-msdf.json");

// Draw with scale parameter (instead of font size)
carbon_text_begin(text);
carbon_sdf_text_draw(text, sdf, "Sharp text!", x, y, 1.5f);
carbon_sdf_text_draw_colored(text, sdf, "Blue", x, y, 2.0f, 0.2f, 0.5f, 1.0f, 1.0f);
carbon_text_end(text);
```

### SDF Effects

```c
// Outline
carbon_sdf_text_set_outline(text, 0.1f, 0.0f, 0.0f, 0.0f, 1.0f);  // Black outline
carbon_sdf_text_draw(text, sdf, "Outlined", x, y, 2.0f);

// Glow
carbon_sdf_text_set_glow(text, 0.15f, 1.0f, 0.8f, 0.0f, 0.8f);    // Golden glow
carbon_sdf_text_draw(text, sdf, "Glowing", x, y, 2.0f);

// Weight (bolder/lighter)
carbon_sdf_text_set_weight(text, 0.1f);  // Bolder (-0.5 to 0.5)

// Clear effects
carbon_sdf_text_clear_effects(text);
```

## Notes

- All text in a batch must use the same font
- Different font sizes require separate fonts
- SDF and bitmap fonts cannot be mixed in the same batch
- Newlines (`\n`) are supported
