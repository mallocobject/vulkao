#include "vkao_model.hpp"
#include "logger.hpp"
#include "vulkan/vulkan_core.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace vkao {
std::vector<VkVertexInputBindingDescription>
VkaoModel::Vertex::get_binding_description() {
    std::vector<VkVertexInputBindingDescription> binding_descriptions(1);
    binding_descriptions[0].binding = 0;
    binding_descriptions[0].stride = sizeof(Vertex);
    binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return binding_descriptions;
}

std::vector<VkVertexInputAttributeDescription>
VkaoModel::Vertex::get_attribute_description() {
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions(2);

    // position
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // signed
    attribute_descriptions[0].offset = offsetof(Vertex, pos);

    // color
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(Vertex, color);

    return attribute_descriptions;
}

VkaoModel::VkaoModel(VkaoDevice &device, const std::vector<Vertex> &vertexes)
    : vkao_device_(device) {
    create_vertex_buffers(vertexes);
}

VkaoModel::~VkaoModel() {
    vkDestroyBuffer(vkao_device_.get_device(), vertex_buffer_, nullptr);
    vkFreeMemory(vkao_device_.get_device(), vertex_buffer_memory_, nullptr);
}

void VkaoModel::create_vertex_buffers(const std::vector<Vertex> &vertexes) {
    vertex_count_ = static_cast<uint32_t>(vertexes.size());
    VKAO_ASSERT(vertex_count_ >= 3, "vertex count must be at least 3!");

    VkDeviceSize buffer_size = sizeof(vertexes[0]) * vertex_count_;

    vkao_device_.create_buffer(buffer_size,
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               vertex_buffer_,
                               vertex_buffer_memory_);

    void *data{};
    vkMapMemory(vkao_device_.get_device(),
                vertex_buffer_memory_,
                0,
                buffer_size,
                0,
                &data);
    memcpy(data, vertexes.data(), static_cast<size_t>(buffer_size));
    vkUnmapMemory(vkao_device_.get_device(), vertex_buffer_memory_);
}

void VkaoModel::bind(VkCommandBuffer command_buffer) {
    VkBuffer buffers[]{vertex_buffer_};
    VkDeviceSize offsets[]{0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, buffers, offsets);
}

void VkaoModel::draw(VkCommandBuffer command_buffer) {
    vkCmdDraw(command_buffer, vertex_count_, 1, 0, 0);
}
} // namespace vkao