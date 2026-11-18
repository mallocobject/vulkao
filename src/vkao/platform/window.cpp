#include "platform/window.hpp"

#include "core/logger.hpp"

namespace vkao {
Window::Window(uint32_t w, uint32_t h, std::string_view name)
    : width_{w}, height_{h}, window_name_(name) {
    VKAO_INFO("hello {}", window_name_);

    init_window();
}

Window::~Window() {
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

void Window::on_window_resized(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    frame_buffer_resized_ = true;
}

void Window::init_window() {
    SDL_Init(SDL_INIT_VIDEO);
    window_ = SDL_CreateWindow(window_name_.c_str(),
                               width_,
                               height_,
                               SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
}

void Window::create_window_surface(VkInstance instance,
                                       VkSurfaceKHR *surface) {
    VKAO_ASSERT(SDL_Vulkan_CreateSurface(window_, instance, nullptr, surface),
                "failed to craete window surface");
}
} // namespace vkao