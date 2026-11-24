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
#include "graphics/cascaded_shadow.hpp"
#include "graphics/descriptor.hpp"
#include "graphics/frame_info.hpp"
#include "graphics/mesh.hpp"
#include "graphics/shadow_config.hpp"
#include "graphics/shadow_map.hpp"
#include "graphics/swap_chain.hpp"
#include "graphics/texture.hpp"
#include "graphics/uniform_data.hpp"
#include "pass/forward_pass.hpp"
#include "pass/shadow_pass.hpp"
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
namespace {
struct ShadowFrameResources {
    Ref<ShadowMap> atlas;
    Ref<Buffer> sampling_buffer;
    VkDescriptorSet draw_set{VK_NULL_HANDLE};
    VkDescriptorSet sampling_set{VK_NULL_HANDLE};
};

struct FrameResources {
    Ref<Buffer> camera_buffer;
    Ref<Buffer> lighting_buffer;
    VkDescriptorSet global_set{VK_NULL_HANDLE};
    ShadowFrameResources shadows;
    inline static VkDescriptorSet material_set{VK_NULL_HANDLE};
};
} // namespace

FirstApp::FirstApp() {
    constexpr uint32_t F = SwapChain::MAX_FRAMES_IN_FLIGHT;
    constexpr uint32_t C = CASCADE_COUNT;
    constexpr uint32_t M = Texture::MAX_MATERIALS;

    global_pool_ =
        DescriptorPool::Builder(device_)
            .set_max_sets(F * (C + 2) + M)
            .add_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, F * (C + 3))
            .add_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, F * C + M)
            .build();

    load_entities();
}

