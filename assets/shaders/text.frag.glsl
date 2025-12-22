#version 450

layout(set = 2, binding = 0) uniform sampler2D font_texture;

layout(location = 0) in vec2 frag_texcoord;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

void main() {
    float alpha = texture(font_texture, frag_texcoord).r;
    out_color = vec4(frag_color.rgb, frag_color.a * alpha);
}
