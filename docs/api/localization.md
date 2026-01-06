# Localization System

The Agentite Localization system provides multi-language support for games:
- TOML-based string tables with nested categories
- Runtime language switching
- Parameter substitution (positional `{0}` and named `{name}`)
- Pluralization with language-specific rules
- Font and text direction association per language
- Validation tools for missing/extra keys

## Header

```c
#include "agentite/localization.h"
```

## Quick Start

```c
// Create localization with default config
Agentite_LocalizationConfig config = AGENTITE_LOCALIZATION_CONFIG_DEFAULT;
config.locales_path = "assets/locales";
config.fallback_locale = "en";

Agentite_Localization *loc = agentite_loc_create(&config);
if (!loc) {
    printf("Error: %s\n", agentite_get_last_error());
    return 1;
}

// Set as global for convenience macros
agentite_loc_set_global(loc);

// Switch language
agentite_loc_set_language(loc, "de");

// Simple string lookup
const char *title = LOC("game_title");

// Named parameter substitution
// "Hello, {name}!" -> "Hello, Player1!"
const char *greeting = LOCF("greeting", "name", "Player1", NULL);

// Pluralization
// "{count} item|{count} items" -> "5 items"
const char *items = LOCP("item_count", 5);

// Cleanup
agentite_loc_destroy(loc);
```

## TOML File Format

Place language files in your locales directory (e.g., `assets/locales/en.toml`):

```toml
[meta]
language = "English"
locale = "en"
direction = "ltr"    # "ltr" or "rtl"
font = "default"     # Font key for this language

[strings]
game_title = "My Awesome Game"
greeting = "Hello, {name}!"
item_count = "{count} item|{count} items"

[strings.menu]
start = "Start Game"
options = "Options"
quit = "Quit"

[strings.ui]
button_ok = "OK"
button_cancel = "Cancel"
```

### Key Path Resolution

Keys use dot notation for nested categories:
- `"game_title"` → `[strings]` game_title
- `"menu.start"` → `[strings.menu]` start
- `"ui.button_ok"` → `[strings.ui]` button_ok

## Configuration

```c
Agentite_LocalizationConfig config = AGENTITE_LOCALIZATION_CONFIG_DEFAULT;
config.locales_path = "assets/locales";  // Directory with .toml files
config.fallback_locale = "en";           // Fallback when key missing
config.max_languages = 0;                // 0 = no limit (default: 32)
config.format_buffer_size = 0;           // 0 = default 4096 bytes

Agentite_Localization *loc = agentite_loc_create(&config);
```

The system automatically loads all `.toml` files from the locales directory on creation.

## String Lookup

### Basic Lookup

```c
// Using explicit API
const char *text = agentite_loc_get(loc, "menu.start");

// Using global shortcut macro
const char *text = LOC("menu.start");

// Check if key exists
if (agentite_loc_has_key(loc, "menu.start")) {
    // Key exists in current language
}
```

If a key is not found, the system tries the fallback language. If still not found, it returns the key itself (useful for debugging missing translations).

## Parameter Substitution

### Positional Parameters

```c
// String: "Dealt {0} {1} damage"
// Result: "Dealt 50 Fire damage"
const char *msg = agentite_loc_format(loc, "damage_msg", "50", "Fire", NULL);
```

### Named Parameters

```c
// String: "Hello, {name}! You have {gold} gold."
// Result: "Hello, Player1! You have 100 gold."
const char *msg = agentite_loc_format_named(loc, "status",
    "name", "Player1",
    "gold", "100",
    NULL);

// Using LOCF macro
const char *msg = LOCF("status", "name", "Player1", "gold", "100", NULL);
```

### Integer Formatting

```c
// String: "Score: {count}"
// Result: "Score: 1250"
const char *msg = agentite_loc_format_int(loc, "score", 1250);
```

## Pluralization

### Basic Usage

```c
// String: "{count} item|{count} items"
// count=1: "1 item"
// count=5: "5 items"
const char *text = agentite_loc_plural(loc, "item_count", item_count);

// Using LOCP macro
const char *text = LOCP("item_count", item_count);
```

### Plural Forms

Different languages have different plural rules:

| Language | Forms | Example |
|----------|-------|---------|
| English | 2 (singular, plural) | `"1 item\|{count} items"` |
| French | 2 (0-1 singular, 2+ plural) | `"1 élément\|{count} éléments"` |
| Russian | 3 (one, few, many) | `"{count} файл\|{count} файла\|{count} файлов"` |
| Arabic | 6 forms | Complex rules |
| Japanese | 1 (no plural) | `"{count}個のアイテム"` |

