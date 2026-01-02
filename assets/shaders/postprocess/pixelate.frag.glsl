#version 450

layout(set = 2, binding = 0) uniform sampler2D source_texture;

layout(set = 3, binding = 0) uniform Params {
    float pixel_size;
    vec3 _pad;
};

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

void main() {
    vec2 tex_size = textureSize(source_texture, 0);
    vec2 uv = floor(frag_texcoord * tex_size / pixel_size) * pixel_size / tex_size;
    out_color = texture(source_texture, uv);
}
