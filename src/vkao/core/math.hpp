#pragma once

#include "glm/ext/matrix_float3x3.hpp"
#include <glm/ext.hpp>
#include <glm/glm.hpp>

namespace vkao {
inline glm::mat4 SRT(const glm::vec3 &scale,
                     const glm::vec3 &rotation,
                     const glm::vec3 &translation) {
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);
    return glm::mat4{{
                         scale.x * (c1 * c3 + s1 * s2 * s3),
                         scale.x * (c2 * s3),
                         scale.x * (c1 * s2 * s3 - c3 * s1),
                         0.f,
                     },
                     {
                         scale.y * (c3 * s1 * s2 - c1 * s3),
                         scale.y * (c2 * c3),
                         scale.y * (c1 * c3 * s2 + s1 * s3),
                         0.f,
                     },
                     {
                         scale.z * (c2 * s1),
                         scale.z * (-s2),
                         scale.z * (c1 * c2),
                         0.f,
                     },
                     {translation.x, translation.y, translation.z, 1.f}};
}

inline glm::mat3 SRTNormal(const glm::vec3 &scale, const glm::vec3 &rotation) {
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);

    const glm::vec3 inv_scale = 1.f / scale;

    return glm::mat3{{
                         inv_scale.x * (c1 * c3 + s1 * s2 * s3),
                         inv_scale.x * (c2 * s3),
                         inv_scale.x * (c1 * s2 * s3 - c3 * s1),
                     },
                     {
                         inv_scale.y * (c3 * s1 * s2 - c1 * s3),
                         inv_scale.y * (c2 * c3),
                         inv_scale.y * (c1 * c3 * s2 + s1 * s3),
                     },
                     {
                         inv_scale.z * (c2 * s1),
                         inv_scale.z * (-s2),
                         inv_scale.z * (c1 * c2),
                     }};
}

inline glm::mat3 rotation_of(const glm::mat4 &transform) {
    glm::mat3 rotation{transform};
    for (int column = 0; column < 3; column++) {
        const float length = glm::length(rotation[column]);
        rotation[column] /= length > 0.f ? length : 1.f;
    }
    return rotation;
}

inline glm::mat4 rotation_matrix_from_euler(const glm::vec3 &rotation) {
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);
    return glm::mat4{{
                         (c1 * c3 + s1 * s2 * s3),
                         (c2 * s3),
                         (c1 * s2 * s3 - c3 * s1),
                         0.f,
                     },
                     {
                         (c3 * s1 * s2 - c1 * s3),
                         (c2 * c3),
                         (c1 * c3 * s2 + s1 * s3),
                         0.f,
                     },
                     {
                         (c2 * s1),
                         (-s2),
                         (c1 * c2),
                         0.f,
                     },
                     {0.f, 0.f, 0.f, 1.f}};
}
} // namespace vkao