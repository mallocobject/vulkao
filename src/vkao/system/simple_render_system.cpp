#include "system/simple_render_system.hpp"

#include "core/logger.hpp"
#include "core/ref.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/gtc/constants.hpp"
#include "graphics/pipeline.hpp"
#include "scene/component.hpp"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {
struct SimplePushConstant {
    alignas(16) glm::mat4 model_matrix{1.f};
    alignas(16) glm::mat4 normal_matrix{1.f};
};
static_assert(sizeof(SimplePushConstant) == 128);

SimpleRenderSystem::SimpleRenderSystem(Device &device,
                                       VkRenderPass render_pass,
                                       VkDescriptorSetLayout set_layout)
    : device_(device) {
    create_pipeline_layout(set_layout);
    create_pipeline(render_pass);
}

SimpleRenderSystem::~SimpleRenderSystem() {
    vkDestroyPipelineLayout(device_.get_device(), pipeline_layout_, nullptr);
}

void SimpleRenderSystem::create_pipeline_layout(
    VkDescriptorSetLayout set_layout) {
    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(SimplePushConstant);

    std::vector<VkDescriptorSetLayout> descriptor_set_layouts{set_layout};

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount =
        static_cast<uint32_t>(descriptor_set_layouts.size());
    pipeline_layout_info.pSetLayouts = descriptor_set_layouts.data();
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

void SimpleRenderSystem::render(FrameInfo &frame_info) {
    pipeline_->bind(frame_info.command_buffer);

    vkCmdBindDescriptorSets(frame_info.command_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout_,
                            0,
                            1,
                            &frame_info.descriptor_set,
                            0,
                            nullptr);

    for (auto [handle, tf, mesh] :
         frame_info.registry.view<TransformComponent, MeshComponent>().each()) {
        SimplePushConstant push{};
        push.model_matrix = tf.model_matrix();
        push.normal_matrix = glm::mat4{tf.normal_matrix()};

        vkCmdPushConstants(frame_info.command_buffer,
                           pipeline_layout_,
                           VK_SHADER_STAGE_VERTEX_BIT |
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(SimplePushConstant),
                           &push);

        mesh.mesh->bind(frame_info.command_buffer);
        mesh.mesh->draw(frame_info.command_buffer);
    }
}

} // namespace vkao