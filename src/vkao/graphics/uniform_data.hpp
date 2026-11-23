#pragma once

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

static_assert(offsetof(CameraUBO, view_proj) == 16);
static_assert(sizeof(CameraUBO) == 80);
static_assert(offsetof(Light, direction) == 16);
static_assert(offsetof(Light, falloff) == 32);
static_assert(offsetof(Light, color) == 48);
static_assert(sizeof(Light) == 64);
static_assert(offsetof(LightingUBO, lights) == 16);
static_assert(offsetof(LightingUBO, light_count) == 656);
static_assert(sizeof(LightingUBO) == 672);
} // namespace vkao