/*
 * Carbon MSDF Generator - Edge Coloring and Generation
 *
 * This file contains:
 * - Edge coloring algorithms
 * - SDF/MSDF/MTSDF generation
 * - Error correction
 */

#include "carbon/msdf.h"
#include "carbon/error.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ============================================================================
 * Edge Coloring
 * ============================================================================ */

/* Simple PRNG for deterministic coloring */
static uint64_t msdf_rand_state = 0;

static void msdf_srand(uint64_t seed)
{
    msdf_rand_state = seed ? seed : 12345678901234567ULL;
}

static uint64_t msdf_rand(void)
{
    /* xorshift64 */
    msdf_rand_state ^= msdf_rand_state << 13;
    msdf_rand_state ^= msdf_rand_state >> 7;
    msdf_rand_state ^= msdf_rand_state << 17;
    return msdf_rand_state;
}

/* Get end point of edge segment */
static MSDF_Vector2 edge_end_point(const MSDF_EdgeSegment *edge)
{
    switch (edge->type) {
        case MSDF_EDGE_LINEAR:    return edge->p[1];
        case MSDF_EDGE_QUADRATIC: return edge->p[2];
        case MSDF_EDGE_CUBIC:     return edge->p[3];
    }
    return edge->p[0];
}

/* Check if vertex between two edges is a corner */
static bool is_corner(const MSDF_EdgeSegment *prev, const MSDF_EdgeSegment *next,
                      double angle_threshold)
{
    /* Get outgoing direction from prev edge (at t=1) */
    MSDF_Vector2 dir_out = msdf_edge_direction_at(prev, 1.0);

    /* Get incoming direction to next edge (at t=0) */
    MSDF_Vector2 dir_in = msdf_edge_direction_at(next, 0.0);

    /* Normalize directions */
    dir_out = msdf_vec2_normalize(dir_out);
    dir_in = msdf_vec2_normalize(dir_in);

    /* Calculate angle between directions */
    double dot = msdf_vec2_dot(dir_out, dir_in);

    /* Clamp dot product to valid range for acos */
    if (dot < -1.0) dot = -1.0;
    if (dot > 1.0) dot = 1.0;

    double angle = acos(dot);

    /* Check cross product for turn direction */
    double cross = msdf_vec2_cross(dir_out, dir_in);

    /* It's a corner if angle exceeds threshold (for convex corners)
     * or if it's a sharp concave turn */
    return angle > M_PI - angle_threshold || fabs(cross) > sin(angle_threshold);
}

/* Switch to next color in cycle */
static MSDF_EdgeColor next_color(MSDF_EdgeColor current)
{
    switch (current) {
        case MSDF_COLOR_CYAN:    return MSDF_COLOR_MAGENTA;
        case MSDF_COLOR_MAGENTA: return MSDF_COLOR_YELLOW;
        case MSDF_COLOR_YELLOW:  return MSDF_COLOR_CYAN;
        default:                 return MSDF_COLOR_CYAN;
    }
}

