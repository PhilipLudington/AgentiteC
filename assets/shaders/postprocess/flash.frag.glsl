#version 450

layout(set = 2, binding = 0) uniform sampler2D source_texture;

layout(set = 3, binding = 0) uniform Params {
    vec4 flash_color;   /* Flash RGBA color */
    float intensity;    /* Flash intensity (0-1) */
    vec3 _pad;
};

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

void main() {
    vec4 color = texture(source_texture, frag_texcoord);
    vec3 result = mix(color.rgb, flash_color.rgb, intensity * flash_color.a);
    out_color = vec4(result, color.a);
}
