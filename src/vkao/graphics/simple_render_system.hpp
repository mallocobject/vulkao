#pragma once

#include "core/ref.hpp"
#include "graphics/camera.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "scene/entity.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

namespace vkao {
struct SimplePushConstant {
    alignas(16) glm::mat4 transform{1.f};
    alignas(16) glm::mat4 normal_matrix{1.f};
};
static_assert(sizeof(SimplePushConstant) == 128);

class SimpleRenderSystem {
  public:
    SimpleRenderSystem(Device &device, VkRenderPass render_pass);
    ~SimpleRenderSystem();

    SimpleRenderSystem(const SimpleRenderSystem &) = delete;

    void
    draw(VkCommandBuffer command_buffer, Entity entity, const Camera &camera);

  private:
    void create_pipeline_layout();
    void create_pipeline(VkRenderPass render_pass);

    Device &device_;

    Ref<Pipeline> pipeline_;
    VkPipelineLayout pipeline_layout_;
};
} // namespace vkao