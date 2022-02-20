#pragma once

#include "VkGlobals.hpp"

#include <string>

class Image
{
public:
    static void reset_count();

    // Decodes an image and uploads it to the GPU. The result is provided in the
    // shader read-only layout.
    static Image create_from_file(char const* path);

    // Creates a device image with matching dimensions. The image layout that
    // results is undefined.
    static Image
    create(const Image& other, VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT, bool attachment = false);

    // Creates a host image with matching dimensions suitable for readback.
    static Image create_readback(Image const& other,
                                 VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

    void reset();
    float aspect() const
    {
        return static_cast<float>(width_) / height_;
    }

    void readback(VkCommandBuffer cb, Image& readback);
    void write(std::string const& path);

    void set_extents();

    VkImageMemoryBarrier start_barrier(VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);
    VkImageMemoryBarrier blit_barrier();
    VkImageMemoryBarrier war_barrier();
    VkImageMemoryBarrier raw_barrier(VkAccessFlags access = VK_ACCESS_MEMORY_WRITE_BIT);
    VkImageMemoryBarrier rar_barrier(VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkImageMemoryBarrier waw_barrier();
    VkImageMemoryBarrier sample_barrier(VkAccessFlags src_access = VK_ACCESS_MEMORY_WRITE_BIT);
    VkImageMemoryBarrier readback_barrier();

    VkImage image_            = VK_NULL_HANDLE;
    VkImageView image_view_   = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    VkImageLayout layout_     = VK_IMAGE_LAYOUT_UNDEFINED;

    VkExtent2D extent2_ = {};
    VkExtent3D extent3_ = {};
    int32_t width_      = 0;
    int32_t height_     = 0;
    int32_t channels_   = 0;
    uint32_t index_     = 0;
    bool writable_      = false;
};
