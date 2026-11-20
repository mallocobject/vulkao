#include "graphics/simple_render_system.hpp"

#include "core/logger.hpp"
#include "core/ref.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/gtc/constants.hpp"
#include "graphics/pipeline.hpp"
#include "scene/component.hpp"
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {
SimpleRenderSystem::SimpleRenderSystem(Device &device, VkRenderPass render_pass)
    : device_(device) {
    create_pipeline_layout();
    create_pipeline(render_pass);
}

SimpleRenderSystem::~SimpleRenderSystem() {
    vkDestroyPipelineLayout(device_.get_device(), pipeline_layout_, nullptr);
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

    VKAO_ASSERT(vkCreatePipelineLayout(device_.get_device(),
                                       &pipeline_layout_info,
                                       nullptr,
                                       &pipeline_layout_) == VK_SUCCESS,
                "failed to create pipeline layout!");
}

void SimpleRenderSystem::create_pipeline(VkRenderPass render_pass) {
    VKAO_ASSERT(pipeline_layout_,
                "cannot create pipeline before pipeline layout!");

    PipelineConfigInfo config_info{};

    Pipeline::create_default_pipeline_config_info(config_info);

    config_info.pipeline_layout = pipeline_layout_;
    config_info.render_pass = render_pass;
    pipeline_ = make_ref<Pipeline>(
        device_, "shader.vert.spv", "shader.frag.spv", config_info);
}

void SimpleRenderSystem::draw(VkCommandBuffer command_buffer,
                              Entity entity,
                              const Camera &camera) {
    pipeline_->bind(command_buffer);

    auto proj_view = camera.get_projection() * camera.get_view();

    auto &tf = entity.get_component<TransformComponent>();
    SimplePushConstant push{};
    push.transform = proj_view * tf.model_matrix();
    push.normal_matrix = glm::mat4{tf.normal_matrix()};

    vkCmdPushConstants(command_buffer,
                       pipeline_layout_,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(SimplePushConstant),
                       &push);

    auto &mesh = entity.get_component<MeshComponent>().mesh;

    mesh->bind(command_buffer);
    mesh->draw(command_buffer);
}

} // namespace vkao