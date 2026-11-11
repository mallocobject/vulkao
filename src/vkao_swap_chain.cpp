#include "vkao_swap_chain.h"
#include "log.hpp"
#include "ref.hpp"
#include "vkao_device.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

namespace vkao {
VkaoSwapChain::VkaoSwapChain(VkaoDevice &device, VkExtent2D window_extent)
    : VkaoSwapChain(device, window_extent, Ref<VkaoSwapChain>()) {
}

VkaoSwapChain::VkaoSwapChain(VkaoDevice &device,
                             VkExtent2D window_extent,
                             const Ref<VkaoSwapChain> &previous)
    : device_{device}, window_extent_{window_extent},
      old_swap_chain_{previous} {
    init();
    old_swap_chain_.reset();
}

void VkaoSwapChain::init() {
    create_swap_chain();
    create_image_views();
    create_render_pass();
    create_depth_resources();
    create_frame_buffers();
    create_sync_objects();
}

VkaoSwapChain::~VkaoSwapChain() {
    for (auto image_view : swap_chain_image_views_) {
        vkDestroyImageView(device_.get_device(), image_view, nullptr);
    }
    swap_chain_image_views_.clear();

    if (swap_chain_ != nullptr) {
        vkDestroySwapchainKHR(device_.get_device(), swap_chain_, nullptr);
        swap_chain_ = nullptr;
    }

    for (int i = 0; i < depth_images_.size(); i++) {
        vkDestroyImageView(
            device_.get_device(), depth_image_views_[i], nullptr);
        vkDestroyImage(device_.get_device(), depth_images_[i], nullptr);
        vkFreeMemory(device_.get_device(), depth_image_memorys_[i], nullptr);
    }

    for (auto frame_buffer : swap_chain_frame_buffers_) {
        vkDestroyFramebuffer(device_.get_device(), frame_buffer, nullptr);
    }

    vkDestroyRenderPass(device_.get_device(), render_pass_, nullptr);

    // cleanup synchronization objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(
            device_.get_device(), render_finished_semaphores_[i], nullptr);
        vkDestroySemaphore(
            device_.get_device(), image_available_semaphores_[i], nullptr);
        vkDestroyFence(device_.get_device(), inflight_fences_[i], nullptr);
    }
}

VkResult VkaoSwapChain::acquire_next_image(uint32_t *image_index) {
    vkWaitForFences(device_.get_device(),
                    1,
                    &inflight_fences_[current_frame_],
                    VK_TRUE,
                    std::numeric_limits<uint64_t>::max());

    VkResult result = vkAcquireNextImageKHR(
        device_.get_device(),
        swap_chain_,
        std::numeric_limits<uint64_t>::max(),
        image_available_semaphores_[current_frame_], // must be a not signaled
                                                     // semaphore
        VK_NULL_HANDLE,
        image_index);

    return result;
}

VkResult VkaoSwapChain::submit_command_buffers(const VkCommandBuffer *buffers,
                                               uint32_t *image_index) {
    if (images_inflight_[*image_index] != VK_NULL_HANDLE) {
        vkWaitForFences(device_.get_device(),
                        1,
                        &images_inflight_[*image_index],
                        VK_TRUE,
                        UINT64_MAX);
    }
    images_inflight_[*image_index] = inflight_fences_[current_frame_];

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {
        image_available_semaphores_[current_frame_]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = buffers;

    VkSemaphore signalSemaphores[] = {
        render_finished_semaphores_[current_frame_]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(device_.get_device(), 1, &inflight_fences_[current_frame_]);

    VKAO_ASSERT(vkQueueSubmit(device_.get_graphics_queue(),
                              1,
                              &submitInfo,
                              inflight_fences_[current_frame_]) == VK_SUCCESS,
                "failed to submit draw command buffer!");

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swap_chain_};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = image_index;

    auto result = vkQueuePresentKHR(device_.get_present_queue(), &presentInfo);

    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;

    return result;
}

void VkaoSwapChain::create_swap_chain() {
    SwapChainSupportDetails swapChainSupport = device_.get_swap_chain_support();

    VkSurfaceFormatKHR surfaceFormat =
        choose_swap_surface_format(swapChainSupport.formats);
    VkPresentModeKHR presentMode =
        choose_swap_present_mode(swapChainSupport.presentModes);
    VkExtent2D extent = choose_swap_extent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = device_.get_surface();

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = device_.find_physical_queue_families();
    uint32_t queueFamilyIndices[] = {indices.graphics_family.value(),
                                     indices.present_family.value()};

    if (indices.graphics_family != indices.present_family) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;     // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain =
        old_swap_chain_ ? old_swap_chain_->swap_chain_ : VK_NULL_HANDLE;

    VKAO_ASSERT(vkCreateSwapchainKHR(
                    device_.get_device(), &createInfo, nullptr, &swap_chain_) ==
                    VK_SUCCESS,
                "failed to create swap chain!");

    // we only specified a minimum number of images in the swap chain, so the
    // implementation is allowed to create a swap chain with more. That's why
    // we'll first query the final number of images with
    // vkGetSwapchainImagesKHR, then resize the container and finally call it
    // again to retrieve the handles.
    vkGetSwapchainImagesKHR(
        device_.get_device(), swap_chain_, &imageCount, nullptr);
    swap_chain_images_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_.get_device(),
                            swap_chain_,
                            &imageCount,
                            swap_chain_images_.data());

    swap_chain_image_format_ = surfaceFormat.format;
    swap_chain_extent_ = extent;
}

