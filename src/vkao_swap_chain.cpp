#include "vkao_swap_chain.hpp"

#include "log.hpp"
#include "ref.hpp"
#include "vkao_device.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace vkao {
VkaoSwapChain::VkaoSwapChain(VkaoDevice &device, VkExtent2D window_extent)
    : VkaoSwapChain(device, window_extent, Ref<VkaoSwapChain>()) {
}

VkaoSwapChain::VkaoSwapChain(VkaoDevice &device,
                             VkExtent2D window_extent,
                             const Ref<VkaoSwapChain> &previous)
    : vkao_device_{device}, window_extent_{window_extent},
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
        vkDestroyImageView(vkao_device_.get_device(), image_view, nullptr);
    }
    swap_chain_image_views_.clear();

    if (swap_chain_ != nullptr) {
        vkDestroySwapchainKHR(vkao_device_.get_device(), swap_chain_, nullptr);
        swap_chain_ = nullptr;
    }

    for (int i = 0; i < depth_images_.size(); i++) {
        vkDestroyImageView(
            vkao_device_.get_device(), depth_image_views_[i], nullptr);
        vkDestroyImage(vkao_device_.get_device(), depth_images_[i], nullptr);
        vkFreeMemory(
            vkao_device_.get_device(), depth_image_memorys_[i], nullptr);
    }

    for (auto frame_buffer : swap_chain_frame_buffers_) {
        vkDestroyFramebuffer(vkao_device_.get_device(), frame_buffer, nullptr);
    }

    vkDestroyRenderPass(vkao_device_.get_device(), render_pass_, nullptr);

    // cleanup synchronization objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(
            vkao_device_.get_device(), render_finished_semaphores_[i], nullptr);
        vkDestroySemaphore(
            vkao_device_.get_device(), image_available_semaphores_[i], nullptr);
        vkDestroyFence(vkao_device_.get_device(), inflight_fences_[i], nullptr);
    }
}

