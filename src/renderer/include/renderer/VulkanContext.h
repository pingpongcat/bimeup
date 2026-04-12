#pragma once

#include <vulkan/vulkan.h>

#include <span>

namespace bimeup::renderer {

class VulkanContext {
public:
    explicit VulkanContext(bool enableValidation);
    VulkanContext(bool enableValidation, std::span<const char* const> requiredExtensions);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;

    [[nodiscard]] VkInstance GetInstance() const;
    [[nodiscard]] bool HasValidationLayers() const;
    [[nodiscard]] bool HasDebugMessenger() const;

private:
    void CreateInstance(bool enableValidation, std::span<const char* const> extraExtensions);
    void SetupDebugMessenger();
    static bool CheckValidationLayerSupport();

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    bool m_validationEnabled = false;
};

}  // namespace bimeup::renderer
