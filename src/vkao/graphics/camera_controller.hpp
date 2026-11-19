#pragma once

#include "SDL3/SDL_video.h"
#include "glm/ext/vector_float2.hpp"
#include "scene/entity.hpp"
#include <SDL3/SDL_keycode.h>
namespace vkao {
class CameraController {
  public:
    void move_in_plane_XZ(SDL_Window *window, float dt, Entity entity);
    void rotate_around_X_and_Y(SDL_Window *window, float dt, Entity entity);

  private:
    float move_speed_{2.f};
    float rotate_speed_{2.f};
};
} // namespace vkao