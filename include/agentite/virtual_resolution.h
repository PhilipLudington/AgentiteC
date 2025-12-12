/**
 * Agentite Virtual Resolution System
 *
 * Provides a fixed coordinate space (default 1920x1080) that automatically
 * scales to fit any window size with letterboxing for aspect ratio preservation.
 * Includes HiDPI/Retina display support.
 *
 * Ported from AgentiteZ (Zig) virtual resolution system.
 *
 * Usage:
 *   // Create virtual resolution handler
 *   Agentite_VirtualResolution *vr = agentite_vres_create(1920, 1080);
 *
 *   // Update when window resizes
 *   agentite_vres_update(vr, window_width, window_height, dpi_scale);
 *
 *   // Convert game coordinates to screen coordinates
 *   float screen_x, screen_y;
 *   agentite_vres_to_screen(vr, game_x, game_y, &screen_x, &screen_y);
 *
 *   // Convert mouse input to game coordinates
 *   float game_x, game_y;
 *   agentite_vres_to_virtual(vr, mouse_x, mouse_y, &game_x, &game_y);
 *
 *   // Get viewport for rendering
 *   Agentite_Viewport viewport = agentite_vres_get_viewport(vr);
 *   SDL_SetRenderViewport(renderer, &viewport.rect);
 *
 *   agentite_vres_destroy(vr);
 */

#ifndef AGENTITE_VIRTUAL_RESOLUTION_H
#define AGENTITE_VIRTUAL_RESOLUTION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/* Default virtual resolution (1080p) */
#define AGENTITE_VRES_DEFAULT_WIDTH   1920
#define AGENTITE_VRES_DEFAULT_HEIGHT  1080

/*============================================================================
 * Scaling Modes
 *============================================================================*/

typedef enum Agentite_ScaleMode {
    AGENTITE_SCALE_LETTERBOX,     /* Preserve aspect ratio, add bars */
    AGENTITE_SCALE_STRETCH,       /* Stretch to fill (distorts) */
    AGENTITE_SCALE_PIXEL_PERFECT, /* Integer scaling only */
    AGENTITE_SCALE_OVERSCAN,      /* Fill screen, crop edges */
} Agentite_ScaleMode;

/*============================================================================
 * Structures
 *============================================================================*/

/**
 * Rectangle structure for viewport.
 */
typedef struct Agentite_Rect {
    int x, y;
    int w, h;
} Agentite_Rect;

/**
 * Viewport information.
 */
typedef struct Agentite_Viewport {
    Agentite_Rect rect;           /* Viewport rectangle in screen space */
    float scale_x;                /* Horizontal scale factor */
    float scale_y;                /* Vertical scale factor */
    float scale;                  /* Uniform scale (min of x and y) */
    int letterbox_x;              /* Horizontal letterbox size */
    int letterbox_y;              /* Vertical letterbox size */
} Agentite_Viewport;

/**
 * Forward declaration.
 */
typedef struct Agentite_VirtualResolution Agentite_VirtualResolution;

/*============================================================================
 * Creation and Destruction
 *============================================================================*/

/**
 * Create a virtual resolution handler.
 *
 * @param virtual_width  Virtual coordinate space width
 * @param virtual_height Virtual coordinate space height
 * @return New virtual resolution handler or NULL on failure
 */
Agentite_VirtualResolution *agentite_vres_create(int virtual_width, int virtual_height);

/**
 * Create with default 1920x1080 resolution.
 *
 * @return New virtual resolution handler or NULL on failure
 */
Agentite_VirtualResolution *agentite_vres_create_default(void);

/**
 * Destroy virtual resolution handler.
 *
 * @param vr Virtual resolution handler to destroy
 */
void agentite_vres_destroy(Agentite_VirtualResolution *vr);

/*============================================================================
 * Update and Configuration
 *============================================================================*/

/**
 * Update with current window dimensions.
 * Call this when the window resizes or DPI changes.
 *
 * @param vr            Virtual resolution handler
 * @param window_width  Current window width in pixels
 * @param window_height Current window height in pixels
 * @param dpi_scale     DPI scale factor (1.0 for standard, 2.0 for Retina)
 */
void agentite_vres_update(
    Agentite_VirtualResolution *vr,
    int window_width,
    int window_height,
    float dpi_scale
);

/**
 * Set scaling mode.
 *
 * @param vr   Virtual resolution handler
 * @param mode Scaling mode
 */
void agentite_vres_set_scale_mode(Agentite_VirtualResolution *vr, Agentite_ScaleMode mode);

/**
 * Get current scaling mode.
 *
 * @param vr Virtual resolution handler
 * @return Current scaling mode
 */
Agentite_ScaleMode agentite_vres_get_scale_mode(const Agentite_VirtualResolution *vr);

/**
 * Set virtual resolution (changes coordinate space).
 *
 * @param vr     Virtual resolution handler
 * @param width  New virtual width
 * @param height New virtual height
 */
void agentite_vres_set_virtual_size(
    Agentite_VirtualResolution *vr,
    int width,
    int height
);

/*============================================================================
 * Coordinate Conversion
 *============================================================================*/

/**
 * Convert virtual coordinates to screen coordinates.
 *
 * @param vr        Virtual resolution handler
 * @param virtual_x X coordinate in virtual space
 * @param virtual_y Y coordinate in virtual space
 * @param out_x     Output screen X coordinate
 * @param out_y     Output screen Y coordinate
 */
