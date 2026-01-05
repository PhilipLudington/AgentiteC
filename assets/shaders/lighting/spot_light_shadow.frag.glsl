#version 450

// Spot light with shadow map sampling
// Uses a 2D shadow map atlas (720 x 8) where each row is one light's shadow distances
// For spot lights, the shadow map covers rays within the cone angle

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D shadow_map;

layout(std140, set = 3, binding = 0) uniform SpotLightShadowParams {
    vec2 light_center;    // Light center in UV space (0-1)
    vec2 direction;       // Normalized direction vector
    float radius;         // Max distance in UV space
    float inner_angle;    // Cosine of inner cone angle
    float outer_angle;    // Cosine of outer cone angle
    float intensity;      // Light intensity multiplier
    vec4 color;           // RGBA color
    float falloff_type;   // Falloff curve type
    float shadow_softness;// Shadow edge softness (world units)
    vec2 aspect;          // Aspect ratio correction
    vec2 lightmap_size;   // Lightmap dimensions for UV to world conversion
    float radius_world;   // Light radius in world units
    float shadow_row;     // Which row in shadow atlas (0-7)
    float atlas_height;   // Total atlas height (8.0)
    float cone_angle_rad; // Outer cone angle in radians
    float _pad1;
    float _pad2;
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

    // Check cone angle (use aspect-corrected direction for visual consistency)
    vec2 to_frag = normalize(delta_aspect);
    float angle_cos = dot(to_frag, direction);

    // Outside outer cone = no light
    if (angle_cos < outer_angle) {
        out_color = vec4(0.0);
        return;
    }

    // Calculate cone attenuation (smooth between inner and outer)
    float cone_atten = 1.0;
    if (angle_cos < inner_angle) {
        cone_atten = (angle_cos - outer_angle) / (inner_angle - outer_angle);
        cone_atten = smoothstep(0.0, 1.0, cone_atten);
    }

    // Calculate angle relative to spot light direction for shadow map lookup
    // The shadow map stores distances for rays within the cone
    // We need to map the fragment's angle to the 0-1 range of the shadow map

    // Get the angle of the fragment relative to the light direction
    float frag_angle = atan(delta_world.y, delta_world.x);
    float dir_angle = atan(direction.y, direction.x);
    float rel_angle = frag_angle - dir_angle;

    // Normalize to [-PI, PI]
    if (rel_angle > PI) rel_angle -= 2.0 * PI;
    if (rel_angle < -PI) rel_angle += 2.0 * PI;

    // Map from [-cone_angle, +cone_angle] to [0, 1]
    float angle_normalized = (rel_angle / cone_angle_rad + 1.0) * 0.5;
    angle_normalized = clamp(angle_normalized, 0.001, 0.999);

    // Sample shadow map atlas at this angle and the correct row for this light
    float v = (shadow_row + 0.5) / atlas_height;
    float shadow_dist = texture(shadow_map, vec2(angle_normalized, v)).r;

    // Shadow test with soft edge
    float softness = shadow_softness;
    if (softness < 0.001) softness = 4.0;

    float shadow = smoothstep(shadow_dist - softness,
                              shadow_dist + softness,
                              dist_world);

    // Apply distance falloff
    float dist_atten = apply_falloff(dist_normalized, falloff_type);

    // Combined attenuation
    float attenuation = cone_atten * dist_atten * (1.0 - shadow);

    // Final color with intensity
    out_color = color;
    out_color.rgb *= attenuation * intensity;
    out_color.a = attenuation * intensity;
}
