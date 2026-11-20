#pragma once

#include "graphics/camera.hpp"
#include <SDL3/SDL_events.h>
#include <glm/glm.hpp>

namespace vkao {
class CameraController {
  public:
    void set_position_target(const glm::vec3 &position,
                             const glm::vec3 &target);
    void handle_event(const SDL_Event &event);
    void update(Camera &camera,
                float fovy,
                float aspect_ratio,
                float viewport_height);

    const glm::vec3 &get_position() const {
        return position_;
    }

    const glm::vec3 &get_target() const {
        return target_;
    }

  private:
    glm::vec3 position_{0.f, 0.f, -1.f};
    glm::vec3 target_{0.f, 0.5f, 0.5f};
    glm::vec2 orbit_delta_{0.f};
    glm::vec2 pan_delta_{0.f};
    float dolly_delta_{0.f};

    float orbit_speed_{1.f};
    float pan_speed_{1.f};
    float zoom_speed_{1.f};
    float min_distance_{0.1f};
    float max_distance_{100.f};
};
} // namespace vkao
