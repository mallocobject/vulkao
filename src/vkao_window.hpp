#pragma once

#include "ref.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <string>
#include <string_view>
#define VK_USE_PLATFORM_WIN32_KHR
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

namespace vkao {
class VkaoWindow : public RefCounted {
  public:
    VkaoWindow(uint32_t w, uint32_t h, std::string_view name);
    ~VkaoWindow();

    VkaoWindow(const VkaoWindow &) = delete;

    VkExtent2D get_extent() const {
        return {width_, height_};
    }

    void on_window_resized(uint32_t width, uint32_t height);

    bool was_window_resized() const {
        return frame_buffer_resized_;
    }

    void reset_window_resized_flag() {
        frame_buffer_resized_ = false;
    }

    void create_window_surface(VkInstance instance, VkSurfaceKHR *surface);

  private:
    void init_window();

    uint32_t width_;
    uint32_t height_;
    bool frame_buffer_resized_{false};

    std::string window_name_;
    SDL_Window *window_;
};
} // namespace vkao