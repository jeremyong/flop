
struct PushConstants
{
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

[[vk::binding(3)]]
SamplerState texture_sampler;

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(uint id : SV_VertexID)
{
    VSOutput OUT;

    // 0 -> (-1, -1, 0.5, 1)
    // 1 -> ( 3, -1, 0.5, 1)
    // 2 -> (-1,  3, 0.5, 1)
    OUT.position.x = id == 1 ? 3.0 : -1.0;
    OUT.position.y = id == 2 ? 3.0 : -1.0;
    OUT.position.zw = float2(0.0, 1.0);
    OUT.uv = OUT.position.xy * 0.5 + 0.5;
    OUT.uv = constants.uv_scale * OUT.uv + constants.uv_offset;

    return OUT;
}

struct PSOutput
{
    float4 color : SV_Target0;
};

PSOutput PSMain(VSOutput IN)
{
    PSOutput OUT;

#ifdef COLORMAP
    if (IN.uv.x > 1.0 || IN.uv.y > 1.0 || IN.uv.x < 0.0 || IN.uv.y < 0.0)
    {
        OUT.color = float4(0.0, 0.0, 0.0, 1.0);
        return OUT;
    }

    float r = textures[constants.input].Sample(texture_sampler, IN.uv).r;
    float u = r * 255.0 + 0.5;
    uint left_index = clamp(floor(u), 0.0, 255.0);
    uint right_index = clamp(ceil(u), left_index, 255.0);
    // Interpolate between left and right endpoints
    float3 left = buffers[constants.color_map].Load<float3>(left_index * 12);
    float3 right = buffers[constants.color_map].Load<float3>(right_index * 12);
    OUT.color = float4(lerp(left, right, frac(u)), 1.0);
#else
    OUT.color = textures[constants.input].Sample(texture_sampler, IN.uv);
#endif
    return OUT;
}
