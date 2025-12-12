/*
 * Agentite Virtual Resolution System
 *
 * Provides a fixed coordinate space that automatically scales to fit
 * any window size with letterboxing for aspect ratio preservation.
 *
 * Ported from AgentiteZ (Zig) virtual resolution system.
 */

#include "agentite/agentite.h"
#include "agentite/virtual_resolution.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

struct Agentite_VirtualResolution {
    /* Virtual (game) resolution */
    int virtual_width;
    int virtual_height;

    /* Actual window resolution */
    int window_width;
    int window_height;

    /* DPI scaling */
    float dpi_scale;

    /* Calculated viewport */
    Agentite_Viewport viewport;

    /* Configuration */
    Agentite_ScaleMode scale_mode;

    /* Cached values */
    float virtual_aspect;
    float window_aspect;
};

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static void recalculate_viewport(Agentite_VirtualResolution *vr) {
    if (!vr) return;

    /* Apply DPI scale to window dimensions */
    int effective_width = (int)(vr->window_width * vr->dpi_scale);
    int effective_height = (int)(vr->window_height * vr->dpi_scale);

    vr->virtual_aspect = (float)vr->virtual_width / (float)vr->virtual_height;
    vr->window_aspect = (float)effective_width / (float)effective_height;

    switch (vr->scale_mode) {
        case AGENTITE_SCALE_LETTERBOX: {
            /* Fit virtual space within window, preserving aspect ratio */
            if (vr->window_aspect > vr->virtual_aspect) {
                /* Window is wider - letterbox on sides */
                vr->viewport.scale = (float)effective_height / (float)vr->virtual_height;
                int scaled_width = (int)(vr->virtual_width * vr->viewport.scale);
                vr->viewport.letterbox_x = (effective_width - scaled_width) / 2;
                vr->viewport.letterbox_y = 0;
                vr->viewport.rect.x = vr->viewport.letterbox_x;
                vr->viewport.rect.y = 0;
                vr->viewport.rect.w = scaled_width;
                vr->viewport.rect.h = effective_height;
            } else {
                /* Window is taller - letterbox on top/bottom */
                vr->viewport.scale = (float)effective_width / (float)vr->virtual_width;
                int scaled_height = (int)(vr->virtual_height * vr->viewport.scale);
                vr->viewport.letterbox_x = 0;
                vr->viewport.letterbox_y = (effective_height - scaled_height) / 2;
                vr->viewport.rect.x = 0;
                vr->viewport.rect.y = vr->viewport.letterbox_y;
                vr->viewport.rect.w = effective_width;
                vr->viewport.rect.h = scaled_height;
            }
            vr->viewport.scale_x = vr->viewport.scale;
            vr->viewport.scale_y = vr->viewport.scale;
            break;
        }

        case AGENTITE_SCALE_STRETCH: {
            /* Fill entire window, ignoring aspect ratio */
            vr->viewport.scale_x = (float)effective_width / (float)vr->virtual_width;
            vr->viewport.scale_y = (float)effective_height / (float)vr->virtual_height;
            vr->viewport.scale = vr->viewport.scale_x < vr->viewport.scale_y ?
                                 vr->viewport.scale_x : vr->viewport.scale_y;
            vr->viewport.letterbox_x = 0;
            vr->viewport.letterbox_y = 0;
            vr->viewport.rect.x = 0;
            vr->viewport.rect.y = 0;
            vr->viewport.rect.w = effective_width;
            vr->viewport.rect.h = effective_height;
            break;
        }

        case AGENTITE_SCALE_PIXEL_PERFECT: {
            /* Integer scaling only */
            float scale_x = (float)effective_width / (float)vr->virtual_width;
            float scale_y = (float)effective_height / (float)vr->virtual_height;
            float min_scale = scale_x < scale_y ? scale_x : scale_y;

            /* Round down to nearest integer */
            int int_scale = (int)min_scale;
            if (int_scale < 1) int_scale = 1;

            vr->viewport.scale = (float)int_scale;
            vr->viewport.scale_x = vr->viewport.scale;
            vr->viewport.scale_y = vr->viewport.scale;

            int scaled_width = vr->virtual_width * int_scale;
            int scaled_height = vr->virtual_height * int_scale;

            vr->viewport.letterbox_x = (effective_width - scaled_width) / 2;
            vr->viewport.letterbox_y = (effective_height - scaled_height) / 2;
            vr->viewport.rect.x = vr->viewport.letterbox_x;
            vr->viewport.rect.y = vr->viewport.letterbox_y;
            vr->viewport.rect.w = scaled_width;
            vr->viewport.rect.h = scaled_height;
            break;
        }

        case AGENTITE_SCALE_OVERSCAN: {
            /* Fill window completely, cropping edges if needed */
            if (vr->window_aspect > vr->virtual_aspect) {
                /* Window is wider - crop top/bottom */
                vr->viewport.scale = (float)effective_width / (float)vr->virtual_width;
            } else {
                /* Window is taller - crop sides */
                vr->viewport.scale = (float)effective_height / (float)vr->virtual_height;
            }
            vr->viewport.scale_x = vr->viewport.scale;
            vr->viewport.scale_y = vr->viewport.scale;

            int scaled_width = (int)(vr->virtual_width * vr->viewport.scale);
            int scaled_height = (int)(vr->virtual_height * vr->viewport.scale);

            /* Center the oversized content */
            vr->viewport.letterbox_x = (effective_width - scaled_width) / 2;
            vr->viewport.letterbox_y = (effective_height - scaled_height) / 2;
            vr->viewport.rect.x = vr->viewport.letterbox_x;
            vr->viewport.rect.y = vr->viewport.letterbox_y;
            vr->viewport.rect.w = scaled_width;
            vr->viewport.rect.h = scaled_height;
            break;
        }
    }
}

