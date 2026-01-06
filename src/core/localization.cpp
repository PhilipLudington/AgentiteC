#include "agentite/localization.h"
#include "agentite/agentite.h"
#include "agentite/error.h"
#include "toml.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOC_MAX_KEY_LENGTH 256
#define LOC_MAX_VALUE_LENGTH 4096
#define LOC_HASH_TABLE_SIZE 1024
#define LOC_MAX_LANGUAGES 32
#define LOC_DEFAULT_BUFFER_SIZE 4096

/* ============================================================================
 * Thread-local buffer for formatting
 * ============================================================================ */

#if defined(_MSC_VER)
    #define LOC_THREAD_LOCAL __declspec(thread)
#elif defined(__cplusplus)
    #define LOC_THREAD_LOCAL thread_local
#else
    #define LOC_THREAD_LOCAL _Thread_local
#endif

static LOC_THREAD_LOCAL char s_format_buffer[LOC_DEFAULT_BUFFER_SIZE];

/* ============================================================================
 * Hash table for string storage
 * ============================================================================ */

typedef struct LocStringEntry {
    char key[LOC_MAX_KEY_LENGTH];
    char *value;                    /* Heap-allocated string value */
    struct LocStringEntry *next;    /* Collision chain */
} LocStringEntry;

/* djb2 hash function */
static unsigned long hash_string(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % LOC_HASH_TABLE_SIZE;
}

/* ============================================================================
 * Language structure
 * ============================================================================ */

typedef struct LocLanguage {
    Agentite_LanguageInfo info;
    LocStringEntry *strings[LOC_HASH_TABLE_SIZE];
    Agentite_PluralRule plural_rule;
    size_t string_count;
} LocLanguage;

/* ============================================================================
 * Main context structure
 * ============================================================================ */

struct Agentite_Localization {
    LocLanguage *languages[LOC_MAX_LANGUAGES];
    size_t language_count;
    size_t current_language;        /* Index into languages array */
    size_t fallback_language;       /* Index for fallback, or SIZE_MAX if none */
    char locales_path[256];
    size_t max_languages;
    size_t format_buffer_size;
};

/* Global context pointer */
static Agentite_Localization *s_global_loc = NULL;

/* ============================================================================
 * Built-in Pluralization Rules
 * ============================================================================ */

/* English, German, etc.: singular (n=1), plural (n!=1) */
static int plural_rule_english(int64_t n) {
    return (n == 1) ? 0 : 1;
}

/* French, Portuguese (Brazil): singular (n=0 or n=1), plural (n>1) */
static int plural_rule_french(int64_t n) {
    return (n <= 1) ? 0 : 1;
}

/* Russian, Polish, Ukrainian: complex Slavic rules */
static int plural_rule_slavic(int64_t n) {
    int64_t mod10 = n % 10;
    int64_t mod100 = n % 100;

    if (mod10 == 1 && mod100 != 11) return 0;           /* one */
    if (mod10 >= 2 && mod10 <= 4 &&
        (mod100 < 12 || mod100 > 14)) return 1;         /* few */
    return 2;                                            /* many */
}

/* Arabic: 6 forms */
static int plural_rule_arabic(int64_t n) {
    if (n == 0) return 0;
    if (n == 1) return 1;
    if (n == 2) return 2;
    int64_t mod100 = n % 100;
    if (mod100 >= 3 && mod100 <= 10) return 3;
    if (mod100 >= 11 && mod100 <= 99) return 4;
    return 5;
}

/* Japanese, Chinese, Korean, etc.: no plural forms */
static int plural_rule_none(int64_t n) {
    (void)n;
    return 0;
}

/* Get default plural rule for a locale */
static Agentite_PluralRule get_default_plural_rule(const char *locale) {
    if (!locale) return plural_rule_english;

    /* Check prefix matches */
    if (strncmp(locale, "ru", 2) == 0 ||
        strncmp(locale, "pl", 2) == 0 ||
        strncmp(locale, "uk", 2) == 0) {
        return plural_rule_slavic;
    }

    if (strncmp(locale, "fr", 2) == 0 ||
        strncmp(locale, "pt-BR", 5) == 0) {
        return plural_rule_french;
    }

    if (strncmp(locale, "ja", 2) == 0 ||
        strncmp(locale, "zh", 2) == 0 ||
        strncmp(locale, "ko", 2) == 0) {
        return plural_rule_none;
    }

    if (strncmp(locale, "ar", 2) == 0) {
        return plural_rule_arabic;
    }

    return plural_rule_english;
}

