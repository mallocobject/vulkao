#include "graphics/camera_controller.hpp"

#include <SDL3/SDL_mouse.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace vkao {
void CameraController::set_position_target(const glm::vec3 &position,
                                           const glm::vec3 &target) {
    const float distance = glm::length(position - target);
    assert(distance > 0.f && "Camera position must differ from its target");
    if (!(distance > 0.f) || !std::isfinite(distance)) {
        return;
    }

    target_ = target;
    position_ = target + (position - target) *
                            (std::clamp(distance, min_distance_, max_distance_) /
                             distance);
    orbit_delta_ = glm::vec2{0.f};
    pan_delta_ = glm::vec2{0.f};
    dolly_delta_ = 0.f;
}

void CameraController::handle_event(const SDL_Event &event) {
    switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION: {
        const glm::vec2 delta{event.motion.xrel, event.motion.yrel};
        if (event.motion.state & SDL_BUTTON_LMASK) {
            orbit_delta_ += delta;
        } else if (event.motion.state & SDL_BUTTON_RMASK) {
            pan_delta_ += delta;
        }
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
        dolly_delta_ += event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                            ? -event.wheel.y
                            : event.wheel.y;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        orbit_delta_ = glm::vec2{0.f};
        pan_delta_ = glm::vec2{0.f};
        dolly_delta_ = 0.f;
        break;
    }
}

void CameraController::update(Camera &camera,
                              float fovy,
                              float aspect_ratio,
                              float viewport_height) {
    if (!(viewport_height > 0.f && aspect_ratio > 0.f &&
          fovy > 0.f && fovy < glm::pi<float>())) {
        orbit_delta_ = glm::vec2{0.f};
        pan_delta_ = glm::vec2{0.f};
        dolly_delta_ = 0.f;
        return;
    }

    constexpr float pole_margin = 0.001f;
    const glm::vec3 world_up{0.f, -1.f, 0.f};
    const glm::vec3 from_target = position_ - target_;
    const float distance = glm::length(from_target);
    const float horizontal_distance =
        glm::length(glm::vec2{from_target.x, from_target.z});
    if (orbit_delta_ == glm::vec2{0.f} && pan_delta_ == glm::vec2{0.f} &&
        dolly_delta_ == 0.f && horizontal_distance > 0.f) {
        camera.set_view_target(position_, target_, world_up);
        return;
    }
    float theta = std::atan2(from_target.x, from_target.z);
    float phi = std::atan2(horizontal_distance, -from_target.y);

    theta += orbit_speed_ * glm::two_pi<float>() * orbit_delta_.x /
             (viewport_height * aspect_ratio);
    phi -= orbit_speed_ * glm::two_pi<float>() * orbit_delta_.y /
           viewport_height;
    phi = std::clamp(phi, pole_margin, glm::pi<float>() - pole_margin);

    // The polar axis is -Y, matching the camera's world-up direction.
    const glm::vec3 direction{
        std::sin(phi) * std::sin(theta),
        -std::cos(phi),
        std::sin(phi) * std::cos(theta)};
    const glm::vec3 forward = -direction;
    const glm::vec3 right = glm::normalize(glm::cross(forward, world_up));
    const glm::vec3 up = glm::cross(right, forward);
    const float world_per_pixel =
        2.f * distance * std::tan(fovy / 2.f) / viewport_height;

    target_ += pan_speed_ * world_per_pixel *
               (-right * pan_delta_.x + up * pan_delta_.y);

    const float radius = std::clamp(
        distance * std::pow(0.95f, dolly_delta_ * zoom_speed_),
        min_distance_,
        max_distance_);
    position_ = target_ + radius * direction;
    camera.set_view_target(position_, target_, world_up);

    orbit_delta_ = glm::vec2{0.f};
    pan_delta_ = glm::vec2{0.f};
    dolly_delta_ = 0.f;
}
} // namespace vkao
