#include <renderer/Swapchain.h>
#include <renderer/Device.h>
#include <tools/Log.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace bimeup::renderer {

Swapchain::Swapchain(const Device& device, VkSurfaceKHR surface, VkExtent2D windowExtent)
    : m_device(device) {
    Create(surface, windowExtent, VK_NULL_HANDLE);
}

Swapchain::~Swapchain() {
    Cleanup();
}

void Swapchain::Recreate(VkSurfaceKHR surface, VkExtent2D windowExtent) {
    vkDeviceWaitIdle(m_device.GetDevice());

    VkSwapchainKHR oldSwapchain = m_swapchain;
    m_swapchain = VK_NULL_HANDLE;

    // Destroy old image views but not the old swapchain yet (passed to create for reuse)
    for (auto view : m_imageViews) {
        vkDestroyImageView(m_device.GetDevice(), view, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();

    Create(surface, windowExtent, oldSwapchain);

    vkDestroySwapchainKHR(m_device.GetDevice(), oldSwapchain, nullptr);
}

VkSwapchainKHR Swapchain::GetSwapchain() const {
    return m_swapchain;
}

VkFormat Swapchain::GetFormat() const {
    return m_format;
}

VkExtent2D Swapchain::GetExtent() const {
    return m_extent;
}

uint32_t Swapchain::GetImageCount() const {
    return static_cast<uint32_t>(m_images.size());
}

const std::vector<VkImage>& Swapchain::GetImages() const {
    return m_images;
}

const std::vector<VkImageView>& Swapchain::GetImageViews() const {
    return m_imageViews;
}

void Swapchain::Create(VkSurfaceKHR surface, VkExtent2D windowExtent,
                       VkSwapchainKHR oldSwapchain) {
    VkPhysicalDevice physDevice = m_device.GetPhysicalDevice();

    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surface, &capabilities);

    // Query surface formats
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &formatCount, nullptr);
    if (formatCount == 0) {
        throw std::runtime_error("No surface formats available");
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &formatCount, formats.data());

    // Query present modes
    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &modeCount, nullptr);
    if (modeCount == 0) {
        throw std::runtime_error("No present modes available");
    }
    std::vector<VkPresentModeKHR> presentModes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &modeCount,
                                              presentModes.data());

    VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
    VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);
    VkExtent2D extent = ChooseExtent(capabilities, windowExtent);

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t graphicsFamily = m_device.GetGraphicsQueueFamily();
    uint32_t presentFamily = m_device.GetPresentQueueFamily();

    if (graphicsFamily != presentFamily) {
        uint32_t queueFamilyIndices[] = {graphicsFamily, presentFamily};
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VkResult result =
        vkCreateSwapchainKHR(m_device.GetDevice(), &createInfo, nullptr, &m_swapchain);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }

    m_format = surfaceFormat.format;
    m_extent = extent;

    // Retrieve swapchain images
    vkGetSwapchainImagesKHR(m_device.GetDevice(), m_swapchain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device.GetDevice(), m_swapchain, &imageCount, m_images.data());

    CreateImageViews();

    if (bimeup::tools::Log::GetLogger()) {
        LOG_INFO("Swapchain created: {}x{}, {} images", m_extent.width, m_extent.height,
                 m_images.size());
    }
}

void Swapchain::CreateImageViews() {
    m_imageViews.resize(m_images.size());

    for (size_t i = 0; i < m_images.size(); ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkResult result =
            vkCreateImageView(m_device.GetDevice(), &viewInfo, nullptr, &m_imageViews[i]);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain image view");
        }
    }
}

void Swapchain::Cleanup() {
    for (auto view : m_imageViews) {
        vkDestroyImageView(m_device.GetDevice(), view, nullptr);
    }
    m_imageViews.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device.GetDevice(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats[0];
}

VkPresentModeKHR Swapchain::ChoosePresentMode(
    const std::vector<VkPresentModeKHR>& modes) {
    // Cap to display refresh rate (vsync) to avoid wasting compute. Prefer
    // FIFO_RELAXED so a frame that misses a vblank tears and presents
    // immediately instead of waiting a full refresh interval — the extra
    // latency is what makes debug-mode orbiting feel jaggy under strict FIFO.
    for (const auto& mode : modes) {
        if (mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                   VkExtent2D windowExtent) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent;
    extent.width = std::clamp(windowExtent.width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(windowExtent.height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
    return extent;
}

}  // namespace bimeup::renderer
