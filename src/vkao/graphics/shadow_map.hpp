#pragma once

#include "core/ref.hpp"
#include "graphics/device.hpp"
#include "graphics/render_target.hpp"

namespace vkao {
class ShadowMap : public RefCounted {
  public:
    explicit ShadowMap(Device &device, VkExtent2D extent = {2048, 2048});
    ~ShadowMap();
    ShadowMap(const ShadowMap &) = delete;

    RenderTarget get_render_target() const {
        return {render_pass_, framebuffer_, extent_};
    }

    VkDescriptorImageInfo descriptor_info() const {
        return {
            sampler_,
            depth_view_,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        };
    }

    VkRenderPass get_render_pass() const {
        return render_pass_;
    }

  private:
    void create_depth_resources();
    void create_sampler();
    void create_render_pass();
    void create_framebuffer();

    Device &device_;
    VkExtent2D extent_;
    VkFormat depth_format_{VK_FORMAT_UNDEFINED};

    VkImage depth_image_{VK_NULL_HANDLE};
    VkDeviceMemory depth_memory_{VK_NULL_HANDLE};
    VkImageView depth_view_{VK_NULL_HANDLE};
    VkSampler sampler_{VK_NULL_HANDLE};
    VkRenderPass render_pass_{VK_NULL_HANDLE};
    VkFramebuffer framebuffer_{VK_NULL_HANDLE};
};
} // namespace vkao