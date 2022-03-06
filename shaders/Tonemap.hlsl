#include "Common.hlsli"

// Assumes the input and output image dimensions are the same
struct PushConstants
{
    uint2 extent;
    float2 uv_offset;
    float uv_scale;
    uint input;
    float exposure;
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

    color.rgb = aces_tonemap(constants.exposure * color.rgb);

    return color;
}
