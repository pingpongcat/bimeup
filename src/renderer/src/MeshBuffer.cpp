#include <renderer/MeshBuffer.h>

#include <renderer/Buffer.h>
#include <renderer/Device.h>

#include <algorithm>

namespace bimeup::renderer {

MeshBuffer::MeshBuffer(const Device& device) : m_device(device) {}

MeshBuffer::~MeshBuffer() = default;

MeshHandle MeshBuffer::Upload(const MeshData& data) {
    MeshHandle handle = m_nextHandle++;

    DrawParams params;
    params.indexCount = static_cast<uint32_t>(data.indices.size());
    params.firstIndex = static_cast<uint32_t>(m_indices.size());
    params.vertexOffset = static_cast<int32_t>(m_vertices.size());
    params.edgeIndexCount = static_cast<uint32_t>(data.edgeIndices.size());
    params.firstEdgeIndex = static_cast<uint32_t>(m_edgeIndices.size());

    m_meshes[handle] = params;

    m_vertices.insert(m_vertices.end(), data.vertices.begin(), data.vertices.end());
    m_indices.insert(m_indices.end(), data.indices.begin(), data.indices.end());
    m_edgeIndices.insert(m_edgeIndices.end(), data.edgeIndices.begin(), data.edgeIndices.end());
    m_baselineColors.reserve(m_vertices.size());
    for (const auto& v : data.vertices) {
        m_baselineColors.push_back(v.color);
    }

    m_dirty = true;
    RebuildGpuBuffers();

    return handle;
}

void MeshBuffer::SetVertexColorOverride(const std::vector<uint32_t>& indices, glm::vec4 color) {
    m_colorOverrideIndices = indices;
    m_colorOverrideColor = color;
    RecomputeVertexColors();
    vkDeviceWaitIdle(m_device.GetDevice());
    m_dirty = true;
    RebuildGpuBuffers();
}

void MeshBuffer::SetVertexAlphaOverride(const std::vector<std::pair<uint32_t, float>>& alphas) {
    m_alphaOverrides.clear();
    m_alphaOverrides.reserve(alphas.size());
    for (const auto& [idx, a] : alphas) {
        m_alphaOverrides[idx] = std::clamp(a, 0.0F, 1.0F);
    }
    RecomputeVertexColors();
    vkDeviceWaitIdle(m_device.GetDevice());
    m_dirty = true;
    RebuildGpuBuffers();
}

void MeshBuffer::RecomputeVertexColors() {
    if (m_vertices.size() != m_baselineColors.size()) {
        return;
    }
    // Layer 1: baseline.
    for (size_t i = 0; i < m_vertices.size(); ++i) {
        m_vertices[i].color = m_baselineColors[i];
    }
    // Layer 2: alpha override (preserves RGB, replaces alpha).
    for (const auto& [idx, a] : m_alphaOverrides) {
        if (idx < m_vertices.size()) {
            m_vertices[idx].color.a = a;
        }
    }
    // Layer 3: color override (full RGBA replacement, last-wins).
    for (uint32_t idx : m_colorOverrideIndices) {
        if (idx < m_vertices.size()) {
            m_vertices[idx].color = m_colorOverrideColor;
        }
    }
}

void MeshBuffer::Remove(MeshHandle handle) {
    m_meshes.erase(handle);
}

bool MeshBuffer::IsValid(MeshHandle handle) const {
    return m_meshes.contains(handle);
}

DrawParams MeshBuffer::GetDrawParams(MeshHandle handle) const {
    auto it = m_meshes.find(handle);
    if (it == m_meshes.end()) {
        return {};
    }
    return it->second;
}

std::size_t MeshBuffer::MeshCount() const {
    return m_meshes.size();
}

void MeshBuffer::Bind(VkCommandBuffer cmd) const {
    if (!m_vertexBuffer || !m_indexBuffer) {
        return;
    }
    VkBuffer vb = m_vertexBuffer->GetBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

void MeshBuffer::Draw(VkCommandBuffer cmd, MeshHandle handle) const {
    auto it = m_meshes.find(handle);
    if (it == m_meshes.end()) {
        return;
    }
    const auto& params = it->second;
    vkCmdDrawIndexed(cmd, params.indexCount, 1, params.firstIndex, params.vertexOffset, 0);
}

void MeshBuffer::BindEdges(VkCommandBuffer cmd) const {
    if (!m_vertexBuffer || !m_edgeIndexBuffer) {
        return;
    }
    VkBuffer vb = m_vertexBuffer->GetBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, m_edgeIndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

void MeshBuffer::DrawEdges(VkCommandBuffer cmd, MeshHandle handle) const {
    auto it = m_meshes.find(handle);
    if (it == m_meshes.end()) {
        return;
    }
    const auto& params = it->second;
    if (params.edgeIndexCount == 0) {
        return;
    }
    vkCmdDrawIndexed(cmd, params.edgeIndexCount, 1, params.firstEdgeIndex,
                     params.vertexOffset, 0);
}

void MeshBuffer::RebuildGpuBuffers() {
    if (!m_dirty || m_vertices.empty()) {
        return;
    }

    m_vertexBuffer = std::make_unique<Buffer>(
        m_device, BufferType::Vertex,
        m_vertices.size() * sizeof(Vertex), m_vertices.data());

    m_indexBuffer = std::make_unique<Buffer>(
        m_device, BufferType::Index,
        m_indices.size() * sizeof(uint32_t), m_indices.data());

    if (!m_edgeIndices.empty()) {
        m_edgeIndexBuffer = std::make_unique<Buffer>(
            m_device, BufferType::Index,
            m_edgeIndices.size() * sizeof(uint32_t), m_edgeIndices.data());
    } else {
        m_edgeIndexBuffer.reset();
    }

    m_dirty = false;
}

}  // namespace bimeup::renderer
