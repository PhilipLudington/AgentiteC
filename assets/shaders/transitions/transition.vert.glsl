#version 450

// Shared vertex shader for all transition effects
// Renders a fullscreen quad with UV coordinates and projection matrix

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 frag_texcoord;

// Projection matrix (pushed to slot 0)
layout(std140, set = 1, binding = 0) uniform Uniforms {
    mat4 projection;
};

void main() {
    gl_Position = projection * vec4(position, 0.0, 1.0);
    frag_texcoord = texcoord;
}
