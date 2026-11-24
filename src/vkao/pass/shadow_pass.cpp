#include "pass/shadow_pass.hpp"
#include "core/logger.hpp"
#include "graphics/shadow_config.hpp"
#include "scene/component.hpp"

namespace vkao {
namespace {
struct PushConstant {
    alignas(16) glm::mat4 model_matrix{1.f};
};
static_assert(sizeof(PushConstant) <= 128);
} // namespace

ShadowPass::ShadowPass(Device &device,
                       VkRenderPass render_pass,
                       std::vector<VkDescriptorSetLayout> set_layouts)
    : device_(device) {
    create_pipeline_layout(set_layouts);
    create_pipeline(render_pass);
}

ShadowPass::~ShadowPass() {
    vkDestroyPipelineLayout(device_.get_device(), pipeline_layout_, nullptr);
}

void ShadowPass::render(FrameInfo &frame_info, const ShadowDraw &draw) {
    uint32_t instance_count = 1;
    switch (draw.projection) {
    case ShadowProjection::DirectionalAtlas:
        instance_count = CASCADE_COUNT;
        break;
    case ShadowProjection::Spot:
        instance_count = 1;
        break;
    case ShadowProjection::PointCube:
        VKAO_ASSERT(false, "PointCube requires a layered shadow target");
        return;
    }

    begin(frame_info.command_buffer, draw.target, draw.projection);
    draw_scene(frame_info, draw.draw_set, instance_count);
    end(frame_info.command_buffer);
}

void ShadowPass::create_pipeline_layout(
    std::vector<VkDescriptorSetLayout> set_layouts) {
    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
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

void ShadowPass::create_pipeline(VkRenderPass render_pass) {
    VKAO_ASSERT(pipeline_layout_,
                "cannot create pipeline before pipeline layout!");

    PipelineConfigInfo config_info{};

    Pipeline::create_default_pipeline_config_info(config_info);
    config_info.attribute_description.resize(1); // 只读取 position

    config_info.shader_stages = {
        {VK_SHADER_STAGE_VERTEX_BIT, "shadow.vert.spv"},
    };

    config_info.color_blend_attachments.clear();

    config_info.depth_stencil_info.depthTestEnable = VK_TRUE;
    config_info.depth_stencil_info.depthWriteEnable = VK_TRUE;
    config_info.depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS;

    config_info.rasterization_info.depthBiasEnable = VK_TRUE;
    config_info.rasterization_info.depthBiasConstantFactor = 1.25f;
    config_info.rasterization_info.depthBiasSlopeFactor = 1.75f;

    config_info.pipeline_layout = pipeline_layout_;
    config_info.render_pass = render_pass;

    config_info.viewport_info.viewportCount = CASCADE_COUNT;
    config_info.viewport_info.scissorCount = CASCADE_COUNT;

    pipeline_ = make_ref<Pipeline>(device_, config_info);
}

void ShadowPass::begin(VkCommandBuffer command_buffer,
                       const RenderTarget &target,
                       ShadowProjection projection) {
    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = target.render_pass;
    render_pass_info.framebuffer = target.framebuffer;
    render_pass_info.renderArea.offset = {0, 0};

    render_pass_info.renderArea.extent = target.extent;
    VkClearValue clear_value{};
    clear_value.depthStencil = {1.f, 0};

    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(
        command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    std::array<VkViewport, CASCADE_COUNT> viewports{};
    std::array<VkRect2D, CASCADE_COUNT> scissors{};
    static_assert(CASCADE_COUNT == 4);

    const bool atlas = projection == ShadowProjection::DirectionalAtlas;
    const uint32_t width =
        atlas ? target.extent.width / 2 : target.extent.width;
    const uint32_t height =
        atlas ? target.extent.height / 2 : target.extent.height;

    for (uint32_t i = 0; i < CASCADE_COUNT; ++i) {
        const uint32_t x = atlas ? (i % 2) * width : 0;
        const uint32_t y = atlas ? (i / 2) * height : 0;

        viewports[i] = {
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(width),
            static_cast<float>(height),
            0.f,
            1.f,
        };

        scissors[i].offset = {static_cast<int32_t>(x), static_cast<int32_t>(y)};
        scissors[i].extent = {width, height};
    }

    vkCmdSetViewport(command_buffer, 0, CASCADE_COUNT, viewports.data());
    vkCmdSetScissor(command_buffer, 0, CASCADE_COUNT, scissors.data());
}

void ShadowPass::draw_scene(FrameInfo &frame_info,
                            VkDescriptorSet draw_set,
                            uint32_t instance_count) {
    auto command_buffer = frame_info.command_buffer;

    pipeline_->bind(frame_info.command_buffer);

    vkCmdBindDescriptorSets(command_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout_,
                            0,
                            1,
                            &draw_set,
                            0,
                            nullptr);

    for (auto [handle, tf, mesh] :
         frame_info.registry.view<TransformComponent, MeshComponent>().each()) {
        PushConstant push{};
        push.model_matrix = tf.model_matrix();

        vkCmdPushConstants(command_buffer,
                           pipeline_layout_,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(PushConstant),
                           &push);

        mesh.mesh->bind(command_buffer);
        mesh.mesh->draw(command_buffer, instance_count);
    }
}

void ShadowPass::end(VkCommandBuffer command_buffer) {
    vkCmdEndRenderPass(command_buffer);
}
} // namespace vkao