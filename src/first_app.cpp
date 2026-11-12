#include "first_app.hpp"

#include "logger.hpp"
#include "ref.hpp"
#include "vkao_model.hpp"
#include "vkao_pipeline.hpp"
#include "vulkan/vulkan_core.h"
#include <vector>

namespace vkao {
FirstApp::FirstApp() {
    load_models();
    create_pipeline_layout();
    create_pipeline();
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

        if (auto command_buffer = vkao_renderer_.begin_frame()) {
            vkao_renderer_.begin_swap_chain_render_pass(command_buffer);
            // TODO
            draw(command_buffer);
            vkao_renderer_.end_swap_chain_render_pass(command_buffer);
            vkao_renderer_.end_frame();
        }
    }

    vkDeviceWaitIdle(vkao_device_.get_device());
}

void FirstApp::load_models() {
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
    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(SimplePushConstant);

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;    // Optional
    pipeline_layout_info.pSetLayouts = nullptr; // Optional
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    VKAO_ASSERT(vkCreatePipelineLayout(vkao_device_.get_device(),
                                       &pipeline_layout_info,
                                       nullptr,
                                       &pipeline_layout_) == VK_SUCCESS,
                "failed to create pipeline layout!");
}

void FirstApp::create_pipeline() {
    VKAO_ASSERT(pipeline_layout_,
                "cannot create pipeline before pipeline layout!");

    PipelineConfigInfo config_info{};

    VkaoPipeline::create_default_pipeline_config_info(config_info);

    config_info.pipeline_layout = pipeline_layout_;
    config_info.render_pass = vkao_renderer_.get_swap_chain_render_pass();
    vkao_pipeline_ = make_ref<VkaoPipeline>(
        vkao_device_, "shader.vert.spv", "shader.frag.spv", config_info);
}

void FirstApp::draw(VkCommandBuffer command_buffer) {
    vkao_pipeline_->bind(command_buffer);
    vkao_model_->bind(command_buffer);

    for (int i = 0; i < 4; i++) {
        SimplePushConstant push{};
        push.offset = glm::vec3{0.f, -0.2f + 0.2f * i, 0.f};
        push.color = glm::vec3{0.f, 0.f, 0.2f + 0.2f * i};

        vkCmdPushConstants(command_buffer,
                           pipeline_layout_,
                           VK_SHADER_STAGE_VERTEX_BIT |
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(SimplePushConstant),
                           &push);

        vkao_model_->draw(command_buffer);
    }
}

} // namespace vkao