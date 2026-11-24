#pragma once

#include "core/ref.hpp"
#include "graphics/device.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/render_target.hpp"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {
enum class ShadowProjection {
    DirectionalAtlas,
    PointCube,
    Spot,
};

struct ShadowDraw {
    ShadowProjection projection;
    RenderTarget target;
    VkDescriptorSet draw_set;
};

class ShadowPass : public RefCounted {
  public:
    ShadowPass(Device &device,
               VkRenderPass render_pass,
               std::vector<VkDescriptorSetLayout> set_layouts);
    ~ShadowPass();
    ShadowPass(const ShadowPass &) = delete;

    void render(FrameInfo &frame_info, const ShadowDraw &draw);

  private:
    void create_pipeline_layout(std::vector<VkDescriptorSetLayout> set_layouts);
    void create_pipeline(VkRenderPass render_pass);

    void begin(VkCommandBuffer command_buffer,
               const RenderTarget &target,
               ShadowProjection projection);
    void draw_scene(FrameInfo &frame_info,
                    VkDescriptorSet draw_set,
                    uint32_t instance_count = 1);
    void end(VkCommandBuffer command_buffer);

    Device &device_;
    Ref<Pipeline> pipeline_;
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
};
} // namespace vkao