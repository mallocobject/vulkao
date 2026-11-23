#pragma once

#include <vulkan/vulkan_core.h>

namespace vkao {
struct RenderTarget {
    VkRenderPass render_pass{VK_NULL_HANDLE};
    VkFramebuffer framebuffer{VK_NULL_HANDLE};
    VkExtent2D extent{};
};
} // namespace vkao