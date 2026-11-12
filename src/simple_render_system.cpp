#include "simple_render_system.hpp"

#include "glm/gtc/constants.hpp"
#include "logger.hpp"
#include "ref.hpp"
#include "vkao_component.hpp"
#include "vkao_pipeline.hpp"
#include "vulkan/vulkan_core.h"
#include <vector>

namespace vkao {
SimpleRenderSystem::SimpleRenderSystem(VkaoDevice &device,
                                       VkRenderPass render_pass)
    : vkao_device_(device) {
    create_pipeline_layout();
    create_pipeline(render_pass);
}

SimpleRenderSystem::~SimpleRenderSystem() {
    vkDestroyPipelineLayout(
        vkao_device_.get_device(), pipeline_layout_, nullptr);
}

void SimpleRenderSystem::create_pipeline_layout() {
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

void SimpleRenderSystem::create_pipeline(VkRenderPass render_pass) {
    VKAO_ASSERT(pipeline_layout_,
                "cannot create pipeline before pipeline layout!");

    PipelineConfigInfo config_info{};

    VkaoPipeline::create_default_pipeline_config_info(config_info);

    config_info.pipeline_layout = pipeline_layout_;
    config_info.render_pass = render_pass;
    vkao_pipeline_ = make_ref<VkaoPipeline>(
        vkao_device_, "shader.vert.spv", "shader.frag.spv", config_info);
}

void SimpleRenderSystem::draw(VkCommandBuffer command_buffer, Entity entity) {
    vkao_pipeline_->bind(command_buffer);

    auto &mesh = entity.get_component<MeshComponent>().mesh;

    mesh->bind(command_buffer);

    auto &transform = entity.get_component<TransformComponent>();
    transform.scale = {0.55f, 0.55f, 0.55f};

    static int cnt = 0;
    transform.rotation = {glm::mod(cnt * 0.001f, glm::two_pi<float>()),
                          glm::mod(cnt * 0.0005f, glm::two_pi<float>()),
                          0.f};
    cnt++;

    transform.translation = {0.f, 0.f, 0.5f};

    SimplePushConstant push{};
    push.transform = entity.get_component<TransformComponent>().load_matrix();

    vkCmdPushConstants(command_buffer,
                       pipeline_layout_,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(SimplePushConstant),
                       &push);

    mesh->draw(command_buffer);
}

} // namespace vkao