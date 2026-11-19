#include "camera_controller.hpp"
#include "SDL3/SDL_mouse.h"
#include "SDL3/SDL_scancode.h"
#include "core/math.hpp"
#include "glm/ext/quaternion_geometric.hpp"
#include "glm/ext/vector_float2.hpp"
#include "scene/component.hpp"
#include <limits>

namespace vkao {
void CameraController::move_in_plane_XZ(SDL_Window *window,
                                        float dt,
                                        Entity entity) {

    glm::vec2 movement{0.f};
    const bool *keyboardState = SDL_GetKeyboardState(nullptr);
    if (keyboardState[SDL_SCANCODE_A]) {
        movement.x -= 1;
    }
    if (keyboardState[SDL_SCANCODE_D]) {
        movement.x += 1;
    }
    if (keyboardState[SDL_SCANCODE_W]) {
        movement.y += 1;
    }
    if (keyboardState[SDL_SCANCODE_S]) {
        movement.y -= 1;
    }

    if (glm::dot(movement, movement) > std::numeric_limits<float>::epsilon()) {
        auto &transform = entity.get_component<TransformComponent>();
        const auto &rotation = rotation_matrix_from_euler(transform.rotation);
        movement = glm::normalize(movement);
        transform.translation +=
            move_speed_ * dt *
            (rotation[0] * movement.x + rotation[2] * movement.y);
    }
}

void CameraController::rotate_around_X_and_Y(SDL_Window *window,
                                             float dt,
                                             Entity entity) {
    float dx = 0.f;
    float dy = 0.f;
    SDL_MouseButtonFlags buttons = SDL_GetRelativeMouseState(&dx, &dy);

    if (buttons & SDL_BUTTON_LMASK) {
        auto &transform = entity.get_component<TransformComponent>();

        transform.rotation.x += rotate_speed_ * dt * dy;
        transform.rotation.y -= rotate_speed_ * dt * dx;

        const float limit = glm::radians(89.f);
        transform.rotation.x = std::clamp(transform.rotation.x, -limit, limit);
    }
}
} // namespace vkao