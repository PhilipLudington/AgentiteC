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
    // Convert unit coords (0-1) to clip space (-1 to +1), flip Y for screen coords
    float2 clip_pos = in.position * 2.0 - 1.0;
    clip_pos.y = -clip_pos.y;
    out.position = float4(clip_pos, 0.0, 1.0);
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
 * Point Light Shadow Shader (Vertex + Fragment)
 *
 * Renders a radial gradient with shadow map sampling for occluder shadows.
 * The shadow map is a 1D texture containing distances from the light center
 * to the nearest occluder at each angle.
 * ============================================================================ */

static const char *point_light_shadow_msl = R"(
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

struct PointLightShadowParams {
    float2 light_center;    // Light center in UV space (0-1)
    float radius;           // Light radius in UV space
    float intensity;        // Light intensity multiplier
    float4 color;           // RGBA color
    float falloff_type;     // 0=linear, 1=quadratic, 2=smooth, 3=none
    float shadow_softness;  // Shadow edge softness (world units)
    float2 aspect;          // Aspect ratio correction (width/height, 1.0)
    float2 lightmap_size;   // Lightmap dimensions (width, height) for UV to world
    float radius_world;     // Light radius in world units
    float shadow_row;       // Which row in shadow atlas (0-7)
    float atlas_height;     // Total atlas height (8.0)
    float3 _pad;
};

vertex VertexOutput lighting_vertex(VertexInput in [[stage_in]]) {
    VertexOutput out;
    // Convert unit coords (0-1) to clip space (-1 to +1), flip Y for screen coords
    float2 clip_pos = in.position * 2.0 - 1.0;
    clip_pos.y = -clip_pos.y;
    out.position = float4(clip_pos, 0.0, 1.0);
    out.texcoord = in.texcoord;
    return out;
}

float apply_falloff_shadow(float dist, float falloff_type) {
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

fragment float4 point_light_shadow_fragment(
    VertexOutput in [[stage_in]],
    texture2d<float> shadow_map [[texture(0)]],
    sampler shadow_sampler [[sampler(0)]],
    constant PointLightShadowParams& params [[buffer(0)]]
) {
    // Calculate delta in UV space
    float2 delta_uv = in.texcoord - params.light_center;

    // Convert to world coordinates for shadow map comparison
    // UV space (0-1) * lightmap dimensions = world/pixel coordinates
    float2 delta_world = delta_uv * params.lightmap_size;
    float dist_world = length(delta_world);

    // Calculate distance WITH aspect correction (for circular falloff display)
    float2 delta_aspect = delta_uv * params.aspect;
    float dist_uv = length(delta_aspect);
    float dist_normalized = dist_uv / params.radius;

    // Outside radius = no contribution
    if (dist_normalized >= 1.0) {
        return float4(0.0);
    }

    // Calculate angle in WORLD coordinates (matching shadow map generation)
    // Shadow map was generated with rays going OUTWARD from light at each angle
    // For a pixel at position P, we need the angle of the ray FROM LIGHT TO PIXEL
    // atan2 gives us the direction from light to pixel, which matches the shadow map
    float angle = atan2(delta_world.y, delta_world.x);  // -PI to PI
    if (angle < 0.0) angle += 2.0 * M_PI_F;  // Convert to [0, 2*PI]
    float angle_normalized = angle / (2.0 * M_PI_F);  // 0 to 1

    // Clamp to valid range to avoid sampling artifacts at edges
    angle_normalized = clamp(angle_normalized, 0.001, 0.999);

    // Sample shadow map atlas at this angle and the correct row for this light
    // shadow_map is an atlas with each row containing one light's shadow distances
    float v = (params.shadow_row + 0.5) / params.atlas_height;
    float shadow_dist = shadow_map.sample(shadow_sampler, float2(angle_normalized, v)).r;

    // Shadow test with soft edge
    // If fragment is further than shadow distance, it's in shadow
    float shadow_softness = params.shadow_softness;
    if (shadow_softness < 0.001) shadow_softness = 4.0;  // Default softness in world units

    float shadow = smoothstep(shadow_dist - shadow_softness,
                              shadow_dist + shadow_softness,
                              dist_world);

    // Apply falloff using aspect-corrected normalized distance
    float attenuation = apply_falloff_shadow(dist_normalized, params.falloff_type);

    // Reduce light contribution in shadow regions
    attenuation *= (1.0 - shadow);

    // Final color with intensity
    float4 result = params.color;
    result.rgb *= attenuation * params.intensity;
    result.a = attenuation * params.intensity;

    return result;
}
)";

/* ============================================================================
 * Spot Light Shadow Shader (Vertex + Fragment)
 *
 * Renders a cone-shaped light with shadow map sampling for occluder shadows.
 * The shadow map stores distances from the light center to the nearest occluder
 * for rays within the cone. Rays are mapped from cone angle space to 0-1 UV range.
 * ============================================================================ */