void msdf_edge_coloring_simple(MSDF_Shape *shape, double angle_threshold, uint64_t seed)
{
    if (!shape) return;

    msdf_srand(seed);

    /* Process each contour independently */
    for (int c = 0; c < shape->contour_count; c++) {
        MSDF_Contour *contour = &shape->contours[c];
        if (contour->edge_count == 0) continue;

        /* Find corners in this contour */
        int *corners = (int *)malloc(contour->edge_count * sizeof(int));
        int corner_count = 0;

        for (int i = 0; i < contour->edge_count; i++) {
            int prev_idx = (i + contour->edge_count - 1) % contour->edge_count;
            MSDF_EdgeSegment *prev = &contour->edges[prev_idx];
            MSDF_EdgeSegment *curr = &contour->edges[i];

            if (is_corner(prev, curr, angle_threshold)) {
                corners[corner_count++] = i;
            }
        }

        if (corner_count == 0) {
            /* No corners - smooth contour, use all edges with same color (WHITE) */
            /* But split into 3 groups with different colors for MSDF to work */
            if (contour->edge_count >= 3) {
                int third = contour->edge_count / 3;

                MSDF_EdgeColor colors[] = {MSDF_COLOR_CYAN, MSDF_COLOR_MAGENTA, MSDF_COLOR_YELLOW};
                int color_idx = 0;

                for (int i = 0; i < contour->edge_count; i++) {
                    if (i == third || i == 2 * third) {
                        color_idx++;
                    }
                    contour->edges[i].color = colors[color_idx % 3];
                }
            } else {
                /* Very few edges, just make them all white */
                for (int i = 0; i < contour->edge_count; i++) {
                    contour->edges[i].color = MSDF_COLOR_WHITE;
                }
            }
        } else if (corner_count == 1) {
            /* Single corner - special handling */
            /* Split at corner, use different colors on each side */
            MSDF_EdgeColor colors[] = {MSDF_COLOR_CYAN, MSDF_COLOR_MAGENTA, MSDF_COLOR_YELLOW};

            for (int i = 0; i < contour->edge_count; i++) {
                /* Determine which segment this edge is in (before/after corner) */
                int dist_to_corner = (i - corners[0] + contour->edge_count) % contour->edge_count;
                int segment = (3 * dist_to_corner) / contour->edge_count;
                contour->edges[i].color = colors[segment];
            }
        } else {
            /* Multiple corners - color edges between corners */
            /* Start with a random color */
            MSDF_EdgeColor color = (msdf_rand() % 3) == 0 ? MSDF_COLOR_CYAN
                                 : (msdf_rand() % 2) == 0 ? MSDF_COLOR_MAGENTA
                                 : MSDF_COLOR_YELLOW;

            for (int i = 0; i < corner_count; i++) {
                int start = corners[i];
                int end = corners[(i + 1) % corner_count];

                /* Color all edges from start to end (exclusive) */
                int j = start;
                do {
                    contour->edges[j].color = color;
                    j = (j + 1) % contour->edge_count;
                } while (j != end);

                /* Switch color at each corner */
                color = next_color(color);
            }
        }

        free(corners);
    }
}

void msdf_edge_coloring_ink_trap(MSDF_Shape *shape, double angle_threshold, uint64_t seed)
{
    /* For now, fall back to simple coloring */
    /* TODO: Implement proper ink trap handling */
    msdf_edge_coloring_simple(shape, angle_threshold, seed);
}

void msdf_edge_coloring_by_distance(MSDF_Shape *shape, double angle_threshold, uint64_t seed)
{
    /* For now, fall back to simple coloring */
    /* TODO: Implement distance-based coloring optimization */
    msdf_edge_coloring_simple(shape, angle_threshold, seed);
}

/* ============================================================================
 * SDF Generation
 * ============================================================================ */

/* Calculate signed distance from a point to the entire shape */
static double shape_signed_distance(const MSDF_Shape *shape, MSDF_Vector2 point)
{
    double min_dist = DBL_MAX;
    bool has_negative = false;

    for (int c = 0; c < shape->contour_count; c++) {
        MSDF_Contour *contour = &shape->contours[c];

        for (int e = 0; e < contour->edge_count; e++) {
            double param;
            MSDF_SignedDistance sd = msdf_edge_signed_distance(&contour->edges[e], point, &param);

            double abs_dist = fabs(sd.distance);
            if (abs_dist < fabs(min_dist)) {
                min_dist = sd.distance;
            }
            if (sd.distance < 0) {
                has_negative = true;
            }
        }
    }

    return min_dist;
}

/* Calculate per-channel signed distances for MSDF */
static void shape_multi_distance(const MSDF_Shape *shape, MSDF_Vector2 point,
                                  double *out_r, double *out_g, double *out_b)
{
    MSDF_SignedDistance min_r = { DBL_MAX, 0 };
    MSDF_SignedDistance min_g = { DBL_MAX, 0 };
    MSDF_SignedDistance min_b = { DBL_MAX, 0 };

    for (int c = 0; c < shape->contour_count; c++) {
        MSDF_Contour *contour = &shape->contours[c];

        for (int e = 0; e < contour->edge_count; e++) {
            MSDF_EdgeSegment *edge = &contour->edges[e];
            double param;
            MSDF_SignedDistance sd = msdf_edge_signed_distance(edge, point, &param);

            /* Contribute to channels based on edge color */
            if ((edge->color & MSDF_COLOR_RED) && msdf_distance_less(sd, min_r)) {
                min_r = sd;
            }
            if ((edge->color & MSDF_COLOR_GREEN) && msdf_distance_less(sd, min_g)) {
                min_g = sd;
            }
            if ((edge->color & MSDF_COLOR_BLUE) && msdf_distance_less(sd, min_b)) {
                min_b = sd;
            }
        }
    }

    *out_r = min_r.distance;
    *out_g = min_g.distance;
    *out_b = min_b.distance;
}

