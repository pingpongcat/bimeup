#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace bimeup::renderer {

class Device;
class Shader;

struct PipelineConfig {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    bool depthTestEnable = false;
    bool depthWriteEnable = false;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
    bool alphaBlendEnable = false;
    // RP.10c — when true, attachment 0's blend state is configured as
    // srcColorBlendFactor = ONE, dstColorBlendFactor = ONE, colorBlendOp =
    // ADD so the fragment output is added to the target. Used by the bloom
    // upsample pipeline to composite onto the higher mip without a second
    // tap or a composite shader. Mutually exclusive with `alphaBlendEnable`
    // (set at most one; setting both is caller error — alpha wins).
    bool additiveBlend = false;
    uint32_t colorAttachmentCount = 1;  // set to 0 for depth-only passes
    // If true, colour attachments beyond attachment 0 have colorWriteMask = 0 —
    // i.e. the fragment shader emits only `outColor` and leaves every other
    // attachment untouched. Intended for overlay pipelines (section-fill, disk
    // marker, transparent) bound to the MRT main pass that doesn't populate the
    // normal G-buffer. Ignored when colorAttachmentCount <= 1.
    bool disableSecondaryColorWrites = false;
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;
};

class Pipeline {
public:
    Pipeline(const Device& device, const Shader& vertexShader, const Shader& fragmentShader,
             const PipelineConfig& config);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

    void Bind(VkCommandBuffer cmd) const;

private:
    void CreateLayout(const PipelineConfig& config);
    void CreatePipeline(const Shader& vertexShader, const Shader& fragmentShader,
                        const PipelineConfig& config);

    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
