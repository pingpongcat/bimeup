#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Buffer;
class Device;

/// GPU-facing vertex for the disk marker. Matches the vertex-input layout
/// declared in `DiskMarkerPipeline` (vec3 position + vec4 color, 28 bytes).
struct DiskVertex {
    glm::vec3 position;
    glm::vec4 color;
};

/// Build a flat, filled disk as a triangle list (fan topology). Each triangle
/// is `{center, ring[i], ring[i+1]}`. The outer-ring vertices carry alpha=0 so
/// the pipeline's alpha blend produces a naturally soft edge while the center
/// is fully opaque (at the caller's provided alpha). Returns an empty list on
/// invalid input (radius ≤ 0 or zero-length normal). `segments` is clamped to
/// ≥ 3.
[[nodiscard]] std::vector<DiskVertex> BuildDiskVertices(const glm::vec3& center,
                                                       const glm::vec3& normal,
                                                       float radius,
                                                       const glm::vec4& color,
                                                       int segments = 48);

/// GPU-resident disk geometry. `Rebuild` hashes the inputs; if nothing has
/// changed since the previous call, it skips the rebuild + GPU upload. `Clear`
/// drops the buffer so `IsEmpty` returns true and no draw call is issued.
class DiskMarkerBuffer {
public:
    explicit DiskMarkerBuffer(const Device& device);
    ~DiskMarkerBuffer();

    DiskMarkerBuffer(const DiskMarkerBuffer&) = delete;
    DiskMarkerBuffer& operator=(const DiskMarkerBuffer&) = delete;
    DiskMarkerBuffer(DiskMarkerBuffer&&) = delete;
    DiskMarkerBuffer& operator=(DiskMarkerBuffer&&) = delete;

    void Rebuild(const glm::vec3& center,
                 const glm::vec3& normal,
                 float radius,
                 const glm::vec4& color,
                 int segments = 48);

    void Clear();

    [[nodiscard]] VkBuffer GetVertexBuffer() const;
    [[nodiscard]] std::uint32_t GetVertexCount() const {
        return static_cast<std::uint32_t>(vertices_.size());
    }
    [[nodiscard]] bool IsEmpty() const { return vertices_.empty(); }

private:
    const Device& device_;
    std::vector<DiskVertex> vertices_;
    std::unique_ptr<Buffer> vertexBuffer_;
    std::size_t lastHash_ = 0;
    bool hasPrevious_ = false;
};

}  // namespace bimeup::renderer
