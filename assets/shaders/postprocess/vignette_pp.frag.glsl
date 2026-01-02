#version 450

layout(set = 2, binding = 0) uniform sampler2D source_texture;

layout(set = 3, binding = 0) uniform Params {
    float intensity;
    float softness;
    vec2 _pad;
};

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

void main() {
    vec4 color = texture(source_texture, frag_texcoord);
    vec2 uv = frag_texcoord - 0.5;
    float dist = length(uv * 2.0);
    float start = 1.0 - softness;
    float vignette = 1.0 - smoothstep(start, 1.4, dist);
    vignette = mix(1.0 - intensity, 1.0, vignette);
    out_color = vec4(color.rgb * vignette, color.a);
}
