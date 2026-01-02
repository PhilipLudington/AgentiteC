#version 450

layout(set = 2, binding = 0) uniform sampler2D source_texture;

layout(set = 3, binding = 0) uniform Params {
    float intensity;    /* Line visibility (0-1) */
    float count;        /* Lines per screen height */
    vec2 _pad;
};

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

void main() {
    vec4 color = texture(source_texture, frag_texcoord);

    /* Calculate scanline effect */
    float line_count = count > 0.0 ? count : 240.0;
    float scanline = sin(frag_texcoord.y * line_count * 3.14159265);
    scanline = scanline * 0.5 + 0.5;  /* Map to 0-1 */
    scanline = 1.0 - (intensity * (1.0 - scanline));

    out_color = vec4(color.rgb * scanline, color.a);
}
