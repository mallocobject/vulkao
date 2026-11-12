#include "first_app.hpp"

#include "logger.hpp"
#include "ref.hpp"
#include "simple_render_system.hpp"
#include "vkao_model.hpp"
#include "vulkan/vulkan_core.h"
#include <vector>

namespace vkao {
FirstApp::FirstApp() {
    load_models();
}

FirstApp::~FirstApp() {
}

void FirstApp::run() {
    SimpleRenderSystem render_system{
        vkao_device_, vkao_renderer_.get_swap_chain_render_pass()};

    while (running_) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                running_ = false;
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                vkao_window_.on_window_resized(event.window.data1,
                                               event.window.data2);
                break;
            }
        }

        if (auto command_buffer = vkao_renderer_.begin_frame()) {
            vkao_renderer_.begin_swap_chain_render_pass(command_buffer);
            // TODO
            render_system.draw(command_buffer, vkao_model_);
            vkao_renderer_.end_swap_chain_render_pass(command_buffer);
            vkao_renderer_.end_frame();
        }
    }

    vkDeviceWaitIdle(vkao_device_.get_device());
}

void FirstApp::load_models() {
    std::vector<VkaoModel::Vertex> vertexes{{
                                                {0.f, -0.5f, 0.f},
                                                {1.f, 0.f, 0.f},
                                            },
                                            {
                                                {0.5f, 0.5f, 0.f},
                                                {0.f, 1.f, 0.f},
                                            },
                                            {
                                                {-0.5f, 0.5f, 0.f},
                                                {0.f, 0.f, 1.f},
                                            }};

    vkao_model_ = make_ref<VkaoModel>(vkao_device_, vertexes);
}

} // namespace vkao