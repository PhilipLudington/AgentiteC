/*
 * Carbon MSDF Generator - Core Implementation
 *
 * Pure C11 implementation of Multi-channel Signed Distance Field generation.
 * Based on the msdfgen algorithm by Viktor Chlumsky.
 *
 * This file contains:
 * - Shape construction and memory management
 * - Shape extraction from stb_truetype
 * - Shape utilities (bounds, winding, normalize)
 */

#include "agentite/msdf.h"
#include "agentite/agentite.h"
#include "agentite/error.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Include stb_truetype header (implementation is in text_font.cpp) */
#include "stb_truetype.h"

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define INITIAL_CONTOUR_CAPACITY 4
#define INITIAL_EDGE_CAPACITY 16

/* ============================================================================
 * Shape Construction
 * ============================================================================ */

MSDF_Shape *msdf_shape_create(void)
{
    MSDF_Shape *shape = AGENTITE_ALLOC(MSDF_Shape);
    if (!shape) {
        agentite_set_error("Failed to allocate MSDF shape");
        return NULL;
    }

    shape->contours = NULL;
    shape->contour_count = 0;
    shape->contour_capacity = 0;
    shape->inverse_y_axis = false;

    return shape;
}

void msdf_shape_free(MSDF_Shape *shape)
{
    if (!shape) return;

    for (int i = 0; i < shape->contour_count; i++) {
        free(shape->contours[i].edges);
    }
    free(shape->contours);
    free(shape);
}

MSDF_Contour *msdf_shape_add_contour(MSDF_Shape *shape)
{
    if (!shape) return NULL;

    /* Grow contour array if needed */
    if (shape->contour_count >= shape->contour_capacity) {
        int new_capacity = shape->contour_capacity == 0
            ? INITIAL_CONTOUR_CAPACITY
            : shape->contour_capacity * 2;

        MSDF_Contour *new_contours = AGENTITE_REALLOC(
            shape->contours, MSDF_Contour, new_capacity);
        if (!new_contours) {
            agentite_set_error("Failed to grow contour array");
            return NULL;
        }

        shape->contours = new_contours;
        shape->contour_capacity = new_capacity;
    }

    /* Initialize new contour */
    MSDF_Contour *contour = &shape->contours[shape->contour_count];
    contour->edges = NULL;
    contour->edge_count = 0;
    contour->edge_capacity = 0;

    shape->contour_count++;
    return contour;
}

void msdf_contour_add_edge(MSDF_Contour *contour, const MSDF_EdgeSegment *edge)
{
    if (!contour || !edge) return;

    /* Grow edge array if needed */
    if (contour->edge_count >= contour->edge_capacity) {
        int new_capacity = contour->edge_capacity == 0
            ? INITIAL_EDGE_CAPACITY
            : contour->edge_capacity * 2;

        MSDF_EdgeSegment *new_edges = AGENTITE_REALLOC(
            contour->edges, MSDF_EdgeSegment, new_capacity);
        if (!new_edges) {
            agentite_set_error("Failed to grow edge array");
            return;
        }

        contour->edges = new_edges;
        contour->edge_capacity = new_capacity;
    }

    contour->edges[contour->edge_count] = *edge;
    contour->edge_count++;
}

void msdf_contour_add_line(MSDF_Contour *contour, MSDF_Vector2 p0, MSDF_Vector2 p1)
{
    MSDF_EdgeSegment edge = {
        .type = MSDF_EDGE_LINEAR,
        .color = MSDF_COLOR_WHITE,
        .p = { p0, p1, {0, 0}, {0, 0} }
    };
    msdf_contour_add_edge(contour, &edge);
}

void msdf_contour_add_quadratic(MSDF_Contour *contour,
                                 MSDF_Vector2 p0, MSDF_Vector2 p1, MSDF_Vector2 p2)
{
    MSDF_EdgeSegment edge = {
        .type = MSDF_EDGE_QUADRATIC,
        .color = MSDF_COLOR_WHITE,
        .p = { p0, p1, p2, {0, 0} }
    };
    msdf_contour_add_edge(contour, &edge);
}

void msdf_contour_add_cubic(MSDF_Contour *contour,
                             MSDF_Vector2 p0, MSDF_Vector2 p1,
                             MSDF_Vector2 p2, MSDF_Vector2 p3)
{
    MSDF_EdgeSegment edge = {
        .type = MSDF_EDGE_CUBIC,
        .color = MSDF_COLOR_WHITE,
        .p = { p0, p1, p2, p3 }
    };
    msdf_contour_add_edge(contour, &edge);
}

/* ============================================================================
 * Shape Extraction from stb_truetype
 * ============================================================================ */

