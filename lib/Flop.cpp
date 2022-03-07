#include <flop/Flop.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <vector>
#include <volk.h>

#include "ColorMaps.hpp"
#include "FlopContext.hpp"
#include "VkGlobals.hpp"

#include <CSFFilterX_spv.h>
#include <CSFFilterY_spv.h>
#include <ColorCompare_spv.h>
#include <ErrorColorMap_spv.h>
#include <FeatureFilterX_spv.h>
#include <FeatureFilterY_spv.h>
#include <Summarize_spv.h>
#include <YyCxCz_spv.h>

static char const* s_error = "";
#ifdef NDEBUG
static bool s_validation_enabled = false;
#else
static bool s_validation_enabled = true;
#endif

static bool s_initialized;

static int create_device(char const* preferred_device, bool swapchain);
static void create_kernels();

using namespace flop;

static VkResult
CreateDebugUtilsMessengerEXT(VkInstance instance,
                             const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                             const VkAllocationCallbacks* pAllocator,
                             VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (fn)
    {
        return fn(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }

    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                          VkDebugUtilsMessengerEXT messenger,
                                          const VkAllocationCallbacks* pAllocator)
{
    auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (fn)
    {
        fn(instance, messenger, pAllocator);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
onVkDebugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                 VkDebugUtilsMessageTypeFlagBitsEXT type,
                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                 void* pUserData)
{
    std::cerr << pCallbackData->pMessage << '\n';
    return VK_FALSE;
}

void flop_config_enable_validation()
{
    s_validation_enabled = true;
}

int flop_init(uint32_t instanceExtensionCount,
              char const** requiredInstanceExtensions)
{
    if (s_initialized)
    {
        return 0;
    }
    s_initialized = true;

    if (volkInitialize() != VK_SUCCESS)
    {
        s_error = "Failed to initialize Vulkan loader.";
        return 1;
    }

    std::vector<char const*> exts;
    exts.reserve(instanceExtensionCount + 3);
    exts.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    for (uint32_t i = 0; i != instanceExtensionCount; ++i)
    {
        exts.push_back(requiredInstanceExtensions[i]);
    }

    VkApplicationInfo appInfo{
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "FLOP",
        .applicationVersion = 0,
        .pEngineName        = "FLOP",
        .apiVersion         = VK_API_VERSION_1_2,
    };

    const char* layers = "VK_LAYER_KHRONOS_validation";
    if (s_validation_enabled)
    {
        std::cout << "Vulkan validation requested\n";
    }
    VkDebugUtilsMessengerCreateInfoEXT debugInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback
        = reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(onVkDebugMessage),
    };

    VkInstanceCreateInfo instanceInfo{
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext                   = s_validation_enabled ? &debugInfo : nullptr,
        .flags                   = 0,
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = s_validation_enabled ? 1u : 0u,
        .ppEnabledLayerNames     = s_validation_enabled ? &layers : nullptr,
        .enabledExtensionCount   = static_cast<uint32_t>(exts.size()),
        .ppEnabledExtensionNames = exts.data()};
    if (vkCreateInstance(&instanceInfo, nullptr, &g_instance) != VK_SUCCESS)
    {
        s_error = "Failed to create Vulkan instance.";
        return 1;
    }

    // Load instance function pointers
    volkLoadInstance(g_instance);

    vkCmdBeginDebugUtilsLabel
        = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(g_instance, "vkCmdBeginDebugUtilsLabelEXT"));
    vkCmdEndDebugUtilsLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(g_instance, "vkCmdEndDebugUtilsLabelEXT"));

    if (create_device(nullptr, instanceExtensionCount != 0))
    {
        return 1;
    }

    create_kernels();

    g_error_histogram = Buffer::create(sizeof(uint32_t) * 32);

    upload_color_maps();

    return 0;
}

