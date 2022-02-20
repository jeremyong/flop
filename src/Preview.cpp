#include "Preview.hpp"

#include <GLFW/glfw3.h>
#include <Kernel.hpp>
#include <algorithm>
#include <iostream>
#include <cassert>

#include <PreviewPSColorMap_spv.h>
#include <PreviewPS_spv.h>
#include <PreviewVS_spv.h>

Preview* previews[4];

static VkPipeline s_pipeline;
static VkPipeline s_pipeline_color_map;
static VkPipelineLayout s_pipeline_layout;

struct PushConstants
{
    float uv_offset[2];
    float uv_scale;
    uint32_t input;
    uint32_t color_map;
};
using namespace flop;

void Preview::init(VkRenderPass render_pass)
{
    VkPushConstantRange push_constant_range{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo layout_info{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &g_descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range};
    vkCreatePipelineLayout(g_device, &layout_info, nullptr, &s_pipeline_layout);

    VkShaderModule vs_shader
        = Kernel::compile_shader(PreviewVS_spv_data, PreviewVS_spv_size);
    VkShaderModule ps_shader
        = Kernel::compile_shader(PreviewPS_spv_data, PreviewPS_spv_size);
    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vs_shader,
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

    VkGraphicsPipelineCreateInfo pipeline_info{
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
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
        .layout              = s_pipeline_layout,
        .renderPass          = render_pass,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = 0,
    };
    vkCreateGraphicsPipelines(
        g_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &s_pipeline);

    vkDestroyShaderModule(g_device, ps_shader, nullptr);
    ps_shader = Kernel::compile_shader(
        PreviewPSColorMap_spv_data, PreviewPSColorMap_spv_size);
    stages[1].module = ps_shader;

    vkCreateGraphicsPipelines(
        g_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &s_pipeline_color_map);

    vkDestroyShaderModule(g_device, vs_shader, nullptr);
    vkDestroyShaderModule(g_device, ps_shader, nullptr);
}

void Preview::set_quadrant(Quadrant quadrant)
{
    quadrant_ = quadrant;
}

void Preview::set_image(Image& image)
{
    image_ = &image;
}

void Preview::set_viewport(Rect2D viewport)
{
    viewport_ = viewport;
    if (!image_ || !image_->image_)
    {
        viewport_dirty_ = true;
        return;
    }

    if (image_->aspect() < preview_viewport_.width / preview_viewport_.height)
    {
        preview_viewport_.height = viewport.y2 - viewport.y1;
        preview_viewport_.width  = preview_viewport_.height / image_->aspect();
    }
    else
    {
        preview_viewport_.width  = viewport.x2 - viewport.x1;
        preview_viewport_.height = preview_viewport_.width / image_->aspect();
    }
    preview_viewport_.minDepth = 0.f;
    preview_viewport_.maxDepth = 1.f;

    preview_scissor_.extent.width  = 0.5f * (viewport.x2 - viewport.x1);
    preview_scissor_.extent.height = 0.5f * (viewport.y2 - viewport.y1);

    if (image_->aspect() < preview_viewport_.width / preview_viewport_.height)
    {
        uv_scale_ = preview_viewport_.width / preview_scissor_.extent.width;
    }
    else
    {
        uv_scale_ = preview_viewport_.height / preview_scissor_.extent.height;
    }

    switch (quadrant_)
    {
        using enum Quadrant;
    case TopLeft:
        preview_viewport_.x = viewport.x1;
        preview_viewport_.y = viewport.y1;
        break;
    case TopRight:
        preview_viewport_.x = (viewport.x2 - viewport.x1) * 0.5f;
        preview_viewport_.y = viewport.y1;
        break;
    case BottomRight:
        preview_viewport_.x = (viewport.x2 - viewport.x1) * 0.5f;
        preview_viewport_.y = (viewport.y2 - viewport.y1) * 0.5f;
        break;
    case BottomLeft:
        preview_viewport_.x = viewport.x1;
        preview_viewport_.y = (viewport.y2 - viewport.y1) * 0.5f;
        break;
    default:
        break;
    }

    preview_scissor_.offset.x = preview_viewport_.x;
    preview_scissor_.offset.y = preview_viewport_.y;
}

void Preview::set_color_map(ColorMap color_map)
{
    Buffer& buffer = get_color_map(color_map);
    color_map_     = buffer.index_;
    assert(buffer.size_ / sizeof(float) / 3 == 256);
    use_color_map_ = true;
}

void Preview::viewport_extent(int& vw, int& vh) const
{
    vw = viewport_.x2 - viewport_.x1;
    vh = viewport_.y2 - viewport_.y1;
}

float Preview::viewport_aspect() const
{
    int vw;
    int vh;
    viewport_extent(vw, vh);
    return static_cast<float>(vw) / vh;
}

float Preview::max_scale() const
{
    // The maximum scale corresponds to the scale factor such that a pixel in
    // the source image is stretched out to encompass the width or height of the
    // quadrant viewport (whichever is smaller)

    int vw;
    int vh;
    viewport_extent(vw, vh);

    return 0.5f * std::min(vw, vh);
}

float Preview::min_scale() const
{
    // The minimum scale is the smallest scale that constrains the image to the
    // quadrant
    float vaspect = viewport_aspect();
    float aspect  = image_->aspect();

    if (aspect < vaspect)
    {
        return static_cast<float>(viewport_.y2 - viewport_.y1) / image_->height_
               * 0.5f;
    }

    return static_cast<float>(viewport_.x2 - viewport_.x1) / image_->width_
           * 0.5f;
}

void Preview::render(GLFWwindow* window, VkCommandBuffer cb)
{
    if (image_ == nullptr || image_->image_ == VK_NULL_HANDLE || viewport_.x2 == 0)
    {
        return;
    }

    if (viewport_dirty_)
    {
        set_viewport(viewport_);
        viewport_dirty_ = false;
    }

    if (pan_active_)
    {
        double cx;
        double cy;
        glfwGetCursorPos(window, &cx, &cy);

        double delta_x = cx - cursor_[0];
        double delta_y = cy - cursor_[1];

        uv_offset_[0]
            = old_uv_offset_[0] - delta_x * uv_scale_ / preview_viewport_.width;
        uv_offset_[1]
            = old_uv_offset_[1] - delta_y * uv_scale_ / preview_viewport_.height;
    }

    PushConstants push_constants{.uv_offset = {uv_offset_[0], uv_offset_[1]},
                                 .uv_scale  = uv_scale_,
                                 .input     = image_->index_,
                                 .color_map = color_map_};
    vkCmdPushConstants(cb,
                       s_pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(PushConstants),
                       &push_constants);

    vkCmdSetViewport(cb, 0, 1, &preview_viewport_);

    vkCmdSetScissor(cb, 0, 1, &preview_scissor_);

    vkCmdBindDescriptorSets(cb,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            s_pipeline_layout,
                            0,
                            1,
                            &g_descriptor_set,
                            0,
                            nullptr);

    if (use_color_map_)
    {
        vkCmdBindPipeline(
            cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_pipeline_color_map);
    }
    else
    {
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_pipeline);
    }

    vkCmdDraw(cb, 3, 1, 0, 0);
}

