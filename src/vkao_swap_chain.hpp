#pragma once

#include "ref.hpp"
#include "vkao_device.hpp"

namespace vkao {
class VkaoSwapChain : public RefCounted {
  public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    VkaoSwapChain(VkaoDevice &device, VkExtent2D window_extent);
    VkaoSwapChain(VkaoDevice &device,
                  VkExtent2D window_extent,
                  const Ref<VkaoSwapChain> &previous);

    ~VkaoSwapChain();

    VkaoSwapChain(const VkaoSwapChain &) = delete;

    VkFramebuffer get_frame_buffer(int index) {
        return swap_chain_frame_buffers_[index];
    }
    VkRenderPass get_render_pass() {
        return render_pass_;
    }
    VkImageView get_image_view(int index) {
        return swap_chain_image_views_[index];
    }
    size_t image_count() {
        return swap_chain_images_.size();
    }
    VkFormat get_swap_chain_image_format() {
        return swap_chain_image_format_;
    }
    VkExtent2D get_swap_chain_extent() {
        return swap_chain_extent_;
    }
    uint32_t width() {
        return swap_chain_extent_.width;
    }
    uint32_t height() {
        return swap_chain_extent_.height;
    }

    float extent_aspect_ratio() {
        return static_cast<float>(swap_chain_extent_.width) /
               static_cast<float>(swap_chain_extent_.height);
    }
    VkFormat find_depth_format();

    VkResult acquire_next_image(uint32_t *image_index);
    VkResult submit_command_buffers(const VkCommandBuffer *buffers,
                                    uint32_t *image_index);

    bool compare_swap_formats(const VkaoSwapChain &swap_chain) const {
        return swap_chain.swap_chain_depth_format_ ==
                   swap_chain_depth_format_ &&
               swap_chain.swap_chain_image_format_ == swap_chain_image_format_;
    }

  private:
    void init();
    void create_swap_chain();
    void create_image_views();
    void create_depth_resources();
    void create_render_pass();
    void create_frame_buffers();
    void create_sync_objects();

    // Helper functions
    VkSurfaceFormatKHR choose_swap_surface_format(
        const std::vector<VkSurfaceFormatKHR> &available_formats);
    VkPresentModeKHR choose_swap_present_mode(
        const std::vector<VkPresentModeKHR> &available_present_modes);
    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities);

    VkFormat swap_chain_image_format_;
    VkFormat swap_chain_depth_format_;
    VkExtent2D swap_chain_extent_;

    std::vector<VkFramebuffer> swap_chain_frame_buffers_;
    VkRenderPass render_pass_;

    std::vector<VkImage> depth_images_;
    std::vector<VkDeviceMemory> depth_image_memorys_;
    std::vector<VkImageView> depth_image_views_;
    std::vector<VkImage> swap_chain_images_;
    std::vector<VkImageView> swap_chain_image_views_;

    VkaoDevice &device_;
    VkExtent2D window_extent_;

    VkSwapchainKHR swap_chain_;
    Ref<VkaoSwapChain> old_swap_chain_;

    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> inflight_fences_;
    std::vector<VkFence> images_inflight_;
    size_t current_frame_{0};
};
} // namespace vkao