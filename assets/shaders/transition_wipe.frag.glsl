#version 450

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D source_texture;
layout(set = 2, binding = 1) uniform sampler2D dest_texture;

layout(std140, set = 3, binding = 0) uniform TransitionParams {
    float progress;
    float direction; // 0=left, 1=right, 2=up, 3=down, 4=diagonal
    float softness;
    float _pad;
};

void main() {
    vec4 source = texture(source_texture, frag_texcoord);
    vec4 dest = texture(dest_texture, frag_texcoord);

    float edge;
    if (direction < 0.5) {
        // Wipe left (from right to left)
        edge = 1.0 - frag_texcoord.x;
    } else if (direction < 1.5) {
        // Wipe right (from left to right)
        edge = frag_texcoord.x;
    } else if (direction < 2.5) {
        // Wipe up (from bottom to top)
        edge = 1.0 - frag_texcoord.y;
    } else if (direction < 3.5) {
        // Wipe down (from top to bottom)
        edge = frag_texcoord.y;
    } else {
        // Diagonal (top-left to bottom-right)
        edge = (frag_texcoord.x + frag_texcoord.y) * 0.5;
    }

    // Apply progress with softness for smooth edge
    float adjusted_progress = progress * (1.0 + softness);
    float mask = smoothstep(adjusted_progress - softness, adjusted_progress, edge);

    out_color = mix(dest, source, mask);
}
