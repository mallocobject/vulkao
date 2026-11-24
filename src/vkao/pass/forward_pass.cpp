#include "pass/forward_pass.hpp"
#include "core/logger.hpp"
#include "scene/component.hpp"

namespace vkao {
namespace {
struct PushConstant {
    alignas(16) glm::mat4 model_matrix{1.f};
    alignas(16) glm::vec4 normal_columns[3]{};
    alignas(4) uint32_t texture_slot{};
};

static_assert(offsetof(PushConstant, texture_slot) == 112);
static_assert(sizeof(PushConstant) <= 128);
} // namespace

ForwardPass::ForwardPass(Device &device,
                         VkRenderPass render_pass,
                         std::vector<VkDescriptorSetLayout> set_layouts)
    : device_(device) {
    create_pipeline_layout(set_layouts);
    create_pipeline(render_pass);
}

ForwardPass::~ForwardPass() {
    vkDestroyPipelineLayout(device_.get_device(), pipeline_layout_, nullptr);
}

void ForwardPass::render(FrameInfo &frame_info, const RenderTarget &target) {
    begin(frame_info.command_buffer, target);
    draw_scene(frame_info);
    end(frame_info.command_buffer);
}

void ForwardPass::create_pipeline_layout(
    std::vector<VkDescriptorSetLayout> set_layouts) {
    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(PushConstant);

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount =
        static_cast<uint32_t>(set_layouts.size());
    pipeline_layout_info.pSetLayouts = set_layouts.data();
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    VKAO_ASSERT(vkCreatePipelineLayout(device_.get_device(),
                                       &pipeline_layout_info,
                                       nullptr,
                                       &pipeline_layout_) == VK_SUCCESS,
                "failed to create pipeline layout!");
}

void ForwardPass::create_pipeline(VkRenderPass render_pass) {
    VKAO_ASSERT(pipeline_layout_,
                "cannot create pipeline before pipeline layout!");

    PipelineConfigInfo config_info{};

    Pipeline::create_default_pipeline_config_info(config_info);

    config_info.shader_stages = {
        {VK_SHADER_STAGE_VERTEX_BIT, "shader.vert.spv"},
        {VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag.spv"},
    };
    config_info.pipeline_layout = pipeline_layout_;
    config_info.render_pass = render_pass;
    pipeline_ = make_ref<Pipeline>(device_, config_info);
}

void ForwardPass::begin(VkCommandBuffer command_buffer,
                        const RenderTarget &target) {

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = target.render_pass;
    render_pass_info.framebuffer = target.framebuffer;
    render_pass_info.renderArea.offset = {0, 0};

    render_pass_info.renderArea.extent = target.extent;
    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color = {{0.f, 0.f, 0.f, 1.f}};
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
        viewport.width = static_cast<float>(target.extent.width);
        viewport.height = static_cast<float>(target.extent.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = target.extent;
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    }
}

void ForwardPass::draw_scene(FrameInfo &frame_info) {
    auto command_buffer = frame_info.command_buffer;

    pipeline_->bind(frame_info.command_buffer);

    for (int i = 0; i < frame_info.descriptor_sets.size(); i++) {
        vkCmdBindDescriptorSets(command_buffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout_,
                                i,
                                1,
                                &frame_info.descriptor_sets[i],
                                0,
                                nullptr);
    }

    for (auto [handle, tf, mesh, material] :
         frame_info.registry
             .view<TransformComponent, MeshComponent, MaterialComponent>()
             .each()) {

        PushConstant push{};
        push.model_matrix = tf.model_matrix();

        const auto normal_matrix = tf.normal_matrix();
        for (int i = 0; i < 3; ++i) {
            push.normal_columns[i] = glm::vec4{normal_matrix[i], 0.f};
        }

        push.texture_slot = material.texture_slot;

        vkCmdPushConstants(command_buffer,
                           pipeline_layout_,
                           VK_SHADER_STAGE_VERTEX_BIT |
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(PushConstant),
                           &push);

        mesh.mesh->bind(command_buffer);
        mesh.mesh->draw(command_buffer);
    }
}

void ForwardPass::end(VkCommandBuffer command_buffer) {
    vkCmdEndRenderPass(command_buffer);
}
} // namespace vkao