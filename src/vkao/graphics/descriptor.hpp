#pragma once

#include "core/ref.hpp"
#include "graphics/device.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace vkao {
class DescriptorSetLayout : public RefCounted {
    friend class DescriptorWriter;

  public:
    class Builder {
      public:
        Builder(Device &device) : device_(device) {
        }

        Builder &add_binding(uint32_t binding,
                             VkDescriptorType descriptor_type,
                             VkShaderStageFlags stage_flags,
                             uint32_t count = 1);

        Ref<DescriptorSetLayout> build() const;

      private:
        Device &device_;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings_;
    };

    DescriptorSetLayout(
        Device &device,
        const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>
            &bindings);
    ~DescriptorSetLayout();
    DescriptorSetLayout(const DescriptorSetLayout &) = delete;

    VkDescriptorSetLayout get_descriptor_set_layout() const {
        return descriptor_set_layout_;
    }

  private:
    Device &device_;
    VkDescriptorSetLayout descriptor_set_layout_;
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings_;
};

class DescriptorPool : public RefCounted {
    friend class DescriptorWriter;

  public:
    class Builder {
      public:
        Builder(Device &device) : device_(device) {
        }

        Builder &add_pool_size(VkDescriptorType descriptor_type,
                               uint32_t count);

        Builder &set_pool_flags(VkDescriptorPoolCreateFlags flags);

        Builder &set_max_sets(uint32_t count);

        Ref<DescriptorPool> build() const;

      private:
        Device &device_;
        std::vector<VkDescriptorPoolSize> pool_sizes_;
        uint32_t max_sets_{1000};
        VkDescriptorPoolCreateFlags pool_flags_{0};
    };

    DescriptorPool(Device &device,
                   uint32_t max_sets,
                   VkDescriptorPoolCreateFlags pool_flags,
                   const std::vector<VkDescriptorPoolSize> &pool_sizes);
    ~DescriptorPool();
    DescriptorPool(const DescriptorPool &) = delete;

    bool
    allocate_descriptor_set(const VkDescriptorSetLayout descriptor_set_layout,
                            VkDescriptorSet &descriptor) const;

    void free_descriptor_sets(std::vector<VkDescriptorSet> &descriptors) const;

    void reset_pool();

  private:
    Device &device_;
    VkDescriptorPool descriptor_pool_;
};

class DescriptorWriter {
  public:
    DescriptorWriter(DescriptorSetLayout &set_layout, DescriptorPool &pool)
        : set_layout_(set_layout), pool_(pool) {
    }

    DescriptorWriter &write_buffer(uint32_t binding,
                                   const VkDescriptorBufferInfo *buffer_info);

    DescriptorWriter &write_image(uint32_t binding,
                                  const VkDescriptorImageInfo *image_info,
                                  uint32_t count = 1);

    bool build(VkDescriptorSet &set);
    void overwrite(VkDescriptorSet &set);

  private:
    DescriptorSetLayout &set_layout_;
    DescriptorPool &pool_;
    std::vector<VkWriteDescriptorSet> writes_;
};
} // namespace vkao