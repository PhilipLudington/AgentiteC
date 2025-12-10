/*
 * Carbon MSDF Generator
 *
 * Pure C11 implementation of Multi-channel Signed Distance Field generation
 * for runtime font atlas creation. Based on the msdfgen algorithm by Viktor
 * Chlumsky, reimplemented without C++ dependencies.
 *
 * Usage:
 *   // Load font and extract glyph shape
 *   MSDF_Shape *shape = msdf_shape_from_glyph(&stb_font, glyph_index, scale);
 *
 *   // Assign colors to edges
 *   msdf_edge_coloring_simple(shape, 3.0);
 *
 *   // Generate MSDF bitmap
 *   MSDF_Bitmap bitmap;
 *   msdf_bitmap_alloc(&bitmap, 32, 32, MSDF_BITMAP_RGB);
 *   msdf_generate(shape, &bitmap, 4.0, translate_x, translate_y, scale);
 *
 *   // Use the bitmap data...
 *   msdf_bitmap_free(&bitmap);
 *   msdf_shape_free(shape);
 *
 * Reference: https://github.com/Chlumsky/msdfgen
 */

#ifndef AGENTITE_MSDF_H
#define AGENTITE_MSDF_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/* Default angle threshold for corner detection (radians) */
#define MSDF_DEFAULT_ANGLE_THRESHOLD 3.0

/* Default pixel range for distance field */
#define MSDF_DEFAULT_PIXEL_RANGE 4.0

/* Numerical precision epsilon */
#define MSDF_EPSILON 1e-14

/* Maximum iterations for cubic root finding */
#define MSDF_CUBIC_SEARCH_ITERATIONS 8

/* ============================================================================
 * Core Types
 * ============================================================================ */

/* 2D point/vector with double precision for accuracy */
typedef struct MSDF_Vector2 {
    double x;
    double y;
} MSDF_Vector2;

/* Edge segment types */
typedef enum MSDF_EdgeType {
    MSDF_EDGE_LINEAR    = 1,    /* Line segment: 2 control points */
    MSDF_EDGE_QUADRATIC = 2,    /* Quadratic bezier: 3 control points */
    MSDF_EDGE_CUBIC     = 3     /* Cubic bezier: 4 control points */
} MSDF_EdgeType;

/* Edge color channels (bitmask) */
typedef enum MSDF_EdgeColor {
    MSDF_COLOR_BLACK   = 0,
    MSDF_COLOR_RED     = 1,
    MSDF_COLOR_GREEN   = 2,
    MSDF_COLOR_YELLOW  = 3,     /* RED | GREEN */
    MSDF_COLOR_BLUE    = 4,
    MSDF_COLOR_MAGENTA = 5,     /* RED | BLUE */
    MSDF_COLOR_CYAN    = 6,     /* GREEN | BLUE */
    MSDF_COLOR_WHITE   = 7      /* RED | GREEN | BLUE */
} MSDF_EdgeColor;

/* Signed distance with direction for disambiguation */
typedef struct MSDF_SignedDistance {
    double distance;    /* Signed distance (negative = inside) */
    double dot;         /* Dot product with edge direction (for tie-breaking) */
} MSDF_SignedDistance;

/* Edge segment (union-like structure for all curve types) */
typedef struct MSDF_EdgeSegment {
    MSDF_EdgeType type;
    MSDF_EdgeColor color;
    MSDF_Vector2 p[4];  /* Control points (p[0..1] for linear, [0..2] for quad, [0..3] for cubic) */
} MSDF_EdgeSegment;

/* Contour: a closed path of connected edge segments */
typedef struct MSDF_Contour {
    MSDF_EdgeSegment *edges;
    int edge_count;
    int edge_capacity;
} MSDF_Contour;

/* Shape: collection of contours forming a glyph */
typedef struct MSDF_Shape {
    MSDF_Contour *contours;
    int contour_count;
    int contour_capacity;
    bool inverse_y_axis;    /* True if Y increases downward */
} MSDF_Shape;

/* Bitmap pixel format */
typedef enum MSDF_BitmapFormat {
    MSDF_BITMAP_GRAY = 1,   /* Single channel (SDF) */
    MSDF_BITMAP_RGB  = 3,   /* Three channel (MSDF) */
    MSDF_BITMAP_RGBA = 4    /* Four channel (MTSDF: MSDF + true SDF in alpha) */
} MSDF_BitmapFormat;

