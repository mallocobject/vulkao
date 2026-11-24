#pragma once

#include "graphics/buffer.hpp"
#include "graphics/shadow_config.hpp"
#include "graphics/shadow_map.hpp"
#include <array>
#include <glm/glm.hpp>

namespace vkao {
struct CascadeData {
    glm::mat4 light_view_proj{1.f};
    float split_depth{};
};

struct CascadeResources {
    Ref<ShadowMap> shadow_map;
    Ref<Buffer> matrix_buffer;
    VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
};

using CascadeArray = std::array<CascadeData, CASCADE_COUNT>;
using FrameCascades = std::array<CascadeResources, CASCADE_COUNT>;

CascadeArray calculate_cascades(const glm::mat4 &camera_view,
                                const glm::mat4 &camera_projection,
                                float camera_near,
                                float camera_far,
                                const glm::vec3 &light_direction);
} // namespace vkao