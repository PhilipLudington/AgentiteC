#include "agentite/log.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Log file handle */
static FILE *log_file = NULL;
static char log_path[512] = {0};
static Agentite_LogLevel current_level = AGENTITE_LOG_LEVEL_INFO;
static bool console_output = true;
static bool initialized = false;

/* Callback registration */
#define MAX_LOG_CALLBACKS 8

struct LogCallbackEntry {
    Agentite_LogCallback callback;
    void *userdata;
    uint32_t handle;
    bool active;
};

static LogCallbackEntry s_callbacks[MAX_LOG_CALLBACKS] = {0};
static uint32_t s_next_callback_handle = 1;

/* Level names for output (padded to 7 chars for alignment) */
static const char *level_names[] = {
    "ERROR  ",
    "WARNING",
    "INFO   ",
    "DEBUG  "
};

/* Default log path */
#if defined(_WIN32)
    #define DEFAULT_LOG_PATH "carbon.log"
#else
    #define DEFAULT_LOG_PATH "/tmp/carbon.log"
#endif

/* Write timestamp to buffer */
static void write_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* Write session start marker */
static void write_session_start(void) {
    if (!log_file) return;

    char timestamp[32];
    write_timestamp(timestamp, sizeof(timestamp));

    fprintf(log_file, "\n");
    fprintf(log_file, "================================================================================\n");
    fprintf(log_file, "=== Agentite Engine - Session Start: %s\n", timestamp);
    fprintf(log_file, "================================================================================\n");
    fflush(log_file);
}

/* Write session end marker */
static void write_session_end(void) {
    if (!log_file) return;

    char timestamp[32];
    write_timestamp(timestamp, sizeof(timestamp));

    fprintf(log_file, "================================================================================\n");
    fprintf(log_file, "=== Session End: %s\n", timestamp);
    fprintf(log_file, "================================================================================\n\n");
    fflush(log_file);
}

bool agentite_log_init(void) {
    return agentite_log_init_with_path(NULL);
}

bool agentite_log_init_with_path(const char *path) {
    if (initialized) {
        return true;  /* Already initialized */
    }

    /* Use default path if none provided */
    const char *use_path = path ? path : DEFAULT_LOG_PATH;

    /* Store path */
    snprintf(log_path, sizeof(log_path), "%s", use_path);

    /* Open log file in append mode */
    log_file = fopen(log_path, "a");
    if (!log_file) {
        SDL_Log("Failed to open log file: %s", log_path);
        return false;
    }

    initialized = true;
    write_session_start();

    return true;
}

void agentite_log_shutdown(void) {
    if (!initialized) return;

    write_session_end();

    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }

    log_path[0] = '\0';
    initialized = false;
}

bool agentite_log_is_initialized(void) {
    return initialized;
}

void agentite_log_set_level(Agentite_LogLevel level) {
    current_level = level;
}

Agentite_LogLevel agentite_log_get_level(void) {
    return current_level;
}

void agentite_log_set_console_output(bool enabled) {
    console_output = enabled;
}

void agentite_log_v(Agentite_LogLevel level, const char *subsystem, const char *fmt, va_list args) {
    /* Check level filter (errors always pass) */
    if (level != AGENTITE_LOG_LEVEL_ERROR && level > current_level) {
        return;
    }

    /* Format the message */
    char message[1024];
    vsnprintf(message, sizeof(message), fmt, args);

    /* Get timestamp */
    char timestamp[32];
    write_timestamp(timestamp, sizeof(timestamp));

    /* Ensure subsystem name is padded/truncated to 10 chars for alignment */
    char subsystem_padded[11];
    snprintf(subsystem_padded, sizeof(subsystem_padded), "%-10s", subsystem ? subsystem : "Unknown");

    /* Format full log line */
    char log_line[1200];
    snprintf(log_line, sizeof(log_line), "[%s] [%s] [%s] %s",
             timestamp,
             level_names[level],
             subsystem_padded,
             message);

    /* Write to file if initialized */
    if (log_file) {
        fprintf(log_file, "%s\n", log_line);

        /* Auto-flush on errors for crash debugging */
        if (level == AGENTITE_LOG_LEVEL_ERROR) {
            fflush(log_file);
        }
    }

    /* Echo to console if enabled */
    if (console_output) {
        switch (level) {
            case AGENTITE_LOG_LEVEL_ERROR:
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
                break;
            case AGENTITE_LOG_LEVEL_WARNING:
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[%s] %s", subsystem_padded, message);
                break;
            case AGENTITE_LOG_LEVEL_INFO:
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] %s", subsystem_padded, message);
                break;
            case AGENTITE_LOG_LEVEL_DEBUG:
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[%s] %s", subsystem_padded, message);
                break;
        }
    }

    /* Notify registered callbacks */
    for (int i = 0; i < MAX_LOG_CALLBACKS; i++) {
        if (s_callbacks[i].active && s_callbacks[i].callback) {
            s_callbacks[i].callback(level, subsystem_padded, message, s_callbacks[i].userdata);
        }
    }
}

void agentite_log_error(const char *subsystem, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    agentite_log_v(AGENTITE_LOG_LEVEL_ERROR, subsystem, fmt, args);
    va_end(args);
}

void agentite_log_warning(const char *subsystem, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    agentite_log_v(AGENTITE_LOG_LEVEL_WARNING, subsystem, fmt, args);
    va_end(args);
}

void agentite_log_info(const char *subsystem, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    agentite_log_v(AGENTITE_LOG_LEVEL_INFO, subsystem, fmt, args);
    va_end(args);
}

void agentite_log_debug(const char *subsystem, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    agentite_log_v(AGENTITE_LOG_LEVEL_DEBUG, subsystem, fmt, args);
    va_end(args);
}

void agentite_log_flush(void) {
    if (log_file) {
        fflush(log_file);
    }
}

const char *agentite_log_get_path(void) {
    return initialized ? log_path : NULL;
}

uint32_t agentite_log_add_callback(Agentite_LogCallback callback, void *userdata) {
    if (!callback) return 0;

    for (int i = 0; i < MAX_LOG_CALLBACKS; i++) {
        if (!s_callbacks[i].active) {
            s_callbacks[i].callback = callback;
            s_callbacks[i].userdata = userdata;
            s_callbacks[i].handle = s_next_callback_handle++;
            s_callbacks[i].active = true;
            return s_callbacks[i].handle;
        }
    }

    /* No slots available */
    return 0;
}

void agentite_log_remove_callback(uint32_t handle) {
    if (handle == 0) return;

    for (int i = 0; i < MAX_LOG_CALLBACKS; i++) {
        if (s_callbacks[i].active && s_callbacks[i].handle == handle) {
            s_callbacks[i].callback = NULL;
            s_callbacks[i].userdata = NULL;
            s_callbacks[i].handle = 0;
            s_callbacks[i].active = false;
            return;
        }
    }
}
