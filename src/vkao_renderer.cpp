#include "vkao_renderer.hpp"

#include "logger.hpp"
#include "vkao_swap_chain.hpp"
#include "vulkan/vulkan_core.h"
#include <array>

namespace vkao {
VkaoRenderer::VkaoRenderer(VkaoWindow &window, VkaoDevice &device)
    : vkao_window_{window}, vkao_device_{device} {
    recreate_swap_chain();
    create_command_buffers();
}

VkaoRenderer::~VkaoRenderer() {
    free_command_buffers();
}

void VkaoRenderer::create_command_buffers() {
    command_buffers_.resize(VkaoSwapChain::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = vkao_device_.get_command_pool();
    alloc_info.commandBufferCount =
        static_cast<uint32_t>(command_buffers_.size());

    VKAO_ASSERT(vkAllocateCommandBuffers(vkao_device_.get_device(),
                                         &alloc_info,
                                         command_buffers_.data()) == VK_SUCCESS,
                "failed to allocate command buffers!");
}

void VkaoRenderer::free_command_buffers() {
    vkFreeCommandBuffers(vkao_device_.get_device(),
                         vkao_device_.get_command_pool(),
                         static_cast<uint32_t>(command_buffers_.size()),
                         command_buffers_.data());
}

void VkaoRenderer::recreate_swap_chain() {
    auto extent = vkao_window_.get_extent();
    while (extent.width == 0 || extent.height == 0) {
        SDL_Event event;
        SDL_WaitEvent(&event);

        if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            vkao_window_.on_window_resized(event.window.data1,
                                           event.window.data2);
        }
        extent = vkao_window_.get_extent();
    }

    vkDeviceWaitIdle(vkao_device_.get_device());

    auto old_swap_chain = vkao_swap_chain_;
    vkao_swap_chain_ =
        make_ref<VkaoSwapChain>(vkao_device_, extent, old_swap_chain);

    if (old_swap_chain) {
        VKAO_ASSERT(vkao_swap_chain_->compare_swap_formats(*old_swap_chain),
                    "swap chain image(or depth) format has changed!");
    }
}

VkCommandBuffer VkaoRenderer::begin_frame() {
    VKAO_ASSERT(!is_frame_started_,
                "cannot call begin_frame while already in progress!");

    auto result = vkao_swap_chain_->acquire_next_image(&current_image_index_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swap_chain();
        return VK_NULL_HANDLE;
    }

    VKAO_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR,
                "failed to acquire swap chain image!");

    is_frame_started_ = true;
    auto command_buffer = get_current_command_buffer();

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VKAO_ASSERT(vkBeginCommandBuffer(command_buffer, &begin_info) == VK_SUCCESS,
                "failed to begin recording command buffer!");

    return command_buffer;
}

void VkaoRenderer::end_frame() {
    VKAO_ASSERT(is_frame_started_,
                "cannot call end_frame while frame is not in progress!");

    auto command_buffer = get_current_command_buffer();

    VKAO_ASSERT(vkEndCommandBuffer(command_buffer) == VK_SUCCESS,
                "failed to record command buffer!");

    auto result = vkao_swap_chain_->submit_command_buffers(
        &command_buffer, &current_image_index_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        vkao_window_.was_window_resized()) {
        vkao_window_.reset_window_resized_flag();
        recreate_swap_chain();

    } else {
        VKAO_ASSERT(result == VK_SUCCESS,
                    "failed to present swap chain image!");
    }

    is_frame_started_ = false;

    current_frame_index_ =
        (current_frame_index_ + 1) % VkaoSwapChain::MAX_FRAMES_IN_FLIGHT;
}

void VkaoRenderer::begin_swap_chain_render_pass(
    VkCommandBuffer command_buffer) {
    VKAO_ASSERT(is_frame_started_,
                "cannot call begin_swap_chain_render_pass while frame is not "
                "in progress!");
    VKAO_ASSERT(command_buffer == get_current_command_buffer(),
                "cannot begin render pass on command buffer from a "
                "different frame!");

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = vkao_swap_chain_->get_render_pass();
    render_pass_info.framebuffer =
        vkao_swap_chain_->get_frame_buffer(current_image_index_);
    render_pass_info.renderArea.offset = {0, 0};

    render_pass_info.renderArea.extent =
        vkao_swap_chain_->get_swap_chain_extent();
    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color = {0.f, 0.f, 0.f, 1.f};
    clear_values[1].depthStencil = {1.f, 0};

    render_pass_info.clearValueCount =
        static_cast<uint32_t>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(
        command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    {
        VkViewport viewport{};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width =
            static_cast<float>(vkao_swap_chain_->get_swap_chain_extent().width);
        viewport.height = static_cast<float>(
            vkao_swap_chain_->get_swap_chain_extent().height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vkao_swap_chain_->get_swap_chain_extent();
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    }
}

void VkaoRenderer::end_swap_chain_render_pass(VkCommandBuffer command_buffer) {
    VKAO_ASSERT(is_frame_started_,
                "cannot call end_swap_chain_render_pass while frame is not "
                "in progress!");
    VKAO_ASSERT(command_buffer == get_current_command_buffer(),
                "cannot end render pass on command buffer from a "
                "different frame!");

    vkCmdEndRenderPass(command_buffer);
}
} // namespace vkao