VkResult VkaoSwapChain::acquire_next_image(uint32_t *image_index) {
    vkWaitForFences(vkao_device_.get_device(),
                    1,
                    &inflight_fences_[current_frame_],
                    VK_TRUE,
                    std::numeric_limits<uint64_t>::max());

    VkResult result = vkAcquireNextImageKHR(
        vkao_device_.get_device(),
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
        vkWaitForFences(vkao_device_.get_device(),
                        1,
                        &images_inflight_[*image_index],
                        VK_TRUE,
                        UINT64_MAX);
    }
    images_inflight_[*image_index] = inflight_fences_[current_frame_];

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {
        image_available_semaphores_[current_frame_]};
    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = buffers;

    VkSemaphore signal_semaphores[] = {
        render_finished_semaphores_[current_frame_]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkResetFences(
        vkao_device_.get_device(), 1, &inflight_fences_[current_frame_]);

    VKAO_ASSERT(vkQueueSubmit(vkao_device_.get_graphics_queue(),
                              1,
                              &submit_info,
                              inflight_fences_[current_frame_]) == VK_SUCCESS,
                "failed to submit draw command buffer!");

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swap_chains[] = {swap_chain_};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;

    present_info.pImageIndices = image_index;

    auto result =
        vkQueuePresentKHR(vkao_device_.get_present_queue(), &present_info);

    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;

    return result;
}

void VkaoSwapChain::create_swap_chain() {
    SwapChainSupportDetails swap_chain_support =
        vkao_device_.get_swap_chain_support();

    VkSurfaceFormatKHR surface_format =
        choose_swap_surface_format(swap_chain_support.formats);
    VkPresentModeKHR present_mode =
        choose_swap_present_mode(swap_chain_support.presentModes);
    VkExtent2D extent = choose_swap_extent(swap_chain_support.capabilities);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 &&
        image_count > swap_chain_support.capabilities.maxImageCount) {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = vkao_device_.get_surface();

    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = vkao_device_.find_physical_queue_families();
    uint32_t queue_family_indices[] = {indices.graphics_family.value(),
                                       indices.present_family.value()};

    if (indices.graphics_family != indices.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;     // Optional
        create_info.pQueueFamilyIndices = nullptr; // Optional
    }

    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;

    create_info.oldSwapchain =
        old_swap_chain_ ? old_swap_chain_->swap_chain_ : VK_NULL_HANDLE;

    VKAO_ASSERT(vkCreateSwapchainKHR(vkao_device_.get_device(),
                                     &create_info,
                                     nullptr,
                                     &swap_chain_) == VK_SUCCESS,
                "failed to create swap chain!");

    // we only specified a minimum number of images in the swap chain, so the
    // implementation is allowed to create a swap chain with more. That's why
    // we'll first query the final number of images with
    // vkGetSwapchainImagesKHR, then resize the container and finally call it
    // again to retrieve the handles.
    vkGetSwapchainImagesKHR(
        vkao_device_.get_device(), swap_chain_, &image_count, nullptr);
    swap_chain_images_.resize(image_count);
    vkGetSwapchainImagesKHR(vkao_device_.get_device(),
                            swap_chain_,
                            &image_count,
                            swap_chain_images_.data());

    swap_chain_image_format_ = surface_format.format;
    swap_chain_extent_ = extent;
}

void VkaoSwapChain::create_image_views() {
    swap_chain_image_views_.resize(swap_chain_images_.size());
    for (size_t i = 0; i < swap_chain_images_.size(); i++) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = swap_chain_images_[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swap_chain_image_format_;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VKAO_ASSERT(vkCreateImageView(vkao_device_.get_device(),
                                      &view_info,
                                      nullptr,
                                      &swap_chain_image_views_[i]) ==
                        VK_SUCCESS,
                    "failed to create texture image view!");
    }
}

void VkaoSwapChain::create_render_pass() {
    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = find_depth_format();
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref{};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription color_attachment = {};
    color_attachment.format = get_swap_chain_image_format();
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

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

    std::array<VkAttachmentDescription, 2> attachments = {color_attachment,
                                                          depth_attachment};
    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount =
        static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(vkao_device_.get_device(),
                           &render_pass_info,
                           nullptr,
                           &render_pass_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void VkaoSwapChain::create_frame_buffers() {
    swap_chain_frame_buffers_.resize(image_count());
    for (size_t i = 0; i < image_count(); i++) {
        std::array<VkImageView, 2> attachments = {swap_chain_image_views_[i],
                                                  depth_image_views_[i]};

        VkExtent2D swap_chain_extent = get_swap_chain_extent();
        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass_;
        framebuffer_info.attachmentCount =
            static_cast<uint32_t>(attachments.size());
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = swap_chain_extent.width;
        framebuffer_info.height = swap_chain_extent.height;
        framebuffer_info.layers = 1;

        VKAO_ASSERT(vkCreateFramebuffer(vkao_device_.get_device(),
                                        &framebuffer_info,
                                        nullptr,
                                        &swap_chain_frame_buffers_[i]) ==
                        VK_SUCCESS,
                    "failed to create framebuffer!");
    }
}

void VkaoSwapChain::create_depth_resources() {
    VkFormat depth_format = find_depth_format();
    swap_chain_depth_format_ = depth_format;
    VkExtent2D swap_chain_extent = get_swap_chain_extent();

    depth_images_.resize(image_count());
    depth_image_memorys_.resize(image_count());
    depth_image_views_.resize(image_count());

    for (int i = 0; i < depth_images_.size(); i++) {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = swap_chain_extent.width;
        image_info.extent.height = swap_chain_extent.height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = depth_format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.flags = 0;

        vkao_device_.create_image_with_info(image_info,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            depth_images_[i],
                                            depth_image_memorys_[i]);

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = depth_images_[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = depth_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VKAO_ASSERT(vkCreateImageView(vkao_device_.get_device(),
                                      &view_info,
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

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VKAO_ASSERT(vkCreateSemaphore(vkao_device_.get_device(),
                                      &semaphore_info,
                                      nullptr,
                                      &image_available_semaphores_[i]) ==
                            VK_SUCCESS &&
                        vkCreateSemaphore(vkao_device_.get_device(),
                                          &semaphore_info,
                                          nullptr,
                                          &render_finished_semaphores_[i]) ==
                            VK_SUCCESS &&
                        vkCreateFence(vkao_device_.get_device(),
                                      &fence_info,
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
            VKAO_INFO("Present mode: Mailbox");
            return available_present_mode;
        }
    }

    // for (const auto &availablePresentMode : availablePresentModes) {
    //   if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
    //     std::cout << "Present mode: Immediate" << std::endl;
    //     return availablePresentMode;
    //   }
    // }

    VKAO_INFO("Present mode: V-Sync");
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VkaoSwapChain::choose_swap_extent(
    const VkSurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actual_extent = window_extent_;
        actual_extent.width = std::max(
            capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, actual_extent.width));
        actual_extent.height = std::max(
            capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, actual_extent.height));

        return actual_extent;
    }
}

VkFormat VkaoSwapChain::find_depth_format() {
    return vkao_device_.find_supported_format(
        {VK_FORMAT_D32_SFLOAT,
         VK_FORMAT_D32_SFLOAT_S8_UINT,
         VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
} // namespace vkao