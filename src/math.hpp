#pragma once

#include <cmath>
#include <glm/ext.hpp>
#include <glm/glm.hpp>

namespace ivx {
inline glm::mat4 rotation_matrix_from_euler(const glm::vec3 &rotation) {
    float yaw = rotation.y;
    float pitch = rotation.x;
    float roll = rotation.z;

    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const float cr = std::cos(roll), sr = std::sin(roll);

    glm::mat4 R(1.0f);

    // GLM col-main: R[col][row]
    R[0][0] = cy * cr + sy * sp * sr;
    R[0][1] = cp * sr;
    R[0][2] = -sy * cr + cy * sp * sr;

    R[1][0] = -cy * sr + sy * sp * cr;
    R[1][1] = cp * cr;
    R[1][2] = sy * sr + cy * sp * cr;

    R[2][0] = sy * cp;
    R[2][1] = -sp;
    R[2][2] = cy * cp;

    return R;
}
} // namespace ivx