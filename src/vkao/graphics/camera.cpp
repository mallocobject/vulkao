#include "graphics/camera.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

namespace vkao {
void Camera::set_orthographics_projection(
    float left, float right, float top, float bottom, float near, float far) {
    proj_ = glm::orthoRH_ZO(left, right, bottom, top, near, far);
}

void Camera::set_perspective_projection(float fovy,
                                        float aspect,
                                        float near,
                                        float far) {
    proj_ = glm::perspectiveLH_ZO(fovy, aspect, near, far);
}

void Camera::set_view_direction(const glm::vec3 &position,
                                const glm::vec3 &direction,
                                const glm::vec3 &up) {
    const glm::vec3 w{glm::normalize(direction)};
    const glm::vec3 u{glm::normalize(glm::cross(w, up))};
    const glm::vec3 v{glm::cross(w, u)};

    view_[0][0] = u.x;
    view_[1][0] = u.y;
    view_[2][0] = u.z;
    view_[0][1] = v.x;
    view_[1][1] = v.y;
    view_[2][1] = v.z;
    view_[0][2] = w.x;
    view_[1][2] = w.y;
    view_[2][2] = w.z;
    view_[3][0] = -glm::dot(u, position);
    view_[3][1] = -glm::dot(v, position);
    view_[3][2] = -glm::dot(w, position);

    inv_view_[0][0] = u.x;
    inv_view_[0][1] = u.y;
    inv_view_[0][2] = u.z;
    inv_view_[1][0] = v.x;
    inv_view_[1][1] = v.y;
    inv_view_[1][2] = v.z;
    inv_view_[2][0] = w.x;
    inv_view_[2][1] = w.y;
    inv_view_[2][2] = w.z;
    inv_view_[3][0] = position.x;
    inv_view_[3][1] = position.y;
    inv_view_[3][2] = position.z;
}

void Camera::set_view_target(const glm::vec3 &position,
                             const glm::vec3 &target,
                             const glm::vec3 &up) {
    set_view_direction(position, target - position, up);
}

void Camera::set_view_YXZ(const glm::vec3 &position,
                          const glm::vec3 &rotation) {
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);
    const glm::vec3 u{
        (c1 * c3 + s1 * s2 * s3), (c2 * s3), (c1 * s2 * s3 - c3 * s1)};
    const glm::vec3 v{
        (c3 * s1 * s2 - c1 * s3), (c2 * c3), (c1 * c3 * s2 + s1 * s3)};
    const glm::vec3 w{(c2 * s1), (-s2), (c1 * c2)};

    view_[0][0] = u.x;
    view_[1][0] = u.y;
    view_[2][0] = u.z;
    view_[0][1] = v.x;
    view_[1][1] = v.y;
    view_[2][1] = v.z;
    view_[0][2] = w.x;
    view_[1][2] = w.y;
    view_[2][2] = w.z;
    view_[3][0] = -glm::dot(u, position);
    view_[3][1] = -glm::dot(v, position);
    view_[3][2] = -glm::dot(w, position);

    inv_view_[0][0] = u.x;
    inv_view_[0][1] = u.y;
    inv_view_[0][2] = u.z;
    inv_view_[1][0] = v.x;
    inv_view_[1][1] = v.y;
    inv_view_[1][2] = v.z;
    inv_view_[2][0] = w.x;
    inv_view_[2][1] = w.y;
    inv_view_[2][2] = w.z;
    inv_view_[3][0] = position.x;
    inv_view_[3][1] = position.y;
    inv_view_[3][2] = position.z;
}
} // namespace vkao