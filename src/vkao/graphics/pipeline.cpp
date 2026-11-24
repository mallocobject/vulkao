#include "graphics/pipeline.hpp"

#include "core/logger.hpp"
#include "graphics/mesh.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <fstream>
#include <vector>

namespace vkao {
namespace {
static std::vector<char> read_file(const std::filesystem::path &filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    VKAO_ASSERT(file.is_open(), "failed to open file!");

    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), file_size);

    file.close();

    return buffer;
}
} // namespace

Pipeline::Pipeline(Device &device, const PipelineConfigInfo &config_info)
    : device_{device} {
    create_graphics_pipeline(config_info);
}
vkao::Pipeline::~Pipeline() {
    for (auto &shader_module : shader_modules_) {
        vkDestroyShaderModule(device_.get_device(), shader_module, nullptr);
    }

    vkDestroyPipeline(device_.get_device(), graphics_pipeline_, nullptr);
}

void Pipeline::create_graphics_pipeline(const PipelineConfigInfo &config_info) {

    VKAO_ASSERT(config_info.pipeline_layout != VK_NULL_HANDLE,
                "cannot create graphics pipeline: no pipeline_layout provided "
                "on config_info");
    VKAO_ASSERT(config_info.render_pass != VK_NULL_HANDLE,
                "cannot create graphics pipeline: no render_pass provided "
                "on config_info");

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

    for (const auto &shader : config_info.shader_stages) {
        auto code = read_file(shader.filepath);

        VkShaderModule module{VK_NULL_HANDLE};
        create_shader_module(code, &module);
        shader_modules_.push_back(module);

        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = shader.stage;
        stage_info.module = module;
        stage_info.pName = "main";

        shader_stages.push_back(stage_info);
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount =
        static_cast<uint32_t>(config_info.binding_description.size());
    vertex_input_info.pVertexBindingDescriptions =
        config_info.binding_description.data();
    vertex_input_info.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(config_info.attribute_description.size());
    vertex_input_info.pVertexAttributeDescriptions =
        config_info.attribute_description.data();

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &config_info.input_assembly_info;
    pipeline_info.pViewportState = &config_info.viewport_info;
    pipeline_info.pRasterizationState = &config_info.rasterization_info;
    pipeline_info.pMultisampleState = &config_info.multisample_info;
    pipeline_info.pDepthStencilState =
        &config_info.depth_stencil_info; // Optional

    auto color_blend_info = config_info.color_blend_info;
    color_blend_info.attachmentCount =
        static_cast<uint32_t>(config_info.color_blend_attachments.size());
    color_blend_info.pAttachments =
        config_info.color_blend_attachments.empty()
            ? nullptr
            : config_info.color_blend_attachments.data();
    pipeline_info.pColorBlendState = &color_blend_info;

    pipeline_info.pDynamicState = &config_info.dynamic_state_info;

    pipeline_info.layout = config_info.pipeline_layout;
    pipeline_info.renderPass = config_info.render_pass;
    pipeline_info.subpass = config_info.subpass;

    pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipeline_info.basePipelineIndex = -1;              // Optional

    VKAO_ASSERT(vkCreateGraphicsPipelines(device_.get_device(),
                                          VK_NULL_HANDLE,
                                          1,
                                          &pipeline_info,
                                          nullptr,
                                          &graphics_pipeline_) == VK_SUCCESS,
                "failed to create graphics pipeline!");
}

void Pipeline::create_shader_module(const std::vector<char> &code,
                                    VkShaderModule *shader_module) {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VKAO_ASSERT(vkCreateShaderModule(device_.get_device(),
                                     &create_info,
                                     nullptr,
                                     shader_module) == VK_SUCCESS,
                "failed to create shader module!");
}

void Pipeline::bind(VkCommandBuffer command_buffer) {
    vkCmdBindPipeline(
        command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);
}

void Pipeline::create_default_pipeline_config_info(
    PipelineConfigInfo &config_info) {

    config_info.binding_description = Mesh::Vertex::get_binding_description();
    config_info.attribute_description =
        Mesh::Vertex::get_attribute_description();

    config_info.input_assembly_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    config_info.input_assembly_info.topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config_info.input_assembly_info.primitiveRestartEnable = VK_FALSE;

    config_info.viewport_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    config_info.viewport_info.viewportCount = 1;
    config_info.viewport_info.pViewports = nullptr;
    config_info.viewport_info.scissorCount = 1;
    config_info.viewport_info.pScissors = nullptr;

    config_info.rasterization_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    config_info.rasterization_info.depthClampEnable = VK_FALSE;
    config_info.rasterization_info.rasterizerDiscardEnable = VK_FALSE;
    config_info.rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
    config_info.rasterization_info.lineWidth = 1.f;
    config_info.rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
    config_info.rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config_info.rasterization_info.depthBiasEnable = VK_FALSE;
    config_info.rasterization_info.depthBiasConstantFactor = 0.f; // Optional
    config_info.rasterization_info.depthBiasClamp = 0.f;          // Optional
    config_info.rasterization_info.depthBiasSlopeFactor = 0.f;    // Optional

    config_info.multisample_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    config_info.multisample_info.sampleShadingEnable = VK_FALSE;
    config_info.multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    config_info.multisample_info.minSampleShading = 1.f;           // Optional
    config_info.multisample_info.pSampleMask = nullptr;            // Optional
    config_info.multisample_info.alphaToCoverageEnable = VK_FALSE; // Optional
    config_info.multisample_info.alphaToOneEnable = VK_FALSE;      // Optional

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor =
        VK_BLEND_FACTOR_ONE; // Optional
    color_blend_attachment.dstColorBlendFactor =
        VK_BLEND_FACTOR_ZERO;                              // Optional
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    color_blend_attachment.srcAlphaBlendFactor =
        VK_BLEND_FACTOR_ONE; // Optional
    color_blend_attachment.dstAlphaBlendFactor =
        VK_BLEND_FACTOR_ZERO;                              // Optional
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    config_info.color_blend_attachments.push_back(color_blend_attachment);

    config_info.color_blend_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    config_info.color_blend_info.logicOpEnable = VK_FALSE;
    config_info.color_blend_info.logicOp = VK_LOGIC_OP_COPY; // Optional
    config_info.color_blend_info.attachmentCount =
        static_cast<uint32_t>(config_info.color_blend_attachments.size());
    config_info.color_blend_info.pAttachments =
        config_info.color_blend_attachments.data();
    config_info.color_blend_info.blendConstants[0] = 0.0f; // Optional
    config_info.color_blend_info.blendConstants[1] = 0.0f; // Optional
    config_info.color_blend_info.blendConstants[2] = 0.0f; // Optional
    config_info.color_blend_info.blendConstants[3] = 0.0f; // Optional

    config_info.depth_stencil_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    config_info.depth_stencil_info.depthTestEnable = VK_TRUE;
    config_info.depth_stencil_info.depthWriteEnable = VK_TRUE;
    config_info.depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS;
    config_info.depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
    config_info.depth_stencil_info.minDepthBounds = 0.0f; // Optional
    config_info.depth_stencil_info.maxDepthBounds = 1.0f; // Optional
    config_info.depth_stencil_info.stencilTestEnable = VK_FALSE;
    config_info.depth_stencil_info.front = {}; // Optional
    config_info.depth_stencil_info.back = {};  // Optional

    config_info.dynamic_state_enables = {VK_DYNAMIC_STATE_VIEWPORT,
                                         VK_DYNAMIC_STATE_SCISSOR};
    config_info.dynamic_state_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    config_info.dynamic_state_info.pDynamicStates =
        config_info.dynamic_state_enables.data();
    config_info.dynamic_state_info.dynamicStateCount =
        static_cast<uint32_t>(config_info.dynamic_state_enables.size());
    config_info.dynamic_state_info.flags = 0;
}

} // namespace vkao