#version 450

layout(set = 2, binding = 0) uniform sampler2D source_texture;

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

void main() {
    vec4 color = texture(source_texture, frag_texcoord);
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    out_color = vec4(gray, gray, gray, color.a);
}