MSDF_Shape *msdf_shape_from_glyph(const stbtt_fontinfo *font_info,
                                   int glyph_index,
                                   double scale)
{
    if (!font_info) {
        agentite_set_error("NULL font_info");
        return NULL;
    }

    /* Get glyph shape from stb_truetype */
    stbtt_vertex *vertices = NULL;
    int num_vertices = stbtt_GetGlyphShape(font_info, glyph_index, &vertices);

    if (num_vertices <= 0 || !vertices) {
        /* Empty glyph (e.g., space character) - return empty shape */
        return msdf_shape_create();
    }

    MSDF_Shape *shape = msdf_shape_create();
    if (!shape) {
        stbtt_FreeShape(font_info, vertices);
        return NULL;
    }

    /* stb_truetype uses Y-up coordinates, same as our default */
    shape->inverse_y_axis = false;

    MSDF_Contour *current_contour = NULL;
    MSDF_Vector2 last_point = {0, 0};
    MSDF_Vector2 contour_start = {0, 0};

    for (int i = 0; i < num_vertices; i++) {
        stbtt_vertex *v = &vertices[i];

        /* Scale coordinates */
        double x = v->x * scale;
        double y = v->y * scale;
        double cx = v->cx * scale;
        double cy = v->cy * scale;
        double cx1 = v->cx1 * scale;
        double cy1 = v->cy1 * scale;

        switch (v->type) {
            case STBTT_vmove:
                /* Start a new contour */
                current_contour = msdf_shape_add_contour(shape);
                if (!current_contour) {
                    msdf_shape_free(shape);
                    stbtt_FreeShape(font_info, vertices);
                    return NULL;
                }
                last_point = msdf_vec2(x, y);
                contour_start = last_point;
                break;

            case STBTT_vline:
                /* Line segment from last_point to (x, y) */
                if (current_contour) {
                    MSDF_Vector2 end = msdf_vec2(x, y);
                    /* Skip degenerate edges */
                    if (msdf_vec2_length_squared(msdf_vec2_sub(end, last_point)) > MSDF_EPSILON) {
                        msdf_contour_add_line(current_contour, last_point, end);
                    }
                    last_point = end;
                }
                break;

            case STBTT_vcurve:
                /* Quadratic bezier: last_point -> (cx, cy) -> (x, y) */
                if (current_contour) {
                    MSDF_Vector2 control = msdf_vec2(cx, cy);
                    MSDF_Vector2 end = msdf_vec2(x, y);
                    msdf_contour_add_quadratic(current_contour, last_point, control, end);
                    last_point = end;
                }
                break;

            case STBTT_vcubic:
                /* Cubic bezier: last_point -> (cx, cy) -> (cx1, cy1) -> (x, y) */
                if (current_contour) {
                    MSDF_Vector2 control1 = msdf_vec2(cx, cy);
                    MSDF_Vector2 control2 = msdf_vec2(cx1, cy1);
                    MSDF_Vector2 end = msdf_vec2(x, y);
                    msdf_contour_add_cubic(current_contour, last_point, control1, control2, end);
                    last_point = end;
                }
                break;
        }
    }

    /* Close any open contours by adding a line back to start if needed */
    for (int i = 0; i < shape->contour_count; i++) {
        MSDF_Contour *contour = &shape->contours[i];
        if (contour->edge_count > 0) {
            MSDF_EdgeSegment *first_edge = &contour->edges[0];
            MSDF_EdgeSegment *last_edge = &contour->edges[contour->edge_count - 1];

            /* Get end point of last edge and start point of first edge */
            MSDF_Vector2 end_point;
            switch (last_edge->type) {
                case MSDF_EDGE_LINEAR:    end_point = last_edge->p[1]; break;
                case MSDF_EDGE_QUADRATIC: end_point = last_edge->p[2]; break;
                case MSDF_EDGE_CUBIC:     end_point = last_edge->p[3]; break;
                default: end_point = last_edge->p[0]; break;
            }

            MSDF_Vector2 start_point = first_edge->p[0];

            /* Add closing line if not already closed */
            double gap = msdf_vec2_length_squared(msdf_vec2_sub(end_point, start_point));
            if (gap > MSDF_EPSILON) {
                msdf_contour_add_line(contour, end_point, start_point);
            }
        }
    }

    stbtt_FreeShape(font_info, vertices);
    return shape;
}

MSDF_Shape *msdf_shape_from_codepoint(const stbtt_fontinfo *font_info,
                                       int codepoint,
                                       double scale)
{
    if (!font_info) {
        agentite_set_error("NULL font_info");
        return NULL;
    }

    int glyph_index = stbtt_FindGlyphIndex(font_info, codepoint);
    return msdf_shape_from_glyph(font_info, glyph_index, scale);
}

/* ============================================================================
 * Shape Utilities
 * ============================================================================ */

int msdf_shape_edge_count(const MSDF_Shape *shape)
{
    if (!shape) return 0;

    int count = 0;
    for (int i = 0; i < shape->contour_count; i++) {
        count += shape->contours[i].edge_count;
    }
    return count;
}

bool msdf_shape_is_empty(const MSDF_Shape *shape)
{
    return !shape || msdf_shape_edge_count(shape) == 0;
}

MSDF_Bounds msdf_shape_get_bounds(const MSDF_Shape *shape)
{
    MSDF_Bounds bounds = {
        .left = DBL_MAX,
        .bottom = DBL_MAX,
        .right = -DBL_MAX,
        .top = -DBL_MAX
    };

    if (!shape) return bounds;

    for (int i = 0; i < shape->contour_count; i++) {
        MSDF_Contour *contour = &shape->contours[i];

        for (int j = 0; j < contour->edge_count; j++) {
            MSDF_Bounds edge_bounds = msdf_edge_get_bounds(&contour->edges[j]);

            if (edge_bounds.left < bounds.left) bounds.left = edge_bounds.left;
            if (edge_bounds.bottom < bounds.bottom) bounds.bottom = edge_bounds.bottom;
            if (edge_bounds.right > bounds.right) bounds.right = edge_bounds.right;
            if (edge_bounds.top > bounds.top) bounds.top = edge_bounds.top;
        }
    }

    return bounds;
}

