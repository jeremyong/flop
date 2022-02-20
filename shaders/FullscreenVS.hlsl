struct PushConstants
{
    uint2 extent;
    float2 uv_offset;
    float uv_scale;
};
[[vk::push_constant]]
PushConstants constants;

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
