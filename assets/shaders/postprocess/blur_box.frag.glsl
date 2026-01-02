#version 450

layout(set = 2, binding = 0) uniform sampler2D source_texture;

layout(set = 3, binding = 0) uniform Params {
    float radius;
    float sigma;    /* Unused for box blur */
    vec2 _pad;
};

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

void main() {
    vec2 tex_size = textureSize(source_texture, 0);
    vec2 texel = 1.0 / tex_size;

    int iradius = int(radius);
    if (iradius <= 0) iradius = 1;

    vec4 sum = vec4(0.0);
    float count = 0.0;

    for (int x = -iradius; x <= iradius; x++) {
        for (int y = -iradius; y <= iradius; y++) {
            vec2 offset = vec2(float(x), float(y)) * texel;
            sum += texture(source_texture, frag_texcoord + offset);
            count += 1.0;
        }
    }

    out_color = sum / count;
}
