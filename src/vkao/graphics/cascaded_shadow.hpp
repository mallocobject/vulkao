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

using CascadeArray = std::array<CascadeData, CASCADE_COUNT>;

CascadeArray calculate_cascades(const glm::mat4 &camera_view,
                                const glm::mat4 &camera_projection,
                                float camera_near,
                                float camera_far,
                                const glm::vec3 &light_direction);
} // namespace vkao