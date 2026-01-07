/*
 * Carbon MSDF Generator - Edge Coloring and Generation
 *
 * This file contains:
 * - Edge coloring algorithms
 * - SDF/MSDF/MTSDF generation
 * - Error correction
 */

#include "agentite/msdf.h"
#include "agentite/error.h"

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

    /* Calculate dot and cross products */
    double dot = msdf_vec2_dot(dir_out, dir_in);
    double cross = msdf_vec2_cross(dir_out, dir_in);

    /* It's a corner if:
     * 1. The angle between tangents >= 90 degrees (dot <= 0), OR
     * 2. The perpendicular component exceeds the threshold (sharp turn)
     * This matches the original msdfgen algorithm. */
    return dot <= 0 || fabs(cross) > sin(angle_threshold);
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
        if (!corners) continue;  /* Skip this contour on allocation failure */
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

/*
 * Calculate winding number for a point relative to entire shape.
 * This determines if a point is inside (non-zero winding) or outside (zero winding).
 *
 * Uses ray casting with a horizontal ray going right (+X direction).
 * For each edge crossing:
 *   - Upward crossing (edge going up at intersection): +1
 *   - Downward crossing (edge going down at intersection): -1
 */
static double calculate_winding_number(const MSDF_Shape *shape, MSDF_Vector2 point)
{
    double winding = 0.0;

    for (int c = 0; c < shape->contour_count; c++) {
        MSDF_Contour *contour = &shape->contours[c];

        for (int e = 0; e < contour->edge_count; e++) {
            MSDF_EdgeSegment *edge = &contour->edges[e];

            /* Sample the edge at multiple points for accuracy with curves */
            int samples = (edge->type == MSDF_EDGE_LINEAR) ? 1 : 16;

            for (int i = 0; i < samples; i++) {
                double t0 = (double)i / samples;
                double t1 = (double)(i + 1) / samples;

                MSDF_Vector2 p0 = msdf_edge_point_at(edge, t0);
                MSDF_Vector2 p1 = msdf_edge_point_at(edge, t1);

                /* Check if this segment crosses the horizontal ray from point */
                if ((p0.y <= point.y && p1.y > point.y) ||  /* Upward crossing */
                    (p0.y > point.y && p1.y <= point.y)) {  /* Downward crossing */

                    /* Find x-coordinate of intersection with horizontal line y = point.y */
                    double t = (point.y - p0.y) / (p1.y - p0.y);
                    double x_intersect = p0.x + t * (p1.x - p0.x);

                    /* Only count crossings to the right of the point */
                    if (x_intersect > point.x) {
                        if (p1.y > p0.y) {
                            winding += 1.0;  /* Upward crossing */
                        } else {
                            winding -= 1.0;  /* Downward crossing */
                        }
                    }
                }
            }
        }
    }

    return winding;
}

/* Determine if a point is inside the shape (non-zero winding rule) */
static bool is_point_inside(const MSDF_Shape *shape, MSDF_Vector2 point)
{
    double winding = calculate_winding_number(shape, point);
    return fabs(winding) > 0.5;  /* Non-zero winding rule */
}

/* Calculate signed distance from a point to the entire shape
 * Uses proper global inside/outside determination via winding number */
static double shape_signed_distance(const MSDF_Shape *shape, MSDF_Vector2 point)
{
    /* Find minimum UNSIGNED distance to any edge */
    double min_unsigned_dist = DBL_MAX;

    for (int c = 0; c < shape->contour_count; c++) {
        MSDF_Contour *contour = &shape->contours[c];

        for (int e = 0; e < contour->edge_count; e++) {
            double param;
            MSDF_SignedDistance sd = msdf_edge_signed_distance(&contour->edges[e], point, &param);

            double unsigned_dist = fabs(sd.distance);
            if (unsigned_dist < min_unsigned_dist) {
                min_unsigned_dist = unsigned_dist;
            }
        }
    }

    /* Determine sign using global winding number test
     * Inside = negative distance, Outside = positive distance */
    bool inside = is_point_inside(shape, point);
    return inside ? -min_unsigned_dist : min_unsigned_dist;
}

