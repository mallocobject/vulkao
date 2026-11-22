#pragma once

#include "graphics/camera.hpp"
#include "scene/registry.hpp"
#include <vulkan/vulkan_core.h>
namespace vkao {
struct FrameInfo {
    int frame_index;
    float frame_time;
    VkCommandBuffer command_buffer;
    Camera &camera;
    VkDescriptorSet descriptor_set;
    Registry &registry;
};
} // namespace vkao