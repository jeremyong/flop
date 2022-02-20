#include <flop/Flop.h>

#include <FlopContext.hpp>
#include <Image.hpp>
#include <VkGlobals.hpp>

#include "UI.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#undef APIENTRY
#include <CLI/CLI.hpp>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <cstdio>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <numbers>

static VkSurfaceKHR s_surface;
static VkSurfaceFormatKHR s_surface_format;
static GLFWwindow* s_window;
static VkSwapchainKHR s_swapchain;
static VkRenderPass s_render_pass;
static ImGui_ImplVulkanH_Window s_imgui_window;
static bool s_swapchain_broken = false;
static UI s_ui;

using namespace flop;

void on_resize(GLFWwindow* window, int width, int height)
{
    s_swapchain_broken = true;
}

PFN_vkVoidFunction load_vulkan_function(char const* function, void*)
{
    return vkGetInstanceProcAddr(g_instance, function);
}

double ref_width()
{
    return g_reference.source_.width_;
}

double ref_height()
{
    return g_reference.source_.height_;
}

void on_scroll(GLFWwindow* window, double x_offset, double y_offset)
{
    bool magnify = y_offset > 0;
    s_ui.on_scroll(window, magnify);

    ImGui_ImplGlfw_ScrollCallback(window, x_offset, y_offset);
}

void on_mouse_click(GLFWwindow* window, int button, int action, int mods)
{
    s_ui.on_click(window, button, action);
}

void set_pan_zoom_callbacks()
{
    glfwSetScrollCallback(s_window, on_scroll);
    glfwSetMouseButtonCallback(s_window, on_mouse_click);
}

void init_frames()
{
    s_imgui_window.Frames          = new ImGui_ImplVulkanH_Frame[2]{};
    s_imgui_window.FrameSemaphores = new ImGui_ImplVulkanH_FrameSemaphores[2]{};

    for (uint32_t i = 0; i != s_imgui_window.ImageCount; ++i)
    {
        ImGui_ImplVulkanH_Frame& frame = s_imgui_window.Frames[i];
        VkFenceCreateInfo fence_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                     .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        vkCreateFence(g_device, &fence_info, nullptr, &frame.Fence);

        VkSemaphoreCreateInfo semaphore_info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(
            g_device,
            &semaphore_info,
            nullptr,
            &s_imgui_window.FrameSemaphores[i].ImageAcquiredSemaphore);
        vkCreateSemaphore(
            g_device,
            &semaphore_info,
            nullptr,
            &s_imgui_window.FrameSemaphores[i].RenderCompleteSemaphore);
    }
}

void init_frame_command_buffers()
{
    for (uint32_t i = 0; i != s_imgui_window.ImageCount; ++i)
    {
        ImGui_ImplVulkanH_Frame& frame = s_imgui_window.Frames[i];

        if (frame.CommandPool)
        {
            vkFreeCommandBuffers(
                g_device, frame.CommandPool, 1, &frame.CommandBuffer);
            vkDestroyCommandPool(g_device, frame.CommandPool, nullptr);
        }

        VkCommandPoolCreateInfo command_pool_info{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = g_graphics_queue_index,
        };
        vkCreateCommandPool(
            g_device, &command_pool_info, nullptr, &frame.CommandPool);

        VkCommandBufferAllocateInfo command_buffer_info{
            .sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = frame.CommandPool,
            .level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1};
        vkAllocateCommandBuffers(
            g_device, &command_buffer_info, &frame.CommandBuffer);
    }
}

void create_swapchain();

