#pragma once

#include "ref.hpp"
#include "vkao_device.hpp"
#include "vulkan/vulkan_core.h"
#include <filesystem>
#include <vector>

namespace vkao {
struct PipelineConfigInfo {
    PipelineConfigInfo() = default;
    PipelineConfigInfo(const PipelineConfigInfo &) = delete;

    VkPipelineViewportStateCreateInfo viewport_info;
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info;
    VkPipelineRasterizationStateCreateInfo rasterization_info;
    VkPipelineMultisampleStateCreateInfo multisample_info;
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    VkPipelineColorBlendStateCreateInfo color_blend_info;
    VkPipelineDepthStencilStateCreateInfo depth_stencil_info;

    std::vector<VkDynamicState> dynamic_state_enables;
    VkPipelineDynamicStateCreateInfo dynamic_state_info;

    VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
    VkRenderPass render_pass{VK_NULL_HANDLE};
    uint32_t subpass{0};
};

class VkaoPipeline : public RefCounted {
  public:
    VkaoPipeline(VkaoDevice &device,
                 const std::filesystem::path &vert_filepath,
                 const std::filesystem::path &frag_filepath,
                 const PipelineConfigInfo &config_info);
    ~VkaoPipeline();

    VkaoPipeline(const VkaoPipeline &) = delete;

    void bind(VkCommandBuffer command_buffer);

    static void
    create_default_pipeline_config_info(PipelineConfigInfo &config_info);

  private:
    void create_graphics_pipeline(const std::filesystem::path &vert_filepath,
                                  const std::filesystem::path &frag_filepath,
                                  const PipelineConfigInfo &config_info);

    void create_shader_module(const std::vector<char> &code,
                              VkShaderModule *shader_module);

    VkaoDevice &vkao_device_;
    VkPipeline graphics_pipeline_;
    VkShaderModule vert_shader_module_;
    VkShaderModule frag_shader_module_;
};
} // namespace vkao