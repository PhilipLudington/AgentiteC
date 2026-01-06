#ifndef AGENTITE_LOCALIZATION_H
#define AGENTITE_LOCALIZATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Agentite Localization System
 *
 * Provides multi-language support with TOML-based string tables,
 * runtime language switching, parameter substitution, and pluralization.
 *
 * Usage:
 *   Agentite_LocalizationConfig config = AGENTITE_LOCALIZATION_CONFIG_DEFAULT;
 *   config.locales_path = "assets/locales";
 *
 *   Agentite_Localization *loc = agentite_loc_create(&config);
 *   if (!loc) {
 *       printf("Failed: %s\n", agentite_get_last_error());
 *       return 1;
 *   }
 *
 *   agentite_loc_set_global(loc);
 *   agentite_loc_set_language(loc, "de");
 *
 *   // Simple lookup
 *   const char *title = LOC("game_title");
 *
 *   // Named parameters: "Hello, {name}!" -> "Hello, Player1!"
 *   const char *greeting = LOCF("greeting", "name", "Player1", NULL);
 *
 *   // Pluralization: "{count} item|{count} items" -> "5 items"
 *   const char *items = LOCP("item_count", 5);
 *
 *   agentite_loc_destroy(loc);
 *
 * TOML Format (assets/locales/en.toml):
 *   [meta]
 *   language = "English"
 *   locale = "en"
 *   direction = "ltr"
 *   font = "default"
 *
 *   [strings]
 *   game_title = "My Game"
 *   greeting = "Hello, {name}!"
 *   item_count = "{count} item|{count} items"
 *
 *   [strings.ui]
 *   button_ok = "OK"
 */

/* ============================================================================
 * Types
 * ============================================================================ */

/** Opaque localization context */
typedef struct Agentite_Localization Agentite_Localization;

/** Text direction for layout */
typedef enum Agentite_TextDirection {
    AGENTITE_TEXT_LTR = 0,     /** Left-to-right (English, German, etc.) */
    AGENTITE_TEXT_RTL = 1      /** Right-to-left (Arabic, Hebrew, etc.) */
} Agentite_TextDirection;

/** Language information */
typedef struct Agentite_LanguageInfo {
    char locale[16];           /** Locale code (e.g., "en", "de", "ja") */
    char name[64];             /** Display name (e.g., "English", "Deutsch") */
    Agentite_TextDirection direction;
    char font_key[32];         /** Font association key */
} Agentite_LanguageInfo;

/**
 * Pluralization rule callback.
 * Returns the plural form index (0, 1, 2, ...) based on count.
 * For English: 0 = singular (count == 1), 1 = plural (count != 1)
 */
typedef int (*Agentite_PluralRule)(int64_t count);

/** Localization configuration */
typedef struct Agentite_LocalizationConfig {
    const char *locales_path;     /** Directory containing locale TOML files */
    const char *fallback_locale;  /** Fallback locale code (e.g., "en") */
    size_t max_languages;         /** Maximum languages to load (0 = no limit) */
    size_t format_buffer_size;    /** Buffer size for formatted strings (0 = default 4096) */
} Agentite_LocalizationConfig;

/** Default configuration */
#define AGENTITE_LOCALIZATION_CONFIG_DEFAULT { \
    .locales_path = "assets/locales", \
    .fallback_locale = "en", \
    .max_languages = 0, \
    .format_buffer_size = 0 \
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * Create a localization context.
 * Scans the locales directory and loads all .toml files found.
 *
 * @param config Configuration (NULL for defaults)
 * @return New context, or NULL on failure (check agentite_get_last_error())
 *
 * Thread-safe: No (call from main thread only)
 */
Agentite_Localization *agentite_loc_create(const Agentite_LocalizationConfig *config);

/**
 * Destroy a localization context and free all resources.
 * Safe to call with NULL.
 *
 * @param loc Localization context to destroy
 *
 * Thread-safe: No
 */
void agentite_loc_destroy(Agentite_Localization *loc);

/* ============================================================================
 * Language Loading
 * ============================================================================ */

/**
 * Load a language from a TOML file.
 * The language is added to the available languages list.
 *
 * @param loc Localization context
 * @param path Path to the TOML file
 * @return true on success, false on failure
 *
 * Thread-safe: No
 */
bool agentite_loc_load_language(Agentite_Localization *loc, const char *path);

/**
 * Load a language from a TOML string.
 *
 * @param loc Localization context
 * @param toml_string TOML content as string
 * @param locale Locale code for this language
 * @return true on success, false on failure
 *
 * Thread-safe: No
 */
