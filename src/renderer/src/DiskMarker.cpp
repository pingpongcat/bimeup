#include <renderer/DiskMarker.h>

#include <algorithm>
#include <cmath>
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <renderer/Buffer.h>
#include <renderer/Device.h>

namespace bimeup::renderer {

namespace {

void HashMix(std::size_t& h, std::size_t v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
}

std::size_t HashState(const glm::vec3& center, const glm::vec3& normal,
                      float radius, const glm::vec4& color, int segments) {
    std::size_t h = 0;
    const auto hf = std::hash<float>{};
    const auto hi = std::hash<int>{};
    HashMix(h, hf(center.x));
    HashMix(h, hf(center.y));
    HashMix(h, hf(center.z));
    HashMix(h, hf(normal.x));
    HashMix(h, hf(normal.y));
    HashMix(h, hf(normal.z));
    HashMix(h, hf(radius));
    HashMix(h, hf(color.r));
    HashMix(h, hf(color.g));
    HashMix(h, hf(color.b));
    HashMix(h, hf(color.a));
    HashMix(h, hi(segments));
    return h;
}

// Pick any unit vector perpendicular to n. Uses the world axis least aligned
// with n, so the cross product is numerically well-conditioned.
glm::vec3 OrthonormalTangent(const glm::vec3& n) {
    const glm::vec3 axis = (std::abs(n.x) < std::abs(n.y))
                               ? (std::abs(n.x) < std::abs(n.z)
                                      ? glm::vec3(1.0F, 0.0F, 0.0F)
                                      : glm::vec3(0.0F, 0.0F, 1.0F))
                               : (std::abs(n.y) < std::abs(n.z)
                                      ? glm::vec3(0.0F, 1.0F, 0.0F)
                                      : glm::vec3(0.0F, 0.0F, 1.0F));
    return glm::normalize(glm::cross(n, axis));
}

}  // namespace

std::vector<DiskVertex> BuildDiskVertices(const glm::vec3& center,
                                          const glm::vec3& normal,
                                          float radius,
                                          const glm::vec4& color,
                                          int segments) {
    if (radius <= 0.0F) return {};
    const float nlen = glm::length(normal);
    if (nlen <= 0.0F) return {};

    const glm::vec3 n = normal / nlen;
    const int segs = std::max(segments, 3);

    const glm::vec3 t = OrthonormalTangent(n);
    const glm::vec3 b = glm::cross(n, t);

    const glm::vec4 edgeColor(color.r, color.g, color.b, 0.0F);

    std::vector<DiskVertex> verts;
    verts.reserve(static_cast<std::size_t>(segs) * 3U);

    const float step = glm::two_pi<float>() / static_cast<float>(segs);
    for (int i = 0; i < segs; ++i) {
        const float a0 = step * static_cast<float>(i);
        const float a1 = step * static_cast<float>(i + 1);
        const glm::vec3 p0 = center + radius * (std::cos(a0) * t + std::sin(a0) * b);
        const glm::vec3 p1 = center + radius * (std::cos(a1) * t + std::sin(a1) * b);
        verts.push_back({center, color});
        verts.push_back({p0, edgeColor});
        verts.push_back({p1, edgeColor});
    }
    return verts;
}

DiskMarkerBuffer::DiskMarkerBuffer(const Device& device) : device_(device) {}
DiskMarkerBuffer::~DiskMarkerBuffer() = default;

void DiskMarkerBuffer::Rebuild(const glm::vec3& center, const glm::vec3& normal,
                               float radius, const glm::vec4& color,
                               int segments) {
    const std::size_t h = HashState(center, normal, radius, color, segments);
    if (hasPrevious_ && h == lastHash_ && !vertices_.empty()) return;

    auto newVerts = BuildDiskVertices(center, normal, radius, color, segments);

    if (vertexBuffer_ || !newVerts.empty()) {
        vkDeviceWaitIdle(device_.GetDevice());
    }

    if (!newVerts.empty()) {
        vertexBuffer_ = std::make_unique<Buffer>(
            device_, BufferType::Vertex,
            newVerts.size() * sizeof(DiskVertex), newVerts.data());
    } else {
        vertexBuffer_.reset();
    }

    vertices_ = std::move(newVerts);
    lastHash_ = h;
    hasPrevious_ = true;
}

void DiskMarkerBuffer::Clear() {
    if (vertices_.empty() && !vertexBuffer_) return;
    vkDeviceWaitIdle(device_.GetDevice());
    vertices_.clear();
    vertexBuffer_.reset();
    hasPrevious_ = false;
}

VkBuffer DiskMarkerBuffer::GetVertexBuffer() const {
    return vertexBuffer_ ? vertexBuffer_->GetBuffer() : VK_NULL_HANDLE;
}

}  // namespace bimeup::renderer
