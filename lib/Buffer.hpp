#pragma once

#include "VkGlobals.hpp"

class Buffer
{
public:
    // Create a device local buffer and copy supplied data via staging memory
    static Buffer create(void const* data, uint32_t size);

    // Create a writable readback buffer
    static Buffer create(uint32_t size);

    VkBuffer buffer_          = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;

    uint32_t size_  = 0;
    uint32_t index_ = 0;

    // Available only for readback data
    void* data_ = nullptr;
};