void Preview::zoom(GLFWwindow* window, bool magnify)
{
    if (image_ == nullptr || viewport_.x2 == 0)
    {
        // The preview needs to render at least once with default magnification
        // to discover the viewport
        return;
    }

    // Determine the fixed point based on cursor position
    double c_x;
    double c_y;
    glfwGetCursorPos(window, &c_x, &c_y);

    // Quit if the mouse is in the upper right quadrant
    if (c_x > viewport_.x2 / 2 && c_y < viewport_.y2 / 2)
    {
        return;
    }

    // Magnify and minify in 5% stops
    if (magnify)
    {
        uv_scale_ -= 0.05f;
        uv_scale_ = std::max(uv_scale_, 0.05f);
    }
    else
    {
        uv_scale_ += 0.05f;
        uv_scale_ = std::min(uv_scale_, 10.f);
    }
}

void Preview::activate_pan(GLFWwindow* window)
{
    if (image_ == nullptr || viewport_.x2 == 0)
    {
        // The preview needs to render at least once with default magnification
        // to discover the viewport
        return;
    }

    // Determine the fixed point based on cursor position
    glfwGetCursorPos(window, cursor_, cursor_ + 1);

    // Quit if the mouse is in the upper right quadrant
    if (cursor_[0] > viewport_.x2 / 2 && cursor_[1] < viewport_.y2 / 2)
    {
        return;
    }

    pan_active_       = true;
    old_uv_offset_[0] = uv_offset_[0];
    old_uv_offset_[1] = uv_offset_[1];
}

void Preview::deactivate_pan()
{
    pan_active_ = false;
}
