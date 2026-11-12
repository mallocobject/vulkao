#pragma once

#include "ref.hpp"
#include "vkao_device.hpp"
#include "vkao_entity.hpp"
#include "vkao_pipeline.hpp"
#include "vulkan/vulkan_core.h"
#include <glm/glm.hpp>

namespace vkao {
struct SimplePushConstant {
    alignas(16) glm::mat4 transform{1.f};
};
static_assert(sizeof(SimplePushConstant) == 64);

class SimpleRenderSystem {
  public:
    SimpleRenderSystem(VkaoDevice &device, VkRenderPass render_pass);
    ~SimpleRenderSystem();

    SimpleRenderSystem(const SimpleRenderSystem &) = delete;

    void draw(VkCommandBuffer command_buffer, Entity entity);

  private:
    void create_pipeline_layout();
    void create_pipeline(VkRenderPass render_pass);

    VkaoDevice &vkao_device_;

    Ref<VkaoPipeline> vkao_pipeline_;
    VkPipelineLayout pipeline_layout_;
};
} // namespace vkao