/* Output bitmap */
typedef struct MSDF_Bitmap {
    float *data;            /* Pixel data (row-major, channels interleaved) */
    int width;
    int height;
    MSDF_BitmapFormat format;
} MSDF_Bitmap;

/* Bounding box */
typedef struct MSDF_Bounds {
    double left;
    double bottom;
    double right;
    double top;
} MSDF_Bounds;

/* Projection/transformation for mapping shape to bitmap */
typedef struct MSDF_Projection {
    double scale_x;
    double scale_y;
    double translate_x;
    double translate_y;
} MSDF_Projection;

/* Error correction configuration */
typedef enum MSDF_ErrorCorrectionMode {
    MSDF_ERROR_CORRECTION_DISABLED      = 0,
    MSDF_ERROR_CORRECTION_INDISCRIMINATE = 1,
    MSDF_ERROR_CORRECTION_EDGE_PRIORITY = 2,
    MSDF_ERROR_CORRECTION_EDGE_ONLY     = 3
} MSDF_ErrorCorrectionMode;

typedef struct MSDF_ErrorCorrectionConfig {
    MSDF_ErrorCorrectionMode mode;
    double min_deviation_ratio;
    double min_improve_ratio;
} MSDF_ErrorCorrectionConfig;

/* Generator configuration */
typedef struct MSDF_GeneratorConfig {
    bool overlap_support;               /* Support overlapping contours */
    MSDF_ErrorCorrectionConfig error_correction;
} MSDF_GeneratorConfig;

/* Default configurations */
#define MSDF_ERROR_CORRECTION_DEFAULT { \
    .mode = MSDF_ERROR_CORRECTION_EDGE_PRIORITY, \
    .min_deviation_ratio = 1.11111111111111111, \
    .min_improve_ratio = 1.11111111111111111 \
}

#define MSDF_GENERATOR_CONFIG_DEFAULT { \
    .overlap_support = true, \
    .error_correction = MSDF_ERROR_CORRECTION_DEFAULT \
}

/* ============================================================================
 * Shape Construction
 * ============================================================================ */

/* Create an empty shape */
MSDF_Shape *msdf_shape_create(void);

/* Free shape and all contained data */
void msdf_shape_free(MSDF_Shape *shape);

/* Add a new contour to the shape, returns pointer to the contour */
MSDF_Contour *msdf_shape_add_contour(MSDF_Shape *shape);

/* Add edge segment to a contour */
void msdf_contour_add_edge(MSDF_Contour *contour, const MSDF_EdgeSegment *edge);

/* Convenience: add linear edge */
void msdf_contour_add_line(MSDF_Contour *contour, MSDF_Vector2 p0, MSDF_Vector2 p1);

/* Convenience: add quadratic bezier edge */
void msdf_contour_add_quadratic(MSDF_Contour *contour,
                                 MSDF_Vector2 p0, MSDF_Vector2 p1, MSDF_Vector2 p2);

/* Convenience: add cubic bezier edge */
void msdf_contour_add_cubic(MSDF_Contour *contour,
                             MSDF_Vector2 p0, MSDF_Vector2 p1,
                             MSDF_Vector2 p2, MSDF_Vector2 p3);

/* ============================================================================
 * Shape Extraction from stb_truetype
 * ============================================================================ */

/* Forward declaration - actual stbtt_fontinfo is defined in stb_truetype.h */
struct stbtt_fontinfo;

/**
 * Extract shape from a glyph using stb_truetype.
 *
 * @param font_info  Initialized stbtt_fontinfo
 * @param glyph_index  Glyph index (use stbtt_FindGlyphIndex for codepoint)
 * @param scale  Scale factor (use stbtt_ScaleForPixelHeight result)
 * @return  New shape, or NULL on failure. Caller must free with msdf_shape_free().
 */
MSDF_Shape *msdf_shape_from_glyph(const struct stbtt_fontinfo *font_info,
                                   int glyph_index,
                                   double scale);

/**
 * Extract shape from a codepoint.
 * Convenience wrapper around msdf_shape_from_glyph.
 */
MSDF_Shape *msdf_shape_from_codepoint(const struct stbtt_fontinfo *font_info,
                                       int codepoint,
                                       double scale);

/* ============================================================================
 * Shape Utilities
 * ============================================================================ */

/* Calculate tight bounding box of shape */
MSDF_Bounds msdf_shape_get_bounds(const MSDF_Shape *shape);

/* Get total edge count across all contours */
int msdf_shape_edge_count(const MSDF_Shape *shape);