/* Check if a point could potentially be within 'range' of an edge's bounding box */
static inline bool bounds_could_contain(const MSDF_Bounds *bounds, MSDF_Vector2 point, double range)
{
    return point.x >= bounds->left - range && point.x <= bounds->right + range &&
           point.y >= bounds->bottom - range && point.y <= bounds->top + range;
}

/* Calculate per-channel signed distances for MSDF with spatial culling
 *
 * Hybrid MSDF Algorithm:
 * ======================
 * Each color channel tracks a DIFFERENT subset of edges with per-edge pseudo-signs.
 * This enables sharp corner reconstruction via the shader's median(R, G, B).
 *
 * However, per-edge pseudo-signs cause artifacts around inner contours (holes)
 * because their winding is opposite. The solution is INLINE CORRECTION:
 *
 * 1. Compute per-channel pseudo-signed distances (for corner sharpness)
 * 2. Compute global winding to determine TRUE inside/outside
 * 3. If the median's sign disagrees with global winding, correct the signs
 *
 * This gives us both corner sharpness AND correct hole handling.
 */
static void shape_multi_distance_culled(const MSDF_Shape *shape, MSDF_Vector2 point,
                                         const MSDF_Bounds *edge_bounds, double cull_range,
                                         double *out_r, double *out_g, double *out_b)
{
    /* Track closest edge for each channel */
    MSDF_SignedDistance min_r = { DBL_MAX, 0 };
    MSDF_SignedDistance min_g = { DBL_MAX, 0 };
    MSDF_SignedDistance min_b = { DBL_MAX, 0 };

    /* Also track overall minimum unsigned distance */
    double min_unsigned = DBL_MAX;

    int bounds_idx = 0;
    for (int c = 0; c < shape->contour_count; c++) {
        MSDF_Contour *contour = &shape->contours[c];

        for (int e = 0; e < contour->edge_count; e++) {
            /* Early-out: skip edges whose bounding box is too far */
            if (edge_bounds && !bounds_could_contain(&edge_bounds[bounds_idx], point, cull_range)) {
                bounds_idx++;
                continue;
            }
            bounds_idx++;

            MSDF_EdgeSegment *edge = &contour->edges[e];
            double param;
            MSDF_SignedDistance sd = msdf_edge_signed_distance(edge, point, &param);

            double unsigned_dist = fabs(sd.distance);
            if (unsigned_dist < min_unsigned) {
                min_unsigned = unsigned_dist;
            }

            /* Track closest edge for each channel */
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

    /* Get per-channel distances */
    double r = min_r.distance;
    double g = min_g.distance;
    double b = min_b.distance;

    /* Compute median of the three channels */
    double med;
    if (r < g) {
        if (g < b) med = g;
        else med = (r < b) ? b : r;
    } else {
        if (r < b) med = r;
        else med = (g < b) ? b : g;
    }

    /* Check if median sign matches global winding (true inside/outside) */
    bool msdf_says_inside = (med < 0);
    bool truly_inside = is_point_inside(shape, point);

    if (msdf_says_inside != truly_inside) {
        /* Sign conflict - fix using global winding */
        double sign = truly_inside ? -1.0 : 1.0;
        *out_r = sign * fabs(r);
        *out_g = sign * fabs(g);
        *out_b = sign * fabs(b);
    } else {
        /* No conflict - use per-edge pseudo-signed distances */
        *out_r = r;
        *out_g = g;
        *out_b = b;
    }
}

/* Pre-compute bounding boxes for all edges in a shape */
static MSDF_Bounds *precompute_edge_bounds(const MSDF_Shape *shape, int *out_count)
{
    int total_edges = msdf_shape_edge_count(shape);
    if (total_edges == 0) {
        *out_count = 0;
        return NULL;
    }

    MSDF_Bounds *bounds = (MSDF_Bounds *)malloc(total_edges * sizeof(MSDF_Bounds));
    if (!bounds) {
        *out_count = 0;
        return NULL;
    }

    int idx = 0;
    for (int c = 0; c < shape->contour_count; c++) {
        MSDF_Contour *contour = &shape->contours[c];
        for (int e = 0; e < contour->edge_count; e++) {
            bounds[idx++] = msdf_edge_get_bounds(&contour->edges[e]);
        }
    }

    *out_count = total_edges;
    return bounds;
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
    msdf_generate_sdf_ex(shape, bitmap, projection, pixel_range, false);
}

void msdf_generate_sdf_ex(const MSDF_Shape *shape,
                           MSDF_Bitmap *bitmap,
                           const MSDF_Projection *projection,
                           double pixel_range,
                           bool invert_sign)
{
    if (!shape || !bitmap || !projection) return;
    if (bitmap->format != MSDF_BITMAP_GRAY) {
        agentite_set_error("SDF requires MSDF_BITMAP_GRAY format");
        return;
    }

    double range = pixel_range / projection->scale_x;
    double sign_mult = invert_sign ? -1.0 : 1.0;

    for (int y = 0; y < bitmap->height; y++) {
        for (int x = 0; x < bitmap->width; x++) {
            /* Sample at pixel center */
            MSDF_Vector2 point = unproject(projection, x + 0.5, y + 0.5);

            double dist = shape_signed_distance(shape, point) * sign_mult;

            /* Map distance to [0, 1] range
             * Convention: inside glyph (negative dist) -> value > 0.5
             *             outside glyph (positive dist) -> value < 0.5 */
            float value = (float)(0.5 - dist / range);
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
    msdf_generate_msdf_ex(shape, bitmap, projection, pixel_range, false);
}

void msdf_generate_msdf_ex(const MSDF_Shape *shape,
                            MSDF_Bitmap *bitmap,
                            const MSDF_Projection *projection,
                            double pixel_range,
                            bool invert_sign)
{
    if (!shape || !bitmap || !projection) return;
    if (bitmap->format != MSDF_BITMAP_RGB) {
        agentite_set_error("MSDF requires MSDF_BITMAP_RGB format");
        return;
    }

    double range = pixel_range / projection->scale_x;
    double sign_mult = invert_sign ? -1.0 : 1.0;

    /* Pre-compute edge bounds for spatial culling
     * Use a large cull range to ensure all relevant edges are considered.
     * The cull_range must be at least as large as the maximum distance we need
     * to calculate accurately, which is typically the full glyph size. */
    int edge_count = 0;
    MSDF_Bounds *edge_bounds = precompute_edge_bounds(shape, &edge_count);
    (void)edge_count; /* Unused, bounds array is iterated by shape structure */

    /* Use shape bounds to determine cull range - ensures no edges are incorrectly skipped */
    MSDF_Bounds shape_bounds = msdf_shape_get_bounds(shape);
    double shape_size = fmax(shape_bounds.right - shape_bounds.left,
                              shape_bounds.top - shape_bounds.bottom);
    double cull_range = fmax(range, shape_size);

    for (int y = 0; y < bitmap->height; y++) {
        for (int x = 0; x < bitmap->width; x++) {
            /* Sample at pixel center */
            MSDF_Vector2 point = unproject(projection, x + 0.5, y + 0.5);

            double r, g, b;
            shape_multi_distance_culled(shape, point, edge_bounds, cull_range, &r, &g, &b);

            /* Apply sign correction before mapping to [0,1] */
            r *= sign_mult;
            g *= sign_mult;
            b *= sign_mult;

            /* Map distances to [0, 1] range
             * Convention: inside glyph (negative dist) -> value > 0.5
             *             outside glyph (positive dist) -> value < 0.5 */
            float *pixel = msdf_bitmap_pixel(bitmap, x, y);
            if (pixel) {
                pixel[0] = (float)(0.5 - r / range);
                pixel[1] = (float)(0.5 - g / range);
                pixel[2] = (float)(0.5 - b / range);

                /* Clamp */
                if (pixel[0] < 0) pixel[0] = 0; if (pixel[0] > 1) pixel[0] = 1;
                if (pixel[1] < 0) pixel[1] = 0; if (pixel[1] > 1) pixel[1] = 1;
                if (pixel[2] < 0) pixel[2] = 0; if (pixel[2] > 1) pixel[2] = 1;
            }
        }
    }

    free(edge_bounds);
}

void msdf_generate_mtsdf(const MSDF_Shape *shape,
                          MSDF_Bitmap *bitmap,
                          const MSDF_Projection *projection,
                          double pixel_range)
{
    msdf_generate_mtsdf_ex(shape, bitmap, projection, pixel_range, false);
}

void msdf_generate_mtsdf_ex(const MSDF_Shape *shape,
                             MSDF_Bitmap *bitmap,
                             const MSDF_Projection *projection,
                             double pixel_range,
                             bool invert_sign)
{
    if (!shape || !bitmap || !projection) return;
    if (bitmap->format != MSDF_BITMAP_RGBA) {
        agentite_set_error("MTSDF requires MSDF_BITMAP_RGBA format");
        return;
    }

    double range = pixel_range / projection->scale_x;
    double sign_mult = invert_sign ? -1.0 : 1.0;

    /* Pre-compute edge bounds for spatial culling */
    int edge_count = 0;
    MSDF_Bounds *edge_bounds = precompute_edge_bounds(shape, &edge_count);
    (void)edge_count;

    /* Use shape bounds to determine cull range - ensures no edges are incorrectly skipped */
    MSDF_Bounds shape_bounds = msdf_shape_get_bounds(shape);
    double shape_size = fmax(shape_bounds.right - shape_bounds.left,
                              shape_bounds.top - shape_bounds.bottom);
    double cull_range = fmax(range, shape_size);

    for (int y = 0; y < bitmap->height; y++) {
        for (int x = 0; x < bitmap->width; x++) {
            /* Sample at pixel center */
            MSDF_Vector2 point = unproject(projection, x + 0.5, y + 0.5);

            /* MSDF channels with spatial culling */
            double r, g, b;
            shape_multi_distance_culled(shape, point, edge_bounds, cull_range, &r, &g, &b);

            /* True SDF for alpha */
            double true_sdf = shape_signed_distance(shape, point);

            /* Apply sign correction before mapping to [0,1] */
            r *= sign_mult;
            g *= sign_mult;
            b *= sign_mult;
            true_sdf *= sign_mult;

            /* Map distances to [0, 1] range
             * Convention: inside glyph (negative dist) -> value > 0.5
             *             outside glyph (positive dist) -> value < 0.5 */
            float *pixel = msdf_bitmap_pixel(bitmap, x, y);
            if (pixel) {
                pixel[0] = (float)(0.5 - r / range);
                pixel[1] = (float)(0.5 - g / range);
                pixel[2] = (float)(0.5 - b / range);
                pixel[3] = (float)(0.5 - true_sdf / range);

                /* Clamp */
                for (int i = 0; i < 4; i++) {
                    if (pixel[i] < 0) pixel[i] = 0;
                    if (pixel[i] > 1) pixel[i] = 1;
                }
            }
        }
    }

    free(edge_bounds);
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
 * Error Correction (msdfgen-style)
 * ============================================================================
 *
 * MSDF artifacts occur when adjacent pixels have conflicting channel orderings.
 * This happens particularly around inner contours (holes) where the pseudo-signed
 * distance gives the "wrong" sign relative to the global inside/outside state.
 *
 * The solution is CLASH DETECTION:
 * 1. For each pixel, compute the "deviation" (max channel diff from median)
 * 2. Compare with neighboring pixels
 * 3. If a pixel has high deviation but its neighbor is "equalized" (all channels
 *    similar), the pixel is likely an artifact
 * 4. Fix artifacts by setting all channels to the median (equalization)
 */

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

/* Equalize a pixel by setting all channels to the median */
static void equalize_pixel(float *pixel)
{
    float med = median3(pixel[0], pixel[1], pixel[2]);
    pixel[0] = med;
    pixel[1] = med;
    pixel[2] = med;
}

void msdf_error_correction(MSDF_Bitmap *bitmap,
                            const MSDF_Shape *shape,
                            const MSDF_Projection *projection,
                            double pixel_range,
                            const MSDF_ErrorCorrectionConfig *config)
{
    if (!bitmap || !config) return;
    if (bitmap->format != MSDF_BITMAP_RGB && bitmap->format != MSDF_BITMAP_RGBA) return;
    if (config->mode == MSDF_ERROR_CORRECTION_DISABLED) return;

    int width = bitmap->width;
    int height = bitmap->height;

    /* Threshold for artifact detection - lower = more aggressive correction */
    float artifact_threshold = 0.15f;

    /* Create stencil for marking pixels that need correction */
    bool *needs_correction = (bool *)calloc(width * height, sizeof(bool));
    if (!needs_correction) return;

    /*
     * Artifact Detection Strategy:
     * ============================
     * MSDF artifacts (colored halos) occur when a pixel's median disagrees
     * with the true inside/outside state. We detect this by:
     *
     * 1. Computing the median of the three channels
     * 2. Checking if adjacent pixels have conflicting medians (one > 0.5, one < 0.5)
     *    but large channel spread (indicating MSDF disagreement)
     * 3. The artifact pixel is the one with higher channel spread
     */

    /* Pass 1: Detect artifact pixels */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float *pixel = msdf_bitmap_pixel(bitmap, x, y);
            if (!pixel) continue;

            float r = pixel[0], g = pixel[1], b = pixel[2];
            float med = median3(r, g, b);
            float spread = fmaxf(fmaxf(r, g), b) - fminf(fminf(r, g), b);

            /* Only consider pixels with significant channel spread */
            if (spread < artifact_threshold) continue;

            /* Check if this pixel is likely an artifact by comparing with neighbors */
            bool is_artifact = false;

            /* Check 4-connected neighbors */
            int dx[] = {-1, 1, 0, 0};
            int dy[] = {0, 0, -1, 1};

            for (int d = 0; d < 4; d++) {
                int nx = x + dx[d];
                int ny = y + dy[d];
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;

                float *neighbor = msdf_bitmap_pixel(bitmap, nx, ny);
                if (!neighbor) continue;

                float nr = neighbor[0], ng = neighbor[1], nb = neighbor[2];
                float nmed = median3(nr, ng, nb);
                float nspread = fmaxf(fmaxf(nr, ng), nb) - fminf(fminf(nr, ng), nb);

                /* Artifact condition: medians on opposite sides of 0.5,
                 * current pixel has high spread, neighbor has low spread */
                bool opposite_sides = (med > 0.5f) != (nmed > 0.5f);
                bool current_has_spread = (spread > artifact_threshold);
                bool neighbor_low_spread = (nspread < artifact_threshold * 0.5f);

                if (opposite_sides && current_has_spread && neighbor_low_spread) {
                    is_artifact = true;
                    break;
                }

                /* Also detect: both have spread but wildly different medians near edge */
                if (opposite_sides && fabsf(med - 0.5f) < 0.3f && spread > 0.2f) {
                    is_artifact = true;
                    break;
                }
            }

            if (is_artifact) {
                /* For EDGE_PRIORITY: only correct near edges */
                if (config->mode == MSDF_ERROR_CORRECTION_EDGE_PRIORITY) {
                    float edge_dist = fabsf(med - 0.5f);
                    if (edge_dist > 0.35f) continue;
                }
                needs_correction[y * width + x] = true;
            }
        }
    }

    /* Pass 2: Dilate the correction mask slightly to catch edge cases */
    bool *dilated = (bool *)calloc(width * height, sizeof(bool));
    if (dilated) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (needs_correction[y * width + x]) {
                    dilated[y * width + x] = true;
                    /* Also mark immediate neighbors if they have spread */
                    int dx[] = {-1, 1, 0, 0};
                    int dy[] = {0, 0, -1, 1};
                    for (int d = 0; d < 4; d++) {
                        int nx = x + dx[d];
                        int ny = y + dy[d];
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            float *neighbor = msdf_bitmap_pixel(bitmap, nx, ny);
                            if (neighbor) {
                                float nr = neighbor[0], ng = neighbor[1], nb = neighbor[2];
                                float nspread = fmaxf(fmaxf(nr, ng), nb) - fminf(fminf(nr, ng), nb);
                                if (nspread > artifact_threshold * 0.7f) {
                                    dilated[ny * width + nx] = true;
                                }
                            }
                        }
                    }
                }
            }
        }
        memcpy(needs_correction, dilated, width * height * sizeof(bool));
        free(dilated);
    }

    /* Pass 3: Apply corrections by equalizing marked pixels */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (needs_correction[y * width + x]) {
                float *pixel = msdf_bitmap_pixel(bitmap, x, y);
                if (pixel) {
                    equalize_pixel(pixel);
                }
            }
        }
    }

    free(needs_correction);

    (void)shape;
    (void)projection;
    (void)pixel_range;
}
