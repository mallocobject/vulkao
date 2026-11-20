#pragma once

#include "core/ref.hpp"
#include "graphics/descriptor.hpp"
#include "graphics/device.hpp"
#include "graphics/renderer.hpp"
#include "platform/window.hpp"
#include "scene/entity.hpp"
#include "scene/registry.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

namespace vkao {
class FirstApp : public RefCounted {
  public:
    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;

    FirstApp();
    ~FirstApp() = default;

    FirstApp(const FirstApp &) = delete;

    void run();

  private:
    void load_entities();

    Window window_{WIDTH, HEIGHT, "vulkao"};
    Device device_{window_};
    Renderer renderer_{window_, device_};

    // note: order of declarations matters
    Ref<DescriptorPool> global_pool_;

    bool running_{true};

    Registry registry_;
    std::unordered_map<uint32_t, Entity> entities_;
};
} // namespace vkao
