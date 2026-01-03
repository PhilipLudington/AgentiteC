#version 450

layout(location = 0) in vec2 frag_texcoord;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D source_texture;
layout(set = 2, binding = 1) uniform sampler2D dest_texture;

layout(std140, set = 3, binding = 0) uniform TransitionParams {
    float progress;
    float direction; // 0=left, 1=right, 2=up, 3=down
    float is_push;   // 0=slide (only new scene moves), 1=push (both scenes move)
    float _pad;
};

void main() {
    vec2 source_uv = frag_texcoord;
    vec2 dest_uv = frag_texcoord;
    vec2 offset = vec2(0.0);

    // Calculate offset based on direction
    if (direction < 0.5) {
        // Slide/push left (new scene comes from right)
        offset = vec2(1.0 - progress, 0.0);
    } else if (direction < 1.5) {
        // Slide/push right (new scene comes from left)
        offset = vec2(progress - 1.0, 0.0);
    } else if (direction < 2.5) {
        // Slide/push up (new scene comes from bottom)
        offset = vec2(0.0, 1.0 - progress);
    } else {
        // Slide/push down (new scene comes from top)
        offset = vec2(0.0, progress - 1.0);
    }

    // Apply offset
    dest_uv += offset;
    if (is_push > 0.5) {
        source_uv -= offset;
    }

    // Check if we're in bounds
    bool source_valid = source_uv.x >= 0.0 && source_uv.x <= 1.0 &&
                        source_uv.y >= 0.0 && source_uv.y <= 1.0;
    bool dest_valid = dest_uv.x >= 0.0 && dest_uv.x <= 1.0 &&
                      dest_uv.y >= 0.0 && dest_uv.y <= 1.0;

    vec4 source = source_valid ? texture(source_texture, source_uv) : vec4(0.0);
    vec4 dest = dest_valid ? texture(dest_texture, dest_uv) : vec4(0.0);

    // Composite: destination on top when valid
    if (dest_valid) {
        out_color = dest;
    } else if (source_valid) {
        out_color = source;
    } else {
        out_color = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
