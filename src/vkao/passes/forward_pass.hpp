#pragma once

#include "graphics/device.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/render_target.hpp"
#include <vulkan/vulkan_core.h>

namespace vkao {
class ForwardPass {
  public:
    ForwardPass(Device &device,
                VkRenderPass render_pass,
                VkDescriptorSetLayout global_set_layout);

    ~ForwardPass();

    ForwardPass(const ForwardPass &) = delete;

    void render(FrameInfo &frame_info, const RenderTarget &target);

  private:
    void create_pipeline_layout(VkDescriptorSetLayout global_set_layout);
    void create_pipeline(VkRenderPass render_pass);

    void begin(VkCommandBuffer command_buffer, const RenderTarget &target);
    void draw_scene(FrameInfo &frame_info);
    void end(VkCommandBuffer command_buffer);

    Device &device_;
    Ref<Pipeline> pipeline_;
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
};
} // namespace vkao