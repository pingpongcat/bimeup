#include <renderer/Shader.h>
#include <renderer/Device.h>

#include <fstream>
#include <stdexcept>

namespace bimeup::renderer {

Shader::Shader(const Device& device, ShaderStage stage, const std::vector<uint32_t>& spirvCode)
    : m_device(device.GetDevice()), m_stage(stage) {
    if (spirvCode.empty()) {
        throw std::runtime_error("SPIR-V code is empty");
    }
    CreateModule(spirvCode);
}

Shader::Shader(const Device& device, ShaderStage stage, const std::string& filePath)
    : m_device(device.GetDevice()), m_stage(stage) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open SPIR-V file: " + filePath);
    }

    auto fileSize = file.tellg();
    if (fileSize <= 0 || fileSize % sizeof(uint32_t) != 0) {
        throw std::runtime_error("Invalid SPIR-V file size: " + filePath);
    }

    std::vector<uint32_t> spirvCode(static_cast<size_t>(fileSize) / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spirvCode.data()), fileSize);

    CreateModule(spirvCode);
}

Shader::~Shader() {
    if (m_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device, m_module, nullptr);
    }
}

void Shader::CreateModule(const std::vector<uint32_t>& spirvCode) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
    createInfo.pCode = spirvCode.data();

    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &m_module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
}

VkShaderModule Shader::GetModule() const {
    return m_module;
}

ShaderStage Shader::GetStage() const {
    return m_stage;
}

VkPipelineShaderStageCreateInfo Shader::GetStageInfo() const {
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.module = m_module;
    stageInfo.pName = "main";

    switch (m_stage) {
        case ShaderStage::Vertex:
            stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case ShaderStage::Fragment:
            stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
    }

    return stageInfo;
}

}  // namespace bimeup::renderer
