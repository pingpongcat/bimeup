#include <renderer/Device.h>
#include <renderer/RayTracingPipeline.h>
#include <renderer/RtShadowPass.h>

#include <glm/gtc/matrix_inverse.hpp>

#include <array>
#include <stdexcept>
#include <string>

namespace bimeup::renderer {

namespace {

// Shadow-pass descriptor set: TLAS (0), depth image (1), visibility
// storage image (2). All bindings are raygen-only — neither the miss nor
// the (skipped) closest-hit shader reads them directly.
VkDescriptorSetLayout CreateLayout(VkDevice dev) {
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(dev, &info, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("RtShadowPass: vkCreateDescriptorSetLayout failed");
    }
    return layout;
}

}  // namespace

RtShadowPass::RtShadowPass(const Device& device) : m_device(device) {
    if (!m_device.HasRayTracing()) {
        return;
    }
    LoadDispatch();
}

RtShadowPass::~RtShadowPass() { Reset(); }

bool RtShadowPass::IsValid() const {
    return m_image != VK_NULL_HANDLE && m_pipeline && m_pipeline->IsValid();
}

void RtShadowPass::LoadDispatch() {
    VkDevice dev = m_device.GetDevice();
    auto load = [dev](const char* name) -> PFN_vkVoidFunction {
        PFN_vkVoidFunction p = vkGetDeviceProcAddr(dev, name);
        if (p == nullptr) {
            throw std::runtime_error(std::string("RtShadowPass: vkGetDeviceProcAddr failed for ") +
                                     name);
        }
        return p;
    };
    m_pfnTraceRays = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(load("vkCmdTraceRaysKHR"));
}

void RtShadowPass::Reset() {
    VkDevice dev = m_device.GetDevice();

    m_pipeline.reset();

    if (m_ds != VK_NULL_HANDLE) {
        // Freed along with the pool.
        m_ds = VK_NULL_HANDLE;
    }
    if (m_dsPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(dev, m_dsPool, nullptr);
        m_dsPool = VK_NULL_HANDLE;
    }
    if (m_dsLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(dev, m_dsLayout, nullptr);
        m_dsLayout = VK_NULL_HANDLE;
    }
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device.GetAllocator(), m_image, m_allocation);
        m_image = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
    m_imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_width = 0;
    m_height = 0;
}

