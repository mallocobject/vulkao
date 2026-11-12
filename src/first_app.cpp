#include "first_app.hpp"

#include "log.hpp"
#include "ref.hpp"
#include "vkao_model.hpp"
#include "vkao_pipeline.hpp"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cstdint>
#include <queue>
#include <vector>

namespace vkao {
FirstApp::FirstApp() {
    load_models();
    create_pipeline_layout();
    create_pipeline();
    create_command_buffers();
}

FirstApp::~FirstApp() {
    vkDestroyPipelineLayout(
        vkao_device_.get_device(), pipeline_layout_, nullptr);
}

void FirstApp::run() {
    while (running_) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running_ = false;
            }
        }

        draw_frame();
    }

    vkDeviceWaitIdle(vkao_device_.get_device());
}

void FirstApp::load_models() {
    std::queue<VkaoModel::Vertex> vertexes;
    vertexes.push({{0.f, -0.5f}});
    vertexes.push({{0.5f, 0.5f}});
    vertexes.push({{-0.5f, 0.5f}});

    auto dfs = [&](auto &&self, int depth) {
        if (depth > 5) {
            return;
        }

        auto size = vertexes.size();
        for (int i = 0; i < size; i += 3) {
            auto vertex0 = vertexes.front();
            vertexes.pop();
            auto vertex1 = vertexes.front();
            vertexes.pop();
            auto vertex2 = vertexes.front();
            vertexes.pop();

            vertexes.push(vertex0);
            vertexes.push({{0.5f * (vertex0.pos + vertex1.pos)}});
            vertexes.push({{0.5f * (vertex0.pos + vertex2.pos)}});
            vertexes.push({{0.5f * (vertex0.pos + vertex1.pos)}});
            vertexes.push(vertex1);
            vertexes.push({{0.5f * (vertex1.pos + vertex2.pos)}});
            vertexes.push({{0.5f * (vertex0.pos + vertex2.pos)}});
            vertexes.push({{0.5f * (vertex1.pos + vertex2.pos)}});
            vertexes.push(vertex2);
        }

        self(self, depth + 1);
    };

    dfs(dfs, 0);

    std::vector<VkaoModel::Vertex> vec;
    while (!vertexes.empty()) {
        vec.push_back(vertexes.front());
        vertexes.pop();
    }

    vkao_model_ = make_ref<VkaoModel>(vkao_device_, vec);
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
    PipelineConfigInfo config_info{};

    VkaoPipeline::create_default_pipeline_config_info(config_info);

    config_info.pipeline_layout = pipeline_layout_;
    config_info.render_pass = vkao_swap_chain_.get_render_pass();
    vkao_pipeline_ = make_ref<VkaoPipeline>(
        vkao_device_, "shader.vert.spv", "shader.frag.spv", config_info);
}
void FirstApp::create_command_buffers() {
    command_buffers_.resize(vkao_swap_chain_.image_count());

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
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        VKAO_ASSERT(vkBeginCommandBuffer(command_buffers_[i], &begin_info) ==
                        VK_SUCCESS,
                    "failed to begin recording command buffer!");

        VkRenderPassBeginInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = vkao_swap_chain_.get_render_pass();
        render_pass_info.framebuffer = vkao_swap_chain_.get_frame_buffer(i);
        render_pass_info.renderArea.offset = {0, 0};

        render_pass_info.renderArea.extent =
            vkao_swap_chain_.get_swap_chain_extent();
        std::array<VkClearValue, 2> clear_values{};
        clear_values[0].color = {0.f, 0.f, 0.f, 1.f};
        clear_values[1].depthStencil = {1.f, 0};

        render_pass_info.clearValueCount =
            static_cast<uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(
            command_buffers_[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkao_pipeline_->bind(command_buffers_[i]);

        {
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(
                vkao_swap_chain_.get_swap_chain_extent().width);
            viewport.height = static_cast<float>(
                vkao_swap_chain_.get_swap_chain_extent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(command_buffers_[i], 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = vkao_swap_chain_.get_swap_chain_extent();
            vkCmdSetScissor(command_buffers_[i], 0, 1, &scissor);
        }

        vkao_model_->bind(command_buffers_[i]);
        vkao_model_->draw(command_buffers_[i]);

        vkCmdEndRenderPass(command_buffers_[i]);

        VKAO_ASSERT(vkEndCommandBuffer(command_buffers_[i]) == VK_SUCCESS,
                    "failed to record command buffer!");
    }
}
void FirstApp::draw_frame() {
    uint32_t image_index;
    auto result = vkao_swap_chain_.acquire_next_image(&image_index);

    VKAO_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR,
                "failed to acquire swap chain image!");

    result = vkao_swap_chain_.submit_command_buffers(
        &command_buffers_[image_index], &image_index);

    VKAO_ASSERT(result == VK_SUCCESS, "failed to present swap chain image!");
}
} // namespace vkao