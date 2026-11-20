/*
 * Encapsulates a vulkan buffer
 *
 * Initially based off VulkanBuffer by Sascha Willems -
 * https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanBuffer.h
 */

#include "graphics/buffer.hpp"
#include "core/logger.hpp"

// std
#include <cassert>
#include <cstring>

namespace vkao {
namespace {
/**
 * Returns the minimum instance size required to be compatible with
 * devices min_offset_alignment
 *
 * @param instance_size The size of an instance
 * @param min_offset_alignment The minimum required alignment, in
 * bytes, for the offset member (eg minUniformBufferOffsetAlignment)
 *
 * @return VkResult of the buffer mapping call
 */
VkDeviceSize get_alignment(VkDeviceSize instance_size,
                           VkDeviceSize min_offset_alignment) {
    if (min_offset_alignment > 0) {
        return (instance_size + min_offset_alignment - 1) &
               ~(min_offset_alignment - 1);
    }
    return instance_size;
}
} // namespace

Buffer::Buffer(Device &device,
               VkDeviceSize instance_size,
               uint32_t instance_count,
               VkBufferUsageFlags usage_flags,
               VkMemoryPropertyFlags memory_property_flags,
               VkDeviceSize min_offset_alignment)
    : device_{device}, instance_count_{instance_count},
      instance_size_{instance_size}, usage_flags_{usage_flags},
      memory_property_flags_{memory_property_flags} {
    alignment_size_ = get_alignment(instance_size, min_offset_alignment);
    buffer_size_ = alignment_size_ * instance_count;
    device_.create_buffer(
        buffer_size_, usage_flags_, memory_property_flags_, buffer_, memory_);
}

Buffer::~Buffer() {
    unmap();
    vkDestroyBuffer(device_.get_device(), buffer_, nullptr);
    vkFreeMemory(device_.get_device(), memory_, nullptr);
}

/**
 * Map a memory range of this buffer. If successful, mapped points to the
 * specified buffer range.
 *
 * @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to
 * map the complete buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkResult of the buffer mapping call
 */
VkResult Buffer::map(VkDeviceSize size, VkDeviceSize offset) {
    VKAO_ASSERT(buffer_ && memory_, "Called map on buffer before create");

    return vkMapMemory(
        device_.get_device(), memory_, offset, size, 0, &mapped_);
}

/**
 * Unmap a mapped memory range
 *
 * @note Does not return a result as vkUnmapMemory can't fail
 */
void Buffer::unmap() {
    if (mapped_) {
        vkUnmapMemory(device_.get_device(), memory_);
        mapped_ = nullptr;
    }
}

/**
 * Copies the specified data to the mapped buffer. Default value writes whole
 * buffer range
 *
 * @param data Pointer to the data to copy
 * @param size (Optional) Size of the data to copy. Pass VK_WHOLE_SIZE to flush
 * the complete buffer range.
 * @param offset (Optional) Byte offset from beginning of mapped region
 *
 */
void Buffer::write_to_buffer(const void *data,
                             VkDeviceSize size,
                             VkDeviceSize offset) {
    VKAO_ASSERT(mapped_, "Cannot copy to unmapped buffer");

    if (size == VK_WHOLE_SIZE) {
        memcpy(mapped_, data, buffer_size_);
    } else {
        char *mem_offset = (char *)mapped_;
        mem_offset += offset;
        memcpy(mem_offset, data, size);
    }
}

/**
 * Flush a memory range of the buffer to make it visible to the device
 *
 * @note Only required for non-coherent memory
 *
 * @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE
 * to flush the complete buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkResult of the flush call
 */
VkResult Buffer::flush(VkDeviceSize size, VkDeviceSize offset) {
    VkMappedMemoryRange mapped_range = {};
    mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mapped_range.memory = memory_;
    mapped_range.offset = offset;
    mapped_range.size = size;

    return vkFlushMappedMemoryRanges(device_.get_device(), 1, &mapped_range);
}

/**
 * Invalidate a memory range of the buffer to make it visible to the host
 *
 * @note Only required for non-coherent memory
 *
 * @param size (Optional) Size of the memory range to invalidate. Pass
 * VK_WHOLE_SIZE to invalidate the complete buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkResult of the invalidate call
 */
VkResult Buffer::invalidate(VkDeviceSize size, VkDeviceSize offset) {
    VkMappedMemoryRange mapped_range = {};
    mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mapped_range.memory = memory_;
    mapped_range.offset = offset;
    mapped_range.size = size;

    return vkInvalidateMappedMemoryRanges(
        device_.get_device(), 1, &mapped_range);
}

/**
 * Create a buffer info descriptor
 *
 * @param size (Optional) Size of the memory range of the descriptor
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkDescriptorBufferInfo of specified offset and range
 */
VkDescriptorBufferInfo Buffer::descriptor_info(VkDeviceSize size,
                                               VkDeviceSize offset) {
    return VkDescriptorBufferInfo{
        buffer_,
        offset,
        size,
    };
}

/**
 * Copies "instance_size_" bytes of data to the mapped buffer at an offset of
 * index * alignment_size_
 *
 * @param data Pointer to the data to copy
 * @param index Used in offset calculation
 *
 */
void Buffer::write_to_index(const void *data, int index) {
    write_to_buffer(data, instance_size_, index * alignment_size_);
}

/**
 *  Flush the memory range at index * alignment_size_ of the buffer to make it
 * visible to the device
 *
 * @param index Used in offset calculation
 *
 */
VkResult Buffer::flush_index(int index) {
    return flush(alignment_size_, index * alignment_size_);
}

/**
 * Create a buffer info descriptor
 *
 * @param index Specifies the region given by index * alignment_size_
 *
 * @return VkDescriptorBufferInfo for instance at index
 */
VkDescriptorBufferInfo Buffer::descriptor_info_for_index(int index) {
    return descriptor_info(alignment_size_, index * alignment_size_);
}

/**
 * Invalidate a memory range of the buffer to make it visible to the host
 *
 * @note Only required for non-coherent memory
 *
 * @param index Specifies the region to invalidate: index * alignment_size_
 *
 * @return VkResult of the invalidate call
 */
VkResult Buffer::invalidate_index(int index) {
    return invalidate(alignment_size_, index * alignment_size_);
}

} // namespace vkao