int msdf_contour_winding(const MSDF_Contour *contour)
{
    if (!contour || contour->edge_count == 0) return 0;

    /* Calculate signed area using shoelace formula */
    double area = 0.0;

    for (int i = 0; i < contour->edge_count; i++) {
        MSDF_EdgeSegment *edge = &contour->edges[i];

        /* Sample edge at multiple points for curved edges */
        int samples = (edge->type == MSDF_EDGE_LINEAR) ? 1 : 8;

        for (int s = 0; s < samples; s++) {
            double t0 = (double)s / samples;
            double t1 = (double)(s + 1) / samples;

            MSDF_Vector2 p0 = msdf_edge_point_at(edge, t0);
            MSDF_Vector2 p1 = msdf_edge_point_at(edge, t1);

            area += (p1.x - p0.x) * (p1.y + p0.y);
        }
    }

    /* Positive area = clockwise, negative = counter-clockwise */
    if (area > 0) return 1;
    if (area < 0) return -1;
    return 0;
}

void msdf_contour_reverse(MSDF_Contour *contour)
{
    if (!contour || contour->edge_count <= 1) return;

    /* Reverse array order */
    for (int i = 0; i < contour->edge_count / 2; i++) {
        int j = contour->edge_count - 1 - i;
        MSDF_EdgeSegment temp = contour->edges[i];
        contour->edges[i] = contour->edges[j];
        contour->edges[j] = temp;
    }

    /* Reverse each edge's control points */
    for (int i = 0; i < contour->edge_count; i++) {
        MSDF_EdgeSegment *edge = &contour->edges[i];

        switch (edge->type) {
            case MSDF_EDGE_LINEAR: {
                MSDF_Vector2 temp = edge->p[0];
                edge->p[0] = edge->p[1];
                edge->p[1] = temp;
                break;
            }
            case MSDF_EDGE_QUADRATIC: {
                MSDF_Vector2 temp = edge->p[0];
                edge->p[0] = edge->p[2];
                edge->p[2] = temp;
                /* p[1] (control point) stays in place */
                break;
            }
            case MSDF_EDGE_CUBIC: {
                MSDF_Vector2 temp0 = edge->p[0];
                MSDF_Vector2 temp1 = edge->p[1];
                edge->p[0] = edge->p[3];
                edge->p[1] = edge->p[2];
                edge->p[2] = temp1;
                edge->p[3] = temp0;
                break;
            }
        }
    }
}

void msdf_shape_normalize(MSDF_Shape *shape)
{
    if (!shape || msdf_shape_is_empty(shape)) return;

    MSDF_Bounds bounds = msdf_shape_get_bounds(shape);
    double width = bounds.right - bounds.left;
    double height = bounds.top - bounds.bottom;

    if (width <= 0 || height <= 0) return;

    double scale = 1.0 / ((width > height) ? width : height);
    double offset_x = -bounds.left - width * 0.5;
    double offset_y = -bounds.bottom - height * 0.5;

    /* Transform all control points */
    for (int i = 0; i < shape->contour_count; i++) {
        MSDF_Contour *contour = &shape->contours[i];

        for (int j = 0; j < contour->edge_count; j++) {
            MSDF_EdgeSegment *edge = &contour->edges[j];

            int num_points;
            switch (edge->type) {
                case MSDF_EDGE_LINEAR:    num_points = 2; break;
                case MSDF_EDGE_QUADRATIC: num_points = 3; break;
                case MSDF_EDGE_CUBIC:     num_points = 4; break;
                default: num_points = 0; break;
            }

            for (int k = 0; k < num_points; k++) {
                edge->p[k].x = (edge->p[k].x + offset_x) * scale;
                edge->p[k].y = (edge->p[k].y + offset_y) * scale;
            }
        }
    }
}

/* ============================================================================
 * Edge Segment Math
 * ============================================================================ */

MSDF_Vector2 msdf_edge_point_at(const MSDF_EdgeSegment *edge, double t)
{
    if (!edge) return msdf_vec2(0, 0);

    switch (edge->type) {
        case MSDF_EDGE_LINEAR: {
            /* Linear interpolation: P = (1-t)*P0 + t*P1 */
            return msdf_vec2_add(
                msdf_vec2_mul(edge->p[0], 1.0 - t),
                msdf_vec2_mul(edge->p[1], t)
            );
        }

        case MSDF_EDGE_QUADRATIC: {
            /* Quadratic bezier: P = (1-t)^2*P0 + 2*(1-t)*t*P1 + t^2*P2 */
            double t2 = t * t;
            double mt = 1.0 - t;
            double mt2 = mt * mt;

            return msdf_vec2_add(
                msdf_vec2_add(
                    msdf_vec2_mul(edge->p[0], mt2),
                    msdf_vec2_mul(edge->p[1], 2.0 * mt * t)
                ),
                msdf_vec2_mul(edge->p[2], t2)
            );
        }

        case MSDF_EDGE_CUBIC: {
            /* Cubic bezier: P = (1-t)^3*P0 + 3*(1-t)^2*t*P1 + 3*(1-t)*t^2*P2 + t^3*P3 */
            double t2 = t * t;
            double t3 = t2 * t;
            double mt = 1.0 - t;
            double mt2 = mt * mt;
            double mt3 = mt2 * mt;

            return msdf_vec2_add(
                msdf_vec2_add(
                    msdf_vec2_mul(edge->p[0], mt3),
                    msdf_vec2_mul(edge->p[1], 3.0 * mt2 * t)
                ),
                msdf_vec2_add(
                    msdf_vec2_mul(edge->p[2], 3.0 * mt * t2),
                    msdf_vec2_mul(edge->p[3], t3)
                )
            );
        }
    }

    return msdf_vec2(0, 0);
}

