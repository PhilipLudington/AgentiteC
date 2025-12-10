#ifndef AGENTITE_LOG_H
#define AGENTITE_LOG_H

#include <stdbool.h>
#include <stdarg.h>

/**
 * Carbon Logging System
 *
 * File-based logging with subsystem tags and log levels for debugging
 * and post-mortem analysis.
 *
 * Usage:
 *   // Initialize at startup
 *   agentite_log_init();  // Uses default path: /tmp/carbon.log (Unix) or carbon.log (Windows)
 *   // Or: agentite_log_init_with_path("game.log");
 *
 *   // Log messages with subsystem tags
 *   agentite_log_info(AGENTITE_LOG_CORE, "Engine initialized");
 *   agentite_log_warning(AGENTITE_LOG_GRAPHICS, "Texture not found: %s", path);
 *   agentite_log_error(AGENTITE_LOG_AUDIO, "Failed to load sound");
 *   agentite_log_debug(AGENTITE_LOG_AI, "Processing %d entities", count);
 *
 *   // Shutdown at exit
 *   agentite_log_shutdown();
 *
 * Output format:
 *   [2024-01-15 14:30:22] [ERROR  ] [Graphics  ] Failed to load texture
 */

/**
 * Log levels - higher values include lower levels
 */
typedef enum {
    AGENTITE_LOG_LEVEL_ERROR = 0,    /**< Critical errors, always logged, auto-flush */
    AGENTITE_LOG_LEVEL_WARNING = 1,  /**< Warnings that may indicate problems */
    AGENTITE_LOG_LEVEL_INFO = 2,     /**< General information */
    AGENTITE_LOG_LEVEL_DEBUG = 3     /**< Verbose debug output */
} Agentite_LogLevel;

/**
 * Predefined subsystem identifiers
 * Use these or define your own game-specific subsystems
 */
#define AGENTITE_LOG_CORE       "Core"
#define AGENTITE_LOG_ECS        "ECS"
#define AGENTITE_LOG_GRAPHICS   "Graphics"
#define AGENTITE_LOG_AUDIO      "Audio"
#define AGENTITE_LOG_INPUT      "Input"
#define AGENTITE_LOG_AI         "AI"
#define AGENTITE_LOG_UI         "UI"
#define AGENTITE_LOG_GAME       "Game"
#define AGENTITE_LOG_NET        "Network"
#define AGENTITE_LOG_SAVE       "Save"
#define AGENTITE_LOG_SCRIPT     "Script"

/**
 * Initialize the logging system with default log file path.
 * Default path: /tmp/carbon.log (Unix) or carbon.log (Windows)
 *
 * @return true on success, false on failure
 */
bool agentite_log_init(void);

/**
 * Initialize the logging system with a custom log file path.
 *
 * @param path Path to the log file (NULL uses default)
 * @return true on success, false on failure
 */
bool agentite_log_init_with_path(const char *path);

/**
 * Shutdown the logging system.
 * Writes session end marker and closes the log file.
 */
void agentite_log_shutdown(void);

/**
 * Check if the logging system is initialized.
 *
 * @return true if initialized, false otherwise
 */
bool agentite_log_is_initialized(void);

/**
 * Set the current log level filter.
 * Messages above this level will not be logged.
 *
 * @param level The maximum level to log
 */
void agentite_log_set_level(Agentite_LogLevel level);

/**
 * Get the current log level filter.
 *
 * @return The current log level
 */
Agentite_LogLevel agentite_log_get_level(void);

/**
 * Set whether to also output to console (SDL_Log).
 * Enabled by default.
 *
 * @param enabled true to echo to console, false for file only
 */
void agentite_log_set_console_output(bool enabled);

/**
 * Log an error message.
 * Errors are always logged regardless of level, and auto-flush.
 *
 * @param subsystem Subsystem identifier (e.g., AGENTITE_LOG_GRAPHICS)
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void agentite_log_error(const char *subsystem, const char *fmt, ...);

/**
 * Log a warning message.
 *
 * @param subsystem Subsystem identifier
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void agentite_log_warning(const char *subsystem, const char *fmt, ...);

/**
 * Log an info message.
 *
 * @param subsystem Subsystem identifier
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void agentite_log_info(const char *subsystem, const char *fmt, ...);

/**
 * Log a debug message.
 *
 * @param subsystem Subsystem identifier
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void agentite_log_debug(const char *subsystem, const char *fmt, ...);

/**
 * Log with explicit level (variadic version).
 *
 * @param level Log level
 * @param subsystem Subsystem identifier
 * @param fmt Printf-style format string
 * @param args va_list of format arguments
 */
void agentite_log_v(Agentite_LogLevel level, const char *subsystem, const char *fmt, va_list args);

/**
 * Flush the log file to disk.
 * Called automatically on errors, but can be called manually for safety.
 */
void agentite_log_flush(void);

/**
 * Get the path to the current log file.
 *
 * @return Path to log file, or NULL if not initialized
 */
const char *agentite_log_get_path(void);

#endif /* AGENTITE_LOG_H */