void FirstApp::run() {
    auto global_set_layout = DescriptorSetLayout::Builder(device_)
                                 .add_binding(0,
                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                              VK_SHADER_STAGE_VERTEX_BIT |
                                                  VK_SHADER_STAGE_FRAGMENT_BIT)
                                 .add_binding(1,
                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                              VK_SHADER_STAGE_FRAGMENT_BIT)
                                 .build();

    auto shadow_set_layout = DescriptorSetLayout::Builder(device_)
                                 .add_binding(0,
                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                              VK_SHADER_STAGE_VERTEX_BIT)
                                 .build();

    auto cascaded_set_layout =
        DescriptorSetLayout::Builder(device_)
            .add_binding(0,
                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_binding(1,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();

    std::array<FrameResources, SwapChain::MAX_FRAMES_IN_FLIGHT> frames{};

    for (auto &frame : frames) {
        {
            frame.camera_buffer =
                make_ref<Buffer>(device_,
                                 sizeof(CameraUBO),
                                 1,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            frame.lighting_buffer =
                make_ref<Buffer>(device_,
                                 sizeof(LightingUBO),
                                 1,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

            VKAO_ASSERT(frame.camera_buffer->map() == VK_SUCCESS,
                        "failed to map camera buffer");
            VKAO_ASSERT(frame.lighting_buffer->map() == VK_SUCCESS,
                        "failed to map lighting buffer");

            auto camera_info = frame.camera_buffer->descriptor_info();
            auto lighting_info = frame.lighting_buffer->descriptor_info();
            VKAO_ASSERT(DescriptorWriter(*global_set_layout, *global_pool_)
                            .write_buffer(0, &camera_info)
                            .write_buffer(1, &lighting_info)
                            .build(frame.global_set),
                        "failed to allocate global descriptor set");
        }

        {
            frame.shadows.atlas =
                make_ref<ShadowMap>(device_, VkExtent2D{DIRECTIONAL_ATLAS_SIZE,
                                                        DIRECTIONAL_ATLAS_SIZE});

            frame.shadows.sampling_buffer =
                make_ref<Buffer>(device_,
                                 sizeof(ShadowDrawUBO),
                                 1,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

            VKAO_ASSERT(frame.shadows.sampling_buffer->map() == VK_SUCCESS,
                        "failed to map shadow buffer");

            auto buffer_info = frame.shadows.sampling_buffer->descriptor_info();

            VKAO_ASSERT(DescriptorWriter(*shadow_set_layout, *global_pool_)
                            .write_buffer(0, &buffer_info)
                            .build(frame.shadows.draw_set),
                        "failed to allocate shadow descriptor set");
            auto image_info = frame.shadows.atlas->descriptor_info();

            VKAO_ASSERT(DescriptorWriter(*cascaded_set_layout, *global_pool_)
                            .write_buffer(0, &buffer_info)
                            .write_image(1, &image_info)
                            .build(frame.shadows.sampling_set),
                        "failed to allocate cascaded shadow descriptor set");
        }
    }

    constexpr uint32_t material_texture_count = 32;

    auto material_set_layout =
        DescriptorSetLayout::Builder(device_)
            .add_binding(0,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT,
                         material_texture_count)
            .build();

    std::array<VkDescriptorImageInfo, material_texture_count> image_infos{};

    for (auto [handle, material] : registry_.view<MaterialComponent>().each()) {
        VKAO_ASSERT(material.texture_slot < image_infos.size(),
                    "material texture slot out of range");

        image_infos[material.texture_slot] =
            material.base_color_texture->descriptor_info();
    }

    for (auto &info : image_infos) {
        if (info.imageView == VK_NULL_HANDLE) {
            info = white_texture_->descriptor_info();
        }
    }

    VKAO_ASSERT(DescriptorWriter(*material_set_layout, *global_pool_)
                    .write_image(0, image_infos.data(), material_texture_count)
                    .build(FrameResources::material_set),
                "failed to allocate shared material descriptor set");

    ShadowPass shadow_pass{
        device_,
        frames.front().shadows.atlas->get_render_pass(),
        {shadow_set_layout->get_descriptor_set_layout()},
    };

    ForwardPass forward_pass{
        device_,
        renderer_.get_swap_chain_render_pass(),
        {
            global_set_layout->get_descriptor_set_layout(),   // set 0
            material_set_layout->get_descriptor_set_layout(), // set 1
            cascaded_set_layout->get_descriptor_set_layout(), // set 2
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

        if (!running_) {
            break;
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
            auto &frame = frames[index];

            FrameInfo frame_info{index,
                                 dt,
                                 command_buffer,
                                 camera,
                                 registry_,
                                 {
                                     frame.global_set,
                                     frame.material_set,
                                     frame.shadows.sampling_set,
                                 }};

            glm::vec3 directional_light_dir{1.f, 1.f, -1.f};
            for (auto [handle, tf, light] :
                 registry_.view<TransformComponent, LightComponent>().each()) {
                if (light.type == LightComponent::LightType::Directional) {
                    directional_light_dir =
                        glm::vec3{rotation_matrix_from_euler(tf.rotation) *
                                  glm::vec4{0.f, 1.f, 0.f, 0.f}};
                    break;
                }
            }

            const auto cascades = calculate_cascades(camera.get_view(),
                                                     camera.get_projection(),
                                                     0.1f,
                                                     1000.f,
                                                     directional_light_dir);

            ShadowDrawUBO shadow_draw_ubo{};
            shadow_draw_ubo.camera_view = camera.get_view();

            for (uint32_t i = 0; i < CASCADE_COUNT; ++i) {
                shadow_draw_ubo.light_view_proj[i] =
                    cascades[i].light_view_proj;
                shadow_draw_ubo.split_depths[i] = cascades[i].split_depth;
            }

            auto &shadows = frame.shadows;

            shadows.sampling_buffer->write_to_buffer(&shadow_draw_ubo);

            VKAO_ASSERT(shadows.sampling_buffer->flush() == VK_SUCCESS,
                        "failed to flush shadow buffer");

            ShadowDraw draw{
                ShadowProjection::DirectionalAtlas,
                shadows.atlas->get_render_target(),
                frame.shadows.draw_set,
            };
            shadow_pass.render(frame_info, draw);

            // update
            CameraUBO camera_ubo{};
            camera_ubo.camera_position =
                glm::vec4{camera_controller.get_position(), 1.f};
            camera_ubo.view_proj = camera.get_projection() * camera.get_view();
            frame.camera_buffer->write_to_buffer(&camera_ubo);
            VKAO_ASSERT(frame.camera_buffer->flush() == VK_SUCCESS,
                        "failed to flush camera buffer");

            frame.lighting_buffer->write_to_buffer(&lighting_ubo);
            VKAO_ASSERT(frame.lighting_buffer->flush() == VK_SUCCESS,
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
    white_texture_ = make_ref<Texture>(
        device_, 1, 1, reinterpret_cast<const uint8_t *>(&while_pixels));

    {
        auto entity = registry_.create_entity();
        entities_[entity.get_component<IdComponent>().id] = entity;

        auto &tf = entity.get_component<TransformComponent>();
        tf.translation = {0.f, 0.f, 0.f};

        auto &light = entity.add_component<LightComponent>();
        light.type = LightComponent::LightType::Point;
        light.color = glm::vec4{0.f};
    }

    {
        auto entity = registry_.create_entity();
        entities_[entity.get_component<IdComponent>().id] = entity;

        auto &tf = entity.get_component<TransformComponent>();
        tf.rotation = {glm::radians(-45.f), glm::radians(-45.f), 0.f};

        auto &light = entity.add_component<LightComponent>();
        light.type = LightComponent::LightType::Directional;
        light.color = glm::vec4{1.f, 1.f, 1.f, 1.f};
    }

    {
        auto entity = registry_.create_entity();
        entities_[entity.get_component<IdComponent>().id] = entity;

        auto &tf = entity.get_component<TransformComponent>();
        tf.rotation = {1.f, 1.f, -1.f};
        tf.translation = {-1.f, 0.f, 1.f};

        auto &light = entity.add_component<LightComponent>();
        light.type = LightComponent::LightType::Spot;
        light.color = glm::vec4{1.f, 0.f, 0.f, 1.f};
        light.cone.inner = glm::radians(12.5f);
        light.cone.outer = glm::radians(17.5f);
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
        material.texture_slot = 1;
    }

    {
        auto entity = registry_.create_entity();
        entities_[entity.get_component<IdComponent>().id] = entity;

        auto &tf = entity.get_component<TransformComponent>();
        tf.translation = {0.f, 0.5f, 0.5f};

        auto mesh = Mesh::create(device_, ASSETS "/model/flat_vase.obj");
        entity.add_component<MeshComponent>(mesh);

        auto &material = entity.add_component<MaterialComponent>();
        material.base_color_texture = white_texture_;
        material.texture_slot = 0;
    }

    {
        auto entity = registry_.create_entity();
        entities_[entity.get_component<IdComponent>().id] = entity;

        auto &tf = entity.get_component<TransformComponent>();
        tf.translation = {-0.5f, 0.5f, 0.5f};

        auto mesh = Mesh::create(device_, ASSETS "/model/smooth_vase.obj");
        entity.add_component<MeshComponent>(mesh);

        auto &material = entity.add_component<MaterialComponent>();
        material.base_color_texture = white_texture_;
        material.texture_slot = 0;
    }
}

} // namespace vkao