/* Check if shape is empty (no edges) */
bool msdf_shape_is_empty(const MSDF_Shape *shape);

/* Normalize shape coordinates to fit in unit square with margin */
void msdf_shape_normalize(MSDF_Shape *shape);

/* Calculate winding number of a contour (positive = CCW, negative = CW) */
int msdf_contour_winding(const MSDF_Contour *contour);

/* Reverse a contour's direction */
void msdf_contour_reverse(MSDF_Contour *contour);

/* ============================================================================
 * Edge Coloring
 * ============================================================================ */

/**
 * Simple edge coloring based on corner angles.
 *
 * @param shape  Shape to color (modified in place)
 * @param angle_threshold  Angle threshold for corner detection (radians, default 3.0)
 * @param seed  Random seed for consistent coloring (0 for default)
 */
void msdf_edge_coloring_simple(MSDF_Shape *shape, double angle_threshold, uint64_t seed);

/**
 * Ink trap edge coloring (better for display typefaces).
 *
 * @param shape  Shape to color (modified in place)
 * @param angle_threshold  Angle threshold for corner detection (radians)
 * @param seed  Random seed for consistent coloring
 */
void msdf_edge_coloring_ink_trap(MSDF_Shape *shape, double angle_threshold, uint64_t seed);

/**
 * Distance-based edge coloring (highest quality, slower).
 * Tries to maximize color separation for nearby edges.
 *
 * @param shape  Shape to color (modified in place)
 * @param angle_threshold  Angle threshold for corner detection (radians)
 * @param seed  Random seed for consistent coloring
 */
void msdf_edge_coloring_by_distance(MSDF_Shape *shape, double angle_threshold, uint64_t seed);

/* ============================================================================
 * Edge Segment Math
 * ============================================================================ */

/* Evaluate point on edge at parameter t (0 to 1) */
MSDF_Vector2 msdf_edge_point_at(const MSDF_EdgeSegment *edge, double t);

/* Evaluate tangent direction on edge at parameter t */
MSDF_Vector2 msdf_edge_direction_at(const MSDF_EdgeSegment *edge, double t);

/* Calculate signed distance from point to edge */
MSDF_SignedDistance msdf_edge_signed_distance(const MSDF_EdgeSegment *edge,
                                                MSDF_Vector2 point,
                                                double *out_param);

/* Get bounding box of edge segment */
MSDF_Bounds msdf_edge_get_bounds(const MSDF_EdgeSegment *edge);

/* ============================================================================
 * Bitmap Operations
 * ============================================================================ */

/* Allocate bitmap with given dimensions and format */
bool msdf_bitmap_alloc(MSDF_Bitmap *bitmap, int width, int height, MSDF_BitmapFormat format);

/* Free bitmap data */
void msdf_bitmap_free(MSDF_Bitmap *bitmap);

/* Get pixel pointer at (x, y) */
float *msdf_bitmap_pixel(MSDF_Bitmap *bitmap, int x, int y);

/* Get pixel pointer (const version) */
const float *msdf_bitmap_pixel_const(const MSDF_Bitmap *bitmap, int x, int y);

/* ============================================================================
 * MSDF Generation
 * ============================================================================ */

/**
 * Generate single-channel signed distance field.
 *
 * @param shape  Input shape (must be colored)
 * @param bitmap  Output bitmap (must be allocated, format MSDF_BITMAP_GRAY)
 * @param projection  Coordinate transformation
 * @param pixel_range  Distance field range in pixels
 */
void msdf_generate_sdf(const MSDF_Shape *shape,
                        MSDF_Bitmap *bitmap,
                        const MSDF_Projection *projection,
                        double pixel_range);

/**
 * Generate multi-channel signed distance field.
 *
 * @param shape  Input shape (must be colored)
 * @param bitmap  Output bitmap (must be allocated, format MSDF_BITMAP_RGB)
 * @param projection  Coordinate transformation
 * @param pixel_range  Distance field range in pixels
 */
void msdf_generate_msdf(const MSDF_Shape *shape,
                         MSDF_Bitmap *bitmap,
                         const MSDF_Projection *projection,
                         double pixel_range);

/**
 * Generate multi-channel + true SDF (MTSDF).
 * RGB channels contain MSDF, alpha contains true SDF.
 *
 * @param shape  Input shape (must be colored)
 * @param bitmap  Output bitmap (must be allocated, format MSDF_BITMAP_RGBA)
 * @param projection  Coordinate transformation
 * @param pixel_range  Distance field range in pixels
 */
