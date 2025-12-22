#version 450

layout(set = 2, binding = 0) uniform sampler2D scene_texture;

layout(location = 0) in vec2 frag_texcoord;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 scene_color = texture(scene_texture, frag_texcoord);

    /* Calculate vignette based on distance from center */
    vec2 uv = frag_texcoord - vec2(0.5, 0.5);
    float dist = length(uv * vec2(2.0, 2.0));

    /* Smooth vignette falloff: start darkening at 0.6, full effect at 1.4 */
    float vignette = 1.0 - smoothstep(0.6, 1.4, dist);

    /* Mix with max darkness of 0.4 */
    vignette = mix(0.6, 1.0, vignette);

    out_color = vec4(scene_color.rgb * vignette, scene_color.a);
}