MSDF_Vector2 msdf_edge_direction_at(const MSDF_EdgeSegment *edge, double t)
{
    if (!edge) return msdf_vec2(0, 0);

    switch (edge->type) {
        case MSDF_EDGE_LINEAR: {
            /* Constant direction: P1 - P0 */
            return msdf_vec2_sub(edge->p[1], edge->p[0]);
        }

        case MSDF_EDGE_QUADRATIC: {
            /* Derivative: 2*(1-t)*(P1-P0) + 2*t*(P2-P1) */
            MSDF_Vector2 d0 = msdf_vec2_sub(edge->p[1], edge->p[0]);
            MSDF_Vector2 d1 = msdf_vec2_sub(edge->p[2], edge->p[1]);

            MSDF_Vector2 tangent = msdf_vec2_add(
                msdf_vec2_mul(d0, 2.0 * (1.0 - t)),
                msdf_vec2_mul(d1, 2.0 * t)
            );

            /* Handle degenerate case at endpoints */
            if (msdf_vec2_length_squared(tangent) < MSDF_EPSILON) {
                return msdf_vec2_sub(edge->p[2], edge->p[0]);
            }
            return tangent;
        }

        case MSDF_EDGE_CUBIC: {
            /* Derivative: 3*(1-t)^2*(P1-P0) + 6*(1-t)*t*(P2-P1) + 3*t^2*(P3-P2) */
            MSDF_Vector2 d0 = msdf_vec2_sub(edge->p[1], edge->p[0]);
            MSDF_Vector2 d1 = msdf_vec2_sub(edge->p[2], edge->p[1]);
            MSDF_Vector2 d2 = msdf_vec2_sub(edge->p[3], edge->p[2]);

            double mt = 1.0 - t;

            MSDF_Vector2 tangent = msdf_vec2_add(
                msdf_vec2_add(
                    msdf_vec2_mul(d0, 3.0 * mt * mt),
                    msdf_vec2_mul(d1, 6.0 * mt * t)
                ),
                msdf_vec2_mul(d2, 3.0 * t * t)
            );

            /* Handle degenerate case */
            if (msdf_vec2_length_squared(tangent) < MSDF_EPSILON) {
                /* Try second derivative or fallback to chord */
                tangent = msdf_vec2_sub(edge->p[3], edge->p[0]);
            }
            return tangent;
        }
    }

    return msdf_vec2(0, 0);
}

MSDF_Bounds msdf_edge_get_bounds(const MSDF_EdgeSegment *edge)
{
    MSDF_Bounds bounds = {
        .left = DBL_MAX,
        .bottom = DBL_MAX,
        .right = -DBL_MAX,
        .top = -DBL_MAX
    };

    if (!edge) return bounds;

    /* Include all control points */
    int num_points;
    switch (edge->type) {
        case MSDF_EDGE_LINEAR:    num_points = 2; break;
        case MSDF_EDGE_QUADRATIC: num_points = 3; break;
        case MSDF_EDGE_CUBIC:     num_points = 4; break;
        default: return bounds;
    }

    for (int i = 0; i < num_points; i++) {
        if (edge->p[i].x < bounds.left) bounds.left = edge->p[i].x;
        if (edge->p[i].x > bounds.right) bounds.right = edge->p[i].x;
        if (edge->p[i].y < bounds.bottom) bounds.bottom = edge->p[i].y;
        if (edge->p[i].y > bounds.top) bounds.top = edge->p[i].y;
    }

    /* For bezier curves, sample additional points to catch extrema */
    if (edge->type != MSDF_EDGE_LINEAR) {
        for (int i = 1; i < 8; i++) {
            double t = i / 8.0;
            MSDF_Vector2 p = msdf_edge_point_at(edge, t);

            if (p.x < bounds.left) bounds.left = p.x;
            if (p.x > bounds.right) bounds.right = p.x;
            if (p.y < bounds.bottom) bounds.bottom = p.y;
            if (p.y > bounds.top) bounds.top = p.y;
        }
    }

    return bounds;
}

/* ============================================================================
 * Signed Distance Calculation
 * ============================================================================ */

/*
 * MSDF Sign Determination
 * =======================
 * The sign of a signed distance field indicates inside (negative) vs outside (positive).
 *
 * The msdfgen algorithm uses "pseudo-distance" where the sign comes from the relationship
 * between the point and the edge's ORIENTED direction. For a counter-clockwise contour,
 * points to the LEFT of the edge direction are OUTSIDE, points to the RIGHT are INSIDE.
 *
 * However, this local edge sign must be consistent with the global inside/outside state.
 * We achieve this by:
 * 1. Computing the winding number to determine if a point is truly inside/outside
 * 2. Using that global sign for the final distance
 *
 * The multi-channel MSDF then works because each channel tracks different edges,
 * and the MEDIAN in the shader filters out incorrect local signs at corners.
 */