/* ============================================================================
 * Language management helpers
 * ============================================================================ */

static void free_language_strings(LocLanguage *lang) {
    if (!lang) return;

    for (int i = 0; i < LOC_HASH_TABLE_SIZE; i++) {
        LocStringEntry *entry = lang->strings[i];
        while (entry) {
            LocStringEntry *next = entry->next;
            free(entry->value);
            free(entry);
            entry = next;
        }
        lang->strings[i] = NULL;
    }
    lang->string_count = 0;
}

static void destroy_language(LocLanguage *lang) {
    if (!lang) return;
    free_language_strings(lang);
    free(lang);
}

static LocLanguage *create_language(void) {
    LocLanguage *lang = AGENTITE_ALLOC(LocLanguage);
    if (!lang) return NULL;

    /* Set defaults */
    strncpy(lang->info.font_key, "default", sizeof(lang->info.font_key) - 1);
    lang->info.direction = AGENTITE_TEXT_LTR;
    lang->plural_rule = plural_rule_english;

    return lang;
}

static bool add_string(LocLanguage *lang, const char *key, const char *value) {
    if (!lang || !key || !value) return false;

    unsigned long hash = hash_string(key);

    LocStringEntry *entry = (LocStringEntry *)malloc(sizeof(LocStringEntry));
    if (!entry) return false;

    strncpy(entry->key, key, sizeof(entry->key) - 1);
    entry->key[sizeof(entry->key) - 1] = '\0';

    entry->value = strdup(value);
    if (!entry->value) {
        free(entry);
        return false;
    }

    entry->next = lang->strings[hash];
    lang->strings[hash] = entry;
    lang->string_count++;

    return true;
}

