#include <scene/Measurement.h>

namespace bimeup::scene {

MeasureResult Measure(const glm::vec3& a, const glm::vec3& b) {
    MeasureResult r;
    r.pointA = a;
    r.pointB = b;
    r.deltaXYZ = b - a;
    r.distance = glm::length(r.deltaXYZ);
    return r;
}

void MeasureTool::AddPoint(const glm::vec3& worldPos) {
    if (state_ == State::AwaitingSecondPoint && first_.has_value()) {
        result_ = Measure(*first_, worldPos);
        state_ = State::Complete;
        return;
    }
    first_ = worldPos;
    result_.reset();
    state_ = State::AwaitingSecondPoint;
}

void MeasureTool::Reset() {
    state_ = State::Idle;
    first_.reset();
    result_.reset();
}

} // namespace bimeup::scene
