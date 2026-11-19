#pragma once

#include "SDL3/SDL_video.h"
#include "core/ref.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

namespace vkao {
class Window : public RefCounted {
  public:
    Window(uint32_t w, uint32_t h, std::string_view name);
    ~Window();

    Window(const Window &) = delete;

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

    SDL_Window *get_window() const {
        return window_;
    }

  private:
    void init_window();

    uint32_t width_;
    uint32_t height_;
    bool frame_buffer_resized_{false};

    std::string window_name_;
    SDL_Window *window_;
};
} // namespace vkao