static const char *find_string(const LocLanguage *lang, const char *key) {
    if (!lang || !key) return NULL;

    unsigned long hash = hash_string(key);
    LocStringEntry *entry = lang->strings[hash];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

static LocLanguage *find_language_by_locale(const Agentite_Localization *loc, const char *locale) {
    if (!loc || !locale) return NULL;

    for (size_t i = 0; i < loc->language_count; i++) {
        if (strcmp(loc->languages[i]->info.locale, locale) == 0) {
            return loc->languages[i];
        }
    }

    return NULL;
}

static size_t find_language_index(const Agentite_Localization *loc, const char *locale) {
    if (!loc || !locale) return SIZE_MAX;

    for (size_t i = 0; i < loc->language_count; i++) {
        if (strcmp(loc->languages[i]->info.locale, locale) == 0) {
            return i;
        }
    }

    return SIZE_MAX;
}

/* ============================================================================
 * TOML Parsing
 * ============================================================================ */

static void parse_strings_table(LocLanguage *lang, toml_table_t *table, const char *prefix) {
    if (!lang || !table) return;

    int i = 0;
    const char *key;

    while ((key = toml_key_in(table, i++)) != NULL) {
        /* Build full key path */
        char full_key[LOC_MAX_KEY_LENGTH];
        if (prefix && prefix[0]) {
            snprintf(full_key, sizeof(full_key), "%s.%s", prefix, key);
        } else {
            strncpy(full_key, key, sizeof(full_key) - 1);
            full_key[sizeof(full_key) - 1] = '\0';
        }

        /* Try as string first */
        toml_datum_t str = toml_string_in(table, key);
        if (str.ok) {
            add_string(lang, full_key, str.u.s);
            free(str.u.s);
            continue;
        }

        /* Try as nested table */
        toml_table_t *nested = toml_table_in(table, key);
        if (nested) {
            parse_strings_table(lang, nested, full_key);
        }
    }
}

static bool parse_language_toml(LocLanguage *lang, toml_table_t *root) {
    if (!lang || !root) return false;

    /* Parse [meta] section */
    toml_table_t *meta = toml_table_in(root, "meta");
    if (meta) {
        toml_datum_t d;

        d = toml_string_in(meta, "language");
        if (d.ok) {
            strncpy(lang->info.name, d.u.s, sizeof(lang->info.name) - 1);
            lang->info.name[sizeof(lang->info.name) - 1] = '\0';
            free(d.u.s);
        }

        d = toml_string_in(meta, "locale");
        if (d.ok) {
            strncpy(lang->info.locale, d.u.s, sizeof(lang->info.locale) - 1);
            lang->info.locale[sizeof(lang->info.locale) - 1] = '\0';
            free(d.u.s);
        }

        d = toml_string_in(meta, "direction");
        if (d.ok) {
            if (strcmp(d.u.s, "rtl") == 0) {
                lang->info.direction = AGENTITE_TEXT_RTL;
            } else {
                lang->info.direction = AGENTITE_TEXT_LTR;
            }
            free(d.u.s);
        }

        d = toml_string_in(meta, "font");
        if (d.ok) {
            strncpy(lang->info.font_key, d.u.s, sizeof(lang->info.font_key) - 1);
            lang->info.font_key[sizeof(lang->info.font_key) - 1] = '\0';
            free(d.u.s);
        }
    }

    /* Set plural rule based on locale */
    lang->plural_rule = get_default_plural_rule(lang->info.locale);

    /* Parse [strings] section */
    toml_table_t *strings = toml_table_in(root, "strings");
    if (strings) {
        parse_strings_table(lang, strings, "");
    }

    return true;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

Agentite_Localization *agentite_loc_create(const Agentite_LocalizationConfig *config) {
    Agentite_LocalizationConfig cfg = AGENTITE_LOCALIZATION_CONFIG_DEFAULT;
    if (config) {
        cfg = *config;
    }

    Agentite_Localization *loc = AGENTITE_ALLOC(Agentite_Localization);
    if (!loc) {
        agentite_set_error("Failed to allocate localization context");
        return NULL;
    }

    loc->current_language = SIZE_MAX;
    loc->fallback_language = SIZE_MAX;
    loc->max_languages = cfg.max_languages > 0 ? cfg.max_languages : LOC_MAX_LANGUAGES;
    loc->format_buffer_size = cfg.format_buffer_size > 0 ? cfg.format_buffer_size : LOC_DEFAULT_BUFFER_SIZE;

    if (cfg.locales_path) {
        strncpy(loc->locales_path, cfg.locales_path, sizeof(loc->locales_path) - 1);
        loc->locales_path[sizeof(loc->locales_path) - 1] = '\0';
    }

    /* Auto-load all .toml files from locales directory */
    if (cfg.locales_path && cfg.locales_path[0]) {
        DIR *dir = opendir(cfg.locales_path);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                /* Skip . and .. */
                if (entry->d_name[0] == '.') continue;

                /* Check for .toml extension */
                size_t len = strlen(entry->d_name);
                if (len > 5 && strcmp(entry->d_name + len - 5, ".toml") == 0) {
                    char path[512];
                    snprintf(path, sizeof(path), "%s/%s", cfg.locales_path, entry->d_name);
                    agentite_loc_load_language(loc, path);
                }
            }
            closedir(dir);
        }
    }

    /* Set fallback language */
    if (cfg.fallback_locale) {
        loc->fallback_language = find_language_index(loc, cfg.fallback_locale);

        /* Also set as current if no language set yet */
        if (loc->current_language == SIZE_MAX && loc->fallback_language != SIZE_MAX) {
            loc->current_language = loc->fallback_language;
        }
    }

    /* If still no language, use first available */
    if (loc->current_language == SIZE_MAX && loc->language_count > 0) {
        loc->current_language = 0;
    }

    return loc;
}

void agentite_loc_destroy(Agentite_Localization *loc) {
    if (!loc) return;

    /* Clear global if this is it */
    if (s_global_loc == loc) {
        s_global_loc = NULL;
    }

    /* Free all languages */
    for (size_t i = 0; i < loc->language_count; i++) {
        destroy_language(loc->languages[i]);
    }

    free(loc);
}

/* ============================================================================
 * Language Loading
 * ============================================================================ */

