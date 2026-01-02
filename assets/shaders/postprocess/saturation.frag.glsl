#version 450

layout(set = 2, binding = 0) uniform sampler2D source_texture;

layout(set = 3, binding = 0) uniform Params {
    float amount;   /* -1 to 1, 0 = neutral */
    vec3 _pad;
};

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

void main() {
    vec4 color = texture(source_texture, frag_texcoord);
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    float saturation = amount + 1.0;  /* Convert to 0-2 range */
    vec3 adjusted = mix(vec3(gray), color.rgb, saturation);
    out_color = vec4(clamp(adjusted, 0.0, 1.0), color.a);
}
