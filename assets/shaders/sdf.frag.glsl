#version 450

layout(set = 2, binding = 0) uniform sampler2D font_texture;

layout(set = 3, binding = 0) uniform SDFUniforms {
    vec4 params;          // distance_range, scale, weight, edge_threshold
    vec4 outline_params;  // outline_width, pad, pad, pad
    vec4 outline_color;   // RGBA
    vec4 glow_params;     // glow_width, pad, pad, pad
    vec4 glow_color;      // RGBA
    vec4 shadow_params;   // shadow_offset_x, shadow_offset_y, shadow_softness, pad
    vec4 shadow_color;    // RGBA
    uint flags;
    float _padding1;
    float _padding2;
    float _padding3;
};

layout(location = 0) in vec2 frag_texcoord;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

void main() {
    float dist = texture(font_texture, frag_texcoord).r;

    // Extract parameters from packed vec4s
    float distance_range = params.x;
    float scale = params.y;
    float weight = params.z;
    float edge_threshold = params.w;
    float outline_width = outline_params.x;
    float glow_width = glow_params.x;
    vec2 shadow_offset = shadow_params.xy;
    float shadow_softness = shadow_params.z;

    // Screen-space anti-aliasing
    vec2 dxdy = fwidth(frag_texcoord);
    float px_range = distance_range * scale / max(dxdy.x, dxdy.y);
    px_range = max(px_range, 1.0);

    float edge = edge_threshold - weight;
    float aa = 0.5 / px_range;
    float alpha = smoothstep(edge - aa, edge + aa, dist);

    vec4 result = vec4(frag_color.rgb, frag_color.a * alpha);

    // Outline (behind text) - flag bit 0
    if ((flags & 1u) != 0u) {
        float outline_edge = edge - outline_width;
        float outline_alpha = smoothstep(outline_edge - aa, outline_edge + aa, dist);
        outline_alpha = outline_alpha * (1.0 - alpha) * outline_color.a * frag_color.a;
        result = vec4(
            mix(outline_color.rgb, result.rgb, result.a),
            max(result.a, outline_alpha)
        );
    }

    // Glow (behind outline) - flag bit 1
    if ((flags & 2u) != 0u) {
        float glow_edge = edge - glow_width;
        // Softer falloff for smoother glow
        float glow_aa = aa * (2.0 + glow_width * 4.0);
        float glow_alpha = smoothstep(glow_edge - glow_aa, edge, dist);
        glow_alpha = glow_alpha * (1.0 - result.a) * glow_color.a * frag_color.a;
        result = vec4(
            mix(glow_color.rgb, result.rgb, result.a),
            max(result.a, glow_alpha)
        );
    }

    // Shadow (behind everything) - flag bit 2
    if ((flags & 4u) != 0u) {
        // Sample SDF at offset position for shadow
        vec2 shadow_uv = frag_texcoord - shadow_offset * dxdy;
        float shadow_dist = texture(font_texture, shadow_uv).r;
        // Softer edge for shadow based on softness parameter
        float shadow_aa = aa * (1.0 + shadow_softness * 4.0);
        float shadow_alpha = smoothstep(edge - shadow_aa, edge + shadow_aa, shadow_dist);
        shadow_alpha = shadow_alpha * (1.0 - result.a) * shadow_color.a * frag_color.a;
        result = vec4(
            mix(shadow_color.rgb, result.rgb, result.a),
            max(result.a, shadow_alpha)
        );
    }

    out_color = result;
}
