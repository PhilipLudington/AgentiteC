/*
 * Carbon UI Shaders - Metal Shading Language
 *
 * Simple 2D UI rendering with texture sampling.
 */

#include <metal_stdlib>
using namespace metal;

/* Uniform buffer for screen transformation */
struct Uniforms {
    float2 screen_size;
    float2 padding;
};

/* Vertex input from CPU */
struct VertexIn {
    float2 position [[attribute(0)]];
    float2 texcoord [[attribute(1)]];
    uint color [[attribute(2)]];
};

/* Vertex output / Fragment input */
struct VertexOut {
    float4 position [[position]];
    float2 texcoord;
    float4 color;
};

/* Vertex Shader */
vertex VertexOut ui_vertex(
    VertexIn in [[stage_in]],
    constant Uniforms& uniforms [[buffer(0)]]
) {
    VertexOut out;

    /* Convert screen coordinates to normalized device coordinates */
    /* Screen space: (0,0) top-left, (width,height) bottom-right */
    /* NDC: (-1,-1) bottom-left, (1,1) top-right */
    float2 ndc;
    ndc.x = (in.position.x / uniforms.screen_size.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (in.position.y / uniforms.screen_size.y) * 2.0;  /* Flip Y */

    out.position = float4(ndc, 0.0, 1.0);
    out.texcoord = in.texcoord;

    /* Unpack color from uint32 (0xAABBGGRR) to float4 */
    out.color.r = float((in.color >> 0) & 0xFF) / 255.0;
    out.color.g = float((in.color >> 8) & 0xFF) / 255.0;
    out.color.b = float((in.color >> 16) & 0xFF) / 255.0;
    out.color.a = float((in.color >> 24) & 0xFF) / 255.0;

    return out;
}

/* Fragment Shader */
fragment float4 ui_fragment(
    VertexOut in [[stage_in]],
    texture2d<float> font_atlas [[texture(0)]],
    sampler font_sampler [[sampler(0)]]
) {
    /* Sample font atlas (single channel, stored in R) */
    float alpha = font_atlas.sample(font_sampler, in.texcoord).r;

    /* Output color with texture alpha */
    return float4(in.color.rgb, in.color.a * alpha);
}
