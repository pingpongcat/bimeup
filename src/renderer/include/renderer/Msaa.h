#pragma once

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

/// Clamp a requested MSAA sample count (as an integer — 1, 2, 4, 8, 16, ...) to the
/// highest VkSampleCountFlagBits value that is both <= requested and present in the
/// `supported` mask. Returns VK_SAMPLE_COUNT_1_BIT if nothing else is available.
[[nodiscard]] VkSampleCountFlagBits ClampSampleCount(int requested, VkSampleCountFlags supported);

/// Intersect framebufferColorSampleCounts & framebufferDepthSampleCounts on the given
/// physical device — the set of sample counts usable for both color and depth attachments.
[[nodiscard]] VkSampleCountFlags GetUsableSampleCounts(VkPhysicalDevice physicalDevice);

}  // namespace bimeup::renderer
