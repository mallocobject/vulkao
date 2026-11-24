#include "graphics/descriptor.hpp"
#include "core/logger.hpp"
#include "core/ref.hpp"
#include "descriptor.hpp"
#include <cstdint>

namespace vkao {
DescriptorSetLayout::Builder &
DescriptorSetLayout::Builder::add_binding(uint32_t binding,
                                          VkDescriptorType descriptor_type,
                                          VkShaderStageFlags stage_flags,
                                          uint32_t count) {
    VKAO_ASSERT(!bindings_.contains(binding), "Binding already in use");
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = descriptor_type;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stage_flags;
    bindings_[binding] = layoutBinding;

    return *this;
}

Ref<DescriptorSetLayout> DescriptorSetLayout::Builder::build() const {
    return make_ref<DescriptorSetLayout>(device_, bindings_);
}

DescriptorSetLayout::DescriptorSetLayout(
    Device &device,
    const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> &bindings)
    : device_(device), bindings_(bindings) {
    std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings;
    for (auto &[k, v] : bindings_) {
        set_layout_bindings.push_back(v);
    }

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info{};
    descriptor_set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_info.bindingCount =
        static_cast<uint32_t>(set_layout_bindings.size());
    descriptor_set_layout_info.pBindings = set_layout_bindings.data();

    VKAO_ASSERT(vkCreateDescriptorSetLayout(device_.get_device(),
                                            &descriptor_set_layout_info,
                                            nullptr,
                                            &descriptor_set_layout_) ==
                    VK_SUCCESS,
                "failed to create descriptor set layout!");
}

DescriptorSetLayout::~DescriptorSetLayout() {
    vkDestroyDescriptorSetLayout(
        device_.get_device(), descriptor_set_layout_, nullptr);
}

DescriptorPool::Builder &
DescriptorPool::Builder::add_pool_size(VkDescriptorType descriptor_type,
                                       uint32_t count) {
    pool_sizes_.emplace_back(descriptor_type, count);
    return *this;
}

DescriptorPool::Builder &
DescriptorPool::Builder::set_pool_flags(VkDescriptorPoolCreateFlags flags) {
    pool_flags_ = flags;
    return *this;
}

DescriptorPool::Builder &DescriptorPool::Builder::set_max_sets(uint32_t count) {
    max_sets_ = count;
    return *this;
}

Ref<DescriptorPool> DescriptorPool::Builder::build() const {
    return make_ref<DescriptorPool>(
        device_, max_sets_, pool_flags_, pool_sizes_);
}

DescriptorPool::DescriptorPool(
    Device &device,
    uint32_t max_sets,
    VkDescriptorPoolCreateFlags pool_flags,
    const std::vector<VkDescriptorPoolSize> &pool_sizes)
    : device_(device) {
    VkDescriptorPoolCreateInfo descriptor_pool_info{};
    descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.poolSizeCount =
        static_cast<uint32_t>(pool_sizes.size());
    descriptor_pool_info.pPoolSizes = pool_sizes.data();
    descriptor_pool_info.maxSets = max_sets;
    descriptor_pool_info.flags = pool_flags;

    VKAO_ASSERT(vkCreateDescriptorPool(device_.get_device(),
                                       &descriptor_pool_info,
                                       nullptr,
                                       &descriptor_pool_) == VK_SUCCESS,
                "failed to create descriptor pool!");
}

DescriptorPool::~DescriptorPool() {
    vkDestroyDescriptorPool(device_.get_device(), descriptor_pool_, nullptr);
}

bool DescriptorPool::allocate_descriptor_set(
    const VkDescriptorSetLayout descriptor_set_layout,
    VkDescriptorSet &descriptor) const {
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.pSetLayouts = &descriptor_set_layout;
    alloc_info.descriptorSetCount = 1;

    // Might want to create a "DescriptorPoolManager" class that handles this
    // case, and builds a new pool whenever an old pool fills up. But this is
    // beyond our current scope

    return vkAllocateDescriptorSets(
               device_.get_device(), &alloc_info, &descriptor) == VK_SUCCESS;
}

void DescriptorPool::free_descriptor_sets(
    std::vector<VkDescriptorSet> &descriptors) const {
    vkFreeDescriptorSets(device_.get_device(),
                         descriptor_pool_,
                         static_cast<uint32_t>(descriptors.size()),
                         descriptors.data());
}

void DescriptorPool::reset_pool() {
    vkResetDescriptorPool(device_.get_device(), descriptor_pool_, 0);
}

DescriptorWriter &
DescriptorWriter::write_buffer(uint32_t binding,
                               const VkDescriptorBufferInfo *buffer_info) {
    VKAO_ASSERT(set_layout_.bindings_.contains(binding),
                "Layout does not contain specified binding");

    auto &binding_description = set_layout_.bindings_[binding];

    VKAO_ASSERT(binding_description.descriptorCount == 1,
                "Binding single descriptor info, but binding expects multiple");

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = binding_description.descriptorType;
    write.dstBinding = binding;
    write.pBufferInfo = buffer_info;
    write.descriptorCount = 1;

    writes_.push_back(write);
    return *this;
}

DescriptorWriter &DescriptorWriter::write_image(
    uint32_t binding, const VkDescriptorImageInfo *image_info, uint32_t count) {
    VKAO_ASSERT(set_layout_.bindings_.contains(binding),
                "Layout does not contain specified binding");

    auto &binding_description = set_layout_.bindings_[binding];

    VKAO_ASSERT(binding_description.descriptorCount == count,
                "Binding single descriptor info, but binding expects multiple");

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = binding_description.descriptorType;
    write.dstBinding = binding;
    write.pImageInfo = image_info;
    write.descriptorCount = count;

    writes_.push_back(write);
    return *this;
}

bool DescriptorWriter::build(VkDescriptorSet &set) {
    if (!pool_.allocate_descriptor_set(set_layout_.get_descriptor_set_layout(),
                                       set)) {
        return false;
    }
    overwrite(set);
    return true;
}

void DescriptorWriter::overwrite(VkDescriptorSet &set) {
    for (auto &write : writes_) {
        write.dstSet = set;
    }
    vkUpdateDescriptorSets(pool_.device_.get_device(),
                           static_cast<uint32_t>(writes_.size()),
                           writes_.data(),
                           0,
                           nullptr);
}
} // namespace vkao