void msdf_generate_mtsdf(const MSDF_Shape *shape,
                          MSDF_Bitmap *bitmap,
                          const MSDF_Projection *projection,
                          double pixel_range);

/**
 * Generate MSDF with full configuration.
 *
 * @param shape  Input shape (must be colored)
 * @param bitmap  Output bitmap (must be allocated)
 * @param projection  Coordinate transformation
 * @param pixel_range  Distance field range in pixels
 * @param config  Generator configuration
 */
void msdf_generate_ex(const MSDF_Shape *shape,
                       MSDF_Bitmap *bitmap,
                       const MSDF_Projection *projection,
                       double pixel_range,
                       const MSDF_GeneratorConfig *config);

/* ============================================================================
 * Error Correction
 * ============================================================================ */

/**
 * Apply error correction to generated MSDF.
 * Fixes artifacts at channel discontinuities.
 *
 * @param bitmap  MSDF bitmap to correct (modified in place)
 * @param shape  Original shape (may be NULL for fast correction modes)
 * @param projection  Projection used during generation
 * @param pixel_range  Pixel range used during generation
 * @param config  Error correction configuration
 */
void msdf_error_correction(MSDF_Bitmap *bitmap,
                            const MSDF_Shape *shape,
                            const MSDF_Projection *projection,
                            double pixel_range,
                            const MSDF_ErrorCorrectionConfig *config);

/* ============================================================================
 * Atlas Generation
 * ============================================================================ */

/* Opaque atlas handle */
typedef struct MSDF_Atlas MSDF_Atlas;

/* Atlas configuration */
typedef struct MSDF_AtlasConfig {
    const void *font_data;      /* TTF font data */
    int font_data_size;         /* Size of font data */
    bool copy_font_data;        /* If true, atlas copies font data internally */

    int atlas_width;            /* Atlas texture width (default: 1024) */
    int atlas_height;           /* Atlas texture height (default: 1024) */
    float glyph_scale;          /* Glyph rendering size in pixels (default: 48) */
    float pixel_range;          /* SDF range in pixels (default: 4) */
    int padding;                /* Padding between glyphs (default: 2) */
    MSDF_BitmapFormat format;   /* Output format (default: RGB) */
} MSDF_AtlasConfig;

/* Glyph info for rendering */
typedef struct MSDF_GlyphInfo {
    uint32_t codepoint;
    float advance;              /* Horizontal advance (em units) */

    /* Glyph quad bounds relative to baseline (em units) */
    float plane_left, plane_bottom;
    float plane_right, plane_top;

    /* Atlas UV coordinates (normalized 0-1) */
    float atlas_left, atlas_bottom;
    float atlas_right, atlas_top;
} MSDF_GlyphInfo;

/* Font metrics */
typedef struct MSDF_FontMetrics {
    float em_size;
    float ascender;
    float descender;
    float line_height;
    int atlas_width;
    int atlas_height;
} MSDF_FontMetrics;

/* Default configuration */
#define MSDF_ATLAS_CONFIG_DEFAULT { \
    .font_data = NULL, \
    .font_data_size = 0, \
    .copy_font_data = true, \
    .atlas_width = 1024, \
    .atlas_height = 1024, \
    .glyph_scale = 48.0f, \
    .pixel_range = 4.0f, \
    .padding = 2, \
    .format = MSDF_BITMAP_RGB \
}

/**
 * Create an atlas generator from font data.
 *
 * @param config  Atlas configuration
 * @return  New atlas, or NULL on failure. Caller must free with msdf_atlas_destroy().
 */
MSDF_Atlas *msdf_atlas_create(const MSDF_AtlasConfig *config);

/**
 * Destroy atlas and free all resources.
 */
void msdf_atlas_destroy(MSDF_Atlas *atlas);

/**
 * Add a single codepoint to the atlas.
 *
 * @param atlas  Atlas handle
 * @param codepoint  Unicode codepoint to add
 * @return  true if successful
 */
bool msdf_atlas_add_codepoint(MSDF_Atlas *atlas, uint32_t codepoint);

/**
 * Add ASCII printable characters (32-126) to the atlas.
 */
bool msdf_atlas_add_ascii(MSDF_Atlas *atlas);

/**
 * Add a range of codepoints to the atlas.
 *
 * @param atlas  Atlas handle
 * @param first  First codepoint (inclusive)
 * @param last   Last codepoint (inclusive)
 */
bool msdf_atlas_add_range(MSDF_Atlas *atlas, uint32_t first, uint32_t last);

