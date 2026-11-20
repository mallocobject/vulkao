#pragma once

#include "graphics/camera.hpp"
#include <vulkan/vulkan_core.h>
namespace vkao {
struct FrameInfo {
    int frame_index;
    float frame_time;
    VkCommandBuffer command_buffer;
    Camera &camera;
};
} // namespace vkao