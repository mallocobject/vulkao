#include "graphics/mesh.hpp"

#include "core/logger.hpp"
#include "core/ref.hpp"
#include "core/utils.hpp"
#include "glm/ext/vector_float3.hpp"
#include "graphics/device.hpp"
#include "mesh.hpp"
#include "vulkan/vulkan_core.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <rapidobj/rapidobj.hpp>
#include <unordered_map>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace std {
template <>
struct hash<vkao::Mesh::Vertex> {
    size_t operator()(const vkao::Mesh::Vertex &vertex) const {
        size_t seed = 0;
        vkao::hash_combine(
            seed, vertex.position, vertex.color, vertex.uv, vertex.normal);

        return seed;
    }
};
} // namespace std

namespace vkao {
std::vector<VkVertexInputBindingDescription>
Mesh::Vertex::get_binding_description() {
    std::vector<VkVertexInputBindingDescription> binding_descriptions(1);
    binding_descriptions[0].binding = 0;
    binding_descriptions[0].stride = sizeof(Vertex);
    binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return binding_descriptions;
}
std::vector<VkVertexInputAttributeDescription>
Mesh::Vertex::get_attribute_description() {
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions(4);

    // position
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // signed
    attribute_descriptions[0].offset = offsetof(Vertex, position);

    // color
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; // signed
    attribute_descriptions[1].offset = offsetof(Vertex, color);

    // uv
    attribute_descriptions[2].location = 2;
    attribute_descriptions[2].binding = 0;
    attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[2].offset = offsetof(Vertex, uv);

    // normal
    attribute_descriptions[3].location = 3;
    attribute_descriptions[3].binding = 0;
    attribute_descriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[3].offset = offsetof(Vertex, normal);

    return attribute_descriptions;
}

void Mesh::Builder::load_object(const std::filesystem::path &filepath) {
    auto result =
        rapidobj::ParseFile(filepath, rapidobj::MaterialLibrary::Ignore());

    VKAO_ASSERT(!result.error, result.error.code.message());

    VKAO_ASSERT(rapidobj::Triangulate(result), result.error.code.message());

    vertices.clear();
    indices.clear();

    std::unordered_map<Vertex, uint32_t> unique_vertices;
    for (const auto &shape : result.shapes) {
        for (const auto &index : shape.mesh.indices) {
            Vertex vertex{};

            if (index.position_index >= 0) {
                vertex.position = {
                    result.attributes.positions[3 * index.position_index + 0],
                    result.attributes.positions[3 * index.position_index + 1],
                    result.attributes.positions[3 * index.position_index + 2],
                };

                auto color_index = 3 * index.position_index + 2;
                if (color_index < result.attributes.colors.size()) {
                    vertex.color = {
                        result.attributes.colors[color_index - 2],
                        result.attributes.colors[color_index - 1],
                        result.attributes.colors[color_index - 0],
                    };
                } else {
                    vertex.color = glm::vec3{1.f};
                }
            }

            if (index.texcoord_index >= 0) {
                vertex.uv = {
                    result.attributes.texcoords[2 * index.texcoord_index + 0],
                    result.attributes.texcoords[2 * index.texcoord_index + 1],
                };
            }

            if (index.normal_index >= 0) {
                vertex.normal = {
                    result.attributes.normals[3 * index.normal_index + 0],
                    result.attributes.normals[3 * index.normal_index + 1],
                    result.attributes.normals[3 * index.normal_index + 2],
                };
            }

            if (!unique_vertices.contains(vertex)) {
                unique_vertices[vertex] =
                    static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(unique_vertices[vertex]);
        }
    }
} // namespace vkao

Mesh::Mesh(Device &device, const Builder &builder) : device_(device) {
    create_vertex_buffers(builder.vertices);
    create_index_buffers(builder.indices);
}

Mesh::~Mesh() {
    vkDestroyBuffer(device_.get_device(), vertex_buffer_, nullptr);
    vkFreeMemory(device_.get_device(), vertex_buffer_memory_, nullptr);

    if (has_index_buffer_) {
        vkDestroyBuffer(device_.get_device(), index_buffer_, nullptr);
        vkFreeMemory(device_.get_device(), index_buffer_memory_, nullptr);
    }
}

void Mesh::create_vertex_buffers(const std::vector<Vertex> &vertices) {
    vertex_count_ = static_cast<uint32_t>(vertices.size());
    VKAO_ASSERT(vertex_count_ >= 3, "vertex count must be at least 3!");

    VkDeviceSize buffer_size = sizeof(vertices[0]) * vertex_count_;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    device_.create_buffer(buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          staging_buffer,
                          staging_buffer_memory);

    void *data{};
    vkMapMemory(
        device_.get_device(), staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));
    vkUnmapMemory(device_.get_device(), staging_buffer_memory);

    device_.create_buffer(buffer_size,
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          vertex_buffer_,
                          vertex_buffer_memory_);

    device_.copy_buffer(staging_buffer, vertex_buffer_, buffer_size);

    vkDestroyBuffer(device_.get_device(), staging_buffer, nullptr);
    vkFreeMemory(device_.get_device(), staging_buffer_memory, nullptr);
}

void Mesh::create_index_buffers(const std::vector<uint32_t> &indices) {
    index_count_ = static_cast<uint32_t>(indices.size());
    has_index_buffer_ = index_count_ > 0;
    if (!has_index_buffer_) {
        return;
    }

    VkDeviceSize buffer_size = sizeof(indices[0]) * index_count_;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    device_.create_buffer(buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          staging_buffer,
                          staging_buffer_memory);

    void *data{};
    vkMapMemory(
        device_.get_device(), staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, indices.data(), static_cast<size_t>(buffer_size));
    vkUnmapMemory(device_.get_device(), staging_buffer_memory);

    device_.create_buffer(buffer_size,
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          index_buffer_,
                          index_buffer_memory_);

    device_.copy_buffer(staging_buffer, index_buffer_, buffer_size);

    vkDestroyBuffer(device_.get_device(), staging_buffer, nullptr);
    vkFreeMemory(device_.get_device(), staging_buffer_memory, nullptr);
}

void Mesh::bind(VkCommandBuffer command_buffer) {
    VkBuffer buffers[]{vertex_buffer_};
    VkDeviceSize offsets[]{0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, buffers, offsets);

    if (has_index_buffer_) {
        vkCmdBindIndexBuffer(
            command_buffer, index_buffer_, 0, VK_INDEX_TYPE_UINT32);
    }
}

void Mesh::draw(VkCommandBuffer command_buffer) {
    if (has_index_buffer_) {
        vkCmdDrawIndexed(command_buffer, index_count_, 1, 0, 0, 0);
    } else {
        vkCmdDraw(command_buffer, vertex_count_, 1, 0, 0);
    }
}

Ref<Mesh> Mesh::create(Device &device, const std::filesystem::path &filepath) {
    Builder builder{};
    builder.load_object(filepath);

    return make_ref<Mesh>(device, builder);
}

} // namespace vkao