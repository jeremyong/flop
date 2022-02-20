#pragma once

#include "VkGlobals.hpp"

class Image;

class Fullscreen
{
public:
    void init(uint8_t const* shader_bytecode,
              size_t bytecode_size,
              uint8_t pushconstant_size);
    void reset();

    void render(VkCommandBuffer cb, Image const& color_target, void* push_constants) const;

private:
    VkPipeline pipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout layout_   = VK_NULL_HANDLE;
    uint8_t pushconstant_size_ = 0;
};
