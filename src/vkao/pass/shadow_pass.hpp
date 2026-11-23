#pragma once

#include "graphics/device.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/render_pass.hpp"
#include "graphics/render_target.hpp"
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {
class ShadowPass : public RenderPass {
  public:
    ShadowPass(Device &device,
               VkRenderPass render_pass,
               std::vector<VkDescriptorSetLayout> set_layouts);
    ~ShadowPass();

  private:
    void create_pipeline_layout(
        std::vector<VkDescriptorSetLayout> set_layouts) override;
    void create_pipeline(VkRenderPass render_pass) override;

    void begin(VkCommandBuffer command_buffer,
               const RenderTarget &target) override;
    void draw_scene(FrameInfo &frame_info) override;
    void end(VkCommandBuffer command_buffer) override;

    Device &device_;
    Ref<Pipeline> pipeline_;
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
};
} // namespace vkao