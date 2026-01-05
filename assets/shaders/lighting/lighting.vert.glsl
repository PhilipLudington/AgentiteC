#version 450

// Shared vertex shader for all lighting effects
// Renders a fullscreen quad with UV coordinates

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 frag_texcoord;

void main() {
    // Convert unit coords (0-1) to clip space (-1 to +1), flip Y for screen coords
    vec2 clip_pos = position * 2.0 - 1.0;
    clip_pos.y = -clip_pos.y;
    gl_Position = vec4(clip_pos, 0.0, 1.0);
    frag_texcoord = texcoord;
}
