#pragma once

#include "ref.hpp"
#include "vkao_device.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

namespace vkao {
struct Vertex {
    glm::vec3 pos{0.f};
    glm::vec3 normal{0.f, 0.f, 1.f};
    glm::vec2 uv{0.f};

    static std::vector<VkVertexInputBindingDescription>
    get_binding_description();

    static std::vector<VkVertexInputAttributeDescription>
    get_attribute_description();
};

class Mesh : public RefCounted {
  public:
    Mesh(VkaoDevice &device, const std::vector<Vertex> &vertices);
    ~Mesh();

    Mesh(const Mesh &) = delete;

    void bind(VkCommandBuffer command_buffer);
    void draw(VkCommandBuffer command_buffer);

  private:
    void create_vertex_buffers(const std::vector<Vertex> &vertices);

    VkaoDevice &vkao_device_;
    VkBuffer vertex_buffer_;
    VkDeviceMemory vertex_buffer_memory_;
    uint32_t vertex_count_;
};
} // namespace vkao