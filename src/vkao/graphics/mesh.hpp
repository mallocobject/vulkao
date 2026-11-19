#pragma once

#include "core/ref.hpp"
#include "graphics/device.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>
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
    struct Builder {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    Mesh(Device &device, const Builder &builder);
    ~Mesh();

    Mesh(const Mesh &) = delete;

    void bind(VkCommandBuffer command_buffer);
    void draw(VkCommandBuffer command_buffer);

  private:
    void create_vertex_buffers(const std::vector<Vertex> &vertices);
    void create_index_buffers(const std::vector<uint32_t> &indices);

    Device &device_;

    VkBuffer vertex_buffer_;
    VkDeviceMemory vertex_buffer_memory_;
    uint32_t vertex_count_;

    VkBuffer index_buffer_;
    VkDeviceMemory index_buffer_memory_;
    uint32_t index_count_;
    bool has_index_buffer_{false};
};
} // namespace vkao