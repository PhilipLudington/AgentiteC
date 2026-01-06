#version 450

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D source_texture;
layout(set = 2, binding = 1) uniform sampler2D dest_texture;

layout(std140, set = 3, binding = 0) uniform TransitionParams {
    float progress;
    float softness;
    vec2 _pad;
};

void main() {
    vec4 source = texture(source_texture, frag_texcoord);
    vec4 dest = texture(dest_texture, frag_texcoord);
    out_color = mix(source, dest, progress);
}