static const char *spot_light_shadow_msl = R"(
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

struct SpotLightShadowParams {
    float2 light_center;    // Light center in UV space (0-1)
    float2 direction;       // Normalized direction vector
    float radius;           // Max distance in UV space
    float inner_angle;      // Cosine of inner cone angle
    float outer_angle;      // Cosine of outer cone angle
    float intensity;        // Light intensity multiplier
    float4 color;           // RGBA color
    float falloff_type;     // Falloff curve type
    float shadow_softness;  // Shadow edge softness (world units)
    float2 aspect;          // Aspect ratio correction
    float2 lightmap_size;   // Lightmap dimensions for UV to world conversion
    float radius_world;     // Light radius in world units
    float shadow_row;       // Which row in shadow atlas (0-7)
    float atlas_height;     // Total atlas height (8.0)
    float cone_angle_rad;   // Outer cone angle in radians
    float2 _pad;
};

vertex VertexOutput lighting_vertex(VertexInput in [[stage_in]]) {
    VertexOutput out;
    float2 clip_pos = in.position * 2.0 - 1.0;
    clip_pos.y = -clip_pos.y;
    out.position = float4(clip_pos, 0.0, 1.0);
    out.texcoord = in.texcoord;
    return out;
}

float apply_falloff_spot_shadow(float dist, float falloff_type) {
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

fragment float4 spot_light_shadow_fragment(
    VertexOutput in [[stage_in]],
    texture2d<float> shadow_map [[texture(0)]],
    sampler shadow_sampler [[sampler(0)]],
    constant SpotLightShadowParams& params [[buffer(0)]]
) {
    // Calculate delta in UV space
    float2 delta_uv = in.texcoord - params.light_center;

    // Convert to world coordinates for shadow map comparison
    float2 delta_world = delta_uv * params.lightmap_size;
    float dist_world = length(delta_world);

    // Calculate distance WITH aspect correction (for circular falloff display)
    float2 delta_aspect = delta_uv * params.aspect;
    float dist_uv = length(delta_aspect);
    float dist_normalized = dist_uv / params.radius;

    // Outside radius = no contribution
    if (dist_normalized >= 1.0) {
        return float4(0.0);
    }

    // Check cone angle (use aspect-corrected direction for visual consistency)
    float2 to_frag = normalize(delta_aspect);
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

    // Calculate angle relative to spot light direction for shadow map lookup
    // The shadow map stores distances for rays within the cone
    // We need to map the fragment's angle to the 0-1 range of the shadow map

    // Get the angle of the fragment relative to the light direction
    // atan2 gives angle from +X axis, we need angle from light direction
    float frag_angle = atan2(delta_world.y, delta_world.x);
    float dir_angle = atan2(params.direction.y, params.direction.x);
    float rel_angle = frag_angle - dir_angle;

    // Normalize to [-PI, PI]
    if (rel_angle > M_PI_F) rel_angle -= 2.0 * M_PI_F;
    if (rel_angle < -M_PI_F) rel_angle += 2.0 * M_PI_F;

    // Map from [-cone_angle, +cone_angle] to [0, 1]
    // cone_angle_rad is the outer cone angle in radians
    float angle_normalized = (rel_angle / params.cone_angle_rad + 1.0) * 0.5;
    angle_normalized = clamp(angle_normalized, 0.001, 0.999);

    // Sample shadow map atlas at this angle and the correct row for this light
    float v = (params.shadow_row + 0.5) / params.atlas_height;
    float shadow_dist = shadow_map.sample(shadow_sampler, float2(angle_normalized, v)).r;

    // Shadow test with soft edge
    float shadow_softness = params.shadow_softness;
    if (shadow_softness < 0.001) shadow_softness = 4.0;

    float shadow = smoothstep(shadow_dist - shadow_softness,
                              shadow_dist + shadow_softness,
                              dist_world);

    // Apply distance falloff
    float dist_atten = apply_falloff_spot_shadow(dist_normalized, params.falloff_type);

    // Combined attenuation
    float attenuation = cone_atten * dist_atten * (1.0 - shadow);

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
    // Convert unit coords (0-1) to clip space (-1 to +1), flip Y for screen coords
    float2 clip_pos = in.position * 2.0 - 1.0;
    clip_pos.y = -clip_pos.y;
    out.position = float4(clip_pos, 0.0, 1.0);
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
    // Convert unit coords (0-1) to clip space (-1 to +1), flip Y for screen coords
    float2 clip_pos = in.position * 2.0 - 1.0;
    clip_pos.y = -clip_pos.y;
    out.position = float4(clip_pos, 0.0, 1.0);
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
    // Convert unit coords (0-1) to clip space (-1 to +1), flip Y for screen coords
    float2 clip_pos = in.position * 2.0 - 1.0;
    clip_pos.y = -clip_pos.y;
    out.position = float4(clip_pos, 0.0, 1.0);
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
