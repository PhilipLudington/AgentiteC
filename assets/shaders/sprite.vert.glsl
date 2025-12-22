#version 450

layout(set = 1, binding = 0) uniform Uniforms {
    mat4 view_projection;
    vec2 screen_size;
    vec2 padding;
};

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;
layout(location = 2) in vec4 color;

layout(location = 0) out vec2 frag_texcoord;
layout(location = 1) out vec4 frag_color;

void main() {
    vec4 world_pos = vec4(position, 0.0, 1.0);
    gl_Position = view_projection * world_pos;
    frag_texcoord = texcoord;
    frag_color = color;
}