void agentite_vres_to_screen(
    const Agentite_VirtualResolution *vr,
    float virtual_x,
    float virtual_y,
    float *out_x,
    float *out_y
);

/**
 * Convert screen coordinates to virtual coordinates.
 * Use this for mouse input.
 *
 * @param vr       Virtual resolution handler
 * @param screen_x X coordinate in screen space
 * @param screen_y Y coordinate in screen space
 * @param out_x    Output virtual X coordinate
 * @param out_y    Output virtual Y coordinate
 */
void agentite_vres_to_virtual(
    const Agentite_VirtualResolution *vr,
    float screen_x,
    float screen_y,
    float *out_x,
    float *out_y
);

/**
 * Convert a size in virtual space to screen space.
 *
 * @param vr           Virtual resolution handler
 * @param virtual_size Size in virtual units
 * @return Size in screen pixels
 */
float agentite_vres_scale_size(
    const Agentite_VirtualResolution *vr,
    float virtual_size
);

/**
 * Convert a size in screen space to virtual space.
 *
 * @param vr          Virtual resolution handler
 * @param screen_size Size in screen pixels
 * @return Size in virtual units
 */
float agentite_vres_unscale_size(
    const Agentite_VirtualResolution *vr,
    float screen_size
);

/*============================================================================
 * Viewport Information
 *============================================================================*/

/**
 * Get the current viewport.
 *
 * @param vr Virtual resolution handler
 * @return Viewport information
 */
Agentite_Viewport agentite_vres_get_viewport(const Agentite_VirtualResolution *vr);

/**
 * Get virtual width.
 *
 * @param vr Virtual resolution handler
 * @return Virtual width
 */
int agentite_vres_get_virtual_width(const Agentite_VirtualResolution *vr);

/**
 * Get virtual height.
 *
 * @param vr Virtual resolution handler
 * @return Virtual height
 */
int agentite_vres_get_virtual_height(const Agentite_VirtualResolution *vr);

/**
 * Get current window width.
 *
 * @param vr Virtual resolution handler
 * @return Window width in pixels
 */
int agentite_vres_get_window_width(const Agentite_VirtualResolution *vr);

/**
 * Get current window height.
 *
 * @param vr Virtual resolution handler
 * @return Window height in pixels
 */
int agentite_vres_get_window_height(const Agentite_VirtualResolution *vr);

/**
 * Get current DPI scale.
 *
 * @param vr Virtual resolution handler
 * @return DPI scale factor
 */
float agentite_vres_get_dpi_scale(const Agentite_VirtualResolution *vr);

/**
 * Get current scale factor (uniform).
 *
 * @param vr Virtual resolution handler
 * @return Scale factor
 */
float agentite_vres_get_scale(const Agentite_VirtualResolution *vr);

/*============================================================================
 * Bounds Checking
 *============================================================================*/

/**
 * Check if screen coordinates are within the viewport.
 *
 * @param vr       Virtual resolution handler
 * @param screen_x Screen X coordinate
 * @param screen_y Screen Y coordinate
 * @return true if within viewport
 */
bool agentite_vres_is_in_viewport(
    const Agentite_VirtualResolution *vr,
    float screen_x,
    float screen_y
);

/**
 * Check if virtual coordinates are within bounds.
 *
 * @param vr        Virtual resolution handler
 * @param virtual_x Virtual X coordinate
 * @param virtual_y Virtual Y coordinate
 * @return true if within virtual bounds
 */
bool agentite_vres_is_in_bounds(
    const Agentite_VirtualResolution *vr,
    float virtual_x,
    float virtual_y
);

/**
 * Clamp virtual coordinates to bounds.
 *
 * @param vr        Virtual resolution handler
 * @param virtual_x Virtual X coordinate (modified)
 * @param virtual_y Virtual Y coordinate (modified)
 */
void agentite_vres_clamp_to_bounds(
    const Agentite_VirtualResolution *vr,
    float *virtual_x,
    float *virtual_y
);

/*============================================================================
 * Rectangle Conversion
 *============================================================================*/

/**
 * Convert a rectangle from virtual to screen space.
 *
 * @param vr          Virtual resolution handler
 * @param virtual_rect Rectangle in virtual space
 * @return Rectangle in screen space
 */
Agentite_Rect agentite_vres_rect_to_screen(
    const Agentite_VirtualResolution *vr,
    Agentite_Rect virtual_rect
);

/**
 * Convert a rectangle from screen to virtual space.
 *
 * @param vr          Virtual resolution handler
 * @param screen_rect Rectangle in screen space
 * @return Rectangle in virtual space
 */
Agentite_Rect agentite_vres_rect_to_virtual(
    const Agentite_VirtualResolution *vr,
    Agentite_Rect screen_rect
);

/*============================================================================
 * Utility
 *============================================================================*/

/**
 * Get scale mode name.
 *
 * @param mode Scale mode
 * @return Static string name
 */
const char *agentite_scale_mode_name(Agentite_ScaleMode mode);

/**
 * Calculate aspect ratio.
 *
 * @param width  Width
 * @param height Height
 * @return Aspect ratio (width / height)
 */
float agentite_vres_aspect_ratio(int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_VIRTUAL_RESOLUTION_H */
