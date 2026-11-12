#include "vkao_window.hpp"

#include "logger.hpp"

namespace vkao {
VkaoWindow::VkaoWindow(uint32_t w, uint32_t h, std::string_view name)
    : width_{w}, height_{h}, window_name_(name) {
    VKAO_INFO("hello {}", window_name_);

    init_window();
}

VkaoWindow::~VkaoWindow() {
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

void VkaoWindow::on_window_resized(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    frame_buffer_resized_ = true;
}

void VkaoWindow::init_window() {
    SDL_Init(SDL_INIT_VIDEO);
    window_ = SDL_CreateWindow(window_name_.c_str(),
                               width_,
                               height_,
                               SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
}

void VkaoWindow::create_window_surface(VkInstance instance,
                                       VkSurfaceKHR *surface) {
    VKAO_ASSERT(SDL_Vulkan_CreateSurface(window_, instance, nullptr, surface),
                "failed to craete window surface");
}
} // namespace vkao