#include <renderer/Msaa.h>

namespace bimeup::renderer {

VkSampleCountFlagBits ClampSampleCount(int requested, VkSampleCountFlags supported) {
    // Walk highest → lowest; pick the first bit that is both supported and <= requested.
    constexpr VkSampleCountFlagBits kBits[] = {
        VK_SAMPLE_COUNT_64_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_16_BIT,
        VK_SAMPLE_COUNT_8_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_2_BIT,
    };
    for (VkSampleCountFlagBits bit : kBits) {
        if ((supported & bit) != 0 && static_cast<int>(bit) <= requested) {
            return bit;
        }
    }
    return VK_SAMPLE_COUNT_1_BIT;
}

VkSampleCountFlags GetUsableSampleCounts(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    return props.limits.framebufferColorSampleCounts &
           props.limits.framebufferDepthSampleCounts;
}

}  // namespace bimeup::renderer
