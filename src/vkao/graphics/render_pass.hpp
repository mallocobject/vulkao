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

    virtual void render(FrameInfo &frame_info, const RenderTarget &target) = 0;
};

static_assert(!std::is_copy_constructible_v<RenderPass>);
static_assert(!std::is_copy_assignable_v<RenderPass>);
static_assert(!std::is_move_constructible_v<RenderPass>);
static_assert(!std::is_move_assignable_v<RenderPass>);
} // namespace vkao