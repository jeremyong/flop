#include "Kernel.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

VkPipelineLayout s_kernel_layout;
VkPipelineLayout s_compare_kernel_layout;

using namespace flop;

VkShaderModule Kernel::compile_shader(uint8_t const* data, size_t size)
{
    VkShaderModule shader_module;

    VkShaderModuleCreateInfo info{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode    = reinterpret_cast<uint32_t const*>(data)};
    if (vkCreateShaderModule(g_device, &info, nullptr, &shader_module)
        != VK_SUCCESS)
    {
        std::cout << "Failed to create shader module.\n";
    }
    return shader_module;
}

Kernel Kernel::create(uint8_t const* data,
                      size_t size,
                      int thread_count_x,
                      int thread_count_y,
                      bool is_compare_kernel)
{
    Kernel out;
    out.thread_count_x_ = thread_count_x;
    out.thread_count_y_ = thread_count_y;

    VkShaderModule shader_module = compile_shader(data, size);

    if (s_kernel_layout == VK_NULL_HANDLE)
    {
        VkPushConstantRange push_constant_range{
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset     = 0,
            .size       = sizeof(PushConstants)};

        VkPipelineLayoutCreateInfo pipeline_layout_info{
            .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts    = &g_descriptor_set_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &push_constant_range};

        vkCreatePipelineLayout(
            g_device, &pipeline_layout_info, nullptr, &s_kernel_layout);

        push_constant_range.size = sizeof(ComparePushConstants);
        vkCreatePipelineLayout(
            g_device, &pipeline_layout_info, nullptr, &s_compare_kernel_layout);
    }

    VkComputePipelineCreateInfo pipeline_info{
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader_module,
            .pName = "CSMain",
            .pSpecializationInfo = nullptr,
        },
        .layout = is_compare_kernel ? s_compare_kernel_layout : s_kernel_layout,
    };
    vkCreateComputePipelines(
        g_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &out.pipeline_);

    return out;
}

uint32_t div_round_up(int a, int b)
{
    return (a + b - 1) / b;
}

void Kernel::dispatch(VkCommandBuffer cb, Image const& input, Image const& output)
{
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(cb,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            s_kernel_layout,
                            0,
                            1,
                            &g_descriptor_set,
                            0,
                            nullptr);

    PushConstants push_constants{.extent = {input.width_, input.height_},
                                 .input  = input.index_,
                                 .output = output.index_};
    vkCmdPushConstants(cb,
                       s_kernel_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(PushConstants),
                       &push_constants);
    vkCmdDispatch(cb,
                  div_round_up(input.width_, thread_count_x_),
                  div_round_up(input.height_, thread_count_y_),
                  1);
}

void Kernel::dispatch(VkCommandBuffer cb,
                      Image const& input1,
                      Image const& input2,
                      Image const& output)
{
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(cb,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            s_compare_kernel_layout,
                            0,
                            1,
                            &g_descriptor_set,
                            0,
                            nullptr);

    ComparePushConstants push_constants{.extent = {input1.width_, input1.height_},
                                        .input1  = input1.index_,
                                        .input2  = input2.index_,
                                        .output1 = output.index_};
    vkCmdPushConstants(cb,
                       s_compare_kernel_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(ComparePushConstants),
                       &push_constants);
    vkCmdDispatch(cb,
                  div_round_up(input1.width_, thread_count_x_),
                  div_round_up(input1.height_, thread_count_y_),
                  1);
}

void Kernel::dispatch(VkCommandBuffer cb,
                      Image const& input,
                      Image const& output,
                      Buffer const& buffer)
{
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(cb,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            s_compare_kernel_layout,
                            0,
                            1,
                            &g_descriptor_set,
                            0,
                            nullptr);

    ComparePushConstants push_constants{.extent = {input.width_, input.height_},
                                        .input1 = input.index_,
                                        .input2 = output.index_,
                                        .output1 = buffer.index_};
    vkCmdPushConstants(cb,
                       s_compare_kernel_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(ComparePushConstants),
                       &push_constants);
    vkCmdDispatch(cb,
                  div_round_up(input.width_, thread_count_x_),
                  div_round_up(input.height_, thread_count_y_),
                  1);
}

void Kernel::dispatch(VkCommandBuffer cb,
                      Image const& input1,
                      Image const& input2,
                      Image const& output1,
                      Image const& output2)
{
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(cb,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            s_compare_kernel_layout,
                            0,
                            1,
                            &g_descriptor_set,
                            0,
                            nullptr);

    ComparePushConstants push_constants{.extent = {input1.width_, input1.height_},
                                        .input1  = input1.index_,
                                        .input2  = input2.index_,
                                        .output1 = output1.index_,
                                        .output2 = output2.index_};
    vkCmdPushConstants(cb,
                       s_compare_kernel_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(ComparePushConstants),
                       &push_constants);
    vkCmdDispatch(cb,
                  div_round_up(input1.width_, thread_count_x_),
                  div_round_up(input1.height_, thread_count_y_),
                  1);
}

void Kernel::dispatch(VkCommandBuffer cb, Image const& input, Buffer const& output)
{
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(cb,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            s_kernel_layout,
                            0,
                            1,
                            &g_descriptor_set,
                            0,
                            nullptr);

    PushConstants push_constants{.extent = {input.width_, input.height_},
                                 .input  = input.index_,
                                 .output = output.index_};
    vkCmdPushConstants(cb,
                       s_compare_kernel_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(PushConstants),
                       &push_constants);
    vkCmdDispatch(cb,
                  div_round_up(input.width_, thread_count_x_),
                  div_round_up(input.height_, thread_count_y_),
                  1);
}
