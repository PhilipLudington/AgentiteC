# Rich Text Example

Demonstrates the BBCode-style rich text system for formatted and animated text.

## Features

- **Text Formatting**: Bold, italic, underline, strikethrough
- **Colors**: Hex colors (#RRGGBB) and named colors (red, green, blue, etc.)
- **Text Sizes**: Variable font sizes within the same text block
- **Animations**: Wave, shake, rainbow, and fade effects
- **Links**: Clickable URLs with callbacks
- **Nesting**: Full support for nested formatting tags

## Controls

| Key | Action |
|-----|--------|
| 1 | Basic formatting demo |
| 2 | Colors demo |
| 3 | Text sizes demo |
| 4 | Animations demo |
| 5 | Links demo |
| 6 | Complex example |
| ESC | Quit |

## BBCode Tags

### Formatting
```
[b]Bold text[/b]
[i]Italic text[/i]
[u]Underlined text[/u]
[s]Strikethrough text[/s]
```

### Colors
```
[color=#FF6B6B]Hex color[/color]
[color=red]Named color[/color]
```

### Sizes
```
[size=24]Large text[/size]
[size=12]Small text[/size]
```

### Animations
```
[wave]Wavy animated text[/wave]
[shake]Shaking text[/shake]
[rainbow]Rainbow cycling colors[/rainbow]
[fade]Fading in and out[/fade]
```

### Links
```
[url=https://example.com]Click here[/url]
```

## Usage

```c
// Parse BBCode text
AUI_RichText *rt = aui_richtext_parse(
    "[b]Hello[/b] [color=#FF0000]World[/color]!"
);

// Layout for max width
aui_richtext_layout(rt, 400);

// Draw each frame
aui_richtext_draw(ctx, rt, x, y);

// Update animations each frame
aui_richtext_update(rt, delta_time);

// Cleanup
aui_richtext_destroy(rt);
```

## Immediate Mode

For simple cases, use the immediate mode API:

```c
// Draw BBCode text immediately (parses each call)
aui_rich_label(ctx, "[b]Quick[/b] formatted text");
```