Built-in rules exist for: `en`, `de`, `fr`, `es`, `it`, `pt`, `ru`, `pl`, `uk`, `ar`, `ja`, `zh`, `ko`

### Custom Plural Rules

```c
// Custom rule returns form index (0, 1, 2, ...) based on count
int my_plural_rule(int64_t count) {
    if (count == 0) return 0;
    if (count == 1) return 1;
    return 2;
}

agentite_loc_set_plural_rule(loc, "my_lang", my_plural_rule);
```

## Language Management

### Switching Languages

```c
// Switch to German
if (!agentite_loc_set_language(loc, "de")) {
    printf("Language not found: %s\n", agentite_get_last_error());
}

// Get current language
const char *current = agentite_loc_get_language(loc);
```

### Language Information

```c
// Get info about current language
const Agentite_LanguageInfo *info = agentite_loc_get_language_info(loc);
printf("Language: %s (%s)\n", info->name, info->locale);
printf("Direction: %s\n", info->direction == AGENTITE_TEXT_RTL ? "RTL" : "LTR");
printf("Font: %s\n", info->font_key);

// Enumerate available languages
size_t count = agentite_loc_get_language_count(loc);
for (size_t i = 0; i < count; i++) {
    const Agentite_LanguageInfo *lang = agentite_loc_get_language_at(loc, i);
    printf("  %s: %s\n", lang->locale, lang->name);
}
```

### Language Selection UI

```c
void render_language_selector(AUI_Context *ui, Agentite_Localization *loc) {
    size_t count = agentite_loc_get_language_count(loc);
    for (size_t i = 0; i < count; i++) {
        const Agentite_LanguageInfo *info = agentite_loc_get_language_at(loc, i);
        if (aui_button(ui, info->name)) {
            agentite_loc_set_language(loc, info->locale);
            update_fonts_for_language(loc);  // Your font switching code
        }
    }
}
```

## Font and Text Direction

### Font Association

Each language can specify a font key in its `[meta]` section:

```toml
[meta]
font = "cjk"  # For Chinese/Japanese/Korean
```

Use this to switch fonts in your game:

```c
const char *font_key = agentite_loc_get_font_key(loc);

if (strcmp(font_key, "cjk") == 0) {
    aui_set_font(ui, cjk_font);
} else if (strcmp(font_key, "arabic") == 0) {
    aui_set_font(ui, arabic_font);
} else {
    aui_set_font(ui, default_font);
}
```

### Text Direction

```c
Agentite_TextDirection dir = agentite_loc_get_text_direction(loc);
if (dir == AGENTITE_TEXT_RTL) {
    // Adjust UI layout for right-to-left languages
}
```

## UI Integration

The localization system integrates with Agentite UI via the `LOC`, `LOCF`, and `LOCP` macros:

```c
void render_game_ui(AUI_Context *ui) {
    if (aui_begin_panel(ui, "menu", 100, 100, 300, 400, 0)) {
        aui_label(ui, LOC("game_title"));

        if (aui_button(ui, LOC("menu.start"))) {
            start_game();
        }
        if (aui_button(ui, LOC("menu.options"))) {
            show_options();
        }
        if (aui_button(ui, LOC("menu.quit"))) {
            quit_game();
        }

        // Show player stats with parameters
        aui_label(ui, LOCF("player.status",
            "name", player_name,
            "level", level_str,
            NULL));

        // Show inventory count with pluralization
        aui_label(ui, LOCP("inventory.items", item_count));

        aui_end_panel(ui);
    }
}
```

## Validation

### Programmatic Validation

```c
Agentite_LocalizationValidation result;
if (agentite_loc_validate(loc, "de", "en", &result)) {
    if (result.missing_count > 0) {
        printf("Missing %zu keys in German:\n", result.missing_count);
        for (size_t i = 0; i < result.missing_count; i++) {
            printf("  - %s\n", result.missing_keys[i]);
        }
    }
    if (result.extra_count > 0) {
        printf("Extra %zu keys in German:\n", result.extra_count);
        for (size_t i = 0; i < result.extra_count; i++) {
            printf("  - %s\n", result.extra_keys[i]);
        }
    }
    agentite_loc_validation_free(&result);
}
```

### Enumerate All Keys

```c
char **keys;
size_t count;
if (agentite_loc_get_all_keys(loc, &keys, &count)) {
    for (size_t i = 0; i < count; i++) {
        printf("%s = %s\n", keys[i], agentite_loc_get(loc, keys[i]));
    }
    agentite_loc_free_keys(keys, count);
}
```

