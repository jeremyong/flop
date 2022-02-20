#include "Image.hpp"

// Forward declare STBI calls to avoid including a massive header
#include <cstdio>
#include <cstdlib>
extern "C"
{
    unsigned char* stbi_load(char const* filename,
                             int* x,
                             int* y,
                             int* channels_in_file,
                             int desired_channels);
    void stbi_image_free(void* retval_from_stbi_load);
    int stbi_write_png(char const* filename,
                       int w,
                       int h,
                       int comp,
                       const void* data,
                       int stride_in_bytes);
}

using namespace flop;

static int s_image_count;

constexpr static VkImageSubresourceRange s_transfer_range{
    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
    .baseMipLevel   = 0,
    .levelCount     = 1,
    .baseArrayLayer = 0,
    .layerCount     = 1};

constexpr static VkImageSubresourceLayers s_subresource{
    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
    .mipLevel       = 0,
    .baseArrayLayer = 0,
    .layerCount     = 1};

void Image::reset_count()
{
    s_image_count = 0;
}

Image Image::create_from_file(char const* path)
{
    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;
    Image image;

    unsigned char* stb_data
        = stbi_load(path, &image.width_, &image.height_, &image.channels_, 4);
    image.set_extents();

    VmaAllocationCreateInfo staging_allocation_info{
        .usage = VMA_MEMORY_USAGE_CPU_ONLY,
    };
    VkBufferCreateInfo staging_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = static_cast<VkDeviceSize>(image.width_ * image.height_ * 4),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices   = &g_graphics_queue_index,
    };
    vmaCreateBuffer(g_allocator,
                    &staging_info,
                    &staging_allocation_info,
                    &staging_buffer,
                    &staging_allocation,
                    nullptr);
    void* data;
    vmaMapMemory(g_allocator, staging_allocation, &data);
    std::memcpy(data, stb_data, image.width_ * image.height_ * 4);

    VkImageCreateInfo image_info{
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = VK_FORMAT_R8G8B8A8_SRGB,
        .extent      = image.extent3_,
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                 | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices   = &g_graphics_queue_index,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VmaAllocationCreateInfo allocation_info{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };
    vmaCreateImage(g_allocator,
                   &image_info,
                   &allocation_info,
                   &image.image_,
                   &image.allocation_,
                   nullptr);

    VkCommandBuffer cb = g_command_buffers[0];
    VkCommandBufferBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cb, &begin);

    VkImageMemoryBarrier dst_transfer{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_MEMORY_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = g_graphics_queue_index,
        .dstQueueFamilyIndex = g_graphics_queue_index,
        .image               = image.image_,
        .subresourceRange    = s_transfer_range};
    vkCmdPipelineBarrier(cb,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &dst_transfer);
    VkOffset3D offset{.x = 0, .y = 0, .z = 0};
    VkBufferImageCopy copy{.bufferOffset      = 0,
                           .bufferRowLength   = 0,
                           .bufferImageHeight = 0,
                           .imageSubresource  = s_subresource,
                           .imageOffset       = offset,
                           .imageExtent       = image.extent3_};
    vkCmdCopyBufferToImage(cb,
                           staging_buffer,
                           image.image_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &copy);

    VkImageMemoryBarrier src_transfer{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_MEMORY_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = g_graphics_queue_index,
        .dstQueueFamilyIndex = g_graphics_queue_index,
        .image               = image.image_,
        .subresourceRange    = s_transfer_range};
    vkCmdPipelineBarrier(cb,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &src_transfer);
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

    stbi_image_free(stb_data);
    vmaUnmapMemory(g_allocator, staging_allocation);
    vmaDestroyBuffer(g_allocator, staging_buffer, staging_allocation);

    VkImageSubresourceRange range{
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };
    VkImageViewCreateInfo view_info{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = image.image_,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = image_info.format,
        .subresourceRange = range};
    vkCreateImageView(g_device, &view_info, nullptr, &image.image_view_);

    image.index_ = s_image_count++;

    VkDescriptorImageInfo descriptor_info{
        .imageView   = image.image_view_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet descriptor_write{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = g_descriptor_set,
        .dstBinding      = 0,
        .dstArrayElement = image.index_,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo      = &descriptor_info,
    };
    vkUpdateDescriptorSets(g_device, 1, &descriptor_write, 0, nullptr);

    return image;
}