/* Calculate winding number contribution from a single edge segment */
static double edge_winding_contribution(const MSDF_EdgeSegment *edge, MSDF_Vector2 point)
{
    /*
     * For winding number calculation, we count how many times a ray from the point
     * crosses the edge. We use a horizontal ray going right (+X direction).
     *
     * For each crossing:
     * - Upward crossing (edge going up at intersection): +1
     * - Downward crossing (edge going down at intersection): -1
     */
    double winding = 0.0;

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

    return winding;
}

/* Calculate winding number for a point relative to a contour */
static double contour_winding_number(const MSDF_Contour *contour, MSDF_Vector2 point)
{
    double winding = 0.0;

    for (int i = 0; i < contour->edge_count; i++) {
        winding += edge_winding_contribution(&contour->edges[i], point);
    }

    return winding;
}

/* Calculate winding number for a point relative to entire shape */
static double shape_winding_number(const MSDF_Shape *shape, MSDF_Vector2 point)
{
    double winding = 0.0;

    for (int c = 0; c < shape->contour_count; c++) {
        winding += contour_winding_number(&shape->contours[c], point);
    }

    return winding;
}

/* Determine if a point is inside the shape (non-zero winding rule) */
static bool point_inside_shape(const MSDF_Shape *shape, MSDF_Vector2 point)
{
    double winding = shape_winding_number(shape, point);
    return fabs(winding) > 0.5;  /* Non-zero winding rule */
}

/* Helper: solve quadratic equation ax^2 + bx + c = 0, return number of real roots */
static int solve_quadratic(double a, double b, double c, double roots[2])
{
    if (fabs(a) < MSDF_EPSILON) {
        /* Linear equation */
        if (fabs(b) < MSDF_EPSILON) {
            return 0;
        }
        roots[0] = -c / b;
        return 1;
    }

    double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0) {
        return 0;
    }

    if (discriminant < MSDF_EPSILON) {
        roots[0] = -b / (2.0 * a);
        return 1;
    }

    double sqrt_d = sqrt(discriminant);
    roots[0] = (-b - sqrt_d) / (2.0 * a);
    roots[1] = (-b + sqrt_d) / (2.0 * a);
    return 2;
}

/* Helper: solve cubic equation t^3 + a*t^2 + b*t + c = 0 (depressed form) */
static int solve_cubic_normalized(double a, double b, double c, double roots[3])
{
    /* Convert to depressed cubic: t = x - a/3 */
    double a2 = a * a;
    double q = (3.0 * b - a2) / 9.0;
    double r = (9.0 * a * b - 27.0 * c - 2.0 * a2 * a) / 54.0;
    double q3 = q * q * q;
    double d = q3 + r * r;

    double a_over_3 = a / 3.0;

    if (d >= 0) {
        /* One or two real roots */
        double sqrt_d = sqrt(d);
        double s = cbrt(r + sqrt_d);
        double t = cbrt(r - sqrt_d);

        roots[0] = s + t - a_over_3;

        if (fabs(d) < MSDF_EPSILON) {
            /* Two real roots (one is repeated) */
            roots[1] = -0.5 * (s + t) - a_over_3;
            return 2;
        }
        return 1;
    }

    /* Three real roots */
    double theta = acos(r / sqrt(-q3));
    double sqrt_q = 2.0 * sqrt(-q);

    roots[0] = sqrt_q * cos(theta / 3.0) - a_over_3;
    roots[1] = sqrt_q * cos((theta + 2.0 * M_PI) / 3.0) - a_over_3;
    roots[2] = sqrt_q * cos((theta + 4.0 * M_PI) / 3.0) - a_over_3;
    return 3;
}

/*
 * Edge distance functions now return UNSIGNED distances along with the
 * edge-local sign (pseudo-sign). The actual inside/outside determination
 * is done globally using the winding number.
 *
 * The 'pseudo_sign' output indicates which side of the edge the point is on:
 * +1 = left side of edge direction (typically outside for CCW contours)
 * -1 = right side of edge direction (typically inside for CCW contours)
 */

