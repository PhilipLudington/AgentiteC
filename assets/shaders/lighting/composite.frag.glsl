#version 450

// Composite shader
// Blends the lightmap with the scene using different blend modes

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D scene_texture;
layout(set = 2, binding = 1) uniform sampler2D light_texture;

layout(std140, set = 3, binding = 0) uniform CompositeParams {
    vec4 ambient;         // Ambient light RGBA
    float blend_mode;     // 0=multiply, 1=additive, 2=overlay
    float _pad1;
    float _pad2;
    float _pad3;
};

void main() {
    vec4 scene = texture(scene_texture, frag_texcoord);
    vec4 light = texture(light_texture, frag_texcoord);

    // Add ambient to light
    light.rgb += ambient.rgb * ambient.a;

    if (blend_mode < 0.5) {
        // Multiply blend - darkens unlit areas
        out_color.rgb = scene.rgb * light.rgb;
        out_color.a = scene.a;
    } else if (blend_mode < 1.5) {
        // Additive blend - brightens lit areas
        out_color.rgb = scene.rgb + light.rgb * light.a;
        out_color.a = scene.a;
    } else {
        // Overlay blend - balanced lighting
        vec3 base = scene.rgb;
        vec3 blend = light.rgb;

        // Overlay formula
        out_color.rgb = mix(
            2.0 * base * blend,
            1.0 - 2.0 * (1.0 - base) * (1.0 - blend),
            step(0.5, base)
        );
        out_color.a = scene.a;
    }
}