bool agentite_loc_load_language(Agentite_Localization *loc, const char *path) {
    if (!loc || !path) {
        agentite_set_error("Invalid parameters");
        return false;
    }

    if (loc->language_count >= loc->max_languages) {
        agentite_set_error("Maximum number of languages reached");
        return false;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        agentite_set_error("Cannot open file: %s", path);
        return false;
    }

    char errbuf[256];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root) {
        agentite_set_error("TOML parse error in %s: %s", path, errbuf);
        return false;
    }

    LocLanguage *lang = create_language();
    if (!lang) {
        toml_free(root);
        agentite_set_error("Failed to allocate language");
        return false;
    }

    /* Extract locale from filename if not in meta */
    /* e.g., "en.toml" -> locale = "en" */
    const char *filename = strrchr(path, '/');
    if (!filename) filename = strrchr(path, '\\');
    filename = filename ? filename + 1 : path;

    size_t name_len = strlen(filename);
    if (name_len > 5) {
        name_len -= 5; /* Remove .toml */
        if (name_len < sizeof(lang->info.locale)) {
            strncpy(lang->info.locale, filename, name_len);
            lang->info.locale[name_len] = '\0';
        }
    }

    if (!parse_language_toml(lang, root)) {
        toml_free(root);
        destroy_language(lang);
        agentite_set_error("Failed to parse language file: %s", path);
        return false;
    }

    toml_free(root);

    /* Check if locale already exists, replace if so */
    size_t existing = find_language_index(loc, lang->info.locale);
    if (existing != SIZE_MAX) {
        destroy_language(loc->languages[existing]);
        loc->languages[existing] = lang;
    } else {
        loc->languages[loc->language_count++] = lang;
    }

    return true;
}

bool agentite_loc_load_language_string(Agentite_Localization *loc,
                                       const char *toml_string,
                                       const char *locale) {
    if (!loc || !toml_string || !locale) {
        agentite_set_error("Invalid parameters");
        return false;
    }

    if (loc->language_count >= loc->max_languages) {
        agentite_set_error("Maximum number of languages reached");
        return false;
    }

    /* toml_parse needs mutable string */
    char *copy = strdup(toml_string);
    if (!copy) {
        agentite_set_error("Out of memory");
        return false;
    }

    char errbuf[256];
    toml_table_t *root = toml_parse(copy, errbuf, sizeof(errbuf));
    free(copy);

    if (!root) {
        agentite_set_error("TOML parse error: %s", errbuf);
        return false;
    }

    LocLanguage *lang = create_language();
    if (!lang) {
        toml_free(root);
        agentite_set_error("Failed to allocate language");
        return false;
    }

    /* Set locale from parameter */
    strncpy(lang->info.locale, locale, sizeof(lang->info.locale) - 1);
    lang->info.locale[sizeof(lang->info.locale) - 1] = '\0';

    if (!parse_language_toml(lang, root)) {
        toml_free(root);
        destroy_language(lang);
        agentite_set_error("Failed to parse language string");
        return false;
    }

    toml_free(root);

    /* Check if locale already exists, replace if so */
    size_t existing = find_language_index(loc, lang->info.locale);
    if (existing != SIZE_MAX) {
        destroy_language(loc->languages[existing]);
        loc->languages[existing] = lang;
    } else {
        loc->languages[loc->language_count++] = lang;
    }

    return true;
}

/* ============================================================================
 * Language Management
 * ============================================================================ */

bool agentite_loc_set_language(Agentite_Localization *loc, const char *locale) {
    if (!loc || !locale) return false;

    size_t idx = find_language_index(loc, locale);
    if (idx == SIZE_MAX) {
        agentite_set_error("Language not found: %s", locale);
        return false;
    }

    loc->current_language = idx;
    return true;
}

const char *agentite_loc_get_language(const Agentite_Localization *loc) {
    if (!loc || loc->current_language == SIZE_MAX) return NULL;
    return loc->languages[loc->current_language]->info.locale;
}

const Agentite_LanguageInfo *agentite_loc_get_language_info(const Agentite_Localization *loc) {
    if (!loc || loc->current_language == SIZE_MAX) return NULL;
    return &loc->languages[loc->current_language]->info;
}

size_t agentite_loc_get_language_count(const Agentite_Localization *loc) {
    if (!loc) return 0;
    return loc->language_count;
}

const Agentite_LanguageInfo *agentite_loc_get_language_at(const Agentite_Localization *loc,
                                                          size_t index) {
    if (!loc || index >= loc->language_count) return NULL;
    return &loc->languages[index]->info;
}

/* ============================================================================
 * String Lookup
 * ============================================================================ */

const char *agentite_loc_get(const Agentite_Localization *loc, const char *key) {
    if (!loc || !key) return key ? key : "";

    /* Try current language */
    if (loc->current_language != SIZE_MAX) {
        const char *value = find_string(loc->languages[loc->current_language], key);
        if (value) return value;
    }

    /* Try fallback language */
    if (loc->fallback_language != SIZE_MAX && loc->fallback_language != loc->current_language) {
        const char *value = find_string(loc->languages[loc->fallback_language], key);
        if (value) return value;
    }

    /* Return key itself for debugging visibility */
    return key;
}

