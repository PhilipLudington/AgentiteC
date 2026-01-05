#version 450

// Ambient fill shader
// Simple shader that fills the lightmap with ambient color

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

layout(std140, set = 3, binding = 0) uniform AmbientParams {
    vec4 color;   // Ambient RGBA
};

void main() {
    out_color = color;
}
