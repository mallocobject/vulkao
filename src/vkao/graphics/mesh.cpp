#include "graphics/mesh.hpp"

#include "core/logger.hpp"
#include "core/ref.hpp"
#include "core/utils.hpp"
#include "glm/ext/vector_float3.hpp"
#include "graphics/buffer.hpp"
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
    std::vector<VkVertexInputBindingDescription> binding_descriptions;

    binding_descriptions.push_back(
        {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX});

    return binding_descriptions;
}
std::vector<VkVertexInputAttributeDescription>
Mesh::Vertex::get_attribute_description() {
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

    attribute_descriptions.push_back(
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
    attribute_descriptions.push_back(
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
    attribute_descriptions.push_back(
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});
    attribute_descriptions.push_back(
        {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});

    return attribute_descriptions;
}

void Mesh::Builder::load_object(const std::filesystem::path &filepath) {
    auto result =
        rapidobj::ParseFile(filepath, rapidobj::MaterialLibrary::Ignore());

    VKAO_ASSERT(!result.error, result.error.code.message());

    for (auto &shape : result.shapes) {
        auto &mesh = shape.mesh;

        if (mesh.material_ids.empty()) {
            mesh.material_ids =
                rapidobj::Array<int32_t>(mesh.num_face_vertices.size());

            std::fill(mesh.material_ids.begin(), mesh.material_ids.end(), -1);
        }
    }

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

void Mesh::create_vertex_buffers(const std::vector<Vertex> &vertices) {
    vertex_count_ = static_cast<uint32_t>(vertices.size());
    VKAO_ASSERT(vertex_count_ >= 3, "vertex count must be at least 3!");

    uint32_t vertex_size = sizeof(vertices[0]);
    VkDeviceSize buffer_size = vertex_size * vertex_count_;

    Buffer staging_buffer{
        device_,
        vertex_size,
        vertex_count_,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    staging_buffer.map();
    staging_buffer.write_to_buffer(
        reinterpret_cast<const void *>(vertices.data()));

    vertex_buffer_ = make_ref<Buffer>(device_,
                                      vertex_size,
                                      vertex_count_,
                                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    device_.copy_buffer(
        staging_buffer.get_buffer(), vertex_buffer_->get_buffer(), buffer_size);
}

void Mesh::create_index_buffers(const std::vector<uint32_t> &indices) {
    index_count_ = static_cast<uint32_t>(indices.size());
    has_index_buffer_ = index_count_ > 0;
    if (!has_index_buffer_) {
        return;
    }

    uint32_t index_size = sizeof(indices[0]);
    VkDeviceSize buffer_size = index_size * index_count_;

    Buffer staging_buffer{
        device_,
        index_size,
        index_count_,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    staging_buffer.map();
    staging_buffer.write_to_buffer(
        reinterpret_cast<const void *>(indices.data()));

    index_buffer_ = make_ref<Buffer>(device_,
                                     index_size,
                                     index_count_,
                                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    device_.copy_buffer(
        staging_buffer.get_buffer(), index_buffer_->get_buffer(), buffer_size);
}

void Mesh::bind(VkCommandBuffer command_buffer) {
    VkBuffer buffers[]{vertex_buffer_->get_buffer()};
    VkDeviceSize offsets[]{0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, buffers, offsets);

    if (has_index_buffer_) {
        vkCmdBindIndexBuffer(command_buffer,
                             index_buffer_->get_buffer(),
                             0,
                             VK_INDEX_TYPE_UINT32);
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