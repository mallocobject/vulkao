#pragma once

#include "ref.hpp"
#include "vkao_device.hpp"
#include "vkao_model.hpp"
#include "vkao_renderer.hpp"
#include "vkao_window.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace vkao {
class FirstApp {
  public:
    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;

    FirstApp();
    ~FirstApp();

    FirstApp(const FirstApp &) = delete;

    void run();

  private:
    void load_models();

    VkaoWindow vkao_window_{WIDTH, HEIGHT, "vulkao"};
    VkaoDevice vkao_device_{vkao_window_};
    VkaoRenderer vkao_renderer_{vkao_window_, vkao_device_};

    Ref<VkaoModel> vkao_model_;
    bool running_{true};
};
} // namespace vkao