void render()
{
    VkSemaphore acquire_surface
        = s_imgui_window.FrameSemaphores[s_imgui_window.SemaphoreIndex]
              .ImageAcquiredSemaphore;
    VkSemaphore release_surface
        = s_imgui_window.FrameSemaphores[s_imgui_window.SemaphoreIndex]
              .RenderCompleteSemaphore;
    VkResult error = vkAcquireNextImageKHR(g_device,
                                           s_imgui_window.Swapchain,
                                           UINT64_MAX,
                                           acquire_surface,
                                           VK_NULL_HANDLE,
                                           &s_imgui_window.FrameIndex);
    if (error == VK_ERROR_OUT_OF_DATE_KHR || error == VK_SUBOPTIMAL_KHR)
    {
        s_swapchain_broken = true;
        return;
    }

    ImGui_ImplVulkanH_Frame& frame
        = s_imgui_window.Frames[s_imgui_window.FrameIndex];
    vkWaitForFences(g_device, 1, &frame.Fence, VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &frame.Fence);

    vkResetCommandPool(g_device, frame.CommandPool, 0);
    VkCommandBufferBeginInfo begin
        = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
           .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(frame.CommandBuffer, &begin);

    // Blit reference image into position

    VkImageSubresourceRange transfer_range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                           .baseMipLevel   = 0,
                                           .levelCount     = 1,
                                           .baseArrayLayer = 0,
                                           .layerCount     = 1};
    VkImageMemoryBarrier dst_transfer{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_MEMORY_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = g_graphics_queue_index,
        .dstQueueFamilyIndex = g_graphics_queue_index,
        .image               = frame.Backbuffer,
        .subresourceRange    = transfer_range};
    vkCmdPipelineBarrier(frame.CommandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &dst_transfer);

    VkClearValue clear_value{.color = {.int32 = {0, 0, 0, 0}}};
    VkRenderPassBeginInfo render_pass_begin{
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = s_imgui_window.RenderPass,
        .framebuffer = frame.Framebuffer,
        .renderArea
        = {.offset = {.x = 0, .y = 0},
           .extent = {.width  = static_cast<uint32_t>(s_imgui_window.Width),
                      .height = static_cast<uint32_t>(s_imgui_window.Height)}},
        .clearValueCount = 1,
        .pClearValues    = &clear_value};
    vkCmdBeginRenderPass(
        frame.CommandBuffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

    s_ui.render(s_window, frame.CommandBuffer);

    vkCmdEndRenderPass(frame.CommandBuffer);

    VkPipelineStageFlags wait_stage
        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                        .waitSemaphoreCount   = 1,
                        .pWaitSemaphores      = &acquire_surface,
                        .pWaitDstStageMask    = &wait_stage,
                        .commandBufferCount   = 1,
                        .pCommandBuffers      = &frame.CommandBuffer,
                        .signalSemaphoreCount = 1,
                        .pSignalSemaphores    = &release_surface};
    vkEndCommandBuffer(frame.CommandBuffer);
    vkQueueSubmit(g_graphics_queue, 1, &submit, frame.Fence);
}

void present()
{
    if (s_swapchain_broken)
    {
        return;
    }

    VkSemaphore release_surface
        = s_imgui_window.FrameSemaphores[s_imgui_window.SemaphoreIndex]
              .RenderCompleteSemaphore;
    VkPresentInfoKHR present{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                             .waitSemaphoreCount = 1,
                             .pWaitSemaphores    = &release_surface,
                             .swapchainCount     = 1,
                             .pSwapchains        = &s_imgui_window.Swapchain,
                             .pImageIndices      = &s_imgui_window.FrameIndex};
    VkResult error = vkQueuePresentKHR(g_graphics_queue, &present);

    if (error == VK_ERROR_OUT_OF_DATE_KHR || error == VK_SUBOPTIMAL_KHR)
    {
        s_swapchain_broken = true;
        return;
    }

    s_imgui_window.SemaphoreIndex
        = (s_imgui_window.SemaphoreIndex + 1) % s_imgui_window.ImageCount;
}

static void on_drop(GLFWwindow* window, int path_count, const char* paths[])
{
    if (path_count == 1)
    {
        // Determine whether we've dragged onto the reference widget, or the
        // test widget.
    }
}

void init_render_pass();

