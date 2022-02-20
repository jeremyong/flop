#pragma once

#include "Buffer.hpp"
#include "Image.hpp"
#include "Kernel.hpp"
#include "Fullscreen.hpp"

namespace flop
{
struct ImagePacket
{
    // Pipeline:
    // The source_ image is first converted to linearized CIELAB space (YyCxCz)
    // in yycxcz_. The yycxcz_ image is then blurred in the x direction into
    // yycxcz_blur_x. That result is then blurred in the y direction back into
    // yycxcz_.
    Image source_;
    Image yycxcz_;
    Image yycxcz_blur_x_;
    Image yycxcz_blurred_;
    Image feature_blur_x_;
};

inline ImagePacket g_reference;
inline ImagePacket g_test;
inline Image g_error;
inline Image g_error_color;
inline Image g_error_readback;
inline Buffer g_error_histogram;
inline Kernel g_csf_filter_x;
inline Kernel g_csf_filter_y;
inline Kernel g_color_compare;
inline Kernel g_feature_filter_x;
inline Kernel g_feature_filter_y;
inline Kernel g_summarize;
inline Fullscreen g_yycxcz;
inline Fullscreen g_error_color_map;
} // namespace flop
