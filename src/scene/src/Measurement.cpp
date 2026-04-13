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
        MeasureResult r = Measure(*first_, worldPos);
        saved_.push_back(r);
        result_ = r;
        state_ = State::Complete;
        first_.reset();
        return;
    }
    if (state_ == State::Complete) {
        result_.reset();
    }
    first_ = worldPos;
    state_ = State::AwaitingSecondPoint;
}

void MeasureTool::Reset() {
    state_ = State::Idle;
    first_.reset();
    result_.reset();
    saved_.clear();
}

void MeasureTool::Cancel() {
    state_ = State::Idle;
    first_.reset();
    result_.reset();
}

void MeasureTool::ClearMeasurements() {
    saved_.clear();
}

void MeasureTool::RemoveMeasurement(std::size_t index) {
    if (index >= saved_.size()) {
        return;
    }
    saved_.erase(saved_.begin() + static_cast<std::ptrdiff_t>(index));
}

void MeasureTool::SetVisibility(std::size_t index, bool visible) {
    if (index >= saved_.size()) {
        return;
    }
    saved_[index].visible = visible;
}

void MeasureTool::SetAllVisibility(bool visible) {
    for (auto& r : saved_) {
        r.visible = visible;
    }
}

} // namespace bimeup::scene