static int create_device(char const* preferred_device, bool swapchain)
{
    std::vector<VkPhysicalDevice> physicalDevices
        = vk_enumerate<VkPhysicalDevice>(vkEnumeratePhysicalDevices, g_instance);

    for (auto& device : physicalDevices)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        if (preferred_device)
        {
            std::string deviceName{props.deviceName};
            std::transform(deviceName.begin(),
                           deviceName.end(),
                           deviceName.begin(),
                           [](char c) { return std::tolower(c); });

            if (deviceName.find(preferred_device) == std::string::npos)
            {
                g_physical_device       = device;
                g_physical_device_props = props;
                break;
            }
        }
        else
        {
            // Simply select the first discrete GPU available, or the first
            // integrated GPU if no discrete GPU is detected
            if (g_physical_device == VK_NULL_HANDLE)
            {
                g_physical_device       = device;
                g_physical_device_props = props;
                continue;
            }
            else
            {
                if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                    && g_physical_device_props.deviceType
                           != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                {
                    g_physical_device       = device;
                    g_physical_device_props = props;
                }
                else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
                         && g_physical_device_props.deviceType
                                == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
                {
                    g_physical_device       = device;
                    g_physical_device_props = props;
                }
            }
        }
    }

    std::cout << "Using device: " << g_physical_device_props.deviceName << '\n';

    // For now, keep things simple with a single device queue
    std::vector<VkQueueFamilyProperties> queueFamilies
        = vk_enumerate<VkQueueFamilyProperties>(
            vkGetPhysicalDeviceQueueFamilyProperties, g_physical_device);

    for (uint32_t i = 0; i != queueFamilies.size(); ++i)
    {
        if (queueFamilies[i].queueCount > 0)
        {
            if (g_graphics_queue_index == -0u
                && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                g_graphics_queue_index = i;
            }
            else if (g_compute_queue_index == -0u
                     && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                g_compute_queue_index = i;
            }
        }
    }

    float queue_priority = 1.f;
    VkDeviceQueueCreateInfo queue_infos[]
        = {{
               .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
               .queueFamilyIndex = g_graphics_queue_index,
               .queueCount       = 1,
               .pQueuePriorities = &queue_priority,
           },
           {
               .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
               .queueFamilyIndex = g_compute_queue_index,
               .queueCount       = 1,
               .pQueuePriorities = &queue_priority,
           }};

    char const* device_exts[] = {
        "VK_EXT_descriptor_indexing",
        "VK_KHR_timeline_semaphore",
        "VK_EXT_shader_subgroup_ballot",
        "VK_EXT_shader_subgroup_vote",
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        "VK_KHR_swapchain",
    };

    VkPhysicalDeviceFeatures features{
        .robustBufferAccess                      = VK_TRUE,
        .textureCompressionBC                    = VK_TRUE,
        .vertexPipelineStoresAndAtomics          = VK_TRUE,
        .fragmentStoresAndAtomics                = VK_TRUE,
        .shaderUniformBufferArrayDynamicIndexing = VK_TRUE,
        .shaderSampledImageArrayDynamicIndexing  = VK_TRUE,
        .shaderStorageBufferArrayDynamicIndexing = VK_TRUE,
        .shaderStorageImageArrayDynamicIndexing  = VK_TRUE,
        .shaderResourceResidency                 = VK_TRUE,
    };
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &dynamic_rendering,
        .descriptorIndexing                            = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing     = VK_TRUE,
        .shaderStorageImageArrayNonUniformIndexing     = VK_TRUE,
        .descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind  = VK_TRUE,
        .descriptorBindingStorageImageUpdateAfterBind  = VK_TRUE,
        .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingUpdateUnusedWhilePending     = VK_TRUE,
        .descriptorBindingPartiallyBound               = VK_TRUE,
        .descriptorBindingVariableDescriptorCount      = VK_TRUE,
        .runtimeDescriptorArray                        = VK_TRUE,
    };

    VkDeviceCreateInfo device_info{
        .sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                = &features2,
        .queueCreateInfoCount = g_compute_queue_index == -0u ? 1u : 2u,
        .pQueueCreateInfos    = queue_infos,
        .enabledLayerCount    = 0,
        .ppEnabledLayerNames  = nullptr,
        .enabledExtensionCount
        = sizeof(device_exts) / sizeof(char const*) - (swapchain ? 0 : 1),
        .ppEnabledExtensionNames = device_exts,
        .pEnabledFeatures        = &features};

    if (vkCreateDevice(g_physical_device, &device_info, nullptr, &g_device)
        != VK_SUCCESS)
    {
        s_error = "Failed to create Vulkan device.";
        return 1;
    }
    volkLoadDevice(g_device);

    vkGetDeviceQueue(g_device, g_graphics_queue_index, 0, &g_graphics_queue);

    if (g_compute_queue_index != -0u)
    {
        vkGetDeviceQueue(g_device, g_compute_queue_index, 0, &g_compute_queue);
    }

    VmaAllocatorCreateInfo allocator_info{
        .physicalDevice              = g_physical_device,
        .device                      = g_device,
        .preferredLargeHeapBlockSize = 0, // 256 MiB default
        .pAllocationCallbacks        = nullptr,
        .pDeviceMemoryCallbacks      = nullptr,
        .frameInUseCount             = 0,
        .instance                    = g_instance};

    if (vmaCreateAllocator(&allocator_info, &g_allocator) != VK_SUCCESS)
    {
        s_error = "Failed to create Vulkan allocator.";
        return 1;
    }

    VkCommandPoolCreateInfo command_pool_info{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = g_graphics_queue_index,
    };

    if (vkCreateCommandPool(g_device, &command_pool_info, nullptr, &g_command_pool)
        != VK_SUCCESS)
    {
        s_error = "Failed to create Vulkan command pool.";
        return 1;
    }

    VkCommandBufferAllocateInfo command_buffer_info{
        .sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = g_command_pool,
        .level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount
        = sizeof(g_command_buffers) / sizeof(VkCommandBuffer)};
    if (vkAllocateCommandBuffers(g_device, &command_buffer_info, g_command_buffers)
        != VK_SUCCESS)
    {
        s_error = "Failed to allocate Vulkan command buffers.";
        return 1;
    }

    VkDescriptorPoolSize pool_sizes[] = {
        {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 32},
        {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 8},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 32},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 32},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, .descriptorCount = 32},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, .descriptorCount = 32},
    };
    VkDescriptorPoolCreateInfo descriptor_pool_info{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets       = 128,
        .poolSizeCount = sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize),
        .pPoolSizes    = pool_sizes};

    if (vkCreateDescriptorPool(
            g_device, &descriptor_pool_info, nullptr, &g_descriptor_pool)
        != VK_SUCCESS)
    {
        s_error = "Failed to create Vulkan descriptor pool.";
        return 1;
    }

    VkSampler sampler;
    VkSamplerCreateInfo sampler_info{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = VK_FILTER_NEAREST,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .mipLodBias              = 0.f,
        .anisotropyEnable        = VK_FALSE,
        .compareEnable           = VK_FALSE,
        .minLod                  = 0,
        .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    vkCreateSampler(g_device, &sampler_info, nullptr, &sampler);

    VkDescriptorSetLayoutBinding bindings[] = {
        {.binding         = 0,
         .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         .descriptorCount = 10000,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
         .pImmutableSamplers = nullptr},
        {.binding         = 1,
         .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = 10000,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
         .pImmutableSamplers = nullptr},
        {.binding         = 2,
         .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 10000,
         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT
                       | VK_SHADER_STAGE_FRAGMENT_BIT,
         .pImmutableSamplers = nullptr},
        {.binding            = 3,
         .descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER,
         .descriptorCount    = 1,
         .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
         .pImmutableSamplers = &sampler}};

    VkDescriptorBindingFlags binding_flags[]
        = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
               | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
           VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
               | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
           VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
               | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
           0};
    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount  = 4,
        .pBindingFlags = binding_flags,
    };

    VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &binding_flags_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = 4,
        .pBindings    = bindings,
    };
    if (vkCreateDescriptorSetLayout(
            g_device, &layout_info, nullptr, &g_descriptor_set_layout)
        != VK_SUCCESS)
    {
        s_error = "Failed to create Vulkan descriptor set layout.";
        return 1;
    }

    VkDescriptorSetAllocateInfo descriptor_set_alloc{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = g_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &g_descriptor_set_layout};
    if (vkAllocateDescriptorSets(g_device, &descriptor_set_alloc, &g_descriptor_set)
        != VK_SUCCESS)
    {
        s_error = "Failed to allocate Vulkan descriptor set.";
        return 1;
    }

    return 0;
}

