#include "app/first_app.hpp"

#include "core/logger.hpp"
#include "core/ref.hpp"
#include "glm/ext/matrix_float4x4.hpp"
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
#include "pass/forward_pass.hpp"
#include "scene/component.hpp"
#include "scene/entity.hpp"
#include <chrono>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {
struct GlobalUBO {
    alignas(16) glm::vec4 camera_position{0.f};
    alignas(16) glm::mat4 view_proj{1.f};
    alignas(16) glm::vec4 ambient_light_color{
        1.f, 1.f, 1.f, 0.02f}; // w is intensity
    alignas(16) glm::vec4 light_position{1.f, -2.f, -1.f, 1.f};
    alignas(16) glm::vec4 light_color{1.f, 1.f, 1.f, 1.f};
    alignas(16) glm::vec4 falloff{0.f, 0.f, 1.f, 0.f};
    // x constant, y linear, z quadratic, w reserved
};

FirstApp::FirstApp() {
    global_pool_ = DescriptorPool::Builder(device_)
                       .set_max_sets(SwapChain::MAX_FRAMES_IN_FLIGHT +
                                     Texture::MAX_NUM_MATERIALS)
                       .add_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      SwapChain::MAX_FRAMES_IN_FLIGHT)
                       .add_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      Texture::MAX_NUM_MATERIALS)
                       .build();

    load_entities();
}

void FirstApp::run() {
    std::vector<Ref<Buffer>> ubo_buffers(SwapChain::MAX_FRAMES_IN_FLIGHT);

    for (auto &ubo_buffer : ubo_buffers) {
        ubo_buffer = make_ref<Buffer>(device_,
                                      sizeof(GlobalUBO),
                                      1,
                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        ubo_buffer->map();
    }

    auto global_set_layout = DescriptorSetLayout::Builder(device_)
                                 .add_binding(0,
                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                              VK_SHADER_STAGE_ALL_GRAPHICS)
                                 .build();

    std::vector<VkDescriptorSet> global_descriptor_sets(
        SwapChain::MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < global_descriptor_sets.size(); i++) {
        auto buffer_info = ubo_buffers[i]->descriptor_info();
        VKAO_ASSERT(DescriptorWriter(*global_set_layout, *global_pool_)
                        .write_buffer(0, &buffer_info)
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
            GlobalUBO ubo{};
            ubo.camera_position =
                glm::vec4{camera_controller.get_position(), 1.f};
            ubo.view_proj = camera.get_projection() * camera.get_view();
            ubo_buffers[index]->write_to_buffer(
                reinterpret_cast<const void *>(&ubo));
            ubo_buffers[index]->flush();

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
        tf.translation = {0.f, 0.5f, 0.5f};

        auto mesh = Mesh::create(device_, ASSETS "/model/flat_vase.obj");
        entity.add_component<MeshComponent>(mesh);
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
        tf.translation = {-0.5f, 0.5f, 0.5f};

        auto mesh = Mesh::create(device_, ASSETS "/model/smooth_vase.obj");
        entity.add_component<MeshComponent>(mesh);

        entity.add_component<MaterialComponent>();
    }

    for (auto [handle, _] : registry_.view<MeshComponent>()->each()) {
        Entity entity{handle, &registry_};
        if (!entity.has_component<MaterialComponent>()) {
            auto &material = entity.add_component<MaterialComponent>();
            material.base_color_texture = white_texture;
        } else if (auto &base_color_texture =
                       entity.get_component<MaterialComponent>()
                           .base_color_texture;
                   !base_color_texture) {
            base_color_texture = white_texture;
        }
    }
}

} // namespace vkao
