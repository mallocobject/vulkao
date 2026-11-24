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

    void render(FrameInfo &frame_info, const RenderTarget &target) override;
    void render(FrameInfo &frame_info,
                const RenderTarget &target,
                VkDescriptorSet shadow_descriptor_set);

  private:
    void create_pipeline_layout(std::vector<VkDescriptorSetLayout> set_layouts);
    void create_pipeline(VkRenderPass render_pass);

    void begin(VkCommandBuffer command_buffer, const RenderTarget &target);
    void draw_scene(FrameInfo &frame_info);
    void draw_scene(FrameInfo &frame_info,
                    VkDescriptorSet shadow_descriptor_set);
    void end(VkCommandBuffer command_buffer);

    Device &device_;
    Ref<Pipeline> pipeline_;
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
};
} // namespace vkao