/*============================================================================
 * Creation and Destruction
 *============================================================================*/

Agentite_VirtualResolution *agentite_vres_create(int virtual_width, int virtual_height) {
    Agentite_VirtualResolution *vr = AGENTITE_ALLOC(Agentite_VirtualResolution);
    if (!vr) {
        agentite_set_error("Failed to allocate virtual resolution handler");
        return NULL;
    }

    memset(vr, 0, sizeof(Agentite_VirtualResolution));

    vr->virtual_width = virtual_width > 0 ? virtual_width : AGENTITE_VRES_DEFAULT_WIDTH;
    vr->virtual_height = virtual_height > 0 ? virtual_height : AGENTITE_VRES_DEFAULT_HEIGHT;
    vr->window_width = vr->virtual_width;
    vr->window_height = vr->virtual_height;
    vr->dpi_scale = 1.0f;
    vr->scale_mode = AGENTITE_SCALE_LETTERBOX;

    recalculate_viewport(vr);

    return vr;
}

Agentite_VirtualResolution *agentite_vres_create_default(void) {
    return agentite_vres_create(AGENTITE_VRES_DEFAULT_WIDTH, AGENTITE_VRES_DEFAULT_HEIGHT);
}

void agentite_vres_destroy(Agentite_VirtualResolution *vr) {
    if (vr) {
        free(vr);
    }
}

/*============================================================================
 * Update and Configuration
 *============================================================================*/

void agentite_vres_update(
    Agentite_VirtualResolution *vr,
    int window_width,
    int window_height,
    float dpi_scale)
{
    AGENTITE_VALIDATE_PTR(vr);

    vr->window_width = window_width > 0 ? window_width : 1;
    vr->window_height = window_height > 0 ? window_height : 1;
    vr->dpi_scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;

    recalculate_viewport(vr);
}

void agentite_vres_set_scale_mode(Agentite_VirtualResolution *vr, Agentite_ScaleMode mode) {
    AGENTITE_VALIDATE_PTR(vr);
    vr->scale_mode = mode;
    recalculate_viewport(vr);
}

