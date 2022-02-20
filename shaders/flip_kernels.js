// This script was used to compute constants used in the algorithm offline,
// as described in the paper: https://research.nvidia.com/publication/2020-07_FLIP

// PPD (pixels per degree) below assumes a 0.709x0.399 m^2 monitor at 4k resolution (3840x2160)
// at a distance of 0.70 meters. Because the aspect ratio is assumed to be 1:1, we can restrict
// the formula to just one dimension

function ppd(length, resolution, distance)
{
    return distance * resolution / length * Math.PI / 180;
}
// Dimensions of a 32" 4k TV/Monitor
const ppd_x = ppd(0.709, 3840, 0.7);
const ppd_y = ppd(0.399, 2160, 0.7);
console.log('Pixels per degree (x): ', ppd_x);
console.log('Pixels per degree (y): ', ppd_y);

// The original paper rounds this quantity down to 67
const p = Math.ceil((ppd_x + ppd_y) / 2);
const spacing = 1 / p;
console.log('Spacing between two samples: ', spacing);

// CSFs are approximated using 4 gaussians. 1 each for the luminance and red-green CSFs,
// and 2 for the blue-yellow CSF

// Gaussian:
// g(x) = a * sqrt(pi / b) * exp(-pi^2 / b * x^2)

// The filters are normalized later so a isn't needed for S_y and S_x.
// S_y: a -> ?, b -> 0.0047
// S_x: a -> ?, b -> 0.0053
// S_z: a1 -> 34.1, b1 -> 0.04, a2 -> 13.5, b2 -> 0.025

const b_Sy = 0.0047;
const b_Sx = 0.0053;
const a1_Sz = 34.1;
const a2_Sz = 13.5;
const b1_Sz = 0.04;
const b2_Sz = 0.025;

// The filter radius for all 3 CSFs is the same in the paper, which they choose to be the
// radius corresponding to the CSF with the widest spread (S_z with b at 0.04). This is
// pretty inefficient it turns out, given that the filter radii needed for Sy and Sx are
// less than half the size of the filter radius for Sz. Furthermore, the second gaussian
// needed to fit the Sz CSF is slightly smaller as well.

function filter_radius(b)
{
    const sigma = Math.sqrt(b / 2 / (Math.PI * Math.PI));
    return Math.ceil(sigma * 3 * p);
}

// Compute filter values for a pixel-centered box kernel for (x, y) in
// {0, +/- 1, +/- 2, ... +/- r}
// Don't bother doing the full box since we'll be performing the filter in two passes
function filter_weights(a, b, radius_override)
{
    const r = radius_override || filter_radius(b);
    const weights = new Array(r + 1);
    for (let i = 0; i < r + 1; ++i)
    {
        const d = spacing * i;
        const d2 = d * d;
        weights[i] = a * Math.sqrt(Math.PI / b) * Math.exp(-Math.PI * Math.PI / b * d2);
    }
    return [weights, r];
}

function filter_weights_sqrt(a, b, radius_override)
{
    const r = radius_override || filter_radius(b);
    const weights = new Array(r + 1);
    for (let i = 0; i < r + 1; ++i)
    {
        const d = spacing * i;
        const d2 = d * d;
        weights[i] = Math.sqrt(a * Math.sqrt(Math.PI / b)) * Math.exp(-Math.PI * Math.PI / b * d2);
    }
    return [weights, r];
}

const [weights_Sy, r_Sy] = filter_weights(1, b_Sy);
const [weights_Sx, r_Sx] = filter_weights(1, b_Sx);
const [weights1_Sz, r1_Sz] = filter_weights_sqrt(a1_Sz, b1_Sz);
const [weights2_Sz, r2_Sz] = filter_weights_sqrt(a2_Sz, b2_Sz, filter_radius(b1_Sz));

console.log(`Filter radii (r_Sy, r_Sx, r_Sz): ${r_Sy}, ${r_Sx}, ${r1_Sz}`);

function sum_weights(weights)
{
    let sum = 0;
    for (let i = 1; i != weights.length; ++i)
    {
        sum += weights[i];
    }
    // Double both sides, then add the center-point
    sum *= 2;
    sum += weights[0];
    return sum;
}

