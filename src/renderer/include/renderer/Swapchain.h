#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace bimeup::renderer {

class Device;

class Swapchain {
public:
    Swapchain(const Device& device, VkSurfaceKHR surface, VkExtent2D windowExtent);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    void Recreate(VkSurfaceKHR surface, VkExtent2D windowExtent);

    [[nodiscard]] VkSwapchainKHR GetSwapchain() const;
    [[nodiscard]] VkFormat GetFormat() const;
    [[nodiscard]] VkExtent2D GetExtent() const;
    [[nodiscard]] uint32_t GetImageCount() const;
    [[nodiscard]] const std::vector<VkImage>& GetImages() const;
    [[nodiscard]] const std::vector<VkImageView>& GetImageViews() const;

private:
    void Create(VkSurfaceKHR surface, VkExtent2D windowExtent, VkSwapchainKHR oldSwapchain);
    void CreateImageViews();
    void Cleanup();

    static VkSurfaceFormatKHR ChooseSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& formats);
    static VkPresentModeKHR ChoosePresentMode(
        const std::vector<VkPresentModeKHR>& modes);
    static VkExtent2D ChooseExtent(
        const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D windowExtent);

    const Device& m_device;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent = {0, 0};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
};

}  // namespace bimeup::renderer
