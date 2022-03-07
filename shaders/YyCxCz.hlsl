#include "Common.hlsli"

// Convert linearized RGB to YyCxCz
// https://engineering.purdue.edu/~bouman/software/YCxCz/pdf/ColorFidelityMetrics.pdf
// https://engineering.purdue.edu/~bouman/publications/pdf/ei93.pdf
// http://users.ece.utexas.edu/~bevans/papers/2003/colorHalftoning/colorHVSspl00282.pdf

// Assumes the input and output image dimensions are the same
struct PushConstants
{
    uint2 extent;
    float2 uv_offset;
    float uv_scale;
    uint input;
    uint tonemap;
    float exposure;
    // If 1, multiply Yy component by alpha to account for alpha differences
    uint handle_alpha;
};

[[vk::push_constant]]
PushConstants constants;

[[vk::binding(0)]]
Texture2D<float4> textures[];

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSMain(VSOutput IN) : SV_Target0
{
    Texture2D<float4> input_texture = textures[constants.input];
    float4 color = input_texture.Load(int3(IN.uv * constants.extent, 0));

    if (constants.tonemap == 1)
    {
        color.rgb = aces_tonemap(constants.exposure * color.rgb);
    }
    else if (constants.tonemap == 2)
    {
        color.rgb = reinhard_tonemap(constants.exposure * color.rgb);
    }
    else if (constants.tonemap == 3)
    {
        color.rgb = hable_tonemap(constants.exposure * color.rgb);
    }

    float3 YyCxCz = rgb_to_linearized_Lab(color.rgb);

    if (constants.handle_alpha != 0)
    {
        YyCxCz.x *= color.a;
    }

    return float4(YyCxCz, 1.f);
}
