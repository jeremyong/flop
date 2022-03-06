#include "Common.hlsli"

struct PushConstants
{
    uint2 extent;
    uint reference;
    uint test;
    uint output;
};
[[vk::push_constant]]
PushConstants constants;

[[vk::binding(1)]]
RWTexture2D<float4> rwtextures[];

void hunt_adjust(inout float3 Lab)
{
    // Luminance is in the 0 to 100 range, so this scale factor is in [0, 1]
    float scale = 0.01 * Lab.x;

    // Dampen chrominance at lower luminance levels
    Lab.yz *= scale;
}

// Distance metrics for very large color differences
// http://markfairchild.org/PDFs/PAP40.pdf
//
// Both inputs are expected to be in CIELAB space
float HyAB_error(float3 r, float3 t)
{
    float L_distance = abs(r.x - t.x);
    float a_delta = r.y - t.y;
    float b_delta = r.z - t.z;
    float ab_distance = sqrt(a_delta * a_delta + b_delta * b_delta);
    return L_distance + ab_distance;
}

// Max HyAB error is given by computing HyAB_error(green, blue)^0.7 offline
static const float max_HyAB_error = 41.2760963;

// When remapping HyAB error to [0, 1], FLIP linearly remaps values below this
// cutoff to [0, 0.95). Values above this cutoff are linearly remapped to the
// rest of the range.
static const float cutoff = 0.4 * max_HyAB_error;
static const float bias = 0.95;
static const float cutoff_scale = bias / cutoff;

// The FLIP paper notes that large differences ought to be compressed since the
// distinction between white-black or green-blue are 3 times apart.
float remap_HyAB_error(float error)
{
    error = pow(error, 0.7);

    if (error < cutoff)
    {
        error *= cutoff_scale;
    }
    else
    {
        error = 0.05 * (error - cutoff) / (max_HyAB_error - cutoff) + bias;
    }
    return error;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= constants.extent[0] || id.y >= constants.extent[1])
    {
        return;
    }

    // In the original flip paper, they apply an adjustment to the colors in
    // CIELAB space to account for the Hunt effect (chromatic differences are
    // more perceptually pronounced at higher luminance levels)

    RWTexture2D<float4> reference_image = rwtextures[constants.reference];
    RWTexture2D<float4> test_image = rwtextures[constants.test];

    float3 colors[2] = { reference_image[id.xy].rgb, test_image[id.xy].rgb };

    colors[0] = xyz_to_CIELAB(colors[0]);
    colors[1] = xyz_to_CIELAB(colors[1]);

    hunt_adjust(colors[0]);
    hunt_adjust(colors[1]);

    float error = HyAB_error(colors[0], colors[1]);
    error = remap_HyAB_error(error);

    rwtextures[constants.output][id.xy].r = error;
}