Agentite_ScaleMode agentite_vres_get_scale_mode(const Agentite_VirtualResolution *vr) {
    AGENTITE_VALIDATE_PTR_RET(vr, AGENTITE_SCALE_LETTERBOX);
    return vr->scale_mode;
}

void agentite_vres_set_virtual_size(Agentite_VirtualResolution *vr, int width, int height) {
    AGENTITE_VALIDATE_PTR(vr);

    vr->virtual_width = width > 0 ? width : AGENTITE_VRES_DEFAULT_WIDTH;
    vr->virtual_height = height > 0 ? height : AGENTITE_VRES_DEFAULT_HEIGHT;

    recalculate_viewport(vr);
}

/*============================================================================
 * Coordinate Conversion
 *============================================================================*/

void agentite_vres_to_screen(
    const Agentite_VirtualResolution *vr,
    float virtual_x,
    float virtual_y,
    float *out_x,
    float *out_y)
{
    AGENTITE_VALIDATE_PTR(vr);

    if (out_x) {
        *out_x = virtual_x * vr->viewport.scale_x + vr->viewport.letterbox_x;
    }
    if (out_y) {
        *out_y = virtual_y * vr->viewport.scale_y + vr->viewport.letterbox_y;
    }
}

void agentite_vres_to_virtual(
    const Agentite_VirtualResolution *vr,
    float screen_x,
    float screen_y,
    float *out_x,
    float *out_y)
{
    AGENTITE_VALIDATE_PTR(vr);

    if (out_x) {
        float adjusted_x = screen_x - vr->viewport.letterbox_x;
        *out_x = vr->viewport.scale_x > 0 ? adjusted_x / vr->viewport.scale_x : 0;
    }
    if (out_y) {
        float adjusted_y = screen_y - vr->viewport.letterbox_y;
        *out_y = vr->viewport.scale_y > 0 ? adjusted_y / vr->viewport.scale_y : 0;
    }
}

float agentite_vres_scale_size(const Agentite_VirtualResolution *vr, float virtual_size) {
    AGENTITE_VALIDATE_PTR_RET(vr, virtual_size);
    return virtual_size * vr->viewport.scale;
}

float agentite_vres_unscale_size(const Agentite_VirtualResolution *vr, float screen_size) {
    AGENTITE_VALIDATE_PTR_RET(vr, screen_size);
    return vr->viewport.scale > 0 ? screen_size / vr->viewport.scale : screen_size;
}

/*============================================================================
 * Viewport Information
 *============================================================================*/

Agentite_Viewport agentite_vres_get_viewport(const Agentite_VirtualResolution *vr) {
    Agentite_Viewport empty = {0};
    AGENTITE_VALIDATE_PTR_RET(vr, empty);
    return vr->viewport;
}

int agentite_vres_get_virtual_width(const Agentite_VirtualResolution *vr) {
    AGENTITE_VALIDATE_PTR_RET(vr, AGENTITE_VRES_DEFAULT_WIDTH);
    return vr->virtual_width;
}

int agentite_vres_get_virtual_height(const Agentite_VirtualResolution *vr) {
    AGENTITE_VALIDATE_PTR_RET(vr, AGENTITE_VRES_DEFAULT_HEIGHT);
    return vr->virtual_height;
}

int agentite_vres_get_window_width(const Agentite_VirtualResolution *vr) {
    AGENTITE_VALIDATE_PTR_RET(vr, 0);
    return vr->window_width;
}

int agentite_vres_get_window_height(const Agentite_VirtualResolution *vr) {
    AGENTITE_VALIDATE_PTR_RET(vr, 0);
    return vr->window_height;
}

float agentite_vres_get_dpi_scale(const Agentite_VirtualResolution *vr) {
    AGENTITE_VALIDATE_PTR_RET(vr, 1.0f);
    return vr->dpi_scale;
}

