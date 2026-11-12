#include "first_app.hpp"

#include "log.hpp"
#include "ref.hpp"
#include "vkao_model.hpp"
#include "vkao_pipeline.hpp"
#include "vkao_swap_chain.hpp"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cstdint>
#include <queue>
#include <vector>

namespace vkao {
FirstApp::FirstApp() {
    load_models();
    create_pipeline_layout();
    recreate_swap_chain();
}

FirstApp::~FirstApp() {
    vkDestroyPipelineLayout(
        vkao_device_.get_device(), pipeline_layout_, nullptr);
}

void FirstApp::run() {
    while (running_) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                running_ = false;
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                vkao_window_.on_window_resized(event.window.data1,
                                               event.window.data2);
                break;
            }
        }

        draw_frame();
    }

    vkDeviceWaitIdle(vkao_device_.get_device());
}

void FirstApp::load_models() {
    // std::queue<VkaoModel::Vertex> vec;
    // vec.push({{0.f, -0.5f}});
    // vec.push({{0.5f, 0.5f}});
    // vec.push({{-0.5f, 0.5f}});

    // auto dfs = [&](auto &&self, int depth) {
    //     if (depth > 5) {
    //         return;
    //     }

    //     auto size = vec.size();
    //     for (int i = 0; i < size; i += 3) {
    //         auto vertex0 = vec.front();
    //         vec.pop();
    //         auto vertex1 = vec.front();
    //         vec.pop();
    //         auto vertex2 = vec.front();
    //         vec.pop();

    //         vec.push(vertex0);
    //         vec.push({{0.5f * (vertex0.pos + vertex1.pos)}});
    //         vec.push({{0.5f * (vertex0.pos + vertex2.pos)}});
    //         vec.push({{0.5f * (vertex0.pos + vertex1.pos)}});
    //         vec.push(vertex1);
    //         vec.push({{0.5f * (vertex1.pos + vertex2.pos)}});
    //         vec.push({{0.5f * (vertex0.pos + vertex2.pos)}});
    //         vec.push({{0.5f * (vertex1.pos + vertex2.pos)}});
    //         vec.push(vertex2);
    //     }

    //     self(self, depth + 1);
    // };

    // dfs(dfs, 0);

    // std::vector<VkaoModel::Vertex> vertexes;
    // while (!vec.empty()) {
    //     vertexes.push_back(vec.front());
    //     vec.pop();
    // }

    std::vector<VkaoModel::Vertex> vertexes{{
                                                {0.f, -0.5f, 0.f},
                                                {1.f, 0.f, 0.f},
                                            },
                                            {
                                                {0.5f, 0.5f, 0.f},
                                                {0.f, 1.f, 0.f},
                                            },
                                            {
                                                {-0.5f, 0.5f, 0.f},
                                                {0.f, 0.f, 1.f},
                                            }};

    vkao_model_ = make_ref<VkaoModel>(vkao_device_, vertexes);
}

void FirstApp::create_pipeline_layout() {
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;            // Optional
    pipeline_layout_info.pSetLayouts = nullptr;         // Optional
    pipeline_layout_info.pushConstantRangeCount = 0;    // Optional
    pipeline_layout_info.pPushConstantRanges = nullptr; // Optional

    VKAO_ASSERT(vkCreatePipelineLayout(vkao_device_.get_device(),
                                       &pipeline_layout_info,
                                       nullptr,
                                       &pipeline_layout_) == VK_SUCCESS,
                "failed to create pipeline layout!");
}

void FirstApp::create_pipeline() {
    VKAO_ASSERT(pipeline_layout_,
                "cannot create pipeline before pipeline layout!");
    VKAO_ASSERT(vkao_swap_chain_, "cannot create pipeline before swap chain!");

    PipelineConfigInfo config_info{};

    VkaoPipeline::create_default_pipeline_config_info(config_info);

    config_info.pipeline_layout = pipeline_layout_;
    config_info.render_pass = vkao_swap_chain_->get_render_pass();
    vkao_pipeline_ = make_ref<VkaoPipeline>(
        vkao_device_, "shader.vert.spv", "shader.frag.spv", config_info);
}
void FirstApp::create_command_buffers() {
    command_buffers_.resize(vkao_swap_chain_->image_count());

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

    for (int i = 0; i < command_buffers_.size(); i++) {
        record_command_buffer(i);
    }
}
void FirstApp::free_command_buffers() {
    vkFreeCommandBuffers(vkao_device_.get_device(),
                         vkao_device_.get_command_pool(),
                         static_cast<uint32_t>(command_buffers_.size()),
                         command_buffers_.data());
}

void FirstApp::draw_frame() {
    uint32_t image_index;
    auto result = vkao_swap_chain_->acquire_next_image(&image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swap_chain();
        return;
    }

    VKAO_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR,
                "failed to acquire swap chain image!");

    record_command_buffer(image_index);

    result = vkao_swap_chain_->submit_command_buffers(
        &command_buffers_[image_index], &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        vkao_window_.was_window_resized()) {
        vkao_window_.reset_window_resized_flag();
        recreate_swap_chain();
        return;
    }

    VKAO_ASSERT(result == VK_SUCCESS, "failed to present swap chain image!");
}

void FirstApp::recreate_swap_chain() {
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

    if (!vkao_pipeline_ || !old_swap_chain ||
        !vkao_swap_chain_->compare_swap_formats(*old_swap_chain)) {
        create_pipeline();
    }

    if (vkao_swap_chain_->image_count() != command_buffers_.size()) {
        if (!command_buffers_.empty()) {
            free_command_buffers();
        }

        create_command_buffers();
    }
}

void FirstApp::record_command_buffer(int image_index) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VKAO_ASSERT(vkBeginCommandBuffer(command_buffers_[image_index],
                                     &begin_info) == VK_SUCCESS,
                "failed to begin recording command buffer!");

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = vkao_swap_chain_->get_render_pass();
    render_pass_info.framebuffer =
        vkao_swap_chain_->get_frame_buffer(image_index);
    render_pass_info.renderArea.offset = {0, 0};

    render_pass_info.renderArea.extent =
        vkao_swap_chain_->get_swap_chain_extent();
    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color = {0.f, 0.f, 0.f, 1.f};
    clear_values[1].depthStencil = {1.f, 0};

    render_pass_info.clearValueCount =
        static_cast<uint32_t>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(command_buffers_[image_index],
                         &render_pass_info,
                         VK_SUBPASS_CONTENTS_INLINE);

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
        vkCmdSetViewport(command_buffers_[image_index], 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vkao_swap_chain_->get_swap_chain_extent();
        vkCmdSetScissor(command_buffers_[image_index], 0, 1, &scissor);
    }

    vkao_pipeline_->bind(command_buffers_[image_index]);

    vkao_model_->bind(command_buffers_[image_index]);
    vkao_model_->draw(command_buffers_[image_index]);

    vkCmdEndRenderPass(command_buffers_[image_index]);

    VKAO_ASSERT(vkEndCommandBuffer(command_buffers_[image_index]) == VK_SUCCESS,
                "failed to record command buffer!");
}
} // namespace vkao