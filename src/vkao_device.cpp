#include "vkao_device.h"
#include "log.hpp"
#include <cstring>
#include <iostream>
#include <set>
#include <unordered_set>

namespace vkao {
namespace {
static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}
} // namespace

// class member functions
VkaoDevice::VkaoDevice(VkaoWindow &window) : window_{window} {
    create_instance();
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_command_pool();
}

VkaoDevice::~VkaoDevice() {
    vkDestroyCommandPool(device_, command_pool_, nullptr);
    vkDestroyDevice(device_, nullptr);

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
    }

    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vkDestroyInstance(instance_, nullptr);
}

void VkaoDevice::create_instance() {
    VKAO_ASSERT(!enableValidationLayers || check_validation_layer_support(),
                "validation layers requested, but not available!");

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "LittleVulkanEngine App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = get_required_extensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers) {
        createInfo.enabledLayerCount =
            static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populate_debug_messenger_create_info(debugCreateInfo);
        createInfo.pNext =
            (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    VKAO_ASSERT(vkCreateInstance(&createInfo, nullptr, &instance_) ==
                    VK_SUCCESS,
                "failed to create instance!");

    has_SDL_required_instance_extensions();
}

void VkaoDevice::pick_physical_device() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    VKAO_ASSERT(deviceCount, "failed to find GPUs with Vulkan support!");

    std::cout << "Device count: " << deviceCount << std::endl;
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (const auto &device : devices) {
        if (is_device_suitable(device)) {
            physical_device_ = device;
            break;
        }
    }

    VKAO_ASSERT(physical_device_ != VK_NULL_HANDLE,
                "failed to find a suitable GPU!");

    vkGetPhysicalDeviceProperties(physical_device_, &properties);
    std::cout << "physical device: " << properties.deviceName << std::endl;

    VKAO_INFO("physical device: {}", properties.deviceName);
}

void VkaoDevice::create_logical_device() {
    QueueFamilyIndices indices = find_queue_families(physical_device_);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphics_family.value(),
                                              indices.present_family.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VKAO_ASSERT(
        vkCreateDevice(physical_device_, &createInfo, nullptr, &device_) ==
            VK_SUCCESS,
        "failed to create logical device!");

    vkGetDeviceQueue(
        device_, indices.graphics_family.value(), 0, &graphics_queue_);
    vkGetDeviceQueue(
        device_, indices.present_family.value(), 0, &present_queue_);
}

void VkaoDevice::create_command_pool() {
    QueueFamilyIndices queueFamilyIndices = find_physical_queue_families();

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphics_family.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VKAO_ASSERT(vkCreateCommandPool(
                    device_, &poolInfo, nullptr, &command_pool_) == VK_SUCCESS,
                "failed to create command pool!");
}

void VkaoDevice::create_surface() {
    window_.create_window_surface(instance_, &surface_);
}

bool VkaoDevice::is_device_suitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = find_queue_families(device);

    bool extensionsSupported = check_device_extension_support(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport =
            query_swap_chain_support(device);
        swapChainAdequate = !swapChainSupport.formats.empty() &&
                            !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.is_complete() && extensionsSupported && swapChainAdequate &&
           supportedFeatures.samplerAnisotropy;
}

void VkaoDevice::populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr; // Optional
}

