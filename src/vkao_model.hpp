#pragma once

#include "glm/ext/vector_float2.hpp"
#include "ref.hpp"
#include "vkao_device.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <vector>

namespace vkao {
class VkaoModel : public RefCounted {
  public:
    struct Vertex {
        glm::vec2 pos;

        static std::vector<VkVertexInputBindingDescription>
        get_binding_description();

        static std::vector<VkVertexInputAttributeDescription>
        get_attribute_description();
    };
    static_assert(sizeof(Vertex) == 8);

    VkaoModel(VkaoDevice &device, const std::vector<Vertex> &vertexes);
    ~VkaoModel();

    VkaoModel(const VkaoModel &) = delete;

    void bind(VkCommandBuffer command_buffer);
    void draw(VkCommandBuffer command_buffer);

  private:
    void create_vertex_buffers(const std::vector<Vertex> &vertexes);

    VkaoDevice &vkao_device_;
    VkBuffer vertex_buffer_;
    VkDeviceMemory vertex_buffer_memory_;
    uint32_t vertex_count_;
};
} // namespace vkao