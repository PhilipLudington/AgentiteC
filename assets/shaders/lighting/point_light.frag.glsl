#version 450

// Point light fragment shader
// Renders a radial gradient with configurable falloff

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

layout(std140, set = 3, binding = 0) uniform PointLightParams {
    vec2 light_center;    // Light center in UV space (0-1)
    float radius;         // Light radius in UV space
    float intensity;      // Light intensity multiplier
    vec4 color;           // RGBA color
    float falloff_type;   // 0=linear, 1=quadratic, 2=smooth, 3=none
    float _pad_align;     // Padding for alignment
    vec2 aspect;          // Aspect ratio correction (width/height, 1.0)
};

float apply_falloff(float dist, float falloff) {
    if (falloff < 0.5) {
        // Linear falloff
        return 1.0 - dist;
    } else if (falloff < 1.5) {
        // Quadratic falloff
        return 1.0 / (1.0 + dist * dist * 4.0);
    } else if (falloff < 2.5) {
        // Smooth (smoothstep) falloff
        return 1.0 - dist * dist * (3.0 - 2.0 * dist);
    } else {
        // No falloff
        return 1.0;
    }
}

void main() {
    // Calculate distance from light center with aspect correction
    vec2 delta = (frag_texcoord - light_center) * aspect;
    float dist = length(delta) / radius;

    // Outside radius = no contribution
    if (dist >= 1.0) {
        out_color = vec4(0.0);
        return;
    }

    // Apply falloff
    float attenuation = apply_falloff(dist, falloff_type);

    // Final color with intensity
    out_color = color;
    out_color.rgb *= attenuation * intensity;
    out_color.a = attenuation * intensity;
}
