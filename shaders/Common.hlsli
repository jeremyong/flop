#pragma once

// RGB in this context refers to gamma-expanded sRGB values
static const float3x3 rgb_to_xyz =
    float3x3(
        0.4124564, 0.3575761, 0.1804375,
        0.2126729, 0.7151522, 0.0721750,
        0.0193339, 0.1191920, 0.9503041);

static const float3x3 xyz_to_rgb =
    float3x3(
        3.2404542, -1.5371385, -0.4985314,
        -0.969266,  1.8760108,  0.0415560,
        0.0556434, -0.2040259,  1.0572252);

// Note: The illuminant used here is normalized to 1 instead of 100
static const float3 d65 = float3(0.950489, 1.0, 1.088840);
static const float3 d65_rcp = 1.0 / d65;

float3 rgb_to_linearized_Lab(float3 rgb)
{
    float3 xyz = mul(rgb_to_xyz, rgb) * d65_rcp;

    float Yy = 116 * xyz.y - 16;
    float Cx = 500 * (xyz.x - xyz.y);
    float Cz = 200 * (xyz.y - xyz.z);
    return float3(Yy, Cx, Cz);
}

float3 linearized_Lab_to_rgb(float3 YyCxCz)
{
    float Yy = YyCxCz.x;
    float Cx = YyCxCz.y;
    float Cz = YyCxCz.z;
    float y = (Yy + 16) / 116;
    float x = y + Cx / 500;
    float z = y - Cz / 200;
    float3 xyz = float3(x, y, z) * d65;

    return clamp(mul(xyz_to_rgb, xyz), 0, 1);
}

static const float delta = 0.2068966;
static const float delta2 = delta * delta;
static const float delta2_3 = 3.0 * delta2;
static const float delta2_rcp3 = 1.0 / delta2_3;
static const float delta3 = delta * delta2;
static const float f_bias = 0.1379310;

float CIELAB_f(float v)
{
    if (v > delta3)
    {
        return pow(v, 0.3333333);
    }
    else
    {
        return delta2_rcp3 * v + f_bias;
    }
}

float3 rgb_to_CIELAB(float3 rgb)
{
    float3 xyz = mul(rgb_to_xyz, rgb) * d65_rcp;
    float f_y = CIELAB_f(xyz.y);
    float L = 116 * f_y - 16;
    float a = 500 * (CIELAB_f(xyz.x) - f_y);
    float b = 200 * (f_y - CIELAB_f(xyz.z));
    return float3(L, a, b);
}

float CIELAB_f_inv(float v)
{
    if (v > delta)
    {
        return pow(v, 3);
    }
    else
    {
        return delta2_3 * (v - f_bias);
    }
}

float3 CIELAB_to_rgb(float3 Lab)
{
    float y_arg = (Lab.x + 16) / 116;
    float x = CIELAB_f_inv(y_arg + Lab.y / 500);
    float y = CIELAB_f_inv(y_arg);
    float z = CIELAB_f_inv(y_arg - Lab.z / 200);
    float3 xyz = d65 * float3(x, y, z);
    return clamp(mul(xyz_to_rgb, xyz), 0, 1);
}
