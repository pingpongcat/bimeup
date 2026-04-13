#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

namespace bimeup::scene {

struct MeasureResult {
    glm::vec3 pointA{0.0f};
    glm::vec3 pointB{0.0f};
    float distance{0.0f};
    glm::vec3 deltaXYZ{0.0f};
};

MeasureResult Measure(const glm::vec3& a, const glm::vec3& b);

class MeasureTool {
public:
    enum class State : std::uint8_t {
        Idle,
        AwaitingSecondPoint,
        Complete,
    };

    void AddPoint(const glm::vec3& worldPos);
    void Reset();
    /// Discard the current in-progress measurement (the first point if pending).
    /// Saved history is preserved.
    void Cancel();
    /// Erase all saved measurements.
    void ClearMeasurements();

    [[nodiscard]] State GetState() const { return state_; }
    [[nodiscard]] const std::optional<glm::vec3>& GetFirstPoint() const { return first_; }
    [[nodiscard]] const std::optional<MeasureResult>& GetResult() const { return result_; }
    [[nodiscard]] const std::vector<MeasureResult>& GetMeasurements() const { return saved_; }

private:
    State state_{State::Idle};
    std::optional<glm::vec3> first_;
    std::optional<MeasureResult> result_;
    std::vector<MeasureResult> saved_;
};

} // namespace bimeup::scene
