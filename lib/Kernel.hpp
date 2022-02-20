#pragma once

#include "VkGlobals.hpp"

#include "Buffer.hpp"
#include "Image.hpp"

class Kernel
{
public:
    struct PushConstants
    {
        int32_t extent[2];
        uint32_t input;
        uint32_t output;
    };

    struct ComparePushConstants
    {
        int32_t extent[2];
        uint32_t input1;
        uint32_t input2;
        uint32_t output1;
        // output2 isn't always used
        uint32_t output2;
    };

    static void init_dxc();
    static Kernel create(uint8_t const* data,
                         size_t size,
                         int thread_count_x,
                         int thread_count_y,
                         bool is_compare_kernel);
    static VkShaderModule compile_shader(uint8_t const* data, size_t size);

    void dispatch(VkCommandBuffer cb, Image const& input, Image const& output);
    void dispatch(VkCommandBuffer cb,
                  Image const& input1,
                  Image const& input2,
                  Image const& output);
    void dispatch(VkCommandBuffer cb,
                  Image const& input,
                  Image const& output,
                  Buffer const& buffer);
    void dispatch(VkCommandBuffer cb,
                  Image const& input1,
                  Image const& input2,
                  Image const& output1,
                  Image const& output2);
    void dispatch(VkCommandBuffer cb, Image const& input, Buffer const& output);

private:
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    int thread_count_x_  = 0;
    int thread_count_y_  = 0;
};