/* Signed distance to linear segment */
static MSDF_SignedDistance linear_signed_distance(const MSDF_EdgeSegment *edge,
                                                    MSDF_Vector2 point,
                                                    double *out_param)
{
    MSDF_Vector2 p0 = edge->p[0];
    MSDF_Vector2 p1 = edge->p[1];

    MSDF_Vector2 aq = msdf_vec2_sub(point, p0);
    MSDF_Vector2 ab = msdf_vec2_sub(p1, p0);

    double ab_len_sq = msdf_vec2_dot(ab, ab);
    if (ab_len_sq < MSDF_EPSILON) {
        /* Degenerate edge */
        if (out_param) *out_param = 0;
        return (MSDF_SignedDistance){ msdf_vec2_length(aq), 0 };
    }

    double param = msdf_vec2_dot(aq, ab) / ab_len_sq;

    /* Clamp to segment */
    if (param < 0) param = 0;
    else if (param > 1) param = 1;

    MSDF_Vector2 closest = msdf_vec2_add(p0, msdf_vec2_mul(ab, param));
    MSDF_Vector2 to_point = msdf_vec2_sub(point, closest);

    double distance = msdf_vec2_length(to_point);

    /*
     * Pseudo-sign based on which side of the edge the point is on.
     * cross(edge_direction, to_point) > 0 means point is to the LEFT of the edge.
     *
     * TrueType coordinate convention (stb_truetype outputs Y-up):
     * - Outer contours wind COUNTER-CLOCKWISE in Y-up
     * - Inner contours (holes) wind CLOCKWISE in Y-up
     *
     * For CCW outer contour: LEFT of edge = INSIDE = negative distance
     * Standard SDF convention: negative = inside, positive = outside
     * So: cross > 0 (left) → INSIDE → negative distance
     */
    double cross = msdf_vec2_cross(ab, to_point);
    double pseudo_sign = (cross > 0) ? -1.0 : 1.0;

    /* Dot product for disambiguation (orthogonality) */
    double dot;
    if (distance > MSDF_EPSILON) {
        MSDF_Vector2 dir = msdf_vec2_normalize(ab);
        MSDF_Vector2 to_point_norm = msdf_vec2_normalize(to_point);
        dot = fabs(msdf_vec2_dot(dir, to_point_norm));
    } else {
        dot = 0;
    }

    if (out_param) *out_param = param;

    /* Return distance with pseudo-sign applied */
    return (MSDF_SignedDistance){ pseudo_sign * distance, dot };
}

/* Signed distance to quadratic bezier */
static MSDF_SignedDistance quadratic_signed_distance(const MSDF_EdgeSegment *edge,
                                                       MSDF_Vector2 point,
                                                       double *out_param)
{
    MSDF_Vector2 p0 = edge->p[0];
    MSDF_Vector2 p1 = edge->p[1];
    MSDF_Vector2 p2 = edge->p[2];

    /* Coefficients matching msdfgen exactly:
     * qa = p0 - origin (NOT origin - p0!)
     * ab = p1 - p0
     * br = p2 - p1 - ab = p2 - 2*p1 + p0 (NOT just p2 - p1!)
     *
     * The cubic equation at³ + bt² + ct + d = 0 finds where
     * the derivative of |B(t) - origin|² equals zero.
     */
    MSDF_Vector2 qa = msdf_vec2_sub(p0, point);  /* qa = p0 - origin */
    MSDF_Vector2 ab = msdf_vec2_sub(p1, p0);
    MSDF_Vector2 br = msdf_vec2_sub(msdf_vec2_sub(p2, p1), ab);  /* br = (p2-p1) - (p1-p0) */

    double a = msdf_vec2_dot(br, br);
    double b = 3.0 * msdf_vec2_dot(ab, br);
    double c = 2.0 * msdf_vec2_dot(ab, ab) + msdf_vec2_dot(qa, br);
    double d = msdf_vec2_dot(qa, ab);

    /* Solve cubic for parameter t */
    double roots[3];
    int num_roots = 0;

    if (fabs(a) > MSDF_EPSILON) {
        /* Normalize to t^3 + ... form */
        num_roots = solve_cubic_normalized(b / a, c / a, d / a, roots);
    } else if (fabs(b) > MSDF_EPSILON) {
        /* Quadratic */
        num_roots = solve_quadratic(b, c, d, roots);
    } else if (fabs(c) > MSDF_EPSILON) {
        /* Linear */
        roots[0] = -d / c;
        num_roots = 1;
    }

    /*
     * msdfgen approach: compute signed distance for each candidate point
     * (endpoints and roots), keeping track of the minimum absolute distance
     * while preserving the correct sign from the tangent direction.
     */

    /* Helper function behavior: nonZeroSign returns +1 if > 0, else -1 */
    #define nonZeroSign(x) (((x) > 0) ? 1.0 : -1.0)

    /* Start with endpoint at t=0 */
    MSDF_Vector2 epDir = msdf_edge_direction_at(edge, 0);
    double minDistance = nonZeroSign(msdf_vec2_cross(epDir, qa)) * msdf_vec2_length(qa);
    double param = -msdf_vec2_dot(qa, epDir) / msdf_vec2_dot(epDir, epDir);

    /* Check endpoint at t=1 */
    {
        MSDF_Vector2 qc = msdf_vec2_sub(p2, point);  /* p2 - origin */
        double distance = msdf_vec2_length(qc);
        if (distance < fabs(minDistance)) {
            epDir = msdf_edge_direction_at(edge, 1);
            minDistance = nonZeroSign(msdf_vec2_cross(epDir, qc)) * distance;
            MSDF_Vector2 origin_minus_p1 = msdf_vec2_sub(point, p1);
            param = msdf_vec2_dot(origin_minus_p1, epDir) / msdf_vec2_dot(epDir, epDir);
        }
    }

    /* Check roots in (0, 1) */
    for (int i = 0; i < num_roots; i++) {
        double t = roots[i];
        if (t > 0 && t < 1) {
            /* qe = qa + 2*t*ab + t²*br = B(t) - origin (using msdfgen's qa convention) */
            MSDF_Vector2 qe = msdf_vec2_add(
                msdf_vec2_add(qa, msdf_vec2_mul(ab, 2.0 * t)),
                msdf_vec2_mul(br, t * t)
            );
            double distance = msdf_vec2_length(qe);
            if (distance <= fabs(minDistance)) {
                /* Tangent at t: direction = 2*(ab + t*br) */
                MSDF_Vector2 tangent = msdf_vec2_add(ab, msdf_vec2_mul(br, t));
                minDistance = nonZeroSign(msdf_vec2_cross(tangent, qe)) * distance;
                param = t;
            }
        }
    }

    #undef nonZeroSign

    /* Compute dot product for disambiguation */
    double dot;
    if (param >= 0 && param <= 1) {
        dot = 0;  /* Perpendicular to edge - best case */
    } else if (param < 0.5) {
        MSDF_Vector2 dir0 = msdf_vec2_normalize(msdf_edge_direction_at(edge, 0));
        MSDF_Vector2 qa_norm = msdf_vec2_normalize(qa);
        dot = fabs(msdf_vec2_dot(dir0, qa_norm));
    } else {
        MSDF_Vector2 dir1 = msdf_vec2_normalize(msdf_edge_direction_at(edge, 1));
        MSDF_Vector2 qc = msdf_vec2_sub(p2, point);
        MSDF_Vector2 qc_norm = msdf_vec2_normalize(qc);
        dot = fabs(msdf_vec2_dot(dir1, qc_norm));
    }

    if (out_param) *out_param = param;

    return (MSDF_SignedDistance){ minDistance, dot };
}