/**
 * Add all characters from a string to the atlas.
 * Currently supports ASCII; extend for UTF-8 if needed.
 */
bool msdf_atlas_add_string(MSDF_Atlas *atlas, const char *str);

/**
 * Generate the atlas bitmap.
 * Must be called after adding all desired glyphs.
 *
 * @param atlas  Atlas handle
 * @return  true if successful
 */
bool msdf_atlas_generate(MSDF_Atlas *atlas);

/**
 * Get glyph information for rendering.
 *
 * @param atlas  Atlas handle
 * @param codepoint  Unicode codepoint
 * @param out_info  Output glyph info
 * @return  true if glyph exists
 */
bool msdf_atlas_get_glyph(const MSDF_Atlas *atlas, uint32_t codepoint,
                          MSDF_GlyphInfo *out_info);

/**
 * Get number of glyphs in atlas.
 */
int msdf_atlas_get_glyph_count(const MSDF_Atlas *atlas);

/**
 * Get the generated atlas bitmap.
 * Returns NULL if atlas hasn't been generated yet.
 */
const MSDF_Bitmap *msdf_atlas_get_bitmap(const MSDF_Atlas *atlas);

/**
 * Get font metrics.
 */
void msdf_atlas_get_metrics(const MSDF_Atlas *atlas, MSDF_FontMetrics *out_metrics);

/**
 * Export atlas bitmap to RGBA8 format for GPU upload.
 *
 * @param atlas  Atlas handle
 * @param out_data  Output buffer (must be atlas_width * atlas_height * 4 bytes)
 * @return  true if successful
 */
bool msdf_atlas_get_bitmap_rgba8(const MSDF_Atlas *atlas, unsigned char *out_data);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* Vector operations */
static inline MSDF_Vector2 msdf_vec2(double x, double y) {
    return (MSDF_Vector2){ x, y };
}

static inline MSDF_Vector2 msdf_vec2_add(MSDF_Vector2 a, MSDF_Vector2 b) {
    return (MSDF_Vector2){ a.x + b.x, a.y + b.y };
}

static inline MSDF_Vector2 msdf_vec2_sub(MSDF_Vector2 a, MSDF_Vector2 b) {
    return (MSDF_Vector2){ a.x - b.x, a.y - b.y };
}

static inline MSDF_Vector2 msdf_vec2_mul(MSDF_Vector2 v, double s) {
    return (MSDF_Vector2){ v.x * s, v.y * s };
}

static inline double msdf_vec2_dot(MSDF_Vector2 a, MSDF_Vector2 b) {
    return a.x * b.x + a.y * b.y;
}

static inline double msdf_vec2_cross(MSDF_Vector2 a, MSDF_Vector2 b) {
    return a.x * b.y - a.y * b.x;
}

static inline double msdf_vec2_length(MSDF_Vector2 v) {
    return sqrt(v.x * v.x + v.y * v.y);
}

static inline double msdf_vec2_length_squared(MSDF_Vector2 v) {
    return v.x * v.x + v.y * v.y;
}

static inline MSDF_Vector2 msdf_vec2_normalize(MSDF_Vector2 v) {
    double len = msdf_vec2_length(v);
    if (len > 0) {
        return msdf_vec2_mul(v, 1.0 / len);
    }
    return v;
}

/* Signed distance comparison (closer to zero = closer to edge) */
static inline bool msdf_distance_less(MSDF_SignedDistance a, MSDF_SignedDistance b) {
    double abs_a = a.distance < 0 ? -a.distance : a.distance;
    double abs_b = b.distance < 0 ? -b.distance : b.distance;
    if (abs_a != abs_b) return abs_a < abs_b;
    return a.dot < b.dot;
}

/* Create projection for glyph-to-bitmap mapping */
static inline MSDF_Projection msdf_projection_from_bounds(MSDF_Bounds bounds,
                                                           int bitmap_width,
                                                           int bitmap_height,
                                                           double padding) {
    double shape_w = bounds.right - bounds.left;
    double shape_h = bounds.top - bounds.bottom;
    double scale_x = (bitmap_width - 2 * padding) / shape_w;
    double scale_y = (bitmap_height - 2 * padding) / shape_h;
    double scale = (scale_x < scale_y) ? scale_x : scale_y;

    return (MSDF_Projection){
        .scale_x = scale,
        .scale_y = scale,
        .translate_x = padding - bounds.left * scale,
        .translate_y = padding - bounds.bottom * scale
    };
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_MSDF_H */
