#pragma once

#include "core/ref.hpp"
#include "graphics/device.hpp"
#include <cstdint>
#include <filesystem>
#include <vulkan/vulkan_core.h>

namespace vkao {
class Texture : public RefCounted {
  public:
    static constexpr int MAX_NUM_MATERIALS = 32;

    Texture(Device &device, const std::filesystem::path &filepath);
    Texture(Device &device,
            uint32_t width,
            uint32_t height,
            const uint8_t *rgba_pixels);
    ~Texture();
    Texture(const Texture &) = delete;

    VkDescriptorImageInfo descriptor_info() const;

  private:
    void create_texture_image(const std::filesystem::path &filepath);
    void create_texture_image(uint32_t width,
                              uint32_t height,
                              const uint8_t *rgba_pixels);
    void create_texture_image_detail(VkDeviceSize pixel_size,
                                     uint32_t width,
                                     uint32_t height,
                                     const void *pixels);
    void create_image(uint32_t width,
                      uint32_t height,
                      VkFormat format,
                      VkImageTiling tiling,
                      VkImageUsageFlags uasge,
                      VkMemoryPropertyFlags properties);

    void transition_layout(VkImageLayout old_layout, VkImageLayout new_layout);
    void create_image_view();
    void create_sampler();

    Device &device_;

    VkImage image_{VK_NULL_HANDLE};
    VkDeviceMemory image_memory_{VK_NULL_HANDLE};
    VkImageView image_view_{VK_NULL_HANDLE};
    VkSampler sampler_{VK_NULL_HANDLE};
};
} // namespace vkao