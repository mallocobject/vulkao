#pragma once

#include "ref.hpp"
#include "vkao_device.hpp"
#include "vkao_model.hpp"
#include "vkao_pipeline.hpp"
#include "vulkan/vulkan_core.h"
#include <glm/glm.hpp>

namespace vkao {
struct SimplePushConstant {
    alignas(16) glm::mat3 transform{1.f};
    alignas(16) glm::vec3 offset;
    alignas(16) glm::vec3 color;
};
static_assert(sizeof(SimplePushConstant) == 80);

class SimpleRenderSystem {
  public:
    SimpleRenderSystem(VkaoDevice &device, VkRenderPass render_pass);
    ~SimpleRenderSystem();

    SimpleRenderSystem(const SimpleRenderSystem &) = delete;

    void draw(VkCommandBuffer command_buffer, const Ref<VkaoModel> &model);

  private:
    void create_pipeline_layout();
    void create_pipeline(VkRenderPass render_pass);

    VkaoDevice &vkao_device_;

    Ref<VkaoPipeline> vkao_pipeline_;
    VkPipelineLayout pipeline_layout_;
};
} // namespace vkao