bool agentite_loc_load_language_string(Agentite_Localization *loc,
                                       const char *toml_string,
                                       const char *locale);

/* ============================================================================
 * Language Management
 * ============================================================================ */

/**
 * Set the current language.
 *
 * @param loc Localization context
 * @param locale Locale code (e.g., "en", "de")
 * @return true if language was found and set, false otherwise
 *
 * Thread-safe: No
 */
bool agentite_loc_set_language(Agentite_Localization *loc, const char *locale);

/**
 * Get the current language locale code.
 *
 * @param loc Localization context
 * @return Current locale code (borrowed pointer, do not free)
 *
 * Thread-safe: Yes (read-only)
 */
const char *agentite_loc_get_language(const Agentite_Localization *loc);

/**
 * Get information about the current language.
 *
 * @param loc Localization context
 * @return Language info (borrowed, do not free), or NULL if no language set
 *
 * Thread-safe: Yes (read-only)
 */
const Agentite_LanguageInfo *agentite_loc_get_language_info(const Agentite_Localization *loc);

/**
 * Get the number of available languages.
 *
 * @param loc Localization context
 * @return Number of loaded languages
 *
 * Thread-safe: Yes (read-only)
 */
size_t agentite_loc_get_language_count(const Agentite_Localization *loc);

/**
 * Get information about a language by index.
 *
 * @param loc Localization context
 * @param index Language index (0 to count-1)
 * @return Language info, or NULL if index out of range
 *
 * Thread-safe: Yes (read-only)
 */
const Agentite_LanguageInfo *agentite_loc_get_language_at(const Agentite_Localization *loc,
                                                          size_t index);

/* ============================================================================
 * String Lookup
 * ============================================================================ */

/**
 * Get a localized string by key.
 * Returns the key itself if not found (for debugging visibility).
 *
 * Keys support dot notation for nested categories:
 *   "menu_start"      -> [strings] menu_start
 *   "ui.button_ok"    -> [strings.ui] button_ok
 *
 * @param loc Localization context
 * @param key String key
 * @return Localized string (borrowed, valid until language change or destroy)
 *
 * Thread-safe: Yes (read-only after language set)
 */
const char *agentite_loc_get(const Agentite_Localization *loc, const char *key);

/**
 * Check if a key exists in the current language.
 *
 * @param loc Localization context
 * @param key String key to check
 * @return true if key exists, false otherwise
 *
 * Thread-safe: Yes (read-only)
 */
bool agentite_loc_has_key(const Agentite_Localization *loc, const char *key);

/* ============================================================================
 * Parameter Substitution
 * ============================================================================ */

/**
 * Get a localized string with positional parameter substitution.
 * Uses {0}, {1}, {2}, etc. for parameter placeholders.
 *
 * @param loc Localization context
 * @param key String key
 * @param ... String parameters (const char*), NULL-terminated
 * @return Formatted string (borrowed, valid until next format call on same thread)
 *
 * Example: agentite_loc_format(loc, "damage", "50", "Fire", NULL)
 *          with "{0} {1} damage" returns "50 Fire damage"
 *
 * Thread-safe: Yes (uses thread-local buffer)
 */
const char *agentite_loc_format(Agentite_Localization *loc, const char *key, ...);

/**
 * Get a localized string with named parameter substitution.
 * Uses {name} for parameter placeholders. Pass name/value pairs.
 *
 * @param loc Localization context
 * @param key String key
 * @param ... Name/value pairs (const char*, const char*), NULL-terminated
 * @return Formatted string (borrowed, valid until next format call on same thread)
 *
 * Example: agentite_loc_format_named(loc, "greeting", "name", "Player1", NULL)
 *          with "Hello, {name}!" returns "Hello, Player1!"
 *
 * Thread-safe: Yes (uses thread-local buffer)
 */
const char *agentite_loc_format_named(Agentite_Localization *loc, const char *key, ...);

/**
 * Get a localized string with integer formatting.
 * Substitutes {0} or {count} or {value} with the integer value.
 *
 * @param loc Localization context
 * @param key String key
 * @param value Integer value to substitute
 * @return Formatted string
 *
 * Thread-safe: Yes (uses thread-local buffer)
 */
const char *agentite_loc_format_int(Agentite_Localization *loc, const char *key, int64_t value);

/* ============================================================================
 * Pluralization
 * ============================================================================ */

