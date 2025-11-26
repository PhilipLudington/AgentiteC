/*
 * Carbon UI Vertex Shader
 *
 * Transforms screen-space coordinates to clip space and passes through
 * texture coordinates and vertex colors.
 */

cbuffer Uniforms : register(b0, space1)
{
    float2 screen_size;
    float2 padding;  /* Align to 16 bytes */
};

struct VSInput
{
    float2 position : POSITION;
    float2 texcoord : TEXCOORD0;
    uint color : COLOR0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Convert screen coordinates to normalized device coordinates */
    /* Screen space: (0,0) top-left, (width,height) bottom-right */
    /* NDC: (-1,-1) bottom-left, (1,1) top-right */
    float2 ndc;
    ndc.x = (input.position.x / screen_size.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (input.position.y / screen_size.y) * 2.0;  /* Flip Y */

    output.position = float4(ndc, 0.0, 1.0);
    output.texcoord = input.texcoord;

    /* Unpack color from uint32 (0xAABBGGRR) to float4 */
    output.color.r = float((input.color >> 0) & 0xFF) / 255.0;
    output.color.g = float((input.color >> 8) & 0xFF) / 255.0;
    output.color.b = float((input.color >> 16) & 0xFF) / 255.0;
    output.color.a = float((input.color >> 24) & 0xFF) / 255.0;

    return output;
}
