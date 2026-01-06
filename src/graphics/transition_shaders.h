/*
 * Agentite Transition System Embedded Shaders
 *
 * MSL (Metal Shading Language) shaders for scene transition effects.
 * These are used on macOS/iOS. SPIR-V versions are loaded from files for Vulkan.
 *
 * All transition shaders use two textures:
 *   - texture(0): source scene (outgoing)
 *   - texture(1): dest scene (incoming)
 */

#ifndef AGENTITE_TRANSITION_SHADERS_H
#define AGENTITE_TRANSITION_SHADERS_H

/* ============================================================================
 * Shared Vertex Shader
 *
 * Same fullscreen vertex shader as builtins, with projection matrix support.
 * ============================================================================ */

static const char *transition_vertex_msl = R"(
#include <metal_stdlib>
using namespace metal;

struct Uniforms {
    float4x4 projection;
};

struct VertexIn {
    float2 position [[attribute(0)]];
    float2 texcoord [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 texcoord;
};

vertex VertexOut transition_vertex(
    VertexIn in [[stage_in]],
    constant Uniforms &uniforms [[buffer(0)]])
{
    VertexOut out;
    out.position = uniforms.projection * float4(in.position, 0.0, 1.0);
    out.texcoord = in.texcoord;
    return out;
}
)";

/* ============================================================================
 * Crossfade Transition
 *
 * Simple linear blend between source and dest textures.
 * Parameters: progress (0-1), softness (unused for now)
 * ============================================================================ */

static const char *transition_crossfade_msl = R"(
struct CrossfadeParams {
    float progress;
    float softness;
    float2 _pad;
};

fragment float4 crossfade_fragment(
    VertexOut in [[stage_in]],
    texture2d<float> source_tex [[texture(0)]],
    texture2d<float> dest_tex [[texture(1)]],
    sampler samp [[sampler(0)]],
    constant CrossfadeParams& params [[buffer(0)]])
{
    float4 source = source_tex.sample(samp, in.texcoord);
    float4 dest = dest_tex.sample(samp, in.texcoord);
    return mix(source, dest, params.progress);
}
)";

/* ============================================================================
 * Wipe Transition
 *
 * Directional wipe with soft edge.
 * Parameters: progress, direction (0-4), softness
 * Direction: 0=left, 1=right, 2=up, 3=down, 4=diagonal
 * ============================================================================ */

static const char *transition_wipe_msl = R"(
struct WipeParams {
    float progress;
    float direction;
    float softness;
    float _pad;
};

fragment float4 wipe_fragment(
    VertexOut in [[stage_in]],
    texture2d<float> source_tex [[texture(0)]],
    texture2d<float> dest_tex [[texture(1)]],
    sampler samp [[sampler(0)]],
    constant WipeParams& params [[buffer(0)]])
{
    float4 source = source_tex.sample(samp, in.texcoord);
    float4 dest = dest_tex.sample(samp, in.texcoord);

    float edge;
    if (params.direction < 0.5) {
        // Wipe left (from right to left)
        edge = 1.0 - in.texcoord.x;
    } else if (params.direction < 1.5) {
        // Wipe right (from left to right)
        edge = in.texcoord.x;
    } else if (params.direction < 2.5) {
        // Wipe up (from bottom to top)
        edge = 1.0 - in.texcoord.y;
    } else if (params.direction < 3.5) {
        // Wipe down (from top to bottom)
        edge = in.texcoord.y;
    } else {
        // Diagonal (top-left to bottom-right)
        edge = (in.texcoord.x + in.texcoord.y) * 0.5;
    }

    // Apply progress with softness for smooth edge
    float adjusted_progress = params.progress * (1.0 + params.softness);
    float mask = smoothstep(adjusted_progress - params.softness, adjusted_progress, edge);

    return mix(dest, source, mask);
}
)";

/* ============================================================================
 * Circle Transition (Iris)
 *
 * Circular reveal/hide from center point.
 * Parameters: progress, center_x, center_y, is_open
 * is_open: 0=close (to black), 1=open (from black to dest)
 * ============================================================================ */

static const char *transition_circle_msl = R"(
struct CircleParams {
    float progress;
    float center_x;
    float center_y;
    float is_open;
};

fragment float4 circle_fragment(
    VertexOut in [[stage_in]],
    texture2d<float> source_tex [[texture(0)]],
    texture2d<float> dest_tex [[texture(1)]],
    sampler samp [[sampler(0)]],
    constant CircleParams& params [[buffer(0)]])
{
    float4 source = source_tex.sample(samp, in.texcoord);
    float4 dest = dest_tex.sample(samp, in.texcoord);

    // Calculate distance from center
    float2 center = float2(params.center_x, params.center_y);
    float2 uv = in.texcoord - center;
    float dist = length(uv);

    // Maximum distance is approximately sqrt(2)/2 for centered
    float max_dist = 1.0;

    // Circle radius based on progress
    float radius;
    if (params.is_open > 0.5) {
        // Opening: radius grows from 0 to max
        radius = params.progress * max_dist;
    } else {
        // Closing: radius shrinks from max to 0
        radius = (1.0 - params.progress) * max_dist;
    }

    // Apply smooth edge
    float edge = 0.02;
    float mask = smoothstep(radius - edge, radius + edge, dist);

    if (params.is_open > 0.5) {
        // Opening: show dest inside circle, source outside
        return mix(dest, source, mask);
    } else {
        // Closing: show source inside circle, black outside
        float4 black = float4(0.0, 0.0, 0.0, 1.0);
        return mix(source, black, mask);
    }
}
)";

