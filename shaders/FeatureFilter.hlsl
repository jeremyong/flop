// Edge and point filters are used to amplify color differences in the final error map

// Gaussian 3-sigma kernel
static const float kernel[] = {
    0.14530192, 0.13598623, 0.11147196, 0.08003564, 0.05033249, 0.02772429, 0.01337580, 0.00565231, 0.00209209, 0.00067823
};
// First-derivative (edge detector)
// NOTE: When applying the left half of this kernel, the signs must be flipped
static const float kernel1[] = {
    0.00000000, -0.12572107, -0.20611460, -0.22198204, -0.18613221, -0.12815738, -0.07419661, -0.03657945, -0.01547329, -0.00564333
};
// Second-derivative (point detector)
static const float kernel2[] = {
    -0.29897641, -0.24272794, -0.10778385, 0.03217138, 0.11763449, 0.13377647, 0.10521736, 0.06477641, 0.03265116, 0.01377273
};

#define KERNEL_RADIUS 9

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
    // Input image expected to be in YyCxCz space (linearized CIELAB)
    uint input1;
    uint input2;
    uint output1;
    uint output2;
};
[[vk::push_constant]]
PushConstants constants;

[[vk::binding(1)]]
RWTexture2D<float4> rwtextures[];

#if DIRECTION == 0
// In the initial horizontal pass, we filter the luminance component
groupshared float data1[KERNEL_RADIUS * 2 + THREAD_COUNT];
groupshared float data2[KERNEL_RADIUS * 2 + THREAD_COUNT];
#else
// In the vertical pass, we need to filter all three moments
groupshared float3 data1[KERNEL_RADIUS * 2 + THREAD_COUNT];
groupshared float3 data2[KERNEL_RADIUS * 2 + THREAD_COUNT];
#endif

// Normalize luminance to [0, 1]
float normalize_Yy(float Yy)
{
    static const float scale = 1.0 / 116.0;
    static const float bias = 16.0 / 116.0;

    return Yy * scale + bias;
}

#if DIRECTION == 0
[numthreads(THREAD_COUNT, 1, 1)]
#else
[numthreads(1, THREAD_COUNT, 1)]
#endif
void CSMain(uint3 id : SV_DispatchThreadID, int3 gtid : SV_GroupThreadID, int3 gid : SV_GroupID)
{
    RWTexture2D<float4> input1 = rwtextures[constants.input1];
    RWTexture2D<float4> input2 = rwtextures[constants.input2];

    const uint lds_offset = gtid[DIRECTION] + KERNEL_RADIUS;

    // First, fetch all texture values needed starting with the central values
    int2 uv = clamp(id.xy, int2(0, 0), constants.extent - int2(1, 1));

#if DIRECTION == 0
    data1[lds_offset] = normalize_Yy(input1[uv].r);
    data2[lds_offset] = normalize_Yy(input2[uv].r);
#else
    data1[lds_offset] = input1[uv].rgb;
    data2[lds_offset] = input2[uv].rgb;
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
        data1[offset] = normalize_Yy(input1[uv].r);
        data2[offset] = normalize_Yy(input2[uv].r);
#else
        data1[offset] = input1[uv].rgb;
        data2[offset] = input2[uv].rgb;
#endif
    }

    GroupMemoryBarrierWithGroupSync();
    // At this point, all input values in our sliding window are in LDS and ready to use

#if DIRECTION == 0
    float3 moments;
    moments.xz = data1[lds_offset] * float2(kernel[0], kernel[2]);
    moments.y = 0.0;

    for (int i = 1; i != KERNEL_RADIUS; ++i)
    {
        float left = data1[lds_offset - i];
        float right = data1[lds_offset + i];
        moments.xz += (left + right) * float2(kernel[i], kernel2[i]);
        moments.y += kernel1[i] * (right - left);
    }

    if (id.x < constants.extent.x && id.y < constants.extent.y)
    {
        rwtextures[constants.output1][id.xy].rgb = moments;
    }

    moments.xz = data2[lds_offset] * float2(kernel[0], kernel[2]);
    moments.y = 0.0;

    for (int j = 1; j != KERNEL_RADIUS; ++j)
    {
        float left = data2[lds_offset - j];
        float right = data2[lds_offset + j];
        moments.xz += (left + right) * float2(kernel[j], kernel2[j]);
        moments.y += kernel1[j] * (right - left);
    }

    if (id.x < constants.extent.x && id.y < constants.extent.y)
    {
        rwtextures[constants.output2][id.xy].rgb = moments;
    }
#else
    // Find filtered x and y derivatives, with edges in compoment 0, points in component 1
    // Intuitively, we have the Gaussian-blurred luminance in the x direction, so we need to
    // apply the edge and point filters to that quantity. For the y direction, we have the
    // edge and point filtered luminance values, so we convolve those quantities with the
    // Gaussian.
    float2 features1_x = kernel[0] * data1[lds_offset].yz;
    float2 features1_y = float2(kernel[1], kernel[2]) * data1[lds_offset].x;

    for (int i = 1; i != KERNEL_RADIUS; ++i)
    {
        float3 left = data1[lds_offset - i];
        float3 right = data1[lds_offset + i];
        features1_x += kernel[i] * (left.yz + right.yz);
        features1_y.y += kernel2[i] * (left.x + right.x);
        features1_y.x += kernel1[i] * (right.x - left.x);
    }

    float2 features1 = sqrt(features1_x * features1_x + features1_y * features1_y);

    float2 features2_x = kernel[0] * data2[lds_offset].yz;
    float2 features2_y = float2(kernel[1], kernel[2]) * data2[lds_offset].x;

    for (int j = 1; j != KERNEL_RADIUS; ++j)
    {
        float3 left = data2[lds_offset - j];
        float3 right = data2[lds_offset + j];
        features2_x += kernel[j] * (left.yz + right.yz);
        features2_y.y += kernel2[j] * (left.x + right.x);
        features2_y.x += kernel1[j] * (right.x - left.x);
    }

    float2 features2 = sqrt(features2_x * features2_x + features2_y * features2_y);

    if (id.x < constants.extent.x && id.y < constants.extent.y)
    {
        RWTexture2D<float4> output = rwtextures[constants.output1];

        // We can now compare features and use differences in edges and points detected to
        // amplify the color error

        float2 feature_delta = abs(features2 - features1);
        // TODO: make 0.5 configurable to amplify or dampen error due to feature differences
        float feature_error = pow(max(feature_delta.x, feature_delta.y) / sqrt(2), 0.5);

        float color_error = output[id.xy].r;

        // The moment we've all been waiting for
        float flip_error = pow(color_error, 1.0 - feature_error);

        output[id.xy].rgb = flip_error;
    }
#endif
}
