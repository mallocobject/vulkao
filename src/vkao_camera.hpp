#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "ref.hpp"
namespace vkao {
class Camera : public RefCounted {
  public:
    void set_orthographics_projection(float left,
                                      float right,
                                      float top,
                                      float bottom,
                                      float near,
                                      float far);

    void
    set_perspective_projection(float fovy, float aspect, float near, float far);

    const glm::mat4 &get_projection() const {
        return proj_;
    }

  private:
    glm::mat4 proj_{1.f};
};
} // namespace vkao