/* ============================================================================
 * Slide Transition
 *
 * Slide new scene in, optionally pushing old scene out.
 * Parameters: progress, direction (0-3), is_push
 * Direction: 0=left, 1=right, 2=up, 3=down
 * is_push: 0=slide (only new scene moves), 1=push (both scenes move)
 * ============================================================================ */

static const char *transition_slide_msl = R"(
struct SlideParams {
    float progress;
    float direction;
    float is_push;
    float _pad;
};

fragment float4 slide_fragment(
    VertexOut in [[stage_in]],
    texture2d<float> source_tex [[texture(0)]],
    texture2d<float> dest_tex [[texture(1)]],
    sampler samp [[sampler(0)]],
    constant SlideParams& params [[buffer(0)]])
{
    float2 source_uv = in.texcoord;
    float2 dest_uv = in.texcoord;
    bool is_push = params.is_push > 0.5;

    // Calculate UV offsets - both scenes move in the same direction
    // For push: both move together. For slide: only dest moves.
    if (params.direction < 0.5) {
        // LEFT: scenes move left (add to UV to shift image left)
        dest_uv.x = in.texcoord.x + params.progress - 1.0;  // starts at texcoord-1, ends at texcoord
        if (is_push) {
            source_uv.x = in.texcoord.x + params.progress;  // starts at texcoord, ends at texcoord+1
        }
    } else if (params.direction < 1.5) {
        // RIGHT: scenes move right (subtract from UV to shift image right)
        dest_uv.x = in.texcoord.x - params.progress + 1.0;  // starts at texcoord+1, ends at texcoord
        if (is_push) {
            source_uv.x = in.texcoord.x - params.progress;  // starts at texcoord, ends at texcoord-1
        }
    } else if (params.direction < 2.5) {
        // UP: scenes move up
        dest_uv.y = in.texcoord.y + params.progress - 1.0;
        if (is_push) {
            source_uv.y = in.texcoord.y + params.progress;
        }
    } else {
        // DOWN: scenes move down
        dest_uv.y = in.texcoord.y - params.progress + 1.0;
        if (is_push) {
            source_uv.y = in.texcoord.y - params.progress;
        }
    }

    // Check bounds
    bool source_valid = source_uv.x >= 0.0 && source_uv.x <= 1.0 &&
                        source_uv.y >= 0.0 && source_uv.y <= 1.0;
    bool dest_valid = dest_uv.x >= 0.0 && dest_uv.x <= 1.0 &&
                      dest_uv.y >= 0.0 && dest_uv.y <= 1.0;

    // Sample textures (clamp to avoid edge artifacts)
    float4 source = source_tex.sample(samp, clamp(source_uv, float2(0.0), float2(1.0)));
    float4 dest = dest_tex.sample(samp, clamp(dest_uv, float2(0.0), float2(1.0)));

    // Composite: for push, show whichever is valid (they shouldn't overlap)
    // For slide, dest overlays source
    if (dest_valid) {
        return dest;
    } else if (source_valid) {
        return source;
    } else {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
}
)";

/* ============================================================================
 * Dissolve Transition
 *
 * Noise-based dissolve effect.
 * Parameters: progress, edge_width
 * ============================================================================ */

static const char *transition_dissolve_msl = R"(
struct DissolveParams {
    float progress;
    float edge_width;
    float2 _pad;
};

// Simple hash function for procedural noise
float hash(float2 p) {
    return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

// Value noise
float noise(float2 p) {
    float2 i = floor(p);
    float2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // Smooth interpolation

    float a = hash(i);
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

fragment float4 dissolve_fragment(
    VertexOut in [[stage_in]],
    texture2d<float> source_tex [[texture(0)]],
    texture2d<float> dest_tex [[texture(1)]],
    sampler samp [[sampler(0)]],
    constant DissolveParams& params [[buffer(0)]])
{
    float4 source = source_tex.sample(samp, in.texcoord);
    float4 dest = dest_tex.sample(samp, in.texcoord);

    // Generate noise pattern
    float n = noise(in.texcoord * 20.0);

    // Create dissolve mask with edge
    float adjusted_progress = params.progress * (1.0 + params.edge_width);
    float edge = smoothstep(adjusted_progress - params.edge_width, adjusted_progress, n);

    return mix(dest, source, edge);
}
)";

#endif /* AGENTITE_TRANSITION_SHADERS_H */