## API Reference

### Lifecycle

| Function | Description |
|----------|-------------|
| `agentite_loc_create(config)` | Create localization context |
| `agentite_loc_destroy(loc)` | Destroy context (safe with NULL) |
| `agentite_loc_load_language(loc, path)` | Load language from TOML file |
| `agentite_loc_load_language_string(loc, toml, locale)` | Load from string |

### Language Management

| Function | Description |
|----------|-------------|
| `agentite_loc_set_language(loc, locale)` | Set current language |
| `agentite_loc_get_language(loc)` | Get current locale code |
| `agentite_loc_get_language_info(loc)` | Get current language info |
| `agentite_loc_get_language_count(loc)` | Get number of loaded languages |
| `agentite_loc_get_language_at(loc, index)` | Get language info by index |

### String Lookup

| Function | Description |
|----------|-------------|
| `agentite_loc_get(loc, key)` | Get localized string |
| `agentite_loc_has_key(loc, key)` | Check if key exists |
| `agentite_loc_format(loc, key, ...)` | Format with positional params |
| `agentite_loc_format_named(loc, key, ...)` | Format with named params |
| `agentite_loc_format_int(loc, key, value)` | Format with integer |

### Pluralization

| Function | Description |
|----------|-------------|
| `agentite_loc_plural(loc, key, count)` | Get pluralized string |
| `agentite_loc_set_plural_rule(loc, locale, rule)` | Set custom plural rule |

### Font & Direction

| Function | Description |
|----------|-------------|
| `agentite_loc_get_font_key(loc)` | Get font key for current language |
| `agentite_loc_get_text_direction(loc)` | Get text direction (LTR/RTL) |

### Validation

| Function | Description |
|----------|-------------|
| `agentite_loc_validate(loc, target, reference, result)` | Compare two locales |
| `agentite_loc_validation_free(result)` | Free validation result |
| `agentite_loc_get_all_keys(loc, out_keys, out_count)` | Get all keys |
| `agentite_loc_free_keys(keys, count)` | Free keys array |

### Global API

| Function | Description |
|----------|-------------|
| `agentite_loc_set_global(loc)` | Set global context |
| `agentite_loc_get_global()` | Get global context |
| `LOC(key)` | Shortcut for `agentite_loc_get` |
| `LOCF(key, ...)` | Shortcut for `agentite_loc_format_named` |
| `LOCP(key, count)` | Shortcut for `agentite_loc_plural` |

## Thread Safety

| Operation | Thread-Safe |
|-----------|-------------|
| `agentite_loc_create/destroy` | No |
| `agentite_loc_load_language` | No |
| `agentite_loc_set_language` | No |
| `agentite_loc_get` | Yes (after language set) |
| `agentite_loc_format*` | Yes (thread-local buffer) |
| `agentite_loc_plural` | Yes (thread-local buffer) |
| `agentite_loc_has_key` | Yes |
| `agentite_loc_validate` | No |

## Best Practices

### 1. Initialize at Startup

```c
int main(void) {
    Agentite_LocalizationConfig config = AGENTITE_LOCALIZATION_CONFIG_DEFAULT;
    config.locales_path = "assets/locales";
    config.fallback_locale = "en";

    Agentite_Localization *loc = agentite_loc_create(&config);
    agentite_loc_set_global(loc);

    // ... game loop ...

    agentite_loc_destroy(loc);
}
```

### 2. Use Categories for Organization

```toml
[strings.menu]
start = "Start"
quit = "Quit"

[strings.dialog.confirm]
yes = "Yes"
no = "No"

[strings.error]
network = "Network error"
save = "Save failed"
```

### 3. Test All Languages

Run validation during development to catch missing translations:

```c
#ifdef DEBUG
    size_t lang_count = agentite_loc_get_language_count(loc);
    for (size_t i = 0; i < lang_count; i++) {
        const Agentite_LanguageInfo *info = agentite_loc_get_language_at(loc, i);
        if (strcmp(info->locale, "en") == 0) continue;

        Agentite_LocalizationValidation result;
        agentite_loc_validate(loc, info->locale, "en", &result);
        if (result.missing_count > 0) {
            printf("WARNING: %s missing %zu keys\n", info->locale, result.missing_count);
        }
        agentite_loc_validation_free(&result);
    }
#endif
```

## See Also

- [UI System](ui.md) - For displaying localized text in UI
- [Text Rendering](text.md) - For font loading and text drawing
