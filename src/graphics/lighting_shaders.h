/*
 * Agentite Lighting System Embedded Shaders
 *
 * MSL (Metal Shading Language) shaders for the 2D lighting system.
 * These are used on macOS/iOS. SPIR-V versions can be added for Vulkan.
 *
 * Each shader string contains both vertex and fragment shaders as required by MSL.
 */

#ifndef AGENTITE_LIGHTING_SHADERS_H
#define AGENTITE_LIGHTING_SHADERS_H

/* ============================================================================
 * Point Light Shader (Vertex + Fragment)
 *
 * Renders a radial gradient with configurable falloff for point lights.
 * ============================================================================ */

static const char *point_light_msl = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexInput {
    float2 position [[attribute(0)]];
    float2 texcoord [[attribute(1)]];
};

struct VertexOutput {
    float4 position [[position]];
    float2 texcoord;
};

struct PointLightParams {
    float2 light_center;    // Light center in UV space (0-1)
    float radius;           // Light radius in UV space
    float intensity;        // Light intensity multiplier
    float4 color;           // RGBA color
    float falloff_type;     // 0=linear, 1=quadratic, 2=smooth, 3=none
    float2 aspect;          // Aspect ratio correction (width/height, 1.0)
    float _pad;
};

vertex VertexOutput lighting_vertex(VertexInput in [[stage_in]]) {
    VertexOutput out;
    out.position = float4(in.position, 0.0, 1.0);
    out.texcoord = in.texcoord;
    return out;
}

float apply_falloff(float dist, float falloff_type) {
    if (falloff_type < 0.5) {
        // Linear falloff
        return 1.0 - dist;
    } else if (falloff_type < 1.5) {
        // Quadratic falloff
        return 1.0 / (1.0 + dist * dist * 4.0);
    } else if (falloff_type < 2.5) {
        // Smooth (smoothstep) falloff
        return 1.0 - dist * dist * (3.0 - 2.0 * dist);
    } else {
        // No falloff
        return 1.0;
    }
}

fragment float4 point_light_fragment(
    VertexOutput in [[stage_in]],
    constant PointLightParams& params [[buffer(0)]]
) {
    // Calculate distance from light center with aspect correction
    float2 delta = (in.texcoord - params.light_center) * params.aspect;
    float dist = length(delta) / params.radius;

    // Outside radius = no contribution
    if (dist >= 1.0) {
        return float4(0.0);
    }

    // Apply falloff
    float attenuation = apply_falloff(dist, params.falloff_type);

    // Final color with intensity
    float4 result = params.color;
    result.rgb *= attenuation * params.intensity;
    result.a = attenuation * params.intensity;

    return result;
}
)";

/* ============================================================================
 * Spot Light Shader (Vertex + Fragment)
 *
 * Renders a cone-shaped light with direction, inner/outer angles, and falloff.
 * ============================================================================ */

static const char *spot_light_msl = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexInput {
    float2 position [[attribute(0)]];
    float2 texcoord [[attribute(1)]];
};

struct VertexOutput {
    float4 position [[position]];
    float2 texcoord;
};

struct SpotLightParams {
    float2 light_center;    // Light center in UV space
    float2 direction;       // Normalized direction
    float radius;           // Max distance
    float inner_angle;      // Inner cone angle (cos)
    float outer_angle;      // Outer cone angle (cos)
    float intensity;        // Intensity multiplier
    float4 color;           // RGBA color
    float falloff_type;     // Falloff curve type
    float2 aspect;          // Aspect correction
};

vertex VertexOutput lighting_vertex(VertexInput in [[stage_in]]) {
    VertexOutput out;
    out.position = float4(in.position, 0.0, 1.0);
    out.texcoord = in.texcoord;
    return out;
}

float apply_falloff_spot(float dist, float falloff_type) {
    if (falloff_type < 0.5) {
        return 1.0 - dist;
    } else if (falloff_type < 1.5) {
        return 1.0 / (1.0 + dist * dist * 4.0);
    } else if (falloff_type < 2.5) {
        return 1.0 - dist * dist * (3.0 - 2.0 * dist);
    } else {
        return 1.0;
    }
}