void create_kernels()
{
    g_yycxcz.init(YyCxCz_spv_data, YyCxCz_spv_size, 4 * 9);
    g_csf_filter_x
        = Kernel::create(CSFFilterX_spv_data, CSFFilterX_spv_size, 64, 1, false);
    g_csf_filter_y
        = Kernel::create(CSFFilterY_spv_data, CSFFilterY_spv_size, 1, 64, false);
    g_color_compare = Kernel::create(
        ColorCompare_spv_data, ColorCompare_spv_size, 8, 8, true);
    g_error_color_map.init(ErrorColorMap_spv_data, ErrorColorMap_spv_size, 4 * 7);

    g_feature_filter_x = Kernel::create(
        FeatureFilterX_spv_data, FeatureFilterX_spv_size, 64, 1, true);
    g_feature_filter_y = Kernel::create(
        FeatureFilterY_spv_data, FeatureFilterY_spv_size, 1, 64, true);

    g_summarize
        = Kernel::create(Summarize_spv_data, Summarize_spv_size, 8, 8, false);
}

char const* flop_get_error()
{
    return s_error;
}

void flop_init_reference(char const* reference_path)
{
    std::filesystem::path reference_ext
        = std::filesystem::path{reference_path}.extension();

    if (reference_ext == ".exr")
    {
        g_reference.source_ = Image::create_from_exr(reference_path);
    }
    else if (reference_ext == ".png" || reference_ext == ".jpg"
             || reference_ext == ".jpeg" || reference_ext == ".bmp")
    {
        g_reference.source_ = Image::create_from_non_exr(reference_path);
    }
    else
    {
        std::printf(
            "Reference image %s has unrecognized extension\n", reference_path);
    }
}