/**
 * Get a pluralized string based on count.
 * The string value should contain pipe-separated plural forms.
 *
 * Format: "singular|plural" or "zero|one|few|many|other"
 * The appropriate form is selected based on language-specific rules.
 *
 * @param loc Localization context
 * @param key String key (value should contain | separated forms)
 * @param count The count determining plural form
 * @return Appropriate plural form with {count} substituted
 *
 * Example: agentite_loc_plural(loc, "items", 5)
 *          with "{count} item|{count} items" returns "5 items"
 *
 * Thread-safe: Yes (uses thread-local buffer)
 */
const char *agentite_loc_plural(Agentite_Localization *loc, const char *key, int64_t count);

/**
 * Register a custom pluralization rule for a language.
 * Overrides the default rule for the specified locale.
 *
 * Built-in rules exist for: en, de, fr, es, it, pt, ru, pl, uk, ar, ja, zh, ko
 *
 * @param loc Localization context
 * @param locale Locale code to apply rule to
 * @param rule Pluralization callback function
 * @return true on success, false if locale not found
 *
 * Thread-safe: No
 */
bool agentite_loc_set_plural_rule(Agentite_Localization *loc,
                                  const char *locale,
                                  Agentite_PluralRule rule);

/* ============================================================================
 * Font & Direction
 * ============================================================================ */

/**
 * Get the font key for the current language.
 * Games should map this key to actual font resources.
 *
 * @param loc Localization context
 * @return Font key string (borrowed), or "default" if not specified
 *
 * Thread-safe: Yes (read-only)
 */
const char *agentite_loc_get_font_key(const Agentite_Localization *loc);

/**
 * Get the text direction for the current language.
 *
 * @param loc Localization context
 * @return Text direction (LTR or RTL)
 *
 * Thread-safe: Yes (read-only)
 */
Agentite_TextDirection agentite_loc_get_text_direction(const Agentite_Localization *loc);

/* ============================================================================
 * Validation
 * ============================================================================ */

/** Validation result for locale comparison */
typedef struct Agentite_LocalizationValidation {
    char **missing_keys;      /** Keys in reference but not in target (caller must free) */
    size_t missing_count;
    char **extra_keys;        /** Keys in target but not in reference (caller must free) */
    size_t extra_count;
} Agentite_LocalizationValidation;

/**
 * Validate a locale against a reference locale.
 * Reports missing and extra keys.
 *
 * @param loc Localization context
 * @param target_locale Locale to validate
 * @param reference_locale Reference locale to compare against (e.g., "en")
 * @param result Output validation result (caller must free arrays with agentite_loc_validation_free)
 * @return true on success, false on failure
 *
 * Thread-safe: No
 */
bool agentite_loc_validate(const Agentite_Localization *loc,
                           const char *target_locale,
                           const char *reference_locale,
                           Agentite_LocalizationValidation *result);

/**
 * Free validation result arrays.
 *
 * @param result Validation result to free
 */
void agentite_loc_validation_free(Agentite_LocalizationValidation *result);

/**
 * Get all keys in the current language.
 * Useful for debugging and tooling.
 *
 * @param loc Localization context
 * @param out_keys Output array of keys (caller must free with agentite_loc_free_keys)
 * @param out_count Output key count
 * @return true on success, false on failure
 *
 * Thread-safe: No
 */
bool agentite_loc_get_all_keys(const Agentite_Localization *loc,
                               char ***out_keys,
                               size_t *out_count);

/**
 * Free key array returned by agentite_loc_get_all_keys.
 *
 * @param keys Key array to free
 * @param count Number of keys
 */
void agentite_loc_free_keys(char **keys, size_t count);

/* ============================================================================
 * Global Convenience API
 * ============================================================================ */

/**
 * Set the global localization context.
 * Enables use of LOC(), LOCF(), LOCP() shortcut macros.
 *
 * @param loc Localization context (can be NULL to clear)
 *
 * Thread-safe: No (set once at startup)
 */
void agentite_loc_set_global(Agentite_Localization *loc);

/**
 * Get the global localization context.
 *
 * @return Global context, or NULL if not set
 *
 * Thread-safe: Yes (read-only after set)
 */
Agentite_Localization *agentite_loc_get_global(void);

/** Shortcut macro: Get localized string using global context */
#define LOC(key) agentite_loc_get(agentite_loc_get_global(), key)

/** Shortcut macro: Get formatted string with named params using global context */
#define LOCF(key, ...) agentite_loc_format_named(agentite_loc_get_global(), key, __VA_ARGS__)

/** Shortcut macro: Get pluralized string using global context */
#define LOCP(key, count) agentite_loc_plural(agentite_loc_get_global(), key, count)

#endif /* AGENTITE_LOCALIZATION_H */
