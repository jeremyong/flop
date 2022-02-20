struct PushConstants
{
    uint2 extent;
    float2 uv_offset;
    float uv_scale;
    uint input;
    uint color_map;
};
[[vk::push_constant]]
PushConstants constants;

[[vk::binding(0)]]
Texture2D<float4> textures[];

[[vk::binding(2)]]
ByteAddressBuffer buffers[];

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSMain(VSOutput IN)
    : SV_Target0
{
    Texture2D<float4> error_image = textures[constants.input];
    float error      = error_image.Load(int3(IN.uv * constants.extent, 0)).r;
    float u          = error * 255 + 0.5;
    uint left_index  = clamp(floor(u), 0, 255);
    uint right_index = clamp(ceil(u), left_index, 255);
    // Interpolate between left and right endpoints
    float3 left  = buffers[constants.color_map].Load<float3>(left_index * 12);
    float3 right = buffers[constants.color_map].Load<float3>(right_index * 12);

    return float4(lerp(left, right, frac(u)), 1.0);
}
