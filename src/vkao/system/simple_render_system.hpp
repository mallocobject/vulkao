#pragma once

#include "core/ref.hpp"
#include "graphics/camera.hpp"
#include "graphics/device.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/pipeline.hpp"
#include "scene/entity.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

namespace vkao {
class SimpleRenderSystem {
  public:
    SimpleRenderSystem(Device &device,
                       VkRenderPass render_pass,
                       VkDescriptorSetLayout set_layout);
    ~SimpleRenderSystem();

    SimpleRenderSystem(const SimpleRenderSystem &) = delete;

    void render(FrameInfo &frame_info);

  private:
    void create_pipeline_layout(VkDescriptorSetLayout set_layout);
    void create_pipeline(VkRenderPass render_pass);

    Device &device_;

    Ref<Pipeline> pipeline_;
    VkPipelineLayout pipeline_layout_;
};
} // namespace vkao