void VkaoSwapChain::create_image_views() {
    swap_chain_image_views_.resize(swap_chain_images_.size());
    for (size_t i = 0; i < swap_chain_images_.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swap_chain_images_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swap_chain_image_format_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VKAO_ASSERT(vkCreateImageView(device_.get_device(),
                                      &viewInfo,
                                      nullptr,
                                      &swap_chain_image_views_[i]) ==
                        VK_SUCCESS,
                    "failed to create texture image view!");
    }
}

void VkaoSwapChain::create_render_pass() {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = find_depth_format();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = get_swap_chain_image_format();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.dstSubpass = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcAccessMask = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment,
                                                          depthAttachment};
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(
            device_.get_device(), &renderPassInfo, nullptr, &render_pass_) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void VkaoSwapChain::create_frame_buffers() {
    swap_chain_frame_buffers_.resize(image_count());
    for (size_t i = 0; i < image_count(); i++) {
        std::array<VkImageView, 2> attachments = {swap_chain_image_views_[i],
                                                  depth_image_views_[i]};

        VkExtent2D swapChainExtent = get_swap_chain_extent();
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = render_pass_;
        framebufferInfo.attachmentCount =
            static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        VKAO_ASSERT(vkCreateFramebuffer(device_.get_device(),
                                        &framebufferInfo,
                                        nullptr,
                                        &swap_chain_frame_buffers_[i]) ==
                        VK_SUCCESS,
                    "failed to create framebuffer!");
    }
}

void VkaoSwapChain::create_depth_resources() {
    VkFormat depthFormat = find_depth_format();
    swap_chain_depth_format_ = depthFormat;
    VkExtent2D swapChainExtent = get_swap_chain_extent();

    depth_images_.resize(image_count());
    depth_image_memorys_.resize(image_count());
    depth_image_views_.resize(image_count());

    for (int i = 0; i < depth_images_.size(); i++) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapChainExtent.width;
        imageInfo.extent.height = swapChainExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.flags = 0;

        device_.create_image_with_info(imageInfo,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       depth_images_[i],
                                       depth_image_memorys_[i]);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depth_images_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VKAO_ASSERT(vkCreateImageView(device_.get_device(),
                                      &viewInfo,
                                      nullptr,
                                      &depth_image_views_[i]) == VK_SUCCESS,
                    "failed to create texture image view!");
    }
}

void VkaoSwapChain::create_sync_objects() {
    image_available_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inflight_fences_.resize(MAX_FRAMES_IN_FLIGHT);
    images_inflight_.resize(image_count(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VKAO_ASSERT(vkCreateSemaphore(device_.get_device(),
                                      &semaphoreInfo,
                                      nullptr,
                                      &image_available_semaphores_[i]) ==
                            VK_SUCCESS &&
                        vkCreateSemaphore(device_.get_device(),
                                          &semaphoreInfo,
                                          nullptr,
                                          &render_finished_semaphores_[i]) ==
                            VK_SUCCESS &&
                        vkCreateFence(device_.get_device(),
                                      &fenceInfo,
                                      nullptr,
                                      &inflight_fences_[i]) == VK_SUCCESS,
                    "failed to create synchronization objects for a frame!");
    }
}

VkSurfaceFormatKHR VkaoSwapChain::choose_swap_surface_format(
    const std::vector<VkSurfaceFormatKHR> &available_formats) {
    for (const auto &available_format : available_formats) {
        if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_format;
        }
    }

    return available_formats[0];
}

VkPresentModeKHR VkaoSwapChain::choose_swap_present_mode(
    const std::vector<VkPresentModeKHR> &available_present_modes) {
    for (const auto &available_present_mode : available_present_modes) {
        if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            std::cout << "Present mode: Mailbox" << std::endl;
            return available_present_mode;
        }
    }

    // for (const auto &availablePresentMode : availablePresentModes) {
    //   if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
    //     std::cout << "Present mode: Immediate" << std::endl;
    //     return availablePresentMode;
    //   }
    // }

    std::cout << "Present mode: V-Sync" << std::endl;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VkaoSwapChain::choose_swap_extent(
    const VkSurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = window_extent_;
        actualExtent.width = std::max(
            capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(
            capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

VkFormat VkaoSwapChain::find_depth_format() {
    return device_.find_supported_format(
        {VK_FORMAT_D32_SFLOAT,
         VK_FORMAT_D32_SFLOAT_S8_UINT,
         VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
} // namespace vkao