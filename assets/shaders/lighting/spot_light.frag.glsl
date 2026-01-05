#version 450

// Spot light fragment shader
// Renders a cone-shaped light with direction, inner/outer angles, and falloff

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

layout(std140, set = 3, binding = 0) uniform SpotLightParams {
    vec2 light_center;    // Light center in UV space
    vec2 direction;       // Normalized direction
    float radius;         // Max distance
    float inner_angle;    // Inner cone angle (cos)
    float outer_angle;    // Outer cone angle (cos)
    float intensity;      // Intensity multiplier
    vec4 color;           // RGBA color
    float falloff_type;   // Falloff curve type
    float _pad1;
    vec2 aspect;          // Aspect correction
};

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
    // Vector from light to fragment
    vec2 delta = (frag_texcoord - light_center) * aspect;
    float dist = length(delta);

    // Normalized distance
    float norm_dist = dist / radius;
    if (norm_dist >= 1.0) {
        out_color = vec4(0.0);
        return;
    }

    // Check cone angle
    vec2 to_frag = normalize(delta);
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

    // Distance attenuation
    float dist_atten = apply_falloff(norm_dist, falloff_type);

    // Final color
    out_color = color;
    float atten = cone_atten * dist_atten * intensity;
    out_color.rgb *= atten;
    out_color.a = atten;
}
