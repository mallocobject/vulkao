#pragma once

#include "core/ref.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/render_target.hpp"
#include <type_traits>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {
class RenderPass : public RefCounted {
  public:
    RenderPass() = default;
    virtual ~RenderPass() = default;
    RenderPass(const RenderPass &) = delete;

    void render(FrameInfo &frame_info, const RenderTarget &target) {
        begin(frame_info.command_buffer, target);
        draw_scene(frame_info);
        end(frame_info.command_buffer);
    }

  protected:
    virtual void
    create_pipeline_layout(std::vector<VkDescriptorSetLayout> set_layouts) = 0;
    virtual void create_pipeline(VkRenderPass render_pass) = 0;

    virtual void begin(VkCommandBuffer command_buffer,
                       const RenderTarget &target) = 0;
    virtual void draw_scene(FrameInfo &frame_info) = 0;
    virtual void end(VkCommandBuffer command_buffer) = 0;
};

static_assert(!std::is_copy_constructible_v<RenderPass>);
static_assert(!std::is_copy_assignable_v<RenderPass>);
static_assert(!std::is_move_constructible_v<RenderPass>);
static_assert(!std::is_move_assignable_v<RenderPass>);
} // namespace vkao