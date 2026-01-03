#version 450

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D source_texture;

layout(std140, set = 3, binding = 0) uniform TransitionParams {
    vec4 fade_color;
};

void main() {
    vec4 scene = texture(source_texture, frag_texcoord);
    out_color = mix(scene, fade_color, fade_color.a);
}
