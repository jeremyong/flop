#include "Common.hlsli"

// The CSF Gaussian blurs are done in two passes. First, we blur in the x direction,
// then in the y direction (this choice is arbitrary since the Gaussian decomposition
// is commutes).

// These values are computed using the flip_kernels.js script
static const float sy_kernel[] = {
    0.39172750, 0.24189219, 0.05695543, 0.00511357, 0.00017506
    };
static const float sx_kernel[] = {
    0.36889303, 0.24056897, 0.06672016, 0.00786960, 0.00039475
    };
static const float sz_kernel1[] = {
    0.11730367, 0.11084383, 0.09352148, 0.07045487, 0.04739261, 0.02846493, 0.01526544, 0.00730985, 0.00312541, 0.00119318
};
static const float sz_kernel2[] = {
    0.08301017, 0.07581780, 0.05776847, 0.03671896, 0.01947017, 0.00861249, 0.00317810, 0.00097833, 0.00025124, 0.00005382
};

#define KERNEL_RADIUS 9
#define INNER_RADIUS 4

#ifdef DIRECTION_X
#define DIRECTION 0
#endif

#ifdef DIRECTION_Y
#define DIRECTION 1
#endif

#ifndef DIRECTION
#error "DIRECTION not specified. Specify DIRECTION=0 for a horizontal blur, and DIRECTION=1 for a vertical blur"
#endif

#define THREAD_COUNT 64
#if THREAD_COUNT < 2 * KERNEL_RADIUS
#error "THREAD_COUNT is too small for this implementation to work correctly"
#endif

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

groupshared float4 data[KERNEL_RADIUS * 2 + THREAD_COUNT];

#if DIRECTION == 0
[numthreads(THREAD_COUNT, 1, 1)]
#else
[numthreads(1, THREAD_COUNT, 1)]
#endif
void CSMain(uint3 id : SV_DispatchThreadID, int3 gtid : SV_GroupThreadID, int3 gid : SV_GroupID)
{
    RWTexture2D<float4> input = rwtextures[constants.input];
    RWTexture2D<float4> output = rwtextures[constants.output];

    const uint lds_offset = gtid[DIRECTION] + KERNEL_RADIUS;

    // First, fetch all texture values needed starting with the central values
    int2 uv = clamp(id.xy, int2(0, 0), constants.extent - int2(1, 1));
#ifdef DIRECTION_X
    data[lds_offset] = input[uv].rgbb;
#else
    data[lds_offset] = input[uv].rgba;
#endif

    // Now, fetch the front and back of the window
    if (gtid[DIRECTION] < KERNEL_RADIUS * 2)
    {
        uint offset;
        if (gtid[DIRECTION] < KERNEL_RADIUS)
        {
#if DIRECTION == 0
            uv = int2(gid.x * THREAD_COUNT, id.y);
#else
            uv = int2(id.x, gid.y * THREAD_COUNT);
#endif
            uv[DIRECTION] = uv[DIRECTION] - gtid[DIRECTION] - 1;
            offset = KERNEL_RADIUS - gtid[DIRECTION] - 1;
        }
        else
        {
#if DIRECTION == 0
            uv = int2((gid.x + 1) * THREAD_COUNT, id.y);
#else
            uv = int2(id.x, (gid.y + 1) * THREAD_COUNT);
#endif
            uv[DIRECTION] = uv[DIRECTION] + gtid[DIRECTION] - KERNEL_RADIUS;
            offset = THREAD_COUNT + gtid[DIRECTION];
        }

        uv = clamp(uv, int2(0, 0), constants.extent - int2(1, 1));
#if DIRECTION == 0
        data[offset] = input[uv].rgbb;
#else
        data[offset] = input[uv].rgba;
#endif
    }

    GroupMemoryBarrierWithGroupSync();
    // At this point, all input values in our sliding window are in LDS and ready to use

    float4 color = data[lds_offset] * float4(sx_kernel[0], sy_kernel[0], sz_kernel1[0], sz_kernel2[0]);

    [unroll]
    for (int i = 1; i != INNER_RADIUS; ++i)
    {
        float2 xy = data[lds_offset - i].xy + data[lds_offset + i].xy;
        color.xy += float2(sy_kernel[i], sx_kernel[i]) * xy;
    }

    [unroll]
    for (int j = 1; j != KERNEL_RADIUS; ++j)
    {
        float2 zw = data[lds_offset - j].z + data[lds_offset + j].z;
        color.zw += float2(sz_kernel1[j], sz_kernel2[j]) * zw;
    }

    // Write out the result
    if (id.x < constants.extent.x && id.y < constants.extent.y)
    {
#if DIRECTION == 0
        output[id.xy] = color;
#else
        // Now that we've finished the blur passes, convert out of YyCxCz back to sRGB
        output[id.xy] = float4(linearized_Lab_to_rgb(float3(color.rg, color.z + color.w)), 1.0);
#endif
    }
}
