#include "carbon/error.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

/* Thread-local error buffer */
#define CARBON_ERROR_BUFFER_SIZE 1024

#if defined(_MSC_VER)
    #define CARBON_THREAD_LOCAL __declspec(thread)
#else
    #define CARBON_THREAD_LOCAL _Thread_local
#endif

static CARBON_THREAD_LOCAL char error_buffer[CARBON_ERROR_BUFFER_SIZE] = {0};

void carbon_set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    carbon_set_error_v(fmt, args);
    va_end(args);
}

void carbon_set_error_v(const char *fmt, va_list args) {
    if (!fmt) {
        error_buffer[0] = '\0';
        return;
    }
    vsnprintf(error_buffer, CARBON_ERROR_BUFFER_SIZE, fmt, args);
}

const char *carbon_get_last_error(void) {
    return error_buffer;
}

void carbon_clear_error(void) {
    error_buffer[0] = '\0';
}

bool carbon_has_error(void) {
    return error_buffer[0] != '\0';
}

void carbon_set_error_from_sdl(const char *prefix) {
    const char *sdl_error = SDL_GetError();
    if (!sdl_error || sdl_error[0] == '\0') {
        sdl_error = "Unknown SDL error";
    }

    if (prefix && prefix[0] != '\0') {
        snprintf(error_buffer, CARBON_ERROR_BUFFER_SIZE, "%s: %s", prefix, sdl_error);
    } else {
        snprintf(error_buffer, CARBON_ERROR_BUFFER_SIZE, "%s", sdl_error);
    }
}

void carbon_log_and_clear_error(void) {
    if (error_buffer[0] != '\0') {
        SDL_Log("Error: %s", error_buffer);
        error_buffer[0] = '\0';
    }
}
