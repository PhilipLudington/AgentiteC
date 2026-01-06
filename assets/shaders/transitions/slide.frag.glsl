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
    bool is_push_mode = is_push > 0.5;

    // Calculate UV offsets - both scenes move in the same direction
    // For push: both move together. For slide: only dest moves.
    if (direction < 0.5) {
        // LEFT: scenes move left (add to UV to shift image left)
        dest_uv.x = frag_texcoord.x + progress - 1.0;  // starts at texcoord-1, ends at texcoord
        if (is_push_mode) {
            source_uv.x = frag_texcoord.x + progress;  // starts at texcoord, ends at texcoord+1
        }
    } else if (direction < 1.5) {
        // RIGHT: scenes move right (subtract from UV to shift image right)
        dest_uv.x = frag_texcoord.x - progress + 1.0;  // starts at texcoord+1, ends at texcoord
        if (is_push_mode) {
            source_uv.x = frag_texcoord.x - progress;  // starts at texcoord, ends at texcoord-1
        }
    } else if (direction < 2.5) {
        // UP: scenes move up
        dest_uv.y = frag_texcoord.y + progress - 1.0;
        if (is_push_mode) {
            source_uv.y = frag_texcoord.y + progress;
        }
    } else {
        // DOWN: scenes move down
        dest_uv.y = frag_texcoord.y - progress + 1.0;
        if (is_push_mode) {
            source_uv.y = frag_texcoord.y - progress;
        }
    }

    // Check bounds
    bool source_valid = source_uv.x >= 0.0 && source_uv.x <= 1.0 &&
                        source_uv.y >= 0.0 && source_uv.y <= 1.0;
    bool dest_valid = dest_uv.x >= 0.0 && dest_uv.x <= 1.0 &&
                      dest_uv.y >= 0.0 && dest_uv.y <= 1.0;

    // Sample textures (clamp to avoid edge artifacts)
    vec4 source = texture(source_texture, clamp(source_uv, vec2(0.0), vec2(1.0)));
    vec4 dest = texture(dest_texture, clamp(dest_uv, vec2(0.0), vec2(1.0)));

    // Composite: for push, show whichever is valid (they shouldn't overlap)
    // For slide, dest overlays source
    if (dest_valid) {
        out_color = dest;
    } else if (source_valid) {
        out_color = source;
    } else {
        out_color = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
