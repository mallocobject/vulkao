#include "app/first_app.hpp"

#include "core/logger.hpp"
#include "core/ref.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/trigonometric.hpp"
#include "graphics/buffer.hpp"
#include "graphics/camera.hpp"
#include "graphics/camera_controller.hpp"
#include "graphics/descriptor.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/mesh.hpp"
#include "graphics/swap_chain.hpp"
#include "graphics/texture.hpp"
#include "graphics/uniform_data.hpp"
#include "pass/forward_pass.hpp"
#include "scene/component.hpp"
#include "scene/entity.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {

FirstApp::FirstApp() {
    global_pool_ = DescriptorPool::Builder(device_)
                       .set_max_sets(SwapChain::MAX_FRAMES_IN_FLIGHT +
                                     Texture::MAX_MATERIALS)
                       .add_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      2 * SwapChain::MAX_FRAMES_IN_FLIGHT)
                       .add_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      Texture::MAX_MATERIALS)
                       .build();

    load_entities();
}

void FirstApp::run() {
    std::vector<Ref<Buffer>> camera_buffers(SwapChain::MAX_FRAMES_IN_FLIGHT);
    std::vector<Ref<Buffer>> lighting_buffers(SwapChain::MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < camera_buffers.size(); i++) {
        camera_buffers[i] =
            make_ref<Buffer>(device_,
                             sizeof(CameraUBO),
                             1,
                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        lighting_buffers[i] =
            make_ref<Buffer>(device_,
                             sizeof(LightingUBO),
                             1,
                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        VKAO_ASSERT(camera_buffers[i]->map() == VK_SUCCESS,
                    "failed to map camera buffer");
        VKAO_ASSERT(lighting_buffers[i]->map() == VK_SUCCESS,
                    "failed to map lighting buffer");
    }

    auto global_set_layout = DescriptorSetLayout::Builder(device_)
                                 .add_binding(0,
                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                              VK_SHADER_STAGE_VERTEX_BIT |
                                                  VK_SHADER_STAGE_FRAGMENT_BIT)
                                 .add_binding(1,
                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                              VK_SHADER_STAGE_FRAGMENT_BIT)
                                 .build();

    std::vector<VkDescriptorSet> global_descriptor_sets(
        SwapChain::MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < global_descriptor_sets.size(); i++) {
        auto camera_info = camera_buffers[i]->descriptor_info();
        auto lighting_info = lighting_buffers[i]->descriptor_info();
        VKAO_ASSERT(DescriptorWriter(*global_set_layout, *global_pool_)
                        .write_buffer(0, &camera_info)
                        .write_buffer(1, &lighting_info)
                        .build(global_descriptor_sets[i]),
                    "failed to allocate global descriptor set");
    }

    auto material_set_layout =
        DescriptorSetLayout::Builder(device_)
            .add_binding(0,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();

    for (auto [handle, material] : registry_.view<MaterialComponent>().each()) {
        auto image_info = material.base_color_texture->descriptor_info();

        VKAO_ASSERT(DescriptorWriter(*material_set_layout, *global_pool_)
                        .write_image(0, &image_info)
                        .build(material.descriptor_set),
                    "failed to allocate material descriptor set");
    }

    ForwardPass forward_pass{
        device_,
        renderer_.get_swap_chain_render_pass(),
        {
            global_set_layout->get_descriptor_set_layout(),
            material_set_layout->get_descriptor_set_layout(),
        }};

    Camera camera;

    CameraController camera_controller;
    camera_controller.set_position_target(
        {0.f, 0.f, -1.f},
        entities_.begin()
            ->second.get_component<TransformComponent>()
            .translation);
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
        camera.set_perspective_projection(fovy, aspect_ratio, 0.1f, 1000.f);

        // lights
        LightingUBO lighting_ubo{};
        uint32_t light_cnt = 0;
        for (auto [handle, tf, light] :
             registry_.view<TransformComponent, LightComponent>().each()) {
            lighting_ubo.lights[light_cnt++] =
                Light{glm::vec4{tf.translation, light.type},
                      glm::vec4{rotation_matrix_from_euler(tf.rotation) *
                                    glm::vec4{0.f, 1.f, 0.f, 0.f},
                                std::cos(light.cone.inner)},
                      glm::vec4{light.falloff.constant,
                                light.falloff.linear,
                                light.falloff.quadratic,
                                std::cos(light.cone.outer)},
                      glm::vec4{light.color, light.intensity}};

            if (light_cnt >= MAX_LIGHTS) {
                break;
            }
        }
        lighting_ubo.light_count = std::min(light_cnt, MAX_LIGHTS);

        if (auto command_buffer = renderer_.begin_frame()) {
            int index = renderer_.get_frame_index();
            FrameInfo frame_info{
                index,
                dt,
                command_buffer,
                camera,
                global_descriptor_sets[index],
                registry_,
            };

            // update
            CameraUBO camera_ubo{};
            camera_ubo.camera_position =
                glm::vec4{camera_controller.get_position(), 1.f};
            camera_ubo.view_proj = camera.get_projection() * camera.get_view();
            camera_buffers[index]->write_to_buffer(&camera_ubo);
            VKAO_ASSERT(camera_buffers[index]->flush() == VK_SUCCESS,
                        "failed to flush camera buffer");

            lighting_buffers[index]->write_to_buffer(&lighting_ubo);
            VKAO_ASSERT(lighting_buffers[index]->flush() == VK_SUCCESS,
                        "failed to flush lighting buffer");

            // render
            const auto target = renderer_.get_current_render_target();
            forward_pass.render(frame_info, target);

            renderer_.end_frame();
        }
    }

    vkDeviceWaitIdle(device_.get_device());
}

void FirstApp::load_entities() {
    const uint32_t while_pixels = ~0u;
    auto white_texture = make_ref<Texture>(
        device_, 1, 1, reinterpret_cast<const uint8_t *>(&while_pixels));

    {
        auto entity = registry_.create_entity();
        entities_[entity.get_component<IdComponent>().id] = entity;

        auto &tf = entity.get_component<TransformComponent>();
        tf.scale = {0.1f, 0.1f, 0.1f};
        tf.translation = {0.f, 0.f, 0.f};

        auto &light = entity.add_component<LightComponent>();
        light.type = LightComponent::LightType::Point;
    }

    {
        auto entity = registry_.create_entity();
        entities_[entity.get_component<IdComponent>().id] = entity;

        auto &tf = entity.get_component<TransformComponent>();
        tf.translation = {0.f, 0.5f, 0.f};

        auto mesh = Mesh::create(device_, ASSETS "/model/quad.obj");
        entity.add_component<MeshComponent>(mesh);

        auto texture =
            make_ref<Texture>(device_, ASSETS "/texture/Checkerboard.png");
        auto &material = entity.add_component<MaterialComponent>();
        material.base_color_texture = texture;
    }

    {
        auto entity = registry_.create_entity();
        entities_[entity.get_component<IdComponent>().id] = entity;

        auto &tf = entity.get_component<TransformComponent>();
        tf.translation = {0.f, 0.5f, 0.5f};

        auto mesh = Mesh::create(device_, ASSETS "/model/flat_vase.obj");
        entity.add_component<MeshComponent>(mesh);

        auto &material = entity.add_component<MaterialComponent>();
        material.base_color_texture = white_texture;
    }

    {
        auto entity = registry_.create_entity();
        entities_[entity.get_component<IdComponent>().id] = entity;

        auto &tf = entity.get_component<TransformComponent>();
        tf.translation = {-0.5f, 0.5f, 0.5f};

        auto mesh = Mesh::create(device_, ASSETS "/model/smooth_vase.obj");
        entity.add_component<MeshComponent>(mesh);

        auto &material = entity.add_component<MaterialComponent>();
        material.base_color_texture = white_texture;
    }
}

} // namespace vkao
