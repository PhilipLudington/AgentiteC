#version 450

layout(set = 2, binding = 0) uniform sampler2D source_texture;

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

void main() {
    vec2 tex_size = textureSize(source_texture, 0);
    vec2 texel = 1.0 / tex_size;

    /* Sample 3x3 neighborhood */
    float tl = dot(texture(source_texture, frag_texcoord + vec2(-texel.x, -texel.y)).rgb, vec3(0.299, 0.587, 0.114));
    float tm = dot(texture(source_texture, frag_texcoord + vec2(0.0, -texel.y)).rgb, vec3(0.299, 0.587, 0.114));
    float tr = dot(texture(source_texture, frag_texcoord + vec2(texel.x, -texel.y)).rgb, vec3(0.299, 0.587, 0.114));
    float ml = dot(texture(source_texture, frag_texcoord + vec2(-texel.x, 0.0)).rgb, vec3(0.299, 0.587, 0.114));
    float mr = dot(texture(source_texture, frag_texcoord + vec2(texel.x, 0.0)).rgb, vec3(0.299, 0.587, 0.114));
    float bl = dot(texture(source_texture, frag_texcoord + vec2(-texel.x, texel.y)).rgb, vec3(0.299, 0.587, 0.114));
    float bm = dot(texture(source_texture, frag_texcoord + vec2(0.0, texel.y)).rgb, vec3(0.299, 0.587, 0.114));
    float br = dot(texture(source_texture, frag_texcoord + vec2(texel.x, texel.y)).rgb, vec3(0.299, 0.587, 0.114));

    /* Sobel operators */
    float gx = -tl - 2.0*ml - bl + tr + 2.0*mr + br;
    float gy = -tl - 2.0*tm - tr + bl + 2.0*bm + br;

    float edge = sqrt(gx*gx + gy*gy);
    out_color = vec4(vec3(edge), 1.0);
}
