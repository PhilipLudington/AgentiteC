#version 450

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D source_texture;
layout(set = 2, binding = 1) uniform sampler2D dest_texture;

layout(std140, set = 3, binding = 0) uniform TransitionParams {
    float progress;
    float center_x;
    float center_y;
    float is_open; // 0=close (to black), 1=open (from black)
};

void main() {
    vec4 source = texture(source_texture, frag_texcoord);
    vec4 dest = texture(dest_texture, frag_texcoord);

    // Calculate distance from center
    vec2 center = vec2(center_x, center_y);
    vec2 uv = frag_texcoord - center;

    // Account for aspect ratio (assuming square for now, can add aspect uniform)
    float dist = length(uv);

    // Maximum distance is corner to corner (approximately sqrt(2)/2 for centered)
    float max_dist = 1.0;

    // Circle radius based on progress
    float radius;
    if (is_open > 0.5) {
        // Opening: radius grows from 0 to max
        radius = progress * max_dist;
    } else {
        // Closing: radius shrinks from max to 0
        radius = (1.0 - progress) * max_dist;
    }

    // Apply smooth edge
    float edge = 0.02;
    float mask = smoothstep(radius - edge, radius + edge, dist);

    if (is_open > 0.5) {
        // Opening: show dest inside circle, source outside
        out_color = mix(dest, source, mask);
    } else {
        // Closing: show source inside circle, black outside
        vec4 black = vec4(0.0, 0.0, 0.0, 1.0);
        out_color = mix(source, black, mask);
    }
}
