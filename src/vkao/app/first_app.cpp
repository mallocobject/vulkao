#include "app/first_app.hpp"

#include "core/logger.hpp"
#include "core/ref.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/trigonometric.hpp"
#include "graphics/buffer.hpp"
#include "graphics/camera.hpp"
#include "graphics/camera_controller.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/mesh.hpp"
#include "graphics/swap_chain.hpp"
#include "scene/component.hpp"
#include "system/simple_render_system.hpp"
#include <chrono>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {
struct GlobUBO {
    glm::mat4 projection_view{1.f};
    glm::vec3 light_direction{glm::normalize(glm::vec3{1.f, -3.f, -1.f})};
};

FirstApp::FirstApp() {
    load_entities();
}

void FirstApp::run() {
    std::vector<Ref<Buffer>> ubo_buffers(SwapChain::MAX_FRAMES_IN_FLIGHT);

    for (auto &ubo_buffer : ubo_buffers) {
        ubo_buffer = make_ref<Buffer>(device_,
                                      sizeof(GlobUBO),
                                      1,
                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        ubo_buffer->map();
    }

    SimpleRenderSystem render_system{device_,
                                     renderer_.get_swap_chain_render_pass()};

    Camera camera;

    CameraController camera_controller;
    camera_controller.set_position_target(
        {0.f, 0.f, -1.f},
        entities_.begin()->second.get_component<TransformComponent>().translation);
    const float fovy = glm::radians(45.f);

    while (running_) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            camera_controller.handle_event(event);
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
        auto aspect_ratio = renderer_.get_aspect_ratio();
        int window_height = 0;
        SDL_GetWindowSize(window_.get_window(), nullptr, &window_height);
        camera_controller.update(
            camera, fovy, aspect_ratio, static_cast<float>(window_height));
        camera.set_perspective_projection(
            fovy, aspect_ratio, 0.1f, 1000.f);

        if (auto command_buffer = renderer_.begin_frame()) {
            int index = renderer_.get_frame_index();
            FrameInfo frame_info{
                index,
                dt,
                command_buffer,
                camera,
            };

            // update
            GlobUBO ubo{};
            ubo.projection_view = camera.get_projection() * camera.get_view();
            ubo_buffers[index]->write_to_buffer(
                reinterpret_cast<const void *>(&ubo));
            ubo_buffers[index]->flush();

            // render
            renderer_.begin_swap_chain_render_pass(command_buffer);
            render_system.draw(frame_info, entities_.begin()->second);
            renderer_.end_swap_chain_render_pass(command_buffer);
            renderer_.end_frame();
        }
    }

    vkDeviceWaitIdle(device_.get_device());
}

void FirstApp::load_entities() {
    auto entity = registry_.create_entity();
    entities_[entity.get_component<IdComponent>().id] = entity;

    auto &tf = entity.get_component<TransformComponent>();
    tf.translation = {0.f, 0.5f, 0.5f};

    auto mesh = Mesh::create(device_, ASSETS "/model/flat_vase.obj");
    entity.add_component<MeshComponent>(mesh);
}

} // namespace vkao
