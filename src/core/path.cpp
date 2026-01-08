#include "agentite/path.h"
#include "agentite/error.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PATH_SEPARATOR '\\'
#else
#include <unistd.h>
#include <limits.h>
#define PATH_SEPARATOR '/'
#endif

// Check if character is a path separator
static bool is_separator(char c) {
    return c == '/' || c == '\\';
}

// Check if a path component is ".."
static bool is_parent_ref(const char *start, size_t len) {
    return len == 2 && start[0] == '.' && start[1] == '.';
}

bool agentite_path_component_is_safe(const char *name, size_t max_length) {
    if (!name || name[0] == '\0') {
        return false;
    }

    size_t len = strlen(name);

    // Check length limit
    if (max_length > 0 && len > max_length) {
        return false;
    }

    // Check for path separators
    for (size_t i = 0; i < len; i++) {
        if (is_separator(name[i])) {
            return false;
        }
    }

    // Check for ".."
    if (is_parent_ref(name, len)) {
        return false;
    }

    return true;
}

bool agentite_path_is_safe(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    // Reject absolute paths
    if (agentite_path_is_absolute(path)) {
        return false;
    }

    // Scan through path looking for ".." components
    const char *p = path;
    while (*p) {
        // Find start of component
        while (*p && is_separator(*p)) {
            p++;
        }
        if (!*p) break;

        // Find end of component
        const char *start = p;
        while (*p && !is_separator(*p)) {
            p++;
        }
        size_t len = p - start;

        // Check for ".." component
        if (is_parent_ref(start, len)) {
            return false;
        }
    }

    return true;
}

bool agentite_path_is_within(const char *path, const char *base_dir) {
    if (!path || !base_dir) {
        return false;
    }

    char canonical_path[AGENTITE_PATH_MAX];
    char canonical_base[AGENTITE_PATH_MAX];

    // Canonicalize both paths
    if (!agentite_path_canonicalize(path, canonical_path, sizeof(canonical_path))) {
        return false;
    }
    if (!agentite_path_canonicalize(base_dir, canonical_base, sizeof(canonical_base))) {
        return false;
    }

    // Ensure base ends with separator for proper prefix matching
    size_t base_len = strlen(canonical_base);
    if (base_len == 0) {
        return false;
    }

    // Check if path starts with base
    if (strncmp(canonical_path, canonical_base, base_len) != 0) {
        return false;
    }

    // Verify there's a separator after the base (or path equals base)
    if (canonical_path[base_len] == '\0') {
        return true;  // Exact match
    }
    if (is_separator(canonical_path[base_len])) {
        return true;  // Path is under base
    }

    // Path shares prefix but isn't actually under base
    // e.g., base="/foo/bar", path="/foo/barbaz"
    return false;
}

bool agentite_path_normalize(const char *path, char *out, size_t outlen) {
    if (!path || !out || outlen == 0) {
        return false;
    }

    size_t path_len = strlen(path);
    if (path_len >= outlen) {
        agentite_set_error("path_normalize: path too long");
        return false;
    }

    char *dst = out;
    char *dst_end = out + outlen - 1;
    const char *src = path;
    bool last_was_sep = false;

    while (*src && dst < dst_end) {
        if (is_separator(*src)) {
            // Collapse multiple separators
            if (!last_was_sep) {
                *dst++ = '/';  // Normalize to forward slash
                last_was_sep = true;
            }
            src++;
        } else if (*src == '.' && (is_separator(src[1]) || src[1] == '\0')) {
            // Skip "." component
            src++;
            if (*src) src++;  // Skip separator after .
        } else {
            // Copy regular character
            *dst++ = *src++;
            last_was_sep = false;
        }
    }

    // Remove trailing separator (unless it's the root)
    if (dst > out + 1 && dst[-1] == '/') {
        dst--;
    }

    *dst = '\0';
    return true;
}

bool agentite_path_join(const char *base, const char *name, char *out, size_t outlen) {
    if (!name || !out || outlen == 0) {
        return false;
    }

    // Validate the name component
    if (!agentite_path_is_safe(name)) {
        agentite_set_error("path_join: unsafe path component");
        return false;
    }

    size_t base_len = base ? strlen(base) : 0;
    size_t name_len = strlen(name);
    size_t need_sep = 0;

    // Check if we need a separator
    if (base_len > 0 && !is_separator(base[base_len - 1])) {
        need_sep = 1;
    }

    // Check total length
    if (base_len + need_sep + name_len >= outlen) {
        agentite_set_error("path_join: result too long");
        return false;
    }

    // Build the path
    if (base_len > 0) {
        memcpy(out, base, base_len);
    }
    if (need_sep) {
        out[base_len] = '/';
    }
    memcpy(out + base_len + need_sep, name, name_len);
    out[base_len + need_sep + name_len] = '\0';

    return true;
}

bool agentite_path_canonicalize(const char *path, char *out, size_t outlen) {
    if (!path || !out || outlen == 0) {
        return false;
    }

#ifdef _WIN32
    char *result = _fullpath(out, path, outlen);
    if (!result) {
        agentite_set_error("path_canonicalize: _fullpath failed");
        return false;
    }
    // Normalize separators to forward slashes
    for (char *p = out; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    return true;
#else
    char resolved[PATH_MAX];
    if (!realpath(path, resolved)) {
        agentite_set_error("path_canonicalize: realpath failed for '%s'", path);
        return false;
    }
    size_t len = strlen(resolved);
    if (len >= outlen) {
        agentite_set_error("path_canonicalize: result too long");
        return false;
    }
    memcpy(out, resolved, len + 1);
    return true;
#endif
}

bool agentite_path_is_absolute(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }

#ifdef _WIN32
    // Check for drive letter (C:) or UNC path (\\)
    if (isalpha((unsigned char)path[0]) && path[1] == ':') {
        return true;
    }
    if (path[0] == '\\' && path[1] == '\\') {
        return true;
    }
#endif

    // Unix-style absolute path
    return path[0] == '/';
}

const char *agentite_path_filename(const char *path) {
    if (!path) {
        return NULL;
    }

    const char *last_sep = NULL;
    for (const char *p = path; *p; p++) {
        if (is_separator(*p)) {
            last_sep = p;
        }
    }

    return last_sep ? last_sep + 1 : path;
}

bool agentite_path_dirname(const char *path, char *out, size_t outlen) {
    if (!path || !out || outlen == 0) {
        return false;
    }

    const char *last_sep = NULL;
    for (const char *p = path; *p; p++) {
        if (is_separator(*p)) {
            last_sep = p;
        }
    }

    if (!last_sep) {
        // No directory component
        if (outlen < 2) return false;
        out[0] = '.';
        out[1] = '\0';
        return true;
    }

    size_t dir_len = last_sep - path;
    if (dir_len == 0) {
        // Root directory
        if (outlen < 2) return false;
        out[0] = '/';
        out[1] = '\0';
        return true;
    }

    if (dir_len >= outlen) {
        agentite_set_error("path_dirname: result too long");
        return false;
    }

    memcpy(out, path, dir_len);
    out[dir_len] = '\0';
    return true;
}
