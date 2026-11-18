#include "app/first_app.hpp"

#include "core/logger.hpp"
#include "core/ref.hpp"
#include "graphics/camera.hpp"
#include "graphics/simple_render_system.hpp"
#include "scene/component.hpp"
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {
FirstApp::FirstApp() {
    load_entities();
}

FirstApp::~FirstApp() {
}

void FirstApp::run() {
    SimpleRenderSystem render_system{device_,
                                     renderer_.get_swap_chain_render_pass()};

    Camera camera;

    while (running_) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                running_ = false;
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                window_.on_window_resized(event.window.data1,
                                          event.window.data2);
                break;
            }
        }

        auto aspect_ratio = renderer_.get_aspect_ratio();
        camera.set_perspective_projection(
            glm::radians(45.f), aspect_ratio, 0.1f, 10.f);

        if (auto command_buffer = renderer_.begin_frame()) {
            renderer_.begin_swap_chain_render_pass(command_buffer);
            // TODO
            render_system.draw(
                command_buffer, entities_.begin()->second, camera);
            renderer_.end_swap_chain_render_pass(command_buffer);
            renderer_.end_frame();
        }
    }

    vkDeviceWaitIdle(device_.get_device());
}

void FirstApp::load_entities() {
    auto entity = registry_.create_entity();
    entities_[entity.get_component<IdComponent>().id] = entity;

    // Triangle list: two triangles per face. normal stores RGB; uv is unused.
    const std::vector<Vertex> vertices = {
        // +X: red
        {{0.5f, -0.5f, 0.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}},
        {{0.5f, -0.5f, -0.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, -0.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}},
        {{0.5f, -0.5f, 0.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, -0.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, 0.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}},

        // -X: cyan
        {{-0.5f, -0.5f, -0.5f}, {0.f, 1.f, 1.f}, {0.f, 0.f}},
        {{-0.5f, -0.5f, 0.5f}, {0.f, 1.f, 1.f}, {0.f, 0.f}},
        {{-0.5f, 0.5f, 0.5f}, {0.f, 1.f, 1.f}, {0.f, 0.f}},
        {{-0.5f, -0.5f, -0.5f}, {0.f, 1.f, 1.f}, {0.f, 0.f}},
        {{-0.5f, 0.5f, 0.5f}, {0.f, 1.f, 1.f}, {0.f, 0.f}},
        {{-0.5f, 0.5f, -0.5f}, {0.f, 1.f, 1.f}, {0.f, 0.f}},

        // +Y: green
        {{-0.5f, 0.5f, 0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, 0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, -0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
        {{-0.5f, 0.5f, 0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, -0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
        {{-0.5f, 0.5f, -0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},

        // -Y: magenta
        {{-0.5f, -0.5f, -0.5f}, {1.f, 0.f, 1.f}, {0.f, 0.f}},
        {{0.5f, -0.5f, -0.5f}, {1.f, 0.f, 1.f}, {0.f, 0.f}},
        {{0.5f, -0.5f, 0.5f}, {1.f, 0.f, 1.f}, {0.f, 0.f}},
        {{-0.5f, -0.5f, -0.5f}, {1.f, 0.f, 1.f}, {0.f, 0.f}},
        {{0.5f, -0.5f, 0.5f}, {1.f, 0.f, 1.f}, {0.f, 0.f}},
        {{-0.5f, -0.5f, 0.5f}, {1.f, 0.f, 1.f}, {0.f, 0.f}},

        // +Z: blue
        {{-0.5f, -0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},
        {{0.5f, -0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},
        {{-0.5f, -0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},
        {{-0.5f, 0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},

        // -Z: yellow
        {{0.5f, -0.5f, -0.5f}, {1.f, 1.f, 0.f}, {0.f, 0.f}},
        {{-0.5f, -0.5f, -0.5f}, {1.f, 1.f, 0.f}, {0.f, 0.f}},
        {{-0.5f, 0.5f, -0.5f}, {1.f, 1.f, 0.f}, {0.f, 0.f}},
        {{0.5f, -0.5f, -0.5f}, {1.f, 1.f, 0.f}, {0.f, 0.f}},
        {{-0.5f, 0.5f, -0.5f}, {1.f, 1.f, 0.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, -0.5f}, {1.f, 1.f, 0.f}, {0.f, 0.f}},

    };

    auto mesh = make_ref<Mesh>(device_, vertices);
    entity.add_component<MeshComponent>(mesh);
}

} // namespace vkao