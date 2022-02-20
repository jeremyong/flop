#include "Fullscreen.hpp"

#include "Image.hpp"
#include "Kernel.hpp"

#include <FullscreenVS_spv.h>

using namespace flop;

static VkShaderModule s_vs;

void Fullscreen::init(uint8_t const* shader_bytecode,
                      size_t bytecode_size,
                      uint8_t pushconstant_size)
{
    VkPushConstantRange push_constant_range{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = pushconstant_size};
    VkPipelineLayoutCreateInfo layout_info{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &g_descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range};
    vkCreatePipelineLayout(g_device, &layout_info, nullptr, &layout_);

    pushconstant_size_ = pushconstant_size;

    if (s_vs == VK_NULL_HANDLE)
    {
        s_vs = Kernel::compile_shader(
            FullscreenVS_spv_data, FullscreenVS_spv_size);
    }
    VkShaderModule ps_shader
        = Kernel::compile_shader(shader_bytecode, bytecode_size);

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = s_vs,
            .pName  = "VSMain",
        },
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = ps_shader,
            .pName  = "PSMain",
        },
    };

    VkPipelineVertexInputStateCreateInfo vi{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 0,
        .pVertexBindingDescriptions      = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions    = nullptr,
    };

    VkPipelineInputAssemblyStateCreateInfo ia{
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo v{
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = nullptr,
        .scissorCount  = 1,
        .pScissors     = nullptr};
    VkPipelineRasterizationStateCreateInfo rs{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = false,
        .rasterizerDiscardEnable = false,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth               = 1.f};
    VkPipelineMultisampleStateCreateInfo ms{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable   = VK_FALSE,
        .pSampleMask           = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE};
    VkPipelineDepthStencilStateCreateInfo ds{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState attachment{
        .blendEnable    = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                          | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    VkPipelineColorBlendStateCreateInfo cbs{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &attachment};
    VkDynamicState dynamic_states[]
        = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates    = dynamic_states};

    VkFormat color_target = VK_FORMAT_R8G8B8A8_SRGB;
    VkPipelineRenderingCreateInfoKHR rendering_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &color_target,
    };

    VkGraphicsPipelineCreateInfo pipeline_info{
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &rendering_info,
        .stageCount          = 2,
        .pStages             = stages,
        .pVertexInputState   = &vi,
        .pInputAssemblyState = &ia,
        .pTessellationState  = nullptr,
        .pViewportState      = &v,
        .pRasterizationState = &rs,
        .pMultisampleState   = &ms,
        .pDepthStencilState  = &ds,
        .pColorBlendState    = &cbs,
        .pDynamicState       = &dyn,
        .layout              = layout_,
        .renderPass          = VK_NULL_HANDLE,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = 0,
    };
    vkCreateGraphicsPipelines(
        g_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_);

    vkDestroyShaderModule(g_device, ps_shader, nullptr);
}

void Fullscreen::reset()
{
    if (pipeline_ != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(g_device, pipeline_, nullptr);
        vkDestroyPipelineLayout(g_device, layout_, nullptr);
    }
}

void Fullscreen::render(VkCommandBuffer cb, Image const& color_target, void* push_constants) const
{
    VkRect2D render_area = {.offset = {}, .extent = color_target.extent2_};
    VkRenderingAttachmentInfoKHR attachment{
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
        .imageView   = color_target.image_view_,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
    };
    VkRenderingInfoKHR rendering_info{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        .renderArea           = render_area,
        .layerCount           = 1,
        .viewMask             = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &attachment,
    };
    vkCmdBeginRenderingKHR(cb, &rendering_info);

    VkViewport viewport{.x        = 0,
                        .y        = 0,
                        .width    = static_cast<float>(render_area.extent.width),
                        .height   = static_cast<float>(render_area.extent.height),
                        .minDepth = 0.f,
                        .maxDepth = 1.f};
    vkCmdSetViewport(cb, 0, 1, &viewport);

    vkCmdSetScissor(cb, 0, 1, &render_area);

    vkCmdBindDescriptorSets(cb,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            layout_,
                            0,
                            1,
                            &g_descriptor_set,
                            0,
                            nullptr);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    vkCmdPushConstants(cb,
                       layout_,
                       VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       pushconstant_size_,
                       push_constants);

    vkCmdDraw(cb, 3, 1, 0, 0);

    vkCmdEndRenderingKHR(cb);
}