void RtShadowPass::CreateVisibilityImage(uint32_t width, uint32_t height) {
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = kVisibilityFormat;
    ii.extent = {width, height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    // STORAGE for raygen writes; SAMPLED so the Stage-9.8 hybrid
    // composite can bind the visibility result as a read-only texture.
    ii.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo ai{};
    ai.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(m_device.GetAllocator(), &ii, &ai, &m_image, &m_allocation,
                       nullptr) != VK_SUCCESS) {
        throw std::runtime_error("RtShadowPass: vmaCreateImage failed");
    }

    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = m_image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = kVisibilityFormat;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (vkCreateImageView(m_device.GetDevice(), &vi, nullptr, &m_imageView) != VK_SUCCESS) {
        throw std::runtime_error("RtShadowPass: vkCreateImageView failed");
    }

    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.minLod = 0.0F;
    si.maxLod = 0.0F;
    if (vkCreateSampler(m_device.GetDevice(), &si, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("RtShadowPass: vkCreateSampler failed");
    }

    m_imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_width = width;
    m_height = height;
}

void RtShadowPass::CreateDescriptor() {
    VkDevice dev = m_device.GetDevice();
    m_dsLayout = CreateLayout(dev);

    std::array<VkDescriptorPoolSize, 3> sizes{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    sizes[0].descriptorCount = 1;
    sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[1].descriptorCount = 1;
    sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    sizes[2].descriptorCount = 1;

    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.maxSets = 1;
    pi.poolSizeCount = static_cast<uint32_t>(sizes.size());
    pi.pPoolSizes = sizes.data();
    if (vkCreateDescriptorPool(dev, &pi, nullptr, &m_dsPool) != VK_SUCCESS) {
        throw std::runtime_error("RtShadowPass: vkCreateDescriptorPool failed");
    }

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = m_dsPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_dsLayout;
    if (vkAllocateDescriptorSets(dev, &ai, &m_ds) != VK_SUCCESS) {
        throw std::runtime_error("RtShadowPass: vkAllocateDescriptorSets failed");
    }
}

bool RtShadowPass::Build(uint32_t width, uint32_t height, const std::string& shaderDir) {
    if (!m_device.HasRayTracing()) {
        return false;
    }
    if (width == 0 || height == 0) {
        return false;
    }

    Reset();
    CreateVisibilityImage(width, height);
    CreateDescriptor();

    m_pipeline = std::make_unique<RayTracingPipeline>(m_device);

    RayTracingPipelineSettings s;
    s.raygenPath = shaderDir + "/shadow.rgen.spv";
    s.missPath = shaderDir + "/shadow.rmiss.spv";
    // The shadow raygen sets `gl_RayFlagsSkipClosestHitShaderEXT`, so the
    // CH shader is never invoked — we still have to supply one to
    // satisfy the RT-pipeline group layout. Reuse the Stage-9.3 empty
    // `rt_probe.rchit` stub; no dedicated shadow hit shader needed.
    s.closestHitPath = shaderDir + "/rt_probe.rchit.spv";
    s.descriptorSetLayout = m_dsLayout;
    s.pushConstantSize = sizeof(PushConstants);
    s.pushConstantStages = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    s.maxRayRecursionDepth = 1;

    if (!m_pipeline->Build(s)) {
        Reset();
        return false;
    }
    return true;
}

void RtShadowPass::UpdateDescriptor(VkAccelerationStructureKHR tlas,
                                    VkImageView depthView, VkSampler depthSampler) {
    VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
    asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asWrite.accelerationStructureCount = 1;
    asWrite.pAccelerationStructures = &tlas;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depthInfo.imageView = depthView;
    depthInfo.sampler = depthSampler;

    VkDescriptorImageInfo visInfo{};
    visInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    visInfo.imageView = m_imageView;

    std::array<VkWriteDescriptorSet, 3> writes{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = &asWrite;
    writes[0].dstSet = m_ds;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_ds;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &depthInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = m_ds;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[2].pImageInfo = &visInfo;

    vkUpdateDescriptorSets(m_device.GetDevice(), static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

void RtShadowPass::Dispatch(VkCommandBuffer cmd, VkAccelerationStructureKHR tlas,
                            VkImageView depthView, VkSampler depthSampler,
                            const glm::mat4& view, const glm::mat4& proj,
                            const glm::vec3& sunDirWorld) {
    if (!IsValid() || tlas == VK_NULL_HANDLE || depthView == VK_NULL_HANDLE ||
        depthSampler == VK_NULL_HANDLE) {
        return;
    }

    UpdateDescriptor(tlas, depthView, depthSampler);

    // Visibility image → GENERAL for the raygen storage write. Source
    // layout depends on whether this is the first dispatch (UNDEFINED) or
    // a subsequent one (SHADER_READ_ONLY_OPTIMAL from the prior tail
    // transition).
    VkImageMemoryBarrier toGeneral{};
    toGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toGeneral.oldLayout = m_imageLayout;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toGeneral.srcAccessMask = m_imageLayout == VK_IMAGE_LAYOUT_UNDEFINED
                                  ? 0
                                  : VK_ACCESS_SHADER_READ_BIT;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toGeneral.image = m_image;
    toGeneral.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toGeneral.subresourceRange.levelCount = 1;
    toGeneral.subresourceRange.layerCount = 1;
    toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    const VkPipelineStageFlags srcStage = m_imageLayout == VK_IMAGE_LAYOUT_UNDEFINED
                                              ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                              : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    vkCmdPipelineBarrier(cmd, srcStage, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
                         0, nullptr, 0, nullptr, 1, &toGeneral);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline->GetPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            m_pipeline->GetLayout(), 0, 1, &m_ds, 0, nullptr);

    PushConstants pc{};
    pc.invViewProj = glm::inverse(proj * view);
    pc.sunDirWorld = glm::vec4(sunDirWorld, 0.0F);
    pc.extent = glm::uvec2(m_width, m_height);
    vkCmdPushConstants(cmd, m_pipeline->GetLayout(), VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0,
                       sizeof(pc), &pc);

    m_pfnTraceRays(cmd, &m_pipeline->GetRaygenRegion(), &m_pipeline->GetMissRegion(),
                   &m_pipeline->GetHitRegion(), &m_pipeline->GetCallableRegion(),
                   m_width, m_height, 1);

    // Tail: visibility image → SHADER_READ_ONLY_OPTIMAL so the composite
    // pass can sample it without another barrier.
    VkImageMemoryBarrier toRead{};
    toRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toRead.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toRead.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toRead.image = m_image;
    toRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toRead.subresourceRange.levelCount = 1;
    toRead.subresourceRange.layerCount = 1;
    toRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &toRead);

    m_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

}  // namespace bimeup::renderer
