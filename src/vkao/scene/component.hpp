#pragma once

#include "core/math.hpp"
#include "core/ref.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "graphics/mesh.hpp"
#include "graphics/texture.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <glm/glm.hpp>

namespace vkao {
struct IdComponent {
    uint32_t id{0};
};

struct TransformComponent {
    glm::vec3 scale{1.f};
    glm::vec3 rotation{0.f};
    glm::vec3 translation{0.f};

    glm::vec3 world{1.f}; // reserved

    glm::mat4 model_matrix() const {
        return SRT(scale, rotation, translation);
    }

    glm::mat3 normal_matrix() const {
        return SRTNormal(scale, rotation);
    }
};

struct MeshComponent {
    Ref<Mesh> mesh;
};

struct MaterialComponent {
    Ref<Texture> base_color_texture;
    VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
};

struct LightComponent {
    enum class LightType {
        Directional = 0,
        Spot,
        Point,
    } type;

    glm::vec3 color{1.f};
    float intensity{1.f};

    float range;

    struct {
        float constant{1.f};
        float linear{0.f};
        float quadratic{1.f};
    } falloff;

    struct {
        float inner;
        float outer;
    } cone;
};
} // namespace vkao