#pragma once

#include "graphics/shadow_config.hpp"
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

namespace vkao {
constexpr uint32_t MAX_LIGHTS = 10;

struct CameraUBO {
    alignas(16) glm::vec4 camera_position{0.f};
    alignas(16) glm::mat4 view_proj{1.f};
};

struct Light {
    alignas(16) glm::vec4 position;  // xyz = position,     w = type(0/1/2)
    alignas(16) glm::vec4 direction; // xyz = direction,     w = cos(inner cone)
    alignas(16) glm::vec4
        falloff; // x = const, y = linear, z = quad, w = cos(outer cone)
    alignas(16) glm::vec4 color; // rgb = color,     w = intensity
};

struct LightingUBO {
    alignas(16) glm::vec4 ambient_light_color{
        1.f, 1.f, 1.f, 0.02f}; // w is intensity
    std::array<Light, MAX_LIGHTS> lights{};
    alignas(16) uint32_t light_count{1};
};

struct ShadowUBO {
    alignas(16) glm::mat4 light_view_proj{1.f};
};

struct CascadedShadowUBO {
    alignas(16) std::array<glm::mat4, CASCADE_COUNT> light_view_proj{};
    alignas(16) glm::vec4 split_depths{0.f};
    alignas(16) glm::mat4 camera_view{1.f};
};
} // namespace vkao