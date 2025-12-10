#ifndef AGENTITE_ERROR_H
#define AGENTITE_ERROR_H

#include <stdbool.h>
#include <stdarg.h>

/**
 * Carbon Error Handling System
 *
 * Provides thread-local error storage with printf-style formatting.
 * Integrates with SDL_GetError() for graphics/system errors.
 *
 * Usage:
 *   if (!some_operation()) {
 *       agentite_set_error("Operation failed: %s", reason);
 *       return NULL;
 *   }
 *
 *   // Later, retrieve the error:
 *   const char *err = agentite_get_last_error();
 *   if (err[0] != '\0') {
 *       printf("Error: %s\n", err);
 *   }
 */

/**
 * Set an error message with printf-style formatting.
 * The message is stored in a thread-local buffer.
 *
 * @param fmt Format string (printf-style)
 * @param ... Format arguments
 */
void agentite_set_error(const char *fmt, ...);

/**
 * Set an error message with va_list arguments.
 * Useful for wrapper functions that accept variadic arguments.
 *
 * @param fmt Format string (printf-style)
 * @param args va_list of format arguments
 */
void agentite_set_error_v(const char *fmt, va_list args);

/**
 * Get the last error message.
 * Returns an empty string if no error has been set.
 *
 * @return Pointer to the error message (thread-local, do not free)
 */
const char *agentite_get_last_error(void);

/**
 * Clear the last error message.
 * Sets the error buffer to an empty string.
 */
void agentite_clear_error(void);

/**
 * Check if an error is currently set.
 *
 * @return true if an error message is set, false otherwise
 */
bool agentite_has_error(void);

/**
 * Set error from SDL_GetError().
 * Copies the current SDL error into the Carbon error buffer.
 * Useful when an SDL function fails.
 *
 * @param prefix Optional prefix to prepend (can be NULL)
 */
void agentite_set_error_from_sdl(const char *prefix);

/**
 * Log the last error to SDL_Log and clear it.
 * Convenience function for error reporting.
 */
void agentite_log_and_clear_error(void);

#endif /* AGENTITE_ERROR_H */
