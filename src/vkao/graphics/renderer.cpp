#include "graphics/renderer.hpp"

#include "core/logger.hpp"
#include "graphics/swap_chain.hpp"
#include <array>
#include <vulkan/vulkan_core.h>

namespace vkao {
Renderer::Renderer(Window &window, Device &device)
    : window_{window}, device_{device} {
    recreate_swap_chain();
    create_command_buffers();
}

Renderer::~Renderer() {
    free_command_buffers();
}

void Renderer::create_command_buffers() {
    command_buffers_.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = device_.get_command_pool();
    alloc_info.commandBufferCount =
        static_cast<uint32_t>(command_buffers_.size());

    VKAO_ASSERT(vkAllocateCommandBuffers(device_.get_device(),
                                         &alloc_info,
                                         command_buffers_.data()) == VK_SUCCESS,
                "failed to allocate command buffers!");
}

void Renderer::free_command_buffers() {
    vkFreeCommandBuffers(device_.get_device(),
                         device_.get_command_pool(),
                         static_cast<uint32_t>(command_buffers_.size()),
                         command_buffers_.data());
}

void Renderer::recreate_swap_chain() {
    auto extent = window_.get_extent();
    while (extent.width == 0 || extent.height == 0) {
        SDL_Event event;
        SDL_WaitEvent(&event);

        if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            window_.on_window_resized(event.window.data1, event.window.data2);
        }
        extent = window_.get_extent();
    }

    vkDeviceWaitIdle(device_.get_device());

    auto old_swap_chain = swap_chain_;
    swap_chain_ = make_ref<SwapChain>(device_, extent, old_swap_chain);

    if (old_swap_chain) {
        VKAO_ASSERT(swap_chain_->compare_swap_formats(*old_swap_chain),
                    "swap chain image(or depth) format has changed!");
    }
}

RenderTarget Renderer::get_current_render_target() const {
    VKAO_ASSERT(is_frame_started_, "cannot get render target outside a frame!");

    return {
        swap_chain_->get_render_pass(),
        swap_chain_->get_frame_buffer(current_image_index_),
        swap_chain_->get_swap_chain_extent(),
    };
}

VkCommandBuffer Renderer::begin_frame() {
    VKAO_ASSERT(!is_frame_started_,
                "cannot call begin_frame while already in progress!");

    auto result = swap_chain_->acquire_next_image(&current_image_index_);

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

void Renderer::end_frame() {
    VKAO_ASSERT(is_frame_started_,
                "cannot call end_frame while frame is not in progress!");

    auto command_buffer = get_current_command_buffer();

    VKAO_ASSERT(vkEndCommandBuffer(command_buffer) == VK_SUCCESS,
                "failed to record command buffer!");

    auto result = swap_chain_->submit_command_buffers(&command_buffer,
                                                      &current_image_index_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        window_.was_window_resized()) {
        window_.reset_window_resized_flag();
        recreate_swap_chain();

    } else {
        VKAO_ASSERT(result == VK_SUCCESS,
                    "failed to present swap chain image!");
    }

    is_frame_started_ = false;

    current_frame_index_ =
        (current_frame_index_ + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
}
} // namespace vkao