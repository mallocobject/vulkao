#include "graphics/shadow_map.hpp"
#include "core/logger.hpp"
#include "vulkan/vulkan_core.h"
#include <array>

namespace vkao {
ShadowMap::ShadowMap(Device &device, VkExtent2D extent)
    : device_(device), extent_(extent) {
    create_depth_resources();
    create_sampler();
    create_render_pass();
    create_framebuffer();
}

ShadowMap::~ShadowMap() {
    auto device = device_.get_device();

    vkDestroyFramebuffer(device, framebuffer_, nullptr);
    vkDestroyRenderPass(device, render_pass_, nullptr);
    vkDestroySampler(device, sampler_, nullptr);
    vkDestroyImageView(device, depth_view_, nullptr);
    vkDestroyImage(device, depth_image_, nullptr);
    vkFreeMemory(device, depth_memory_, nullptr);
}

void ShadowMap::create_depth_resources() {
    depth_format_ = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = depth_format_;
    image_info.extent = {extent_.width, extent_.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    device_.create_image_with_info(image_info,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   depth_image_,
                                   depth_memory_);

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = depth_image_;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = depth_format_;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VKAO_ASSERT(vkCreateImageView(
                    device_.get_device(), &view_info, nullptr, &depth_view_) ==
                    VK_SUCCESS,
                "failed to create shadow image view!");
}

void ShadowMap::create_sampler() {
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    sampler_info.compareEnable = VK_TRUE;
    sampler_info.compareOp = VK_COMPARE_OP_LESS;

    VKAO_ASSERT(vkCreateSampler(
                    device_.get_device(), &sampler_info, nullptr, &sampler_) ==
                    VK_SUCCESS,
                "failed to create shadow sampler!");
}

void ShadowMap::create_render_pass() {
    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = depth_format_;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depth_reference{};
    depth_reference.attachment = 0;
    depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depth_reference;

    constexpr VkPipelineStageFlags depth_stages =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    std::array<VkSubpassDependency, 2> dependencies{};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = depth_stages;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = depth_stages;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &depth_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount =
        static_cast<uint32_t>(dependencies.size());
    render_pass_info.pDependencies = dependencies.data();

    VKAO_ASSERT(vkCreateRenderPass(device_.get_device(),
                                   &render_pass_info,
                                   nullptr,
                                   &render_pass_) == VK_SUCCESS,
                "failed to create shadow render pass");
}

void ShadowMap::create_framebuffer() {
    VkFramebufferCreateInfo framebuffer_info{};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = render_pass_;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments = &depth_view_;
    framebuffer_info.width = extent_.width;
    framebuffer_info.height = extent_.height;
    framebuffer_info.layers = 1;

    VKAO_ASSERT(vkCreateFramebuffer(device_.get_device(),
                                    &framebuffer_info,
                                    nullptr,
                                    &framebuffer_) == VK_SUCCESS,
                "Failed to create shadow framebuffer");
}
} // namespace vkao