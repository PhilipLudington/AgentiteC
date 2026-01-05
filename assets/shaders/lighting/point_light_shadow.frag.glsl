#version 450

// Point light with shadow map sampling
// Uses a 2D shadow map atlas (720 x 8) where each row is one light's shadow distances

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D shadow_map;

layout(std140, set = 3, binding = 0) uniform PointLightShadowParams {
    vec2 light_center;    // Light center in UV space (0-1)
    float radius;         // Light radius in UV space
    float intensity;      // Light intensity multiplier
    vec4 color;           // RGBA color
    float falloff_type;   // 0=linear, 1=quadratic, 2=smooth, 3=none
    float shadow_softness;// Shadow edge softness (world units)
    vec2 aspect;          // Aspect ratio correction (width/height, 1.0)
    vec2 lightmap_size;   // Lightmap dimensions (width, height) for UV to world
    float radius_world;   // Light radius in world units
    float shadow_row;     // Which row in shadow atlas (0-7)
    float atlas_height;   // Total atlas height (8.0)
    float _pad1;
    float _pad2;
    float _pad3;
};

const float PI = 3.14159265359;

float apply_falloff(float dist, float falloff) {
    if (falloff < 0.5) {
        return 1.0 - dist;
    } else if (falloff < 1.5) {
        return 1.0 / (1.0 + dist * dist * 4.0);
    } else if (falloff < 2.5) {
        return 1.0 - dist * dist * (3.0 - 2.0 * dist);
    } else {
        return 1.0;
    }
}

void main() {
    // Calculate delta in UV space
    vec2 delta_uv = frag_texcoord - light_center;

    // Convert to world coordinates for shadow map comparison
    vec2 delta_world = delta_uv * lightmap_size;
    float dist_world = length(delta_world);

    // Calculate distance WITH aspect correction (for circular falloff display)
    vec2 delta_aspect = delta_uv * aspect;
    float dist_uv = length(delta_aspect);
    float dist_normalized = dist_uv / radius;

    // Outside radius = no contribution
    if (dist_normalized >= 1.0) {
        out_color = vec4(0.0);
        return;
    }

    // Calculate angle in WORLD coordinates (matching shadow map generation)
    float angle = atan(delta_world.y, delta_world.x);  // -PI to PI
    if (angle < 0.0) angle += 2.0 * PI;  // Convert to [0, 2*PI]
    float angle_normalized = angle / (2.0 * PI);  // 0 to 1

    // Clamp to valid range to avoid sampling artifacts at edges
    angle_normalized = clamp(angle_normalized, 0.001, 0.999);

    // Sample shadow map atlas at this angle and the correct row for this light
    float v = (shadow_row + 0.5) / atlas_height;
    float shadow_dist = texture(shadow_map, vec2(angle_normalized, v)).r;

    // Shadow test with soft edge
    float softness = shadow_softness;
    if (softness < 0.001) softness = 4.0;  // Default softness in world units

    float shadow = smoothstep(shadow_dist - softness,
                              shadow_dist + softness,
                              dist_world);

    // Apply falloff using aspect-corrected normalized distance
    float attenuation = apply_falloff(dist_normalized, falloff_type);

    // Reduce light contribution in shadow regions
    attenuation *= (1.0 - shadow);

    // Final color with intensity
    out_color = color;
    out_color.rgb *= attenuation * intensity;
    out_color.a = attenuation * intensity;
}
