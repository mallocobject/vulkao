#include "vkao_camera.hpp"
#include "glm/ext/matrix_clip_space.hpp"

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
} // namespace vkao