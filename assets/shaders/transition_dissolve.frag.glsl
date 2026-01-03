#version 450

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D source_texture;
layout(set = 2, binding = 1) uniform sampler2D dest_texture;

layout(std140, set = 3, binding = 0) uniform TransitionParams {
    float progress;
    float edge_width;
    vec2 _pad;
};

// Simple hash function for procedural noise
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Value noise
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // Smooth interpolation

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    vec4 source = texture(source_texture, frag_texcoord);
    vec4 dest = texture(dest_texture, frag_texcoord);

    // Generate noise pattern
    float n = noise(frag_texcoord * 20.0);

    // Create dissolve mask with edge glow
    float adjusted_progress = progress * (1.0 + edge_width);
    float edge = smoothstep(adjusted_progress - edge_width, adjusted_progress, n);

    out_color = mix(dest, source, edge);
}