Image Image::create(const Image& other, VkFormat format, bool attachment)
{
    // Create an RGB image with matching dimensions to the supplied image
    Image image;
    image.width_    = other.width_;
    image.height_   = other.height_;
    image.writable_ = true;
    image.set_extents();

    VkImageCreateInfo image_info{
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = format,
        .extent      = image.extent3_,
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                 | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                 | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices   = &g_graphics_queue_index,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if (attachment)
    {
        if (format == VK_FORMAT_R8G8B8A8_SRGB)
        {
            image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                               | VK_IMAGE_USAGE_SAMPLED_BIT
                               | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        else
        {
            image_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }

    VmaAllocationCreateInfo allocation_info{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };
    vmaCreateImage(g_allocator,
                   &image_info,
                   &allocation_info,
                   &image.image_,
                   &image.allocation_,
                   nullptr);

    VkImageSubresourceRange range{
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };
    VkImageViewCreateInfo view_info{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = image.image_,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = image_info.format,
        .subresourceRange = range};
    vkCreateImageView(g_device, &view_info, nullptr, &image.image_view_);

    image.index_ = s_image_count++;

    VkDescriptorImageInfo descriptor_info{
        .imageView = image.image_view_, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet descriptor_write{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = g_descriptor_set,
        .dstBinding      = 1,
        .dstArrayElement = image.index_,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo      = &descriptor_info,
    };
    if (image_info.usage & VK_IMAGE_USAGE_STORAGE_BIT)
    {
        vkUpdateDescriptorSets(g_device, 1, &descriptor_write, 0, nullptr);
    }
    descriptor_info.imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptor_write.dstBinding     = 0;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    vkUpdateDescriptorSets(g_device, 1, &descriptor_write, 0, nullptr);

    return image;
}

void Image::set_extents()
{
    extent2_.width  = width_;
    extent2_.height = height_;
    extent3_.width  = width_;
    extent3_.height = height_;
    extent3_.depth  = 1;
}

Image Image::create_readback(Image const& other, VkFormat format)
{
    Image image;
    image.width_  = other.width_;
    image.height_ = other.height_;
    image.set_extents();
    image.writable_ = true;

    VkImageCreateInfo image_info{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = format,
        .extent                = image.extent3_,
        .mipLevels             = 1,
        .arrayLayers           = 1,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_LINEAR,
        .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices   = &g_graphics_queue_index,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocation_info{
        .usage = VMA_MEMORY_USAGE_GPU_TO_CPU,
    };
    vmaCreateImage(g_allocator,
                   &image_info,
                   &allocation_info,
                   &image.image_,
                   &image.allocation_,
                   nullptr);
    return image;
}

void Image::reset()
{
    if (allocation_ != VK_NULL_HANDLE)
    {
        vmaDestroyImage(g_allocator, image_, allocation_);
        if (image_view_ != VK_NULL_HANDLE)
        {
            vkDestroyImageView(g_device, image_view_, nullptr);
            image_view_ = VK_NULL_HANDLE;
        }
        allocation_ = VK_NULL_HANDLE;
        image_      = VK_NULL_HANDLE;
        width_      = 0;
        height_     = 0;
        channels_   = 0;
    }
}

void Image::readback(VkCommandBuffer cb, Image& readback)
{
    VkImageCopy copy{.srcSubresource = s_subresource,
                     .srcOffset      = {.x = 0, .y = 0, .z = 0},
                     .dstSubresource = s_subresource,
                     .dstOffset      = {.x = 0, .y = 0, .z = 0},
                     .extent         = readback.extent3_};
    vkCmdCopyImage(cb,
                   image_,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   readback.image_,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &copy);
}

void Image::write(std::string const& path)
{
    // Query row stride
    VkImageSubresource subresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .arrayLayer = 0};
    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(g_device, image_, &subresource, &layout);

    uint8_t* data;
    vmaMapMemory(g_allocator, allocation_, reinterpret_cast<void**>(&data));
    data += layout.offset;
    stbi_write_png(path.c_str(), width_, height_, 4, data, layout.rowPitch);
    vmaUnmapMemory(g_allocator, allocation_);
}

VkImageMemoryBarrier Image::start_barrier(VkImageLayout layout)
{
    layout_ = layout;
    return {.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_NONE_KHR,
            .dstAccessMask       = VK_ACCESS_MEMORY_WRITE_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = layout_,
            .srcQueueFamilyIndex = g_graphics_queue_index,
            .dstQueueFamilyIndex = g_graphics_queue_index,
            .image               = image_,
            .subresourceRange    = s_transfer_range};
}

VkImageMemoryBarrier Image::war_barrier()
{
    return {.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask       = VK_ACCESS_MEMORY_WRITE_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = g_graphics_queue_index,
            .dstQueueFamilyIndex = g_graphics_queue_index,
            .image               = image_,
            .subresourceRange    = s_transfer_range};
}

VkImageMemoryBarrier Image::raw_barrier(VkAccessFlags access)
{
    VkImageLayout old = layout_;
    layout_           = VK_IMAGE_LAYOUT_GENERAL;
    return {.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = access,
            .dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
            .oldLayout           = old,
            .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = g_graphics_queue_index,
            .dstQueueFamilyIndex = g_graphics_queue_index,
            .image               = image_,
            .subresourceRange    = s_transfer_range};
}

VkImageMemoryBarrier Image::rar_barrier(VkImageLayout layout)
{
    VkImageLayout old = layout_;
    layout_           = layout;
    return {.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout           = old,
            .newLayout           = layout_,
            .srcQueueFamilyIndex = g_graphics_queue_index,
            .dstQueueFamilyIndex = g_graphics_queue_index,
            .image               = image_,
            .subresourceRange    = s_transfer_range};
}

VkImageMemoryBarrier Image::waw_barrier()
{
    VkImageLayout old = layout_;
    layout_           = VK_IMAGE_LAYOUT_GENERAL;
    return {.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask       = VK_ACCESS_MEMORY_WRITE_BIT,
            .oldLayout           = old,
            .newLayout           = layout_,
            .srcQueueFamilyIndex = g_graphics_queue_index,
            .dstQueueFamilyIndex = g_graphics_queue_index,
            .image               = image_,
            .subresourceRange    = s_transfer_range};
}

VkImageMemoryBarrier Image::blit_barrier()
{
    VkImageLayout old = layout_;
    layout_           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    return {.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
            .oldLayout           = old,
            .newLayout           = layout_,
            .srcQueueFamilyIndex = g_graphics_queue_index,
            .dstQueueFamilyIndex = g_graphics_queue_index,
            .image               = image_,
            .subresourceRange    = s_transfer_range};
}

VkImageMemoryBarrier Image::sample_barrier(VkAccessFlags src_access)
{
    VkImageLayout old = layout_;
    layout_           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return {.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = src_access,
            .dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
            .oldLayout           = old,
            .newLayout           = layout_,
            .srcQueueFamilyIndex = g_graphics_queue_index,
            .dstQueueFamilyIndex = g_graphics_queue_index,
            .image               = image_,
            .subresourceRange    = s_transfer_range};
}

VkImageMemoryBarrier Image::readback_barrier()
{
    VkImageLayout old = layout_;
    layout_           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    return {.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_NONE_KHR,
            .dstAccessMask       = VK_ACCESS_MEMORY_WRITE_BIT,
            .oldLayout           = old,
            .newLayout           = layout_,
            .srcQueueFamilyIndex = g_graphics_queue_index,
            .dstQueueFamilyIndex = g_graphics_queue_index,
            .image               = image_,
            .subresourceRange    = s_transfer_range};
}
