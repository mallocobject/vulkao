#pragma once

#include "core/ref.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
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

    void set_view_direction(const glm::vec3 &position,
                            const glm::vec3 &direction,
                            const glm::vec3 &up = {0.f, -1.f, 0.f});

    void set_view_target(const glm::vec3 &position,
                         const glm::vec3 &target,
                         const glm::vec3 &up = {0.f, -1.f, 0.f});

    void set_view_YXZ(const glm::vec3 &postion, const glm::vec3 &rotation);

    const glm::mat4 &get_projection() const {
        return proj_;
    }

    const glm::mat4 &get_view() const {
        return view_;
    }

  private:
    glm::mat4 proj_{1.f};
    glm::mat4 view_{1.f};
    glm::mat4 inv_view_{1.f};
};
} // namespace vkao