const norm_Sy = 1 / sum_weights(weights_Sy);
const norm_Sx = 1 / sum_weights(weights_Sx);
const norm_Sz1 = sum_weights(weights1_Sz);
const norm_Sz2 = sum_weights(weights2_Sz);
console.log(norm_Sz1, norm_Sz2);
const norm_Sz = 1 / Math.sqrt(norm_Sz1 * norm_Sz1 + norm_Sz2 * norm_Sz2);
console.log(`Norm factors: ${norm_Sy}, ${norm_Sx}, ${norm_Sz}`);

// The weights need to be normalized such that the sum of all weights sum to one
function normalize_weights(weights, factor)
{
    for (let i = 0; i != weights.length; ++i)
    {
        weights[i] = weights[i] * factor;
    }
}

normalize_weights(weights_Sy, norm_Sy);
console.log(`Sy: ${weights_Sy.map(v => v.toFixed(8)).join(', ')}`);

normalize_weights(weights_Sx, norm_Sx);
console.log(`Sx: ${weights_Sx.map(v => v.toFixed(8)).join(', ')}`);

normalize_weights(weights1_Sz, norm_Sz);
normalize_weights(weights2_Sz, norm_Sz);
console.log(`Sz: ${weights1_Sz.map(v => v.toFixed(8)).join(', ')}`);
console.log(`Sz: ${weights2_Sz.map(v => v.toFixed(8)).join(', ')}`);

// From "Estimates of edge detection filters in human vision"
// https://www.sciencedirect.com/science/article/pii/S0042698918302050
// HVS width from highest to lowest amplitude is 0.082 degrees
const feature_std_dev = 0.5 * 0.082 * 67;
console.log(`Feature std dev: ${feature_std_dev}`);
const feature_kernel_radius = Math.ceil(feature_std_dev * 3); // 9 pixels
console.log(`Feature kernel radius: ${feature_kernel_radius}`);

// Compute weights for the first and second partial derivatives of a Gaussian kernel

function feature_weights()
{
    // Three weight arrays for the 0th, 1st, and 2nd moments
    const weights = [
        new Array(feature_kernel_radius + 1),
        new Array(feature_kernel_radius + 1),
        new Array(feature_kernel_radius + 1),
    ];
    let b = 0.5 / feature_std_dev / feature_std_dev;

    let sum = 0;
    let sum_dx = 0;
    // For the second partial derivatives, compute the weight sums separately
    let sum_ddx = [0, 0];
    for (let i = 0; i != weights[0].length; ++i)
    {
        weights[0][i] = Math.exp(-i * i * b);
        sum += weights[0][i];

        // First derivative
        weights[1][i] = -i * weights[0][i];
        if (weights[1][i] > 0)
        {
            sum_dx += weights[1][i];
        }
        else
        {
            sum_dx -= weights[1][i];
        }

        // Second derivative
        weights[2][i] = (i * i * b * 2 - 1) * weights[0][i];
        if (weights[2][i] > 0)
        {
            sum_ddx[0] += weights[2][i];
        }
        else
        {
            sum_ddx[1] -= weights[2][i];
        }
    }

    // Fix the sums to account for axial symmetry
    sum = 2 * (sum - weights[0][0]) + weights[0][0];

    // The first derivative kernel is mirrored across the origin and are accounted
    // for already by accumulating both positive and negative weights into a single
    // quantity.

    console.log(sum_ddx);
    console.log(weights[2][0])
    // The second derivative is symmetric about the origin
    if (weights[2][0] > 0)
    {
        sum_ddx[0] = 2 * (sum_ddx[0] - weights[2][0]) + weights[2][0];
        sum_ddx[1] *= 2;
    }
    else
    {
        sum_ddx[0] *= 2;
        sum_ddx[1] = 2 * (sum_ddx[1] + weights[2][0]) - weights[2][0];
    }

    console.log(`Feature sums: ${sum}, ${sum_dx}, ${sum_ddx[0]}:${sum_ddx[1]}`);

    // Normalize weights
    for (let i = 0; i != weights[0].length; ++i)
    {
        weights[0][i] /= sum;
        weights[1][i] /= sum_dx;
        if (weights[2][i] > 0)
        {
            weights[2][i] /= sum_ddx[0];
        }
        else
        {
            weights[2][i] /= sum_ddx[1];
        }
    }
    return weights;
}

const feature_kernel_weights = feature_weights();
console.log(`${feature_kernel_weights[0].map(v => v.toFixed(8)).join(', ')}`);
console.log(`${feature_kernel_weights[1].map(v => v.toFixed(8)).join(', ')}`);
console.log(`${feature_kernel_weights[2].map(v => v.toFixed(8)).join(', ')}`);
