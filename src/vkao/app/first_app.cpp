#include "app/first_app.hpp"

#include "core/logger.hpp"
#include "core/ref.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/trigonometric.hpp"
#include "graphics/camera.hpp"
#include "graphics/camera_controller.hpp"
#include "graphics/mesh.hpp"
#include "graphics/simple_render_system.hpp"
#include "scene/component.hpp"
#include <chrono>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {
FirstApp::FirstApp() {
    viewer_ = registry_.create_entity();
    auto &transform = viewer_.get_component<TransformComponent>();
    transform.translation = glm::vec3{0.f, 0.f, -1.f};
    transform.rotation = glm::vec3{0.f, 0.f, 0.f};

    load_entities();
}

FirstApp::~FirstApp() {
}

void FirstApp::run() {
    SimpleRenderSystem render_system{device_,
                                     renderer_.get_swap_chain_render_pass()};

    Camera camera;

    CameraController camera_controller;

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

        static auto last = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        // update camera
        camera_controller.rotate_around_X_and_Y(
            window_.get_window(), dt, viewer_);
        camera_controller.move_in_plane_XZ(window_.get_window(), dt, viewer_);

        auto &transform = viewer_.get_component<TransformComponent>();
        camera.set_view_YXZ(transform.translation, transform.rotation);

        auto aspect_ratio = renderer_.get_aspect_ratio();
        camera.set_perspective_projection(
            glm::radians(45.f), aspect_ratio, 0.1f, 1000.f);

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
    Mesh::Builder builder;
    builder.vertices = {
        // +X: red
        {{0.5f, -0.5f, 0.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}},  // 0
        {{0.5f, -0.5f, -0.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}}, // 1
        {{0.5f, 0.5f, -0.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}},  // 2
        {{0.5f, 0.5f, 0.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}},   // 3

        // -X: cyan
        {{-0.5f, -0.5f, -0.5f}, {0.f, 1.f, 1.f}, {0.f, 0.f}}, // 4
        {{-0.5f, -0.5f, 0.5f}, {0.f, 1.f, 1.f}, {0.f, 0.f}},  // 5
        {{-0.5f, 0.5f, 0.5f}, {0.f, 1.f, 1.f}, {0.f, 0.f}},   // 6
        {{-0.5f, 0.5f, -0.5f}, {0.f, 1.f, 1.f}, {0.f, 0.f}},  // 7

        // +Y: green
        {{-0.5f, 0.5f, 0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},  // 8
        {{0.5f, 0.5f, 0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},   // 9
        {{0.5f, 0.5f, -0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},  // 10
        {{-0.5f, 0.5f, -0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}}, // 11

        // -Y: magenta
        {{-0.5f, -0.5f, -0.5f}, {1.f, 0.f, 1.f}, {0.f, 0.f}}, // 12
        {{0.5f, -0.5f, -0.5f}, {1.f, 0.f, 1.f}, {0.f, 0.f}},  // 13
        {{0.5f, -0.5f, 0.5f}, {1.f, 0.f, 1.f}, {0.f, 0.f}},   // 14
        {{-0.5f, -0.5f, 0.5f}, {1.f, 0.f, 1.f}, {0.f, 0.f}},  // 15

        // +Z: blue
        {{-0.5f, -0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}}, // 16
        {{0.5f, -0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},  // 17
        {{0.5f, 0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},   // 18
        {{-0.5f, 0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},  // 19

        // -Z: yellow
        {{0.5f, -0.5f, -0.5f}, {1.f, 1.f, 0.f}, {0.f, 0.f}},  // 20
        {{-0.5f, -0.5f, -0.5f}, {1.f, 1.f, 0.f}, {0.f, 0.f}}, // 21
        {{-0.5f, 0.5f, -0.5f}, {1.f, 1.f, 0.f}, {0.f, 0.f}},  // 22
        {{0.5f, 0.5f, -0.5f}, {1.f, 1.f, 0.f}, {0.f, 0.f}},   // 23
    };

    builder.indices = {
        // +X
        0,
        1,
        2,
        0,
        2,
        3,
        // -X
        4,
        5,
        6,
        4,
        6,
        7,
        // +Y
        8,
        9,
        10,
        8,
        10,
        11,
        // -Y
        12,
        13,
        14,
        12,
        14,
        15,
        // +Z
        16,
        17,
        18,
        16,
        18,
        19,
        // -Z
        20,
        21,
        22,
        20,
        22,
        23,
    };

    auto &transform = entity.get_component<TransformComponent>();
    transform.scale = {0.55f, 0.55f, 0.55f};
    transform.translation = {0.f, 0.f, 2.5f};

    auto mesh = make_ref<Mesh>(device_, builder);
    entity.add_component<MeshComponent>(mesh);
}

} // namespace vkao