fragment float4 spot_light_fragment(
    VertexOutput in [[stage_in]],
    constant SpotLightParams& params [[buffer(0)]]
) {
    // Vector from light to fragment
    float2 delta = (in.texcoord - params.light_center) * params.aspect;
    float dist = length(delta);

    // Normalized distance
    float norm_dist = dist / params.radius;
    if (norm_dist >= 1.0) {
        return float4(0.0);
    }

    // Check cone angle
    float2 to_frag = normalize(delta);
    float angle_cos = dot(to_frag, params.direction);

    // Outside outer cone = no light
    if (angle_cos < params.outer_angle) {
        return float4(0.0);
    }

    // Calculate cone attenuation (smooth between inner and outer)
    float cone_atten = 1.0;
    if (angle_cos < params.inner_angle) {
        cone_atten = (angle_cos - params.outer_angle) / (params.inner_angle - params.outer_angle);
        cone_atten = smoothstep(0.0, 1.0, cone_atten);
    }

    // Distance attenuation
    float dist_atten = apply_falloff_spot(norm_dist, params.falloff_type);

    // Final color
    float4 result = params.color;
    float atten = cone_atten * dist_atten * params.intensity;
    result.rgb *= atten;
    result.a = atten;

    return result;
}
)";

/* ============================================================================
 * Composite Shader (Vertex + Fragment)
 *
 * Blends the lightmap with the scene using different blend modes.
 * ============================================================================ */

static const char *composite_msl = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexInput {
    float2 position [[attribute(0)]];
    float2 texcoord [[attribute(1)]];
};

struct VertexOutput {
    float4 position [[position]];
    float2 texcoord;
};

struct CompositeParams {
    float4 ambient;         // Ambient light RGBA
    float blend_mode;       // 0=multiply, 1=additive, 2=overlay
    float3 _pad;
};

vertex VertexOutput lighting_vertex(VertexInput in [[stage_in]]) {
    VertexOutput out;
    out.position = float4(in.position, 0.0, 1.0);
    out.texcoord = in.texcoord;
    return out;
}

fragment float4 composite_fragment(
    VertexOutput in [[stage_in]],
    texture2d<float> scene_texture [[texture(0)]],
    texture2d<float> light_texture [[texture(1)]],
    sampler tex_sampler [[sampler(0)]],
    constant CompositeParams& params [[buffer(0)]]
) {
    float4 scene = scene_texture.sample(tex_sampler, in.texcoord);
    float4 light = light_texture.sample(tex_sampler, in.texcoord);

    // Add ambient to light
    light.rgb += params.ambient.rgb * params.ambient.a;

    float4 result;

    if (params.blend_mode < 0.5) {
        // Multiply blend - darkens unlit areas
        result.rgb = scene.rgb * light.rgb;
        result.a = scene.a;
    } else if (params.blend_mode < 1.5) {
        // Additive blend - brightens lit areas
        result.rgb = scene.rgb + light.rgb * light.a;
        result.a = scene.a;
    } else {
        // Overlay blend - balanced lighting
        float3 base = scene.rgb;
        float3 blend = light.rgb;

        // Overlay formula
        result.rgb = mix(
            2.0 * base * blend,
            1.0 - 2.0 * (1.0 - base) * (1.0 - blend),
            step(0.5, base)
        );
        result.a = scene.a;
    }

    return result;
}
)";

/* ============================================================================
 * Ambient Fill Shader (Vertex + Fragment)
 *
 * Simple shader that fills the lightmap with ambient color.
 * ============================================================================ */

static const char *ambient_msl = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexInput {
    float2 position [[attribute(0)]];
    float2 texcoord [[attribute(1)]];
};

struct VertexOutput {
    float4 position [[position]];
    float2 texcoord;
};

struct AmbientParams {
    float4 color;   // Ambient RGBA
};

vertex VertexOutput lighting_vertex(VertexInput in [[stage_in]]) {
    VertexOutput out;
    out.position = float4(in.position, 0.0, 1.0);
    out.texcoord = in.texcoord;
    return out;
}

fragment float4 ambient_fragment(
    VertexOutput in [[stage_in]],
    constant AmbientParams& params [[buffer(0)]]
) {
    return params.color;
}
)";

#endif /* AGENTITE_LIGHTING_SHADERS_H */
