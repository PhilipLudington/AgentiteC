#ifndef AGENTITE_PATH_H
#define AGENTITE_PATH_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Path Validation Utilities
 *
 * Functions for validating file paths to prevent directory traversal attacks.
 * Use these before any file I/O operations on user-provided or external paths.
 *
 * Security considerations:
 * - Rejects paths containing ".." sequences
 * - Rejects paths with null bytes
 * - Optionally validates paths stay within a base directory
 * - Handles both forward and back slashes as separators
 */

/**
 * Check if a path component (filename or directory name) is safe.
 * Rejects:
 * - NULL or empty strings
 * - Names containing path separators (/ or \)
 * - Names containing ".."
 * - Names that are too long (> max_length)
 * - Names containing null bytes
 *
 * @param name       The path component to validate
 * @param max_length Maximum allowed length (0 = no limit)
 * @return true if the component is safe
 */
bool agentite_path_component_is_safe(const char *name, size_t max_length);

/**
 * Check if a relative path is safe (no directory traversal).
 * Rejects:
 * - NULL or empty strings
 * - Paths containing ".." components
 * - Absolute paths (starting with / or drive letter on Windows)
 * - Paths containing null bytes
 *
 * @param path The relative path to validate
 * @return true if the path is safe
 */
bool agentite_path_is_safe(const char *path);

/**
 * Check if a path stays within a base directory.
 * This is more thorough than agentite_path_is_safe() as it resolves
 * the full path and verifies it's a descendant of base_dir.
 *
 * @param path     The path to validate (can be relative or absolute)
 * @param base_dir The base directory that path must stay within
 * @return true if path resolves to a location within base_dir
 */
bool agentite_path_is_within(const char *path, const char *base_dir);

/**
 * Normalize a path by removing redundant separators and . components.
 * Does NOT resolve .. components (use agentite_path_is_safe() to reject those).
 *
 * @param path   The path to normalize
 * @param out    Output buffer for normalized path
 * @param outlen Size of output buffer
 * @return true if normalization succeeded, false if buffer too small
 */
bool agentite_path_normalize(const char *path, char *out, size_t outlen);

/**
 * Join two path components safely.
 * Ensures proper separator between components and validates result.
 *
 * @param base    Base path (can be empty)
 * @param name    Component to append
 * @param out     Output buffer
 * @param outlen  Size of output buffer
 * @return true if join succeeded and result is safe, false otherwise
 */
bool agentite_path_join(const char *base, const char *name, char *out, size_t outlen);

/**
 * Get the canonical/absolute form of a path.
 * Resolves symbolic links and removes . and .. components.
 *
 * @param path   The path to canonicalize
 * @param out    Output buffer for canonical path
 * @param outlen Size of output buffer
 * @return true if canonicalization succeeded
 *
 * Note: This function may fail if the path does not exist.
 */
bool agentite_path_canonicalize(const char *path, char *out, size_t outlen);

/**
 * Check if a path is absolute.
 *
 * @param path The path to check
 * @return true if the path is absolute
 */
bool agentite_path_is_absolute(const char *path);

/**
 * Extract the filename component from a path.
 *
 * @param path The full path
 * @return Pointer to the filename within path (not a copy)
 */
const char *agentite_path_filename(const char *path);

/**
 * Extract the directory component from a path.
 *
 * @param path   The full path
 * @param out    Output buffer for directory path
 * @param outlen Size of output buffer
 * @return true if extraction succeeded
 */
bool agentite_path_dirname(const char *path, char *out, size_t outlen);

/**
 * Maximum recommended path length for portable code.
 */
#define AGENTITE_PATH_MAX 4096

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_PATH_H */
