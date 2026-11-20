#pragma once

#include "core/ref.hpp"
#include "glm/ext/vector_float3.hpp"
#include "graphics/buffer.hpp"
#include "graphics/device.hpp"
#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {
class Mesh : public RefCounted {
  public:
    struct Vertex {
        glm::vec3 position{0.f};
        glm::vec3 color{1.f};
        glm::vec2 uv{0.f};
        glm::vec3 normal{0.f, 0.f, 1.f};

        static std::vector<VkVertexInputBindingDescription>
        get_binding_description();

        static std::vector<VkVertexInputAttributeDescription>
        get_attribute_description();

        bool operator==(const Vertex &other) const {
            return position == other.position && color == other.color &&
                   uv == other.uv && normal == other.normal;
        }
    };

    struct Builder {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        void load_object(const std::filesystem::path &filepath);
    };

    Mesh(Device &device, const Builder &builder);
    ~Mesh() = default;
    Mesh(const Mesh &) = delete;

    void bind(VkCommandBuffer command_buffer);
    void draw(VkCommandBuffer command_buffer);

    static Ref<Mesh> create(Device &device,
                            const std::filesystem::path &filepath);

  private:
    void create_vertex_buffers(const std::vector<Vertex> &vertices);
    void create_index_buffers(const std::vector<uint32_t> &indices);

    Device &device_;

    Ref<Buffer> vertex_buffer_;
    uint32_t vertex_count_;

    Ref<Buffer> index_buffer_;
    uint32_t index_count_;
    bool has_index_buffer_{false};
};
} // namespace vkao