/* Signed distance to cubic bezier (msdfgen-style Newton-Raphson with second derivative) */
static MSDF_SignedDistance cubic_signed_distance(const MSDF_EdgeSegment *edge,
                                                   MSDF_Vector2 point,
                                                   double *out_param)
{
    /*
     * Cubic Bezier coefficients (matching msdfgen exactly):
     * B(t) = p0 + 3t*ab + 3t²*br + t³*as
     * where:
     *   ab = p1 - p0
     *   br = p2 - p1 - ab = p2 - 2*p1 + p0
     *   as = (p3 - p2) - (p2 - p1) - br = p3 - 3*p2 + 3*p1 - p0
     *
     * Derivatives:
     *   B'(t)  = 3*ab + 6t*br + 3t²*as
     *   B''(t) = 6*br + 6t*as
     */
    MSDF_Vector2 p0 = edge->p[0];
    MSDF_Vector2 p1 = edge->p[1];
    MSDF_Vector2 p2 = edge->p[2];
    MSDF_Vector2 p3 = edge->p[3];

    MSDF_Vector2 qa = msdf_vec2_sub(p0, point);  /* qa = p0 - origin */
    MSDF_Vector2 ab = msdf_vec2_sub(p1, p0);
    MSDF_Vector2 br = msdf_vec2_sub(msdf_vec2_sub(p2, p1), ab);
    MSDF_Vector2 as_vec = msdf_vec2_sub(msdf_vec2_sub(msdf_vec2_sub(p3, p2), msdf_vec2_sub(p2, p1)), br);

    #define nonZeroSign(x) (((x) > 0) ? 1.0 : -1.0)

    /* Start with endpoint at t=0 */
    MSDF_Vector2 epDir = msdf_edge_direction_at(edge, 0);
    double minDistance = nonZeroSign(msdf_vec2_cross(epDir, qa)) * msdf_vec2_length(qa);
    double param = -msdf_vec2_dot(qa, epDir) / msdf_vec2_dot(epDir, epDir);

    /* Check endpoint at t=1 */
    {
        MSDF_Vector2 qc = msdf_vec2_sub(p3, point);
        double distance = msdf_vec2_length(qc);
        if (distance < fabs(minDistance)) {
            epDir = msdf_edge_direction_at(edge, 1);
            minDistance = nonZeroSign(msdf_vec2_cross(epDir, qc)) * distance;
            /* param calculation for endpoint at t=1 */
            MSDF_Vector2 ep_diff = msdf_vec2_sub(epDir, qc);
            param = msdf_vec2_dot(ep_diff, epDir) / msdf_vec2_dot(epDir, epDir);
        }
    }

    /* Iterative search from multiple starting points with improved Newton's method */
    const int SEARCH_STARTS = MSDF_CUBIC_SAMPLES;
    const int SEARCH_STEPS = MSDF_CUBIC_SEARCH_ITERATIONS;

    for (int i = 0; i <= SEARCH_STARTS; i++) {
        double t = (double)i / SEARCH_STARTS;

        /* qe = B(t) - origin = qa + 3t*ab + 3t²*br + t³*as */
        MSDF_Vector2 qe = msdf_vec2_add(
            msdf_vec2_add(
                msdf_vec2_add(qa, msdf_vec2_mul(ab, 3.0 * t)),
                msdf_vec2_mul(br, 3.0 * t * t)
            ),
            msdf_vec2_mul(as_vec, t * t * t)
        );

        /* d1 = B'(t) = 3*ab + 6t*br + 3t²*as */
        MSDF_Vector2 d1 = msdf_vec2_add(
            msdf_vec2_add(
                msdf_vec2_mul(ab, 3.0),
                msdf_vec2_mul(br, 6.0 * t)
            ),
            msdf_vec2_mul(as_vec, 3.0 * t * t)
        );

        /* d2 = B''(t) = 6*br + 6t*as */
        MSDF_Vector2 d2 = msdf_vec2_add(
            msdf_vec2_mul(br, 6.0),
            msdf_vec2_mul(as_vec, 6.0 * t)
        );

        /* Improved Newton's method: t -= dot(qe, d1) / (dot(d1, d1) + dot(qe, d2)) */
        double denom = msdf_vec2_dot(d1, d1) + msdf_vec2_dot(qe, d2);
        if (fabs(denom) < MSDF_EPSILON) continue;

        double improvedT = t - msdf_vec2_dot(qe, d1) / denom;

        if (improvedT > 0 && improvedT < 1) {
            int remainingSteps = SEARCH_STEPS;
            do {
                t = improvedT;

                /* Recompute qe, d1 at new t */
                qe = msdf_vec2_add(
                    msdf_vec2_add(
                        msdf_vec2_add(qa, msdf_vec2_mul(ab, 3.0 * t)),
                        msdf_vec2_mul(br, 3.0 * t * t)
                    ),
                    msdf_vec2_mul(as_vec, t * t * t)
                );

                d1 = msdf_vec2_add(
                    msdf_vec2_add(
                        msdf_vec2_mul(ab, 3.0),
                        msdf_vec2_mul(br, 6.0 * t)
                    ),
                    msdf_vec2_mul(as_vec, 3.0 * t * t)
                );

                if (--remainingSteps == 0) break;

                d2 = msdf_vec2_add(
                    msdf_vec2_mul(br, 6.0),
                    msdf_vec2_mul(as_vec, 6.0 * t)
                );

                denom = msdf_vec2_dot(d1, d1) + msdf_vec2_dot(qe, d2);
                if (fabs(denom) < MSDF_EPSILON) break;

                improvedT = t - msdf_vec2_dot(qe, d1) / denom;
            } while (improvedT > 0 && improvedT < 1);

            double distance = msdf_vec2_length(qe);
            if (distance < fabs(minDistance)) {
                minDistance = nonZeroSign(msdf_vec2_cross(d1, qe)) * distance;
                param = t;
            }
        }
    }

    #undef nonZeroSign

    /* Compute dot product for disambiguation */
    double dot;
    if (param >= 0 && param <= 1) {
        dot = 0;  /* Perpendicular to edge - best case */
    } else if (param < 0.5) {
        MSDF_Vector2 dir0 = msdf_vec2_normalize(msdf_edge_direction_at(edge, 0));
        MSDF_Vector2 qa_norm = msdf_vec2_normalize(qa);
        dot = fabs(msdf_vec2_dot(dir0, qa_norm));
    } else {
        MSDF_Vector2 dir1 = msdf_vec2_normalize(msdf_edge_direction_at(edge, 1));
        MSDF_Vector2 qc = msdf_vec2_sub(p3, point);
        MSDF_Vector2 qc_norm = msdf_vec2_normalize(qc);
        dot = fabs(msdf_vec2_dot(dir1, qc_norm));
    }

    if (out_param) *out_param = param;

    return (MSDF_SignedDistance){ minDistance, dot };
}

