#version 450

layout(set = 2, binding = 0) uniform sampler2D source_texture;

layout(set = 3, binding = 0) uniform Params {
    float offset;   /* Color channel offset in pixels */
    vec3 _pad;
};

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

void main() {
    vec2 tex_size = textureSize(source_texture, 0);
    vec2 texel = 1.0 / tex_size;

    /* Direction from center */
    vec2 dir = frag_texcoord - 0.5;
    dir = normalize(dir) * texel * offset;

    /* Sample each channel with offset */
    float r = texture(source_texture, frag_texcoord - dir).r;
    float g = texture(source_texture, frag_texcoord).g;
    float b = texture(source_texture, frag_texcoord + dir).b;
    float a = texture(source_texture, frag_texcoord).a;

    out_color = vec4(r, g, b, a);
}
