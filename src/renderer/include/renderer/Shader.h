#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

namespace bimeup::renderer {

class Device;

enum class ShaderStage {
    Vertex,
    Fragment
};

class Shader {
public:
    Shader(const Device& device, ShaderStage stage, const std::vector<uint32_t>& spirvCode);
    Shader(const Device& device, ShaderStage stage, const std::string& filePath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) = delete;
    Shader& operator=(Shader&&) = delete;

    [[nodiscard]] VkShaderModule GetModule() const;
    [[nodiscard]] ShaderStage GetStage() const;
    [[nodiscard]] VkPipelineShaderStageCreateInfo GetStageInfo() const;

private:
    void CreateModule(const std::vector<uint32_t>& spirvCode);

    VkDevice m_device = VK_NULL_HANDLE;
    VkShaderModule m_module = VK_NULL_HANDLE;
    ShaderStage m_stage;
};

}  // namespace bimeup::renderer
