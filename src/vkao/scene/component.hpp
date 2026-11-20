#pragma once

#include "core/math.hpp"
#include "core/ref.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "graphics/mesh.hpp"
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
} // namespace vkao