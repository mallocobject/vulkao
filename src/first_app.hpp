#pragma once

#include "glm/ext/matrix_float3x3.hpp"
#include "glm/ext/vector_float3.hpp"
#include "ref.hpp"
#include "vkao_device.hpp"
#include "vkao_model.hpp"
#include "vkao_pipeline.hpp"
#include "vkao_swap_chain.hpp"
#include "vkao_window.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace vkao {
struct SimplePushConstant {
    alignas(16) glm::mat3 transform{1.f};
    alignas(16) glm::vec3 offset;
    alignas(16) glm::vec3 color;
};
static_assert(sizeof(SimplePushConstant) == 80);

class FirstApp {
  public:
    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;

    FirstApp();
    ~FirstApp();

    FirstApp(const FirstApp &) = delete;

    void run();

  private:
    void load_models();
    void create_pipeline_layout();
    void create_pipeline();
    void create_command_buffers();
    void free_command_buffers();
    void draw_frame();
    void recreate_swap_chain();
    void record_command_buffer(int image_index);

    VkaoWindow vkao_window_{WIDTH, HEIGHT, "vulkao"};
    VkaoDevice vkao_device_{vkao_window_};
    Ref<VkaoSwapChain> vkao_swap_chain_;
    Ref<VkaoPipeline> vkao_pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkCommandBuffer> command_buffers_;
    Ref<VkaoModel> vkao_model_;
    bool running_{true};
};
} // namespace vkao