int main(int argc, char const* argv[])
{
    CLI::App app{
        "An image comparison tool and visualizer. All options are optional "
        "unless headless mode is requested.",
        "FLOP"};
    std::string reference;
    app.add_option("-r,--reference", reference, "Path to reference image");
    std::string test;
    app.add_option("-t,--test", test, "Path to test image");
    std::string output;
    app.add_option("-o, ", output, "Path to output file.");

    int headless;
    app.add_flag(
        "--hl,--headless", headless, "Request that a gui not be presented");

    int force;
    app.add_flag("-f,--force",
                 force,
                 "Overwrite image if file exists at specified output path");

    CLI11_PARSE(app, argc, argv);

    if (!output.empty())
    {
        std::error_code ec;
        auto stat = std::filesystem::status(output, ec);
        if (!ec)
        {
            switch (stat.type())
            {
            case std::filesystem::file_type::none:
            case std::filesystem::file_type::not_found:
                break;
            case std::filesystem::file_type::directory:
                std::printf(
                    "Error: Output path supplied (%s) is a directory.\n",
                    output.c_str());
                return 1;
            case std::filesystem::file_type::symlink:
            case std::filesystem::file_type::block:
            case std::filesystem::file_type::character:
            case std::filesystem::file_type::fifo:
            case std::filesystem::file_type::socket:
            case std::filesystem::file_type::regular:
            case std::filesystem::file_type::unknown:
            default:
                if (force == 0)
                {
                    std::printf(
                        "Error: File exists at output path %s. Pass -f or "
                        "--force to overwrite it.\n",
                        output.c_str());
                    return 1;
                }
                break;
            }
        }
    }

    s_ui.set_reference(reference);
    s_ui.set_test(test);
    s_ui.set_output(output);
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW!\n";
        return 1;
    }

    if (headless == 0)
    {
        NFD_Init();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        s_window = glfwCreateWindow(1920, 1080, "FLOP", nullptr, nullptr);

        if (!glfwVulkanSupported())
        {
            std::cout << "Vulkan not supported!\n";
            return 1;
        }

        glfwSetDropCallback(s_window, on_drop);
        glfwSetFramebufferSizeCallback(s_window, on_resize);

        uint32_t extCount;
        char const** exts = glfwGetRequiredInstanceExtensions(&extCount);
        flop_init(extCount, exts);
    }
    else
    {
        flop_init(0, nullptr);
    }

    if (!(reference.empty() || test.empty()))
    {
        if (s_ui.analyze(false))
        {
        }
    }

    if (headless > 0)
    {
        return 0;
    }

    if (glfwCreateWindowSurface(g_instance, s_window, nullptr, &s_surface)
        != VK_SUCCESS)
    {
        std::cout << "Failed to create Vulkan window surface!\n";
        return 1;
    }

    std::vector<VkSurfaceFormatKHR> surface_formats
        = vk_enumerate<VkSurfaceFormatKHR>(
            vkGetPhysicalDeviceSurfaceFormatsKHR, g_physical_device, s_surface);

    bool surface_found = false;
    for (VkSurfaceFormatKHR const& format : surface_formats)
    {
        // TODO: Rec.2020/HDR-swapchain support
        if (format.format == VK_FORMAT_B8G8R8_SRGB)
        {
            s_surface_format = format;
            surface_found    = true;
            break;
        }
    }
    if (!surface_found)
    {
        s_surface_format = surface_formats[0];
    }

    // We're required to validate the surface is supported before use, or the
    // validation layers will complain
    VkBool32 surface_supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(
        g_physical_device, g_graphics_queue_index, s_surface, &surface_supported);
    if (!surface_supported)
    {
        std::cout << "Surface not supported.\n";
        return 1;
    }

    init_render_pass();
    Preview::init(s_render_pass);

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(s_window, true);
    set_pan_zoom_callbacks();

    ImGui_ImplVulkan_LoadFunctions(load_vulkan_function);

    int width;
    int height;
    glfwGetFramebufferSize(s_window, &width, &height);
    s_imgui_window.Surface       = s_surface;
    s_imgui_window.SurfaceFormat = s_surface_format;
    s_imgui_window.PresentMode   = VK_PRESENT_MODE_FIFO_KHR;
    s_imgui_window.ClearEnable   = true;
    s_imgui_window.RenderPass    = s_render_pass;
    s_imgui_window.ImageCount    = 2;

    ImGui_ImplVulkan_InitInfo imgui_vulkan_info{
        .Instance        = g_instance,
        .PhysicalDevice  = g_physical_device,
        .Device          = g_device,
        .QueueFamily     = g_graphics_queue_index,
        .Queue           = g_graphics_queue,
        .PipelineCache   = VK_NULL_HANDLE,
        .DescriptorPool  = g_descriptor_pool,
        .Subpass         = 0,
        .MinImageCount   = 2,
        .ImageCount      = 2,
        .MSAASamples     = VK_SAMPLE_COUNT_1_BIT,
        .Allocator       = nullptr,
        .CheckVkResultFn = nullptr,
    };
    ImGui_ImplVulkan_Init(&imgui_vulkan_info, s_render_pass);
    ImGui_ImplVulkan_SetMinImageCount(2);
    init_frames();
    create_swapchain();

    VkCommandBuffer cb = g_command_buffers[3];
    VkCommandBufferBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(cb, &begin);
    ImGui_ImplVulkan_CreateFontsTexture(cb);
    vkEndCommandBuffer(cb);
    VkSubmitInfo submit{.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                        .commandBufferCount = 1,
                        .pCommandBuffers    = &cb};
    vkQueueSubmit(g_graphics_queue, 1, &submit, VK_NULL_HANDLE);
    vkDeviceWaitIdle(g_device);
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    int image_focus = 1;
    while (!glfwWindowShouldClose(s_window))
    {
        // glfwWaitEventsTimeout(1);
        glfwWaitEvents();

        if (s_swapchain_broken)
        {
            int width;
            int height;
            glfwGetFramebufferSize(s_window, &width, &height);
            if (width > 0 && height > 0)
            {
                create_swapchain();
                s_imgui_window.FrameIndex = 0;
                s_swapchain_broken        = false;
                s_ui.mark_viewport_dirty();
            }
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        s_ui.update();

        ImGui::Render();

        ImDrawData& draw_data = *ImGui::GetDrawData();
        if (draw_data.DisplaySize.x > 0.f && draw_data.DisplaySize.y > 0.f)
        {
            render();
            present();
        }
    }

    return 0;
}