bool agentite_loc_has_key(const Agentite_Localization *loc, const char *key) {
    if (!loc || !key || loc->current_language == SIZE_MAX) return false;
    return find_string(loc->languages[loc->current_language], key) != NULL;
}

/* ============================================================================
 * Parameter Substitution
 * ============================================================================ */

/* Substitute positional parameters {0}, {1}, etc. */
static const char *substitute_positional(const char *template_str,
                                         const char **args, size_t arg_count,
                                         char *buffer, size_t buffer_size) {
    char *out = buffer;
    char *out_end = buffer + buffer_size - 1;
    const char *in = template_str;

    while (*in && out < out_end) {
        if (*in == '{') {
            const char *start = in + 1;
            if (*start >= '0' && *start <= '9') {
                /* Parse parameter index */
                int index = 0;
                const char *num = start;
                while (*num >= '0' && *num <= '9') {
                    index = index * 10 + (*num - '0');
                    num++;
                }

                if (*num == '}') {
                    /* Valid parameter reference */
                    if ((size_t)index < arg_count && args[index]) {
                        const char *arg = args[index];
                        while (*arg && out < out_end) {
                            *out++ = *arg++;
                        }
                    }
                    in = num + 1;
                    continue;
                }
            }
        }

        *out++ = *in++;
    }

    *out = '\0';
    return buffer;
}

/* Substitute named parameters {name}, {count}, etc. */
static const char *substitute_named(const char *template_str,
                                    const char **names, const char **values, size_t pair_count,
                                    char *buffer, size_t buffer_size) {
    char *out = buffer;
    char *out_end = buffer + buffer_size - 1;
    const char *in = template_str;

    while (*in && out < out_end) {
        if (*in == '{') {
            /* Find closing brace */
            const char *end = strchr(in + 1, '}');
            if (end && end > in + 1) {
                size_t name_len = (size_t)(end - in - 1);

                /* Search for matching name */
                bool found = false;
                for (size_t i = 0; i < pair_count; i++) {
                    if (names[i] && strlen(names[i]) == name_len &&
                        strncmp(names[i], in + 1, name_len) == 0) {
                        /* Found match, substitute value */
                        if (values[i]) {
                            const char *val = values[i];
                            while (*val && out < out_end) {
                                *out++ = *val++;
                            }
                        }
                        in = end + 1;
                        found = true;
                        break;
                    }
                }
                if (found) continue;
            }
        }

        *out++ = *in++;
    }

    *out = '\0';
    return buffer;
}

const char *agentite_loc_format(Agentite_Localization *loc, const char *key, ...) {
    if (!loc || !key) return "";

    const char *template_str = agentite_loc_get(loc, key);

    /* Collect args */
    const char *args[16];
    size_t arg_count = 0;

    va_list ap;
    va_start(ap, key);
    while (arg_count < 16) {
        const char *arg = va_arg(ap, const char *);
        if (!arg) break;
        args[arg_count++] = arg;
    }
    va_end(ap);

    return substitute_positional(template_str, args, arg_count,
                                 s_format_buffer, sizeof(s_format_buffer));
}

const char *agentite_loc_format_named(Agentite_Localization *loc, const char *key, ...) {
    if (!loc || !key) return "";

    const char *template_str = agentite_loc_get(loc, key);

    /* Collect name/value pairs */
    const char *names[16];
    const char *values[16];
    size_t pair_count = 0;

    va_list ap;
    va_start(ap, key);
    while (pair_count < 16) {
        const char *name = va_arg(ap, const char *);
        if (!name) break;
        const char *value = va_arg(ap, const char *);
        if (!value) break;
        names[pair_count] = name;
        values[pair_count] = value;
        pair_count++;
    }
    va_end(ap);

    return substitute_named(template_str, names, values, pair_count,
                            s_format_buffer, sizeof(s_format_buffer));
}

