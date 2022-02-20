#include "Buffer.hpp"

using namespace flop;

uint32_t s_buffer_count;

Buffer Buffer::create(void const* data, uint32_t size)
{
    Buffer buffer;
    buffer.size_ = size;

    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = size,
        .usage
        = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices   = &g_graphics_queue_index,
    };

    VkBuffer staging;
    VmaAllocation staging_allocation;
    VmaAllocationCreateInfo allocation_info{.usage = VMA_MEMORY_USAGE_CPU_TO_GPU};
    vmaCreateBuffer(g_allocator,
                    &buffer_info,
                    &allocation_info,
                    &staging,
                    &staging_allocation,
                    nullptr);
    void* dst;
    vmaMapMemory(g_allocator, staging_allocation, &dst);
    std::memcpy(dst, data, size);

    allocation_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    buffer_info.usage
        = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vmaCreateBuffer(g_allocator,
                    &buffer_info,
                    &allocation_info,
                    &buffer.buffer_,
                    &buffer.allocation_,
                    nullptr);

    VkCommandBuffer cb = g_command_buffers[0];
    VkCommandBufferBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(cb, &begin);

    VkBufferCopy copy{.srcOffset = 0, .dstOffset = 0, .size = size};
    vkCmdCopyBuffer(cb, staging, buffer.buffer_, 1, &copy);

    vkEndCommandBuffer(cb);
    VkSubmitInfo submit{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 0,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cb,
        .signalSemaphoreCount = 0,
    };
    vkQueueSubmit(g_graphics_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_graphics_queue);

    vmaUnmapMemory(g_allocator, staging_allocation);
    vmaDestroyBuffer(g_allocator, staging, staging_allocation);

    buffer.index_ = s_buffer_count++;

    VkDescriptorBufferInfo descriptor_info{
        .buffer = buffer.buffer_, .offset = 0, .range = size};
    VkWriteDescriptorSet descriptor_write{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = g_descriptor_set,
        .dstBinding      = 2,
        .dstArrayElement = buffer.index_,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo     = &descriptor_info};
    vkUpdateDescriptorSets(g_device, 1, &descriptor_write, 0, nullptr);

    return buffer;
}

Buffer Buffer::create(uint32_t size)
{
    Buffer buffer;
    buffer.size_ = size;

    VkBufferCreateInfo buffer_info{
        .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size                  = size,
        .usage                 = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices   = &g_graphics_queue_index,
    };

    VmaAllocationCreateInfo allocation_info{.usage = VMA_MEMORY_USAGE_GPU_TO_CPU};
    vmaCreateBuffer(g_allocator,
                    &buffer_info,
                    &allocation_info,
                    &buffer.buffer_,
                    &buffer.allocation_,
                    nullptr);

    vmaMapMemory(g_allocator, buffer.allocation_, &buffer.data_);

    buffer.index_ = s_buffer_count++;

    VkDescriptorBufferInfo descriptor_info{
        .buffer = buffer.buffer_, .offset = 0, .range = size};
    VkWriteDescriptorSet descriptor_write{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = g_descriptor_set,
        .dstBinding      = 2,
        .dstArrayElement = buffer.index_,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo     = &descriptor_info};
    vkUpdateDescriptorSets(g_device, 1, &descriptor_write, 0, nullptr);

    return buffer;
}
