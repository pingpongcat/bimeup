#pragma once

#include <limits>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace bimeup::scene {

class AABB {
public:
    AABB() = default;

    AABB(const glm::vec3& min, const glm::vec3& max)
        : min_(min), max_(max), valid_(true) {}

    static AABB FromVertices(std::span<const glm::vec3> vertices) {
        if (vertices.empty()) {
            return {};
        }
        glm::vec3 lo = vertices[0];
        glm::vec3 hi = vertices[0];
        for (size_t i = 1; i < vertices.size(); ++i) {
            lo = glm::min(lo, vertices[i]);
            hi = glm::max(hi, vertices[i]);
        }
        return {lo, hi};
    }

    static AABB Merge(const AABB& a, const AABB& b) {
        if (!a.valid_) return b;
        if (!b.valid_) return a;
        return {glm::min(a.min_, b.min_), glm::max(a.max_, b.max_)};
    }

    void ExpandToInclude(const glm::vec3& point) {
        if (!valid_) {
            min_ = point;
            max_ = point;
            valid_ = true;
        } else {
            min_ = glm::min(min_, point);
            max_ = glm::max(max_, point);
        }
    }

    [[nodiscard]] bool Contains(const glm::vec3& point) const {
        return valid_ &&
               point.x >= min_.x && point.x <= max_.x &&
               point.y >= min_.y && point.y <= max_.y &&
               point.z >= min_.z && point.z <= max_.z;
    }

    [[nodiscard]] bool IsValid() const { return valid_; }
    [[nodiscard]] const glm::vec3& GetMin() const { return min_; }
    [[nodiscard]] const glm::vec3& GetMax() const { return max_; }
    [[nodiscard]] glm::vec3 GetCenter() const { return (min_ + max_) * 0.5f; }
    [[nodiscard]] glm::vec3 GetSize() const { return max_ - min_; }

private:
    glm::vec3 min_{0.0f};
    glm::vec3 max_{0.0f};
    bool valid_ = false;
};

} // namespace bimeup::scene