const char *agentite_loc_format_int(Agentite_Localization *loc, const char *key, int64_t value) {
    if (!loc || !key) return "";

    const char *template_str = agentite_loc_get(loc, key);

    /* Format integer to string */
    char int_str[32];
    snprintf(int_str, sizeof(int_str), "%lld", (long long)value);

    /* Try both {0} and common named params */
    const char *args[] = { int_str };
    const char *names[] = { "0", "count", "value", "n" };
    const char *vals[] = { int_str, int_str, int_str, int_str };

    /* First try positional */
    substitute_positional(template_str, args, 1, s_format_buffer, sizeof(s_format_buffer));

    /* If nothing changed, try named */
    if (strcmp(s_format_buffer, template_str) == 0) {
        substitute_named(template_str, names, vals, 4, s_format_buffer, sizeof(s_format_buffer));
    }

    return s_format_buffer;
}

/* ============================================================================
 * Pluralization
 * ============================================================================ */

/* Select plural form from pipe-separated string */
static const char *select_plural_form(const char *value, int form_index,
                                       char *buffer, size_t buffer_size) {
    const char *start = value;
    int current_form = 0;

    /* Skip to desired form */
    while (*start && current_form < form_index) {
        if (*start == '|') {
            current_form++;
            start++;
        } else {
            start++;
        }
    }

    /* If we didn't find enough forms, use last one */
    if (current_form < form_index) {
        start = value;
        const char *last_start = value;
        while (*start) {
            if (*start == '|') {
                last_start = start + 1;
            }
            start++;
        }
        start = last_start;
    }

    /* Find end of this form */
    const char *end = start;
    while (*end && *end != '|') {
        end++;
    }

    /* Copy to buffer */
    size_t len = (size_t)(end - start);
    if (len >= buffer_size) {
        len = buffer_size - 1;
    }
    memcpy(buffer, start, len);
    buffer[len] = '\0';

    return buffer;
}

const char *agentite_loc_plural(Agentite_Localization *loc, const char *key, int64_t count) {
    if (!loc || !key) return "";

    const char *template_str = agentite_loc_get(loc, key);

    /* Get plural form index from rule */
    int form_index = 0;
    if (loc->current_language != SIZE_MAX) {
        Agentite_PluralRule rule = loc->languages[loc->current_language]->plural_rule;
        if (rule) {
            form_index = rule(count);
        }
    }

    /* Select the appropriate plural form */
    char form_buffer[LOC_MAX_VALUE_LENGTH];
    select_plural_form(template_str, form_index, form_buffer, sizeof(form_buffer));

    /* Substitute {count} with the number */
    char count_str[32];
    snprintf(count_str, sizeof(count_str), "%lld", (long long)count);

    const char *names[] = { "count", "n", "0" };
    const char *vals[] = { count_str, count_str, count_str };

    return substitute_named(form_buffer, names, vals, 3,
                            s_format_buffer, sizeof(s_format_buffer));
}

bool agentite_loc_set_plural_rule(Agentite_Localization *loc,
                                  const char *locale,
                                  Agentite_PluralRule rule) {
    if (!loc || !locale || !rule) return false;

    LocLanguage *lang = find_language_by_locale(loc, locale);
    if (!lang) {
        agentite_set_error("Language not found: %s", locale);
        return false;
    }

    lang->plural_rule = rule;
    return true;
}

/* ============================================================================
 * Font & Direction
 * ============================================================================ */

const char *agentite_loc_get_font_key(const Agentite_Localization *loc) {
    if (!loc || loc->current_language == SIZE_MAX) return "default";
    return loc->languages[loc->current_language]->info.font_key;
}

Agentite_TextDirection agentite_loc_get_text_direction(const Agentite_Localization *loc) {
    if (!loc || loc->current_language == SIZE_MAX) return AGENTITE_TEXT_LTR;
    return loc->languages[loc->current_language]->info.direction;
}

/* ============================================================================
 * Validation
 * ============================================================================ */

/* Collect all keys from a language */
static bool collect_keys(const LocLanguage *lang, char ***out_keys, size_t *out_count) {
    if (!lang || !out_keys || !out_count) return false;

    size_t count = lang->string_count;
    if (count == 0) {
        *out_keys = NULL;
        *out_count = 0;
        return true;
    }

    char **keys = (char **)malloc(sizeof(char *) * count);
    if (!keys) return false;

    size_t idx = 0;
    for (int i = 0; i < LOC_HASH_TABLE_SIZE && idx < count; i++) {
        LocStringEntry *entry = lang->strings[i];
        while (entry && idx < count) {
            keys[idx] = strdup(entry->key);
            if (!keys[idx]) {
                /* Cleanup on failure */
                for (size_t j = 0; j < idx; j++) free(keys[j]);
                free(keys);
                return false;
            }
            idx++;
            entry = entry->next;
        }
    }

    *out_keys = keys;
    *out_count = idx;
    return true;
}