/* Transform bitmap coordinates to shape coordinates */
static MSDF_Vector2 unproject(const MSDF_Projection *proj, double x, double y)
{
    return msdf_vec2(
        (x - proj->translate_x) / proj->scale_x,
        (y - proj->translate_y) / proj->scale_y
    );
}

void msdf_generate_sdf(const MSDF_Shape *shape,
                        MSDF_Bitmap *bitmap,
                        const MSDF_Projection *projection,
                        double pixel_range)
{
    if (!shape || !bitmap || !projection) return;
    if (bitmap->format != MSDF_BITMAP_GRAY) {
        carbon_set_error("SDF requires MSDF_BITMAP_GRAY format");
        return;
    }

    double range = pixel_range / projection->scale_x;

    for (int y = 0; y < bitmap->height; y++) {
        for (int x = 0; x < bitmap->width; x++) {
            /* Sample at pixel center */
            MSDF_Vector2 point = unproject(projection, x + 0.5, y + 0.5);

            double dist = shape_signed_distance(shape, point);

            /* Map distance to [0, 1] range */
            float value = (float)(0.5 + dist / range);
            if (value < 0) value = 0;
            if (value > 1) value = 1;

            float *pixel = msdf_bitmap_pixel(bitmap, x, y);
            if (pixel) {
                *pixel = value;
            }
        }
    }
}

void msdf_generate_msdf(const MSDF_Shape *shape,
                         MSDF_Bitmap *bitmap,
                         const MSDF_Projection *projection,
                         double pixel_range)
{
    if (!shape || !bitmap || !projection) return;
    if (bitmap->format != MSDF_BITMAP_RGB) {
        carbon_set_error("MSDF requires MSDF_BITMAP_RGB format");
        return;
    }

    double range = pixel_range / projection->scale_x;

    for (int y = 0; y < bitmap->height; y++) {
        for (int x = 0; x < bitmap->width; x++) {
            /* Sample at pixel center */
            MSDF_Vector2 point = unproject(projection, x + 0.5, y + 0.5);

            double r, g, b;
            shape_multi_distance(shape, point, &r, &g, &b);

            /* Map distances to [0, 1] range */
            float *pixel = msdf_bitmap_pixel(bitmap, x, y);
            if (pixel) {
                pixel[0] = (float)(0.5 + r / range);
                pixel[1] = (float)(0.5 + g / range);
                pixel[2] = (float)(0.5 + b / range);

                /* Clamp */
                if (pixel[0] < 0) pixel[0] = 0; if (pixel[0] > 1) pixel[0] = 1;
                if (pixel[1] < 0) pixel[1] = 0; if (pixel[1] > 1) pixel[1] = 1;
                if (pixel[2] < 0) pixel[2] = 0; if (pixel[2] > 1) pixel[2] = 1;
            }
        }
    }
}

void msdf_generate_mtsdf(const MSDF_Shape *shape,
                          MSDF_Bitmap *bitmap,
                          const MSDF_Projection *projection,
                          double pixel_range)
{
    if (!shape || !bitmap || !projection) return;
    if (bitmap->format != MSDF_BITMAP_RGBA) {
        carbon_set_error("MTSDF requires MSDF_BITMAP_RGBA format");
        return;
    }

    double range = pixel_range / projection->scale_x;

    for (int y = 0; y < bitmap->height; y++) {
        for (int x = 0; x < bitmap->width; x++) {
            /* Sample at pixel center */
            MSDF_Vector2 point = unproject(projection, x + 0.5, y + 0.5);

            /* MSDF channels */
            double r, g, b;
            shape_multi_distance(shape, point, &r, &g, &b);

            /* True SDF for alpha */
            double true_sdf = shape_signed_distance(shape, point);

            /* Map distances to [0, 1] range */
            float *pixel = msdf_bitmap_pixel(bitmap, x, y);
            if (pixel) {
                pixel[0] = (float)(0.5 + r / range);
                pixel[1] = (float)(0.5 + g / range);
                pixel[2] = (float)(0.5 + b / range);
                pixel[3] = (float)(0.5 + true_sdf / range);

                /* Clamp */
                for (int i = 0; i < 4; i++) {
                    if (pixel[i] < 0) pixel[i] = 0;
                    if (pixel[i] > 1) pixel[i] = 1;
                }
            }
        }
    }
}