float agentite_vres_get_scale(const Agentite_VirtualResolution *vr) {
    AGENTITE_VALIDATE_PTR_RET(vr, 1.0f);
    return vr->viewport.scale;
}

/*============================================================================
 * Bounds Checking
 *============================================================================*/

bool agentite_vres_is_in_viewport(
    const Agentite_VirtualResolution *vr,
    float screen_x,
    float screen_y)
{
    AGENTITE_VALIDATE_PTR_RET(vr, false);

    const Agentite_Viewport *vp = &vr->viewport;
    return screen_x >= vp->rect.x &&
           screen_x < vp->rect.x + vp->rect.w &&
           screen_y >= vp->rect.y &&
           screen_y < vp->rect.y + vp->rect.h;
}

bool agentite_vres_is_in_bounds(
    const Agentite_VirtualResolution *vr,
    float virtual_x,
    float virtual_y)
{
    AGENTITE_VALIDATE_PTR_RET(vr, false);

    return virtual_x >= 0 &&
           virtual_x < vr->virtual_width &&
           virtual_y >= 0 &&
           virtual_y < vr->virtual_height;
}

void agentite_vres_clamp_to_bounds(
    const Agentite_VirtualResolution *vr,
    float *virtual_x,
    float *virtual_y)
{
    AGENTITE_VALIDATE_PTR(vr);

    if (virtual_x) {
        if (*virtual_x < 0) *virtual_x = 0;
        if (*virtual_x >= vr->virtual_width) *virtual_x = (float)(vr->virtual_width - 1);
    }

    if (virtual_y) {
        if (*virtual_y < 0) *virtual_y = 0;
        if (*virtual_y >= vr->virtual_height) *virtual_y = (float)(vr->virtual_height - 1);
    }
}

/*============================================================================
 * Rectangle Conversion
 *============================================================================*/

Agentite_Rect agentite_vres_rect_to_screen(
    const Agentite_VirtualResolution *vr,
    Agentite_Rect virtual_rect)
{
    Agentite_Rect result = {0};
    AGENTITE_VALIDATE_PTR_RET(vr, result);

    float x, y;
    agentite_vres_to_screen(vr, (float)virtual_rect.x, (float)virtual_rect.y, &x, &y);

    result.x = (int)x;
    result.y = (int)y;
    result.w = (int)(virtual_rect.w * vr->viewport.scale_x);
    result.h = (int)(virtual_rect.h * vr->viewport.scale_y);

    return result;
}

Agentite_Rect agentite_vres_rect_to_virtual(
    const Agentite_VirtualResolution *vr,
    Agentite_Rect screen_rect)
{
    Agentite_Rect result = {0};
    AGENTITE_VALIDATE_PTR_RET(vr, result);

    float x, y;
    agentite_vres_to_virtual(vr, (float)screen_rect.x, (float)screen_rect.y, &x, &y);

    result.x = (int)x;
    result.y = (int)y;
    result.w = vr->viewport.scale_x > 0 ? (int)(screen_rect.w / vr->viewport.scale_x) : 0;
    result.h = vr->viewport.scale_y > 0 ? (int)(screen_rect.h / vr->viewport.scale_y) : 0;

    return result;
}

/*============================================================================
 * Utility
 *============================================================================*/

const char *agentite_scale_mode_name(Agentite_ScaleMode mode) {
    switch (mode) {
        case AGENTITE_SCALE_LETTERBOX:     return "Letterbox";
        case AGENTITE_SCALE_STRETCH:       return "Stretch";
        case AGENTITE_SCALE_PIXEL_PERFECT: return "Pixel Perfect";
        case AGENTITE_SCALE_OVERSCAN:      return "Overscan";
        default:                           return "Unknown";
    }
}

float agentite_vres_aspect_ratio(int width, int height) {
    if (height <= 0) return 0.0f;
    return (float)width / (float)height;
}
