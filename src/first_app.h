#pragma once

#include "ref.hpp"
#include "vkao_device.h"
#include "vkao_pipeline.h"
#include "vkao_swap_chain.h"
#include "vkao_window.h"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <vector>

namespace vkao {
class FirstApp : public RefCounted {
  public:
    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;

    FirstApp();
    ~FirstApp();

    FirstApp(const FirstApp &) = delete;

    void run();

  private:
    void create_pipeline_layout();
    void create_pipeline();
    void create_command_buffers();
    void draw_frame();

    VkaoWindow vkao_window_{WIDTH, HEIGHT, "vulkao"};
    VkaoDevice vkao_device_{vkao_window_};
    VkaoSwapChain vkao_swap_chain_{vkao_device_, vkao_window_.get_extent()};
    Ref<VkaoPipeline> vkao_pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkCommandBuffer> command_buffers_;
    bool running_{true};
};
} // namespace vkao