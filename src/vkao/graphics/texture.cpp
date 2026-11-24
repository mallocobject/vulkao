#include "graphics/texture.hpp"
#include "core/logger.hpp"
#include "graphics/buffer.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <stb_image/stb_image.h>

namespace vkao {
Texture::Texture(Device &device, const std::filesystem::path &filepath)
    : device_(device) {
    create_texture_image(filepath);
    create_image_view();
    create_sampler();
}

Texture::Texture(Device &device,
                 uint32_t width,
                 uint32_t height,
                 const uint8_t *rgba_pixels)
    : device_(device) {
    create_texture_image(width, height, rgba_pixels);
    create_image_view();
    create_sampler();
}

Texture::~Texture() {
    auto device = device_.get_device();

    vkDestroySampler(device, sampler_, nullptr);
    vkDestroyImageView(device, image_view_, nullptr);
    vkDestroyImage(device, image_, nullptr);
    vkFreeMemory(device, image_memory_, nullptr);
}

VkDescriptorImageInfo Texture::descriptor_info() const {
    return {
        sampler_,
        image_view_,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
}

void Texture::create_texture_image(const std::filesystem::path &filepath) {
    int tex_width;
    int tex_height;
    int tex_channels;

    stbi_uc *pixels = stbi_load(filepath.string().c_str(),
                                &tex_width,
                                &tex_height,
                                &tex_channels,
                                STBI_rgb_alpha);
    VKAO_ASSERT(pixels, "failed to load texture image!");

    VkDeviceSize pixel_size = sizeof(stbi_uc);

    create_texture_image_detail(pixel_size,
                                static_cast<uint32_t>(tex_width),
                                static_cast<uint32_t>(tex_height),
                                pixels);

    stbi_image_free(pixels);
}

void Texture::create_texture_image(uint32_t width,
                                   uint32_t height,
                                   const uint8_t *rgba_pixels) {
    create_texture_image_detail(sizeof(uint8_t), width, height, rgba_pixels);
}

void Texture::create_texture_image_detail(VkDeviceSize pixel_size,
                                          uint32_t width,
                                          uint32_t height,
                                          const void *pixels) {

    uint32_t pixel_count = width * height * 4;

    Buffer staging_buffer{
        device_,
        pixel_size,
        pixel_count,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    staging_buffer.map();
    staging_buffer.write_to_buffer(reinterpret_cast<const void *>(pixels));

    create_image(width,
                 height,
                 VK_FORMAT_R8G8B8A8_SRGB,
                 VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    transition_layout(VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    device_.copy_buffer_to_image(
        staging_buffer.get_buffer(), image_, width, height, 1);

    transition_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Texture::create_image(uint32_t width,
                           uint32_t height,
                           VkFormat format,
                           VkImageTiling tiling,
                           VkImageUsageFlags usage,
                           VkMemoryPropertyFlags properties) {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format;
    image_info.extent = {width, height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = tiling;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    device_.create_image_with_info(
        image_info, properties, image_, image_memory_);
}

void Texture::transition_layout(VkImageLayout old_layout,
                                VkImageLayout new_layout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image_;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage{};
    VkPipelineStageFlags dst_stage{};

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        VKAO_ASSERT(false, "unsupported texture layout transition");
    }

    auto command_buffer = device_.begin_single_time_commands();

    vkCmdPipelineBarrier(command_buffer,
                         src_stage,
                         dst_stage,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    device_.end_single_time_commands(command_buffer);
}

void Texture::create_image_view() {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image_;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VKAO_ASSERT(vkCreateImageView(
                    device_.get_device(), &view_info, nullptr, &image_view_) ==
                    VK_SUCCESS,
                "failed to create texture image view!");
}

void Texture::create_sampler() {
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.minLod = 0.f;
    sampler_info.maxLod = 0.f;
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    VKAO_ASSERT(vkCreateSampler(
                    device_.get_device(), &sampler_info, nullptr, &sampler_) ==
                    VK_SUCCESS,
                "failed to create texture sampler!");
}
} // namespace vkao