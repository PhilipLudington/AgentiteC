/*
 * Carbon UI Fragment Shader
 *
 * Samples the font atlas texture and multiplies by vertex color.
 * The font atlas stores alpha in the red channel (R8 format).
 * A white pixel at (0,0) allows solid-color rectangles.
 */

Texture2D<float4> FontAtlas : register(t0, space2);
SamplerState FontSampler : register(s0, space2);

struct PSInput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PSInput input) : SV_Target0
{
    /* Sample font atlas (single channel, stored in R) */
    float alpha = FontAtlas.Sample(FontSampler, input.texcoord).r;

    /* Output color with texture alpha */
    return float4(input.color.rgb, input.color.a * alpha);
}