void VkaoDevice::setup_debug_messenger() {
    if (!enableValidationLayers) {
        return;
    }
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populate_debug_messenger_create_info(createInfo);
    if (CreateDebugUtilsMessengerEXT(
            instance_, &createInfo, nullptr, &debug_messenger_) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

bool VkaoDevice::check_validation_layer_support() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char *layerName : validationLayers) {
        bool layerFound = false;

        for (const auto &layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

std::vector<const char *> VkaoDevice::get_required_extensions() {
    uint32_t extensionCount = 0;
    const char *const *extensionNames =
        SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    std::vector<const char *> extensions(extensionNames,
                                         extensionNames + extensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void VkaoDevice::has_SDL_required_instance_extensions() {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(
        nullptr, &extensionCount, extensions.data());

    std::cout << "available extensions:" << std::endl;
    std::unordered_set<std::string> available;
    for (const auto &extension : extensions) {
        std::cout << "\t" << extension.extensionName << std::endl;
        available.insert(extension.extensionName);
    }

    std::cout << "required extensions:" << std::endl;
    auto required_extensions = get_required_extensions();
    for (const auto &required : required_extensions) {
        std::cout << "\t" << required << std::endl;
        if (available.find(required) == available.end()) {
            throw std::runtime_error("Missing required glfw extension");
        }
    }
}

bool VkaoDevice::check_device_extension_support(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(
        device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(
        device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                             deviceExtensions.end());

    for (const auto &extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices VkaoDevice::find_queue_families(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto &queueFamily : queueFamilies) {
        if (queueFamily.queueCount > 0 &&
            queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            device, i, surface_, &presentSupport);
        if (queueFamily.queueCount > 0 && presentSupport) {
            indices.present_family = i;
        }
        if (indices.is_complete()) {
            break;
        }

        i++;
    }

    return indices;
}

SwapChainSupportDetails
VkaoDevice::query_swap_chain_support(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device, surface_, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        device, surface_, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, surface_, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface_, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface_, &presentModeCount, details.presentModes.data());
    }
    return details;
}

VkFormat
VkaoDevice::find_supported_format(const std::vector<VkFormat> &candidates,
                                  VkImageTiling tiling,
                                  VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physical_device_, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                   (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    VKAO_ASSERT(false, "failed to find supported format!");

    return {};
}

uint32_t VkaoDevice::find_memory_type(uint32_t typeFilter,
                                      VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }

    VKAO_ASSERT(false, "failed to find suitable memory type!");

    return {};
}

void VkaoDevice::create_buffer(VkDeviceSize size,
                               VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties,
                               VkBuffer &buffer,
                               VkDeviceMemory &bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VKAO_ASSERT(vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) ==
                    VK_SUCCESS,
                "failed to create vertex buffer!");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        find_memory_type(memRequirements.memoryTypeBits, properties);

    VKAO_ASSERT(vkAllocateMemory(device_, &allocInfo, nullptr, &bufferMemory) ==
                    VK_SUCCESS,
                "failed to allocate vertex buffer memory!")

    vkBindBufferMemory(device_, buffer, bufferMemory, 0);
}

VkCommandBuffer VkaoDevice::begin_single_time_commands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = command_pool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VkaoDevice::end_single_time_commands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphics_queue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);

    vkFreeCommandBuffers(device_, command_pool_, 1, &commandBuffer);
}

void VkaoDevice::copy_buffer(VkBuffer srcBuffer,
                             VkBuffer dstBuffer,
                             VkDeviceSize size) {
    VkCommandBuffer commandBuffer = begin_single_time_commands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    end_single_time_commands(commandBuffer);
}

void VkaoDevice::copy_buffer_to_image(VkBuffer buffer,
                                      VkImage image,
                                      uint32_t width,
                                      uint32_t height,
                                      uint32_t layerCount) {
    VkCommandBuffer commandBuffer = begin_single_time_commands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = layerCount;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer,
                           buffer,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);
    end_single_time_commands(commandBuffer);
}

void VkaoDevice::create_image_with_info(const VkImageCreateInfo &imageInfo,
                                        VkMemoryPropertyFlags properties,
                                        VkImage &image,
                                        VkDeviceMemory &imageMemory) {
    if (vkCreateImage(device_, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        find_memory_type(memRequirements.memoryTypeBits, properties);

    VKAO_ASSERT(vkAllocateMemory(device_, &allocInfo, nullptr, &imageMemory) ==
                    VK_SUCCESS,
                "failed to allocate image memory!");

    VKAO_ASSERT(vkBindImageMemory(device_, image, imageMemory, 0) == VK_SUCCESS,
                "failed to bind image memory!");
}
} // namespace vkao