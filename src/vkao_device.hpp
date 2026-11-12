#pragma once

#include "SDL3/SDL_vulkan.h"
#include "ref.hpp"
#include "vkao_window.hpp"
#include "vulkan/vulkan_core.h"
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

namespace vkao {
struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;

    bool is_complete() {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VkaoDevice {
  public:
#if defined(NDEBUG)
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    VkaoDevice(VkaoWindow &window);
    ~VkaoDevice();

    VkaoDevice(const VkaoDevice &) = delete;

    VkDevice get_device() const {
        return device_;
    }

    VkQueue get_graphics_queue() const {
        return graphics_queue_;
    }

    VkQueue get_present_queue() const {
        return present_queue_;
    }

    VkSurfaceKHR get_surface() const {
        return surface_;
    }

    SwapChainSupportDetails get_swap_chain_support() {
        return query_swap_chain_support(physical_device_);
    }

    VkCommandPool get_command_pool() const {
        return command_pool_;
    }

    uint32_t find_memory_type(uint32_t type_filter,
                              VkMemoryPropertyFlags properties);

    QueueFamilyIndices find_physical_queue_families() {
        return find_queue_families(physical_device_);
    }

    VkFormat find_supported_format(const std::vector<VkFormat> &candidates,
                                   VkImageTiling tiling,
                                   VkFormatFeatureFlags features);

    // helper functions
    void create_buffer(VkDeviceSize size,
                       VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags properties,
                       VkBuffer &buffer,
                       VkDeviceMemory &buffer_memory);

    VkCommandBuffer begin_single_time_commands();

    void end_single_time_commands(VkCommandBuffer command_buffer);

    void
    copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);

    void copy_buffer_to_image(VkBuffer buffer,
                              VkImage image,
                              uint32_t width,
                              uint32_t height,
                              uint32_t layer_count);

    void create_image_with_info(const VkImageCreateInfo &image_info,
                                VkMemoryPropertyFlags properties,
                                VkImage &image,
                                VkDeviceMemory &image_memory);

    VkPhysicalDeviceProperties properties;

  private:
    void create_instance();
    void setup_debug_messenger();
    void create_surface();
    void pick_physical_device();
    void create_logical_device();
    void create_command_pool();

    // helper functions
    bool is_device_suitable(VkPhysicalDevice device);
    std::vector<const char *> get_required_extensions();
    bool check_validation_layer_support();
    QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
    void populate_debug_messenger_create_info(
        VkDebugUtilsMessengerCreateInfoEXT &create_info);
    void has_SDL_required_instance_extensions();
    bool check_device_extension_support(VkPhysicalDevice device);
    SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device);

    VkaoWindow &vkao_window_;

    VkInstance instance_;
    VkDebugUtilsMessengerEXT debug_messenger_;
    VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
    VkDevice device_;
    VkQueue graphics_queue_;
    VkQueue present_queue_;
    VkSurfaceKHR surface_;
    VkCommandPool command_pool_;

    const std::vector<const char *> validationLayers = {
        "VK_LAYER_KHRONOS_validation"};

    const std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};
} // namespace vkao