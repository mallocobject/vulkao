#pragma once

#include "logger.hpp"
#include "ref.hpp"
#include "vkao_device.hpp"
#include "vkao_swap_chain.hpp"
#include "vkao_window.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace vkao {
class VkaoRenderer {
  public:
    VkaoRenderer(VkaoWindow &window, VkaoDevice &device);
    ~VkaoRenderer();

    VkaoRenderer(const VkaoRenderer &) = delete;

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
        return vkao_swap_chain_->get_render_pass();
    }

    VkCommandBuffer begin_frame();
    void end_frame();
    void begin_swap_chain_render_pass(VkCommandBuffer command_buffer);
    void end_swap_chain_render_pass(VkCommandBuffer command_buffer);

  private:
    void create_command_buffers();
    void free_command_buffers();
    void recreate_swap_chain();

    VkaoWindow &vkao_window_;
    VkaoDevice &vkao_device_;
    Ref<VkaoSwapChain> vkao_swap_chain_;
    std::vector<VkCommandBuffer> command_buffers_;

    uint32_t current_image_index_{0};
    int current_frame_index_{0};
    bool is_frame_started_{false};
};
} // namespace vkao