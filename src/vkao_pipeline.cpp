#include "vkao_pipeline.hpp"

#include "logger.hpp"
#include "vkao_model.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <fstream>
#include <vector>

namespace vkao {
namespace {
static std::vector<char> read_file(const std::filesystem::path &filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), file_size);

    file.close();

    return buffer;
}
} // namespace

VkaoPipeline::VkaoPipeline(VkaoDevice &device,
                           const std::filesystem::path &vert_filepath,
                           const std::filesystem::path &frag_filepath,
                           const PipelineConfigInfo &config_info)
    : vkao_device_{device} {
    create_graphics_pipeline(vert_filepath, frag_filepath, config_info);
}
vkao::VkaoPipeline::~VkaoPipeline() {
    vkDestroyShaderModule(
        vkao_device_.get_device(), frag_shader_module_, nullptr);
    vkDestroyShaderModule(
        vkao_device_.get_device(), vert_shader_module_, nullptr);

    vkDestroyPipeline(vkao_device_.get_device(), graphics_pipeline_, nullptr);
}

void VkaoPipeline::create_graphics_pipeline(
    const std::filesystem::path &vert_filepath,
    const std::filesystem::path &frag_filepath,
    const PipelineConfigInfo &config_info) {

    VKAO_ASSERT(config_info.pipeline_layout != VK_NULL_HANDLE,
                "cannot create graphics pipeline: no pipeline_layout provided "
                "on config_info");
    VKAO_ASSERT(config_info.render_pass != VK_NULL_HANDLE,
                "cannot create graphics pipeline: no render_pass provided "
                "on config_info");

    auto vert_code = read_file(vert_filepath);
    auto frag_code = read_file(frag_filepath);

    create_shader_module(vert_code, &vert_shader_module_);
    create_shader_module(frag_code, &frag_shader_module_);

    VkPipelineShaderStageCreateInfo shader_stage_infos[2]{};
    shader_stage_infos[0].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stage_infos[0].module = vert_shader_module_;
    shader_stage_infos[0].pName = "main";

    shader_stage_infos[1].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stage_infos[1].module = frag_shader_module_;
    shader_stage_infos[1].pName = "main";

    auto binding_descriptions = VkaoModel::Vertex::get_binding_description();
    auto attribute_descriptions =
        VkaoModel::Vertex::get_attribute_description();
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount =
        static_cast<uint32_t>(binding_descriptions.size());
    vertex_input_info.pVertexBindingDescriptions = binding_descriptions.data();
    vertex_input_info.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions =
        attribute_descriptions.data();

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stage_infos;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &config_info.input_assembly_info;
    pipeline_info.pViewportState = &config_info.viewport_info;
    pipeline_info.pRasterizationState = &config_info.rasterization_info;
    pipeline_info.pMultisampleState = &config_info.multisample_info;
    pipeline_info.pDepthStencilState =
        &config_info.depth_stencil_info; // Optional
    pipeline_info.pColorBlendState = &config_info.color_blend_info;
    pipeline_info.pDynamicState = &config_info.dynamic_state_info;

    pipeline_info.layout = config_info.pipeline_layout;
    pipeline_info.renderPass = config_info.render_pass;
    pipeline_info.subpass = config_info.subpass;

    pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipeline_info.basePipelineIndex = -1;              // Optional

    VKAO_ASSERT(vkCreateGraphicsPipelines(vkao_device_.get_device(),
                                          VK_NULL_HANDLE,
                                          1,
                                          &pipeline_info,
                                          nullptr,
                                          &graphics_pipeline_) == VK_SUCCESS,
                "failed to create graphics pipeline!");
}

void VkaoPipeline::create_shader_module(const std::vector<char> &code,
                                        VkShaderModule *shader_module) {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VKAO_ASSERT(vkCreateShaderModule(vkao_device_.get_device(),
                                     &create_info,
                                     nullptr,
                                     shader_module) == VK_SUCCESS,
                "failed to create shader module!");
}

void VkaoPipeline::bind(VkCommandBuffer command_buffer) {
    vkCmdBindPipeline(
        command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);
}

void VkaoPipeline::create_default_pipeline_config_info(
    PipelineConfigInfo &config_info) {

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
    config_info.rasterization_info.cullMode = VK_CULL_MODE_NONE;
    config_info.rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

    config_info.color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    config_info.color_blend_attachment.blendEnable = VK_FALSE;
    config_info.color_blend_attachment.srcColorBlendFactor =
        VK_BLEND_FACTOR_ONE; // Optional
    config_info.color_blend_attachment.dstColorBlendFactor =
        VK_BLEND_FACTOR_ZERO; // Optional
    config_info.color_blend_attachment.colorBlendOp =
        VK_BLEND_OP_ADD; // Optional
    config_info.color_blend_attachment.srcAlphaBlendFactor =
        VK_BLEND_FACTOR_ONE; // Optional
    config_info.color_blend_attachment.dstAlphaBlendFactor =
        VK_BLEND_FACTOR_ZERO; // Optional
    config_info.color_blend_attachment.alphaBlendOp =
        VK_BLEND_OP_ADD; // Optional

    config_info.color_blend_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    config_info.color_blend_info.logicOpEnable = VK_FALSE;
    config_info.color_blend_info.logicOp = VK_LOGIC_OP_COPY; // Optional
    config_info.color_blend_info.attachmentCount = 1;
    config_info.color_blend_info.pAttachments =
        &config_info.color_blend_attachment;
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