MSDF_SignedDistance msdf_edge_signed_distance(const MSDF_EdgeSegment *edge,
                                                MSDF_Vector2 point,
                                                double *out_param)
{
    if (!edge) {
        if (out_param) *out_param = 0;
        return (MSDF_SignedDistance){ DBL_MAX, 0 };
    }

    switch (edge->type) {
        case MSDF_EDGE_LINEAR:
            return linear_signed_distance(edge, point, out_param);
        case MSDF_EDGE_QUADRATIC:
            return quadratic_signed_distance(edge, point, out_param);
        case MSDF_EDGE_CUBIC:
            return cubic_signed_distance(edge, point, out_param);
    }

    if (out_param) *out_param = 0;
    return (MSDF_SignedDistance){ DBL_MAX, 0 };
}

/* ============================================================================
 * Bitmap Operations
 * ============================================================================ */

bool msdf_bitmap_alloc(MSDF_Bitmap *bitmap, int width, int height, MSDF_BitmapFormat format)
{
    if (!bitmap || width <= 0 || height <= 0) {
        agentite_set_error("Invalid bitmap parameters");
        return false;
    }

    size_t size = (size_t)width * height * format * sizeof(float);
    bitmap->data = (float *)malloc(size);
    if (!bitmap->data) {
        agentite_set_error("Failed to allocate bitmap");
        return false;
    }

    /* Initialize to zero */
    memset(bitmap->data, 0, size);

    bitmap->width = width;
    bitmap->height = height;
    bitmap->format = format;

    return true;
}

void msdf_bitmap_free(MSDF_Bitmap *bitmap)
{
    if (bitmap && bitmap->data) {
        free(bitmap->data);
        bitmap->data = NULL;
        bitmap->width = 0;
        bitmap->height = 0;
    }
}

float *msdf_bitmap_pixel(MSDF_Bitmap *bitmap, int x, int y)
{
    if (!bitmap || !bitmap->data) return NULL;
    if (x < 0 || x >= bitmap->width || y < 0 || y >= bitmap->height) return NULL;

    return &bitmap->data[(y * bitmap->width + x) * bitmap->format];
}

const float *msdf_bitmap_pixel_const(const MSDF_Bitmap *bitmap, int x, int y)
{
    if (!bitmap || !bitmap->data) return NULL;
    if (x < 0 || x >= bitmap->width || y < 0 || y >= bitmap->height) return NULL;

    return &bitmap->data[(y * bitmap->width + x) * bitmap->format];
}
