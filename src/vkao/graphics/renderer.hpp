#pragma once

#include "core/logger.hpp"
#include "core/ref.hpp"
#include "graphics/device.hpp"
#include "graphics/render_target.hpp"
#include "graphics/swap_chain.hpp"
#include "platform/window.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {
class Renderer : public RefCounted {
  public:
    Renderer(Window &window, Device &device);
    ~Renderer();

    Renderer(const Renderer &) = delete;

    bool is_frame_in_progress() const {
        return is_frame_started_;
    }

    int get_frame_index() const {
        VKAO_ASSERT(is_frame_started_,
                    "cannot get frame index when frame not in progress!");
        return current_frame_index_;
    }

    VkCommandBuffer get_current_command_buffer() const {
        VKAO_ASSERT(is_frame_started_,
                    "cannot get command buffer when frame not in progress!");
        return command_buffers_[current_frame_index_];
    }

    VkRenderPass get_swap_chain_render_pass() const {
        return swap_chain_->get_render_pass();
    }

    RenderTarget get_current_render_target() const;

    float get_aspect_ratio() const {
        return swap_chain_->extent_aspect_ratio();
    }

    VkCommandBuffer begin_frame();
    void end_frame();

  private:
    void create_command_buffers();
    void free_command_buffers();
    void recreate_swap_chain();

    Window &window_;
    Device &device_;
    Ref<SwapChain> swap_chain_;
    std::vector<VkCommandBuffer> command_buffers_;

    uint32_t current_image_index_{0};
    int current_frame_index_{0};
    bool is_frame_started_{false};
};
} // namespace vkao