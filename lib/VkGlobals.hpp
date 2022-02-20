#pragma once

#include <vector>
#include <volk.h>

#include <vk_mem_alloc.h>

namespace flop
{
inline VkInstance g_instance                              = VK_NULL_HANDLE;
inline VkPhysicalDevice g_physical_device                 = VK_NULL_HANDLE;
inline VkPhysicalDeviceProperties g_physical_device_props = {};
inline PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabel = nullptr;
inline PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabel     = nullptr;
inline uint32_t g_graphics_queue_index                            = -0u;
inline uint32_t g_compute_queue_index                             = -0u;
inline VkQueue g_graphics_queue     = VK_NULL_HANDLE;
inline VkQueue g_compute_queue      = VK_NULL_HANDLE;
inline VkDevice g_device            = VK_NULL_HANDLE;
inline VmaAllocator g_allocator     = VK_NULL_HANDLE;
inline VkCommandPool g_command_pool = VK_NULL_HANDLE;
inline VkCommandBuffer g_command_buffers[]
    = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
inline VkDescriptorPool g_descriptor_pool            = VK_NULL_HANDLE;
inline VkDescriptorSetLayout g_descriptor_set_layout = VK_NULL_HANDLE;
inline VkDescriptorSet g_descriptor_set              = VK_NULL_HANDLE;

// Helper function to retrieve a count, and then populate a vector with
// count entries
template <typename T, typename F, typename... Ts>
std::vector<T> vk_enumerate(F fn, Ts... args)
{
    uint32_t count;
    fn(args..., &count, nullptr);

    std::vector<T> out{count};
    fn(args..., &count, out.data());

    return out;
}
} // namespace flop
