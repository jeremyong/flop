// Reduction kernel to summarize stats in a histogram

struct PushConstants
{
    uint2 extent;
    uint input;
    uint output;
};
[[vk::push_constant]]
PushConstants constants;

[[vk::binding(1)]]
RWTexture2D<float4> rwtextures[];

[[vk::binding(2)]]
RWByteAddressBuffer rwbuffers[];

// Construct an LDS histogram with 32 entries
#define BUCKET_COUNT 32
groupshared uint histogram[BUCKET_COUNT];

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID)
{
    uint linear_gtid = gtid.x * 8 + gtid.y;
    if (linear_gtid < BUCKET_COUNT)
    {
        histogram[linear_gtid] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    if (id.x < constants.extent[0] && id.y < constants.extent[1])
    {
        RWTexture2D<float4> error_texture = rwtextures[constants.input];

        float error = clamp(error_texture[id.xy].r, 0.0, 1.0);

        InterlockedAdd(histogram[floor(error * (BUCKET_COUNT - 1))], 1);
    }

    GroupMemoryBarrierWithGroupSync();

    if (linear_gtid < BUCKET_COUNT)
    {
        rwbuffers[constants.output].InterlockedAdd(linear_gtid * 4, histogram[linear_gtid]);
    }
}