void msdf_generate_ex(const MSDF_Shape *shape,
                       MSDF_Bitmap *bitmap,
                       const MSDF_Projection *projection,
                       double pixel_range,
                       const MSDF_GeneratorConfig *config)
{
    if (!shape || !bitmap || !projection) return;

    /* Generate based on bitmap format */
    switch (bitmap->format) {
        case MSDF_BITMAP_GRAY:
            msdf_generate_sdf(shape, bitmap, projection, pixel_range);
            break;
        case MSDF_BITMAP_RGB:
            msdf_generate_msdf(shape, bitmap, projection, pixel_range);
            break;
        case MSDF_BITMAP_RGBA:
            msdf_generate_mtsdf(shape, bitmap, projection, pixel_range);
            break;
    }

    /* Apply error correction if configured */
    if (config && config->error_correction.mode != MSDF_ERROR_CORRECTION_DISABLED) {
        msdf_error_correction(bitmap, shape, projection, pixel_range,
                              &config->error_correction);
    }
}

/* ============================================================================
 * Error Correction
 * ============================================================================ */

/* Median of three values */
static float median3(float a, float b, float c)
{
    if (a < b) {
        if (b < c) return b;
        return (a < c) ? c : a;
    } else {
        if (a < c) return a;
        return (b < c) ? c : b;
    }
}

void msdf_error_correction(MSDF_Bitmap *bitmap,
                            const MSDF_Shape *shape,
                            const MSDF_Projection *projection,
                            double pixel_range,
                            const MSDF_ErrorCorrectionConfig *config)
{
    if (!bitmap || !config) return;
    if (bitmap->format != MSDF_BITMAP_RGB && bitmap->format != MSDF_BITMAP_RGBA) return;

    /* Simple error correction: clamp channels that deviate too much from median */
    double threshold = config->min_deviation_ratio - 1.0;
    if (threshold <= 0) threshold = 0.1;

    for (int y = 0; y < bitmap->height; y++) {
        for (int x = 0; x < bitmap->width; x++) {
            float *pixel = msdf_bitmap_pixel(bitmap, x, y);
            if (!pixel) continue;

            float r = pixel[0];
            float g = pixel[1];
            float b = pixel[2];

            float med = median3(r, g, b);

            /* Check if any channel deviates significantly from median */
            bool needs_correction = false;

            if (config->mode == MSDF_ERROR_CORRECTION_INDISCRIMINATE) {
                /* Correct all large deviations */
                needs_correction = (fabs(r - med) > threshold) ||
                                   (fabs(g - med) > threshold) ||
                                   (fabs(b - med) > threshold);
            } else if (config->mode == MSDF_ERROR_CORRECTION_EDGE_PRIORITY ||
                       config->mode == MSDF_ERROR_CORRECTION_EDGE_ONLY) {
                /* Only correct near edges (median close to 0.5) */
                float edge_dist = fabs(med - 0.5f);
                if (edge_dist < 0.25f) {
                    needs_correction = (fabs(r - med) > threshold) ||
                                       (fabs(g - med) > threshold) ||
                                       (fabs(b - med) > threshold);
                }
            }

            if (needs_correction) {
                /* Clamp channels toward median */
                float max_dev = (float)threshold;

                if (r - med > max_dev) pixel[0] = med + max_dev;
                else if (med - r > max_dev) pixel[0] = med - max_dev;

                if (g - med > max_dev) pixel[1] = med + max_dev;
                else if (med - g > max_dev) pixel[1] = med - max_dev;

                if (b - med > max_dev) pixel[2] = med + max_dev;
                else if (med - b > max_dev) pixel[2] = med - max_dev;

                /* Re-clamp to [0, 1] */
                if (pixel[0] < 0) pixel[0] = 0; if (pixel[0] > 1) pixel[0] = 1;
                if (pixel[1] < 0) pixel[1] = 0; if (pixel[1] > 1) pixel[1] = 1;
                if (pixel[2] < 0) pixel[2] = 0; if (pixel[2] > 1) pixel[2] = 1;
            }
        }
    }
}