void flop_init_test(char const* test_path)
{
    std::filesystem::path test_ext = std::filesystem::path{test_path}.extension();

    if (test_ext == ".exr")
    {
        g_test.source_ = Image::create_from_exr(test_path);
    }
    else if (test_ext == ".png" || test_ext == ".jpg" || test_ext == ".jpeg"
             || test_ext == ".bmp")
    {
        g_test.source_ = Image::create_from_non_exr(test_path);
    }
    else
    {
        std::printf("Test image %s has unrecognized extension\n", test_path);
    }
}

void flop_reset(bool bypass)
{
    vkDeviceWaitIdle(g_device);
    if (bypass)
    {
        Image::reset_count(2);
    }
    else
    {
        Image::reset_count();
        g_reference.source_.reset();
        g_test.source_.reset();
    }
    g_reference.yycxcz_.reset();
    g_reference.yycxcz_blur_x_.reset();
    g_reference.yycxcz_blurred_.reset();
    g_reference.feature_blur_x_.reset();
    g_test.yycxcz_.reset();
    g_test.yycxcz_blur_x_.reset();
    g_test.yycxcz_blurred_.reset();
    g_test.feature_blur_x_.reset();
    g_error.reset();
    g_error_color.reset();
    g_error_readback.reset();
}

int flop_analyze_impl(char const* reference_path,
                      char const* test_path,
                      char const* output_path,
                      float exposure,
                      int tonemap,
                      FlopSummary* out_summary,
                      bool bypass_initialization)
{
    if (!std::filesystem::exists(reference_path))
    {
        s_error = "Invalid reference path.";
        return 1;
    }

    if (!std::filesystem::exists(test_path))
    {
        s_error = "Invalid test path.";
        return 1;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    if (g_reference.source_.image_ != VK_NULL_HANDLE)
    {
        flop_reset(bypass_initialization);
    }

    if (!bypass_initialization)
    {
        flop_init_reference(reference_path);
        flop_init_test(test_path);
    }

    // Validate that the images have the same dimensions
    if (g_reference.source_.width_ != g_test.source_.width_
        || g_reference.source_.height_ != g_test.source_.height_)
    {
        s_error = "Reference and test images do not have matching extents.";
        return 1;
    }
    if (out_summary)
    {
        out_summary->width  = g_reference.source_.width_;
        out_summary->height = g_reference.source_.height_;
    }

    g_reference.yycxcz_ = Image::create(
        g_reference.source_, VK_FORMAT_R32G32B32A32_SFLOAT, true);
    g_reference.yycxcz_blur_x_ = Image::create(g_reference.source_);
    g_reference.yycxcz_blurred_
        = Image::create(g_reference.source_, VK_FORMAT_R32G32B32A32_SFLOAT);
    g_reference.feature_blur_x_ = Image::create(g_reference.source_);
    g_test.yycxcz_
        = Image::create(g_test.source_, VK_FORMAT_R32G32B32A32_SFLOAT, true);
    g_test.yycxcz_blur_x_ = Image::create(g_test.source_);
    g_test.yycxcz_blurred_
        = Image::create(g_test.source_, VK_FORMAT_R32G32B32A32_SFLOAT);
    g_test.feature_blur_x_ = Image::create(g_test.source_);

    g_error = Image::create(g_reference.source_, VK_FORMAT_R32_SFLOAT);
    if (output_path)
    {
        g_error_color
            = Image::create(g_reference.source_, VK_FORMAT_R8G8B8A8_UNORM, true);
        g_error_readback = Image::create_readback(
            g_reference.source_, VK_FORMAT_R8G8B8A8_UNORM);
    }

    VkCommandBufferBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VkCommandBuffer cb = g_command_buffers[1];
    vkBeginCommandBuffer(cb, &begin);

    // Transfer storage images to a writable state
    VkImageSubresourceRange transfer_range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                           .baseMipLevel   = 0,
                                           .levelCount     = 1,
                                           .baseArrayLayer = 0,
                                           .layerCount     = 1};
    VkImageMemoryBarrier transfers[11] = {
        g_reference.yycxcz_.start_barrier(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        g_test.yycxcz_.start_barrier(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        g_reference.yycxcz_blur_x_.start_barrier(),
        g_reference.yycxcz_blurred_.start_barrier(),
        g_reference.feature_blur_x_.start_barrier(),
        g_test.yycxcz_blur_x_.start_barrier(),
        g_test.yycxcz_blurred_.start_barrier(),
        g_test.feature_blur_x_.start_barrier(),
        g_error.start_barrier(),
        (output_path ? g_error_color.start_barrier() : VkImageMemoryBarrier{}),
        (output_path ? g_error_readback.readback_barrier()
                     : VkImageMemoryBarrier{})};
    vkCmdPipelineBarrier(cb,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         2,
                         transfers);
    vkCmdPipelineBarrier(cb,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         output_path ? 9 : 7,
                         transfers + 2);

    // Transform input images to YyCxCz space

    struct
    {
        uint32_t extent[2];
        float uv_offset[2];
        float uv_scale;
        uint32_t input;
        uint32_t tonemap;
        float exposure;
        uint32_t handle_alpha;
    } data;
    data.extent[0]    = g_reference.source_.width_;
    data.extent[1]    = g_reference.source_.height_;
    data.uv_offset[0] = 0.f;
    data.uv_offset[1] = 0.f;
    data.uv_scale     = 1.f;
    data.input        = g_reference.source_.index_;
    if (g_reference.source_.hdr_)
    {
        data.tonemap  = tonemap;
        data.exposure = std::powf(2.f, exposure);
    }
    else
    {
        data.tonemap  = 0;
        data.exposure = 1.f;
    }
    if (g_reference.source_.channels_ == 4)
    {
        data.handle_alpha = 1;
    }
    else
    {
        data.handle_alpha = 0;
    }
    g_yycxcz.render(cb, g_reference.yycxcz_, &data);
    data.input = g_test.source_.index_;
    if (g_test.source_.channels_ == 4)
    {
        data.handle_alpha = 1;
    }
    else
    {
        data.handle_alpha = 0;
    }
    g_yycxcz.render(cb, g_test.yycxcz_, &data);

    VkEventCreateInfo event_info{.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
                                 .flags = VK_EVENT_CREATE_DEVICE_ONLY_BIT_KHR};

    transfers[0]
        = g_reference.yycxcz_.raw_barrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    transfers[1]
        = g_test.yycxcz_.raw_barrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    vkCmdPipelineBarrier(cb,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         2,
                         transfers);

    // Convolve input images in YyCxCz space with feature-detection kernels
    g_feature_filter_x.dispatch(cb,
                                g_reference.yycxcz_,
                                g_test.yycxcz_,
                                g_reference.feature_blur_x_,
                                g_test.feature_blur_x_);

    // Apply a separable Gaussian filter based on the contrast sensitivity
    // functions
    g_csf_filter_x.dispatch(cb, g_reference.yycxcz_, g_reference.yycxcz_blur_x_);
    g_csf_filter_x.dispatch(cb, g_test.yycxcz_, g_test.yycxcz_blur_x_);

    transfers[0] = g_reference.yycxcz_blur_x_.raw_barrier();
    transfers[1] = g_test.yycxcz_blur_x_.raw_barrier();
    transfers[2] = g_reference.feature_blur_x_.raw_barrier();
    transfers[3] = g_test.feature_blur_x_.raw_barrier();
    vkCmdPipelineBarrier(cb,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         4,
                         transfers);

    g_csf_filter_y.dispatch(
        cb, g_reference.yycxcz_blur_x_, g_reference.yycxcz_blurred_);
    g_csf_filter_y.dispatch(cb, g_test.yycxcz_blur_x_, g_test.yycxcz_blurred_);

    transfers[0] = g_reference.yycxcz_blurred_.raw_barrier();
    transfers[1] = g_test.yycxcz_blurred_.raw_barrier();
    vkCmdPipelineBarrier(cb,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         2,
                         transfers);

    // Use the modified HyAB color difference metric to compute the color-based
    // error
    g_color_compare.dispatch(
        cb, g_reference.yycxcz_blurred_, g_test.yycxcz_blurred_, g_error);

    transfers[0] = g_error.waw_barrier();
    vkCmdPipelineBarrier(cb,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         transfers);

    // Finalize feature detection convolution in the y direction and use feature
    // differences to amplify color error
    g_feature_filter_y.dispatch(
        cb, g_reference.feature_blur_x_, g_test.feature_blur_x_, g_error);

    transfers[0] = g_error.raw_barrier();
    vkCmdPipelineBarrier(cb,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         transfers);

    // Compute a histogram of the final error map
    g_summarize.dispatch(cb, g_error, g_error_histogram);

    if (output_path)
    {
        // Transfer monochromatic error channel via color map

        transfers[0] = g_error.rar_barrier();
        vkCmdPipelineBarrier(cb,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             transfers);

        struct
        {
            uint32_t extent[2];
            float uv_offset[2];
            float uv_scale;
            uint32_t input;
            uint32_t color_map;
        } data;
        data.extent[0]    = g_reference.source_.width_;
        data.extent[1]    = g_reference.source_.height_;
        data.uv_offset[0] = 0.f;
        data.uv_offset[1] = 0.f;
        data.uv_scale     = 1.f;
        data.input        = g_error.index_;
        data.color_map    = get_color_map(ColorMap::Magma).index_;
        g_error_color_map.render(cb, g_error_color, &data);

        transfers[0]               = g_error_color.blit_barrier();
        transfers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(cb,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             transfers);

        // Issue readback and transition host image to general layout
        g_error_color.readback(cb, g_error_readback);
        transfers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        transfers[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        transfers[0].oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        transfers[0].newLayout     = VK_IMAGE_LAYOUT_GENERAL;
        transfers[0].image         = g_error_readback.image_;
        vkCmdPipelineBarrier(cb,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             transfers);
    }

    transfers[0] = g_reference.yycxcz_.sample_barrier();
    transfers[1] = g_test.yycxcz_.sample_barrier();
    transfers[2] = g_reference.source_.sample_barrier();
    transfers[3] = g_test.source_.sample_barrier();
    transfers[4] = g_reference.yycxcz_blurred_.sample_barrier();
    transfers[5] = g_test.yycxcz_blurred_.sample_barrier();
    transfers[6] = g_error.sample_barrier(
        output_path ? VK_ACCESS_MEMORY_READ_BIT : VK_ACCESS_MEMORY_WRITE_BIT);
    transfers[7] = g_reference.yycxcz_blur_x_.sample_barrier();
    vkCmdPipelineBarrier(cb,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         8,
                         transfers);

    vkEndCommandBuffer(cb);
    VkSubmitInfo submit{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 0,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cb,
        .signalSemaphoreCount = 0,
    };
    vkQueueSubmit(g_graphics_queue, 1, &submit, VK_NULL_HANDLE);
    vkDeviceWaitIdle(g_device);

    if (output_path)
    {
        g_error_readback.write(output_path);
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    auto delta = end_time - start_time;
    int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();
    if (out_summary)
    {
        out_summary->milliseconds_elapsed = elapsed;
    }

    std::cout << "Evaluation time: " << elapsed << "ms\n"
              << "Error histogram: \n[";

    uint32_t histogram[32];
    std::memcpy(histogram, g_error_histogram.data_, sizeof(uint32_t) * 32);
    std::printf("%i", histogram[0]);
    uint32_t sample_count = histogram[0];
    for (uint32_t i = 1; i != 32u; ++i)
    {
        std::printf(", %i", histogram[i]);
    }
    std::printf("]\n");

    return 0;
}

int flop_analyze(char const* reference_path,
                 char const* test_path,
                 char const* output_path,
                 FlopSummary* out_summary)
{
    flop_init(0, nullptr);
    return flop_analyze_impl(
        reference_path, test_path, output_path, 1.f, 0, out_summary, false);
}

int flop_analyze_hdr(char const* reference_path,
                     char const* test_path,
                     char const* output_path,
                     float exposure,
                     int tonemapper,
                     FlopSummary* out_summary)
{
    flop_init(0, nullptr);
    return flop_analyze_impl(reference_path,
                             test_path,
                             output_path,
                             exposure,
                             tonemapper + 1,
                             out_summary,
                             false);
}
