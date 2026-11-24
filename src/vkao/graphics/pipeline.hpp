#pragma once

#include "core/ref.hpp"
#include "graphics/device.hpp"
#include <filesystem>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkao {
struct ShaderStageConfig {
    VkShaderStageFlagBits stage;
    std::filesystem::path filepath;
};

struct PipelineConfigInfo {
    PipelineConfigInfo() = default;
    PipelineConfigInfo(const PipelineConfigInfo &) = delete;

    std::vector<ShaderStageConfig> shader_stages;

    std::vector<VkVertexInputBindingDescription> binding_description;
    std::vector<VkVertexInputAttributeDescription> attribute_description;

    VkPipelineViewportStateCreateInfo viewport_info;
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info;
    VkPipelineRasterizationStateCreateInfo rasterization_info;
    VkPipelineMultisampleStateCreateInfo multisample_info;
    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments;
    VkPipelineColorBlendStateCreateInfo color_blend_info;
    VkPipelineDepthStencilStateCreateInfo depth_stencil_info;

    std::vector<VkDynamicState> dynamic_state_enables;
    VkPipelineDynamicStateCreateInfo dynamic_state_info;

    VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
    VkRenderPass render_pass{VK_NULL_HANDLE};
    uint32_t subpass{0};
};

class Pipeline : public RefCounted {
  public:
    Pipeline(Device &device, const PipelineConfigInfo &config_info);
    ~Pipeline();

    Pipeline(const Pipeline &) = delete;

    void bind(VkCommandBuffer command_buffer);

    static void
    create_default_pipeline_config_info(PipelineConfigInfo &config_info);

  private:
    void create_graphics_pipeline(const PipelineConfigInfo &config_info);

    void create_shader_module(const std::vector<char> &code,
                              VkShaderModule *shader_module);

    Device &device_;
    VkPipeline graphics_pipeline_;
    std::vector<VkShaderModule> shader_modules_;
};
} // namespace vkao