void init_render_pass()
{
    // Create a single color target, single subpass render pass.
    VkAttachmentDescription color_attachment{
        .format         = s_surface_format.format,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference color_attachment_reference{
        .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &color_attachment_reference,
        .pDepthStencilAttachment = nullptr};
    VkSubpassDependency subpass_dependency{
        .srcSubpass   = VK_SUBPASS_EXTERNAL,
        .dstSubpass   = 0,
        .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};

    VkRenderPassCreateInfo render_pass_info{
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &color_attachment,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &subpass_dependency};
    if (vkCreateRenderPass(g_device, &render_pass_info, nullptr, &s_render_pass)
        != VK_SUCCESS)
    {
        std::cout << "Failed to create Vulkan render pass!\n";
        abort();
    }
}

void create_swapchain()
{
    VkSwapchainKHR old_swapchain = s_imgui_window.Swapchain;
    s_imgui_window.Swapchain     = VK_NULL_HANDLE;

    vkDeviceWaitIdle(g_device);

    init_frame_command_buffers();

    for (uint32_t i = 0; i != s_imgui_window.ImageCount; ++i)
    {
        if (s_imgui_window.Frames[i].BackbufferView)
        {
            vkDestroyImageView(
                g_device, s_imgui_window.Frames[i].BackbufferView, nullptr);
        }
        if (s_imgui_window.Frames[i].Framebuffer)
        {
            vkDestroyFramebuffer(
                g_device, s_imgui_window.Frames[i].Framebuffer, nullptr);
        }
    }

    if (s_imgui_window.Pipeline)
    {
        vkDestroyPipeline(g_device, s_imgui_window.Pipeline, nullptr);
    }

    VkSwapchainCreateInfoKHR swapchain_info{
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = s_imgui_window.Surface,
        .minImageCount    = 2,
        .imageFormat      = s_imgui_window.SurfaceFormat.format,
        .imageColorSpace  = s_imgui_window.SurfaceFormat.colorSpace,
        .imageArrayLayers = 1,
        .imageUsage
        = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = s_imgui_window.PresentMode,
        .clipped          = VK_TRUE,
        .oldSwapchain     = old_swapchain,
    };
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        g_physical_device, s_imgui_window.Surface, &surface_capabilities);

    if (surface_capabilities.minImageCount < 2)
    {
        swapchain_info.minImageCount = surface_capabilities.minImageCount;
    }

    int width;
    int height;
    glfwGetFramebufferSize(s_window, &width, &height);
    if (surface_capabilities.currentExtent.width == 0xffffffff)
    {
        s_imgui_window.Width              = width;
        swapchain_info.imageExtent.width  = width;
        s_imgui_window.Height             = height;
        swapchain_info.imageExtent.height = height;
    }
    else
    {
        s_imgui_window.Width = surface_capabilities.currentExtent.width;
        swapchain_info.imageExtent.width
            = surface_capabilities.currentExtent.width;
        s_imgui_window.Height = surface_capabilities.currentExtent.height;
        swapchain_info.imageExtent.height
            = surface_capabilities.currentExtent.height;
    }

    vkCreateSwapchainKHR(
        g_device, &swapchain_info, nullptr, &s_imgui_window.Swapchain);
    vkGetSwapchainImagesKHR(
        g_device, s_imgui_window.Swapchain, &s_imgui_window.ImageCount, nullptr);

    VkImage backbuffers[2];
    vkGetSwapchainImagesKHR(g_device,
                            s_imgui_window.Swapchain,
                            &s_imgui_window.ImageCount,
                            backbuffers);

    for (uint32_t i = 0; i != s_imgui_window.ImageCount; ++i)
    {
        s_imgui_window.Frames[i].Backbuffer = backbuffers[i];
    }

    if (old_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(g_device, old_swapchain, nullptr);
    }

    VkImageViewCreateInfo view_info{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = s_imgui_window.SurfaceFormat.format,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    for (uint32_t i = 0; i != s_imgui_window.ImageCount; ++i)
    {
        view_info.image = s_imgui_window.Frames[i].Backbuffer;
        vkCreateImageView(g_device,
                          &view_info,
                          nullptr,
                          &s_imgui_window.Frames[i].BackbufferView);
    }

    VkImageView framebuffer_attachment;
    VkFramebufferCreateInfo framebuffer_info{
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass      = s_render_pass,
        .attachmentCount = 1,
        .pAttachments    = &framebuffer_attachment,
        .width           = static_cast<uint32_t>(s_imgui_window.Width),
        .height          = static_cast<uint32_t>(s_imgui_window.Height),
        .layers          = 1};
    for (uint32_t i = 0; i != s_imgui_window.ImageCount; ++i)
    {
        framebuffer_attachment = s_imgui_window.Frames[i].BackbufferView;
        vkCreateFramebuffer(g_device,
                            &framebuffer_info,
                            nullptr,
                            &s_imgui_window.Frames[i].Framebuffer);
    }
}