bool agentite_loc_validate(const Agentite_Localization *loc,
                           const char *target_locale,
                           const char *reference_locale,
                           Agentite_LocalizationValidation *result) {
    if (!loc || !target_locale || !reference_locale || !result) {
        agentite_set_error("Invalid parameters");
        return false;
    }

    memset(result, 0, sizeof(*result));

    /* Find languages */
    LocLanguage *target = find_language_by_locale(loc, target_locale);
    LocLanguage *reference = find_language_by_locale(loc, reference_locale);

    if (!target) {
        agentite_set_error("Target language not found: %s", target_locale);
        return false;
    }
    if (!reference) {
        agentite_set_error("Reference language not found: %s", reference_locale);
        return false;
    }

    /* Collect keys from both */
    char **ref_keys = NULL, **tgt_keys = NULL;
    size_t ref_count = 0, tgt_count = 0;

    if (!collect_keys(reference, &ref_keys, &ref_count) ||
        !collect_keys(target, &tgt_keys, &tgt_count)) {
        agentite_loc_free_keys(ref_keys, ref_count);
        agentite_loc_free_keys(tgt_keys, tgt_count);
        agentite_set_error("Failed to collect keys");
        return false;
    }

    /* Find missing keys (in reference but not in target) */
    size_t missing_capacity = 16;
    result->missing_keys = (char **)malloc(sizeof(char *) * missing_capacity);
    result->missing_count = 0;

    for (size_t i = 0; i < ref_count; i++) {
        bool found = false;
        for (size_t j = 0; j < tgt_count; j++) {
            if (strcmp(ref_keys[i], tgt_keys[j]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (result->missing_count >= missing_capacity) {
                missing_capacity *= 2;
                result->missing_keys = (char **)realloc(result->missing_keys,
                                                        sizeof(char *) * missing_capacity);
            }
            result->missing_keys[result->missing_count++] = strdup(ref_keys[i]);
        }
    }

    /* Find extra keys (in target but not in reference) */
    size_t extra_capacity = 16;
    result->extra_keys = (char **)malloc(sizeof(char *) * extra_capacity);
    result->extra_count = 0;

    for (size_t i = 0; i < tgt_count; i++) {
        bool found = false;
        for (size_t j = 0; j < ref_count; j++) {
            if (strcmp(tgt_keys[i], ref_keys[j]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (result->extra_count >= extra_capacity) {
                extra_capacity *= 2;
                result->extra_keys = (char **)realloc(result->extra_keys,
                                                      sizeof(char *) * extra_capacity);
            }
            result->extra_keys[result->extra_count++] = strdup(tgt_keys[i]);
        }
    }

    /* Cleanup */
    agentite_loc_free_keys(ref_keys, ref_count);
    agentite_loc_free_keys(tgt_keys, tgt_count);

    return true;
}

void agentite_loc_validation_free(Agentite_LocalizationValidation *result) {
    if (!result) return;

    for (size_t i = 0; i < result->missing_count; i++) {
        free(result->missing_keys[i]);
    }
    free(result->missing_keys);

    for (size_t i = 0; i < result->extra_count; i++) {
        free(result->extra_keys[i]);
    }
    free(result->extra_keys);

    memset(result, 0, sizeof(*result));
}

bool agentite_loc_get_all_keys(const Agentite_Localization *loc,
                               char ***out_keys,
                               size_t *out_count) {
    if (!loc || !out_keys || !out_count) {
        agentite_set_error("Invalid parameters");
        return false;
    }

    if (loc->current_language == SIZE_MAX) {
        *out_keys = NULL;
        *out_count = 0;
        return true;
    }

    return collect_keys(loc->languages[loc->current_language], out_keys, out_count);
}

void agentite_loc_free_keys(char **keys, size_t count) {
    if (!keys) return;
    for (size_t i = 0; i < count; i++) {
        free(keys[i]);
    }
    free(keys);
}

/* ============================================================================
 * Global API
 * ============================================================================ */

void agentite_loc_set_global(Agentite_Localization *loc) {
    s_global_loc = loc;
}

Agentite_Localization *agentite_loc_get_global(void) {
    return s_global_loc;
}
