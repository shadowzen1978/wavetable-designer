#include "core/EditorModel.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace wtd {
EditorModel::EditorModel(std::size_t sampleCount)
    : state_ { Waveform(sampleCount), Shape::sine } {}

EditorModel::Point EditorModel::pointFor(double phase, float amplitude) const {
    if (!std::isfinite(phase) || !std::isfinite(amplitude))
        throw std::invalid_argument("Stroke coordinates must be finite");
    const auto index = std::min(state_.waveform.size() - 1,
        static_cast<std::size_t>(std::floor(std::clamp(phase, 0.0, 1.0)
            * static_cast<double>(state_.waveform.size()) + 0.5)));
    return { index, std::clamp(amplitude, -1.0f, 1.0f) };
}

void EditorModel::beginStroke(double phase, float amplitude) {
    const auto point = pointFor(phase, amplitude);
    endStroke();
    stroke_ = Stroke { state_, point };
    state_.waveform.setSample(point.index, point.amplitude);
    state_.preset.reset();
}

void EditorModel::continueStroke(double phase, float amplitude) {
    if (!stroke_) return;
    const auto point = pointFor(phase, amplitude);
    state_.waveform.drawSegment(stroke_->previous.index, stroke_->previous.amplitude,
                               point.index, point.amplitude);
    stroke_->previous = point;
    state_.preset.reset();
}

bool EditorModel::remember(State before) {
    if (before.waveform == state_.waveform) return false;
    undo_.push_back(std::move(before));
    if (undo_.size() > historyLimit) undo_.pop_front();
    redo_.clear();
    return true;
}

bool EditorModel::endStroke() {
    if (!stroke_) return false;
    auto before = std::move(stroke_->before);
    stroke_.reset();
    if (before.waveform == state_.waveform) {
        state_.preset = before.preset;
        return false;
    }
    return remember(std::move(before));
}

bool EditorModel::cancelStroke() {
    if (!stroke_) return false;
    const bool changed = !(state_.waveform == stroke_->before.waveform);
    state_ = std::move(stroke_->before);
    stroke_.reset();
    return changed;
}

bool EditorModel::applyPreset(Shape shape) {
    endStroke();
    auto before = state_;
    state_.waveform.fill(shape);
    state_.preset = shape;
    return remember(std::move(before));
}

bool EditorModel::resizeCycle(std::size_t sampleCount) {
    endStroke();
    if (sampleCount == state_.waveform.size()) return false;
    auto resampled = state_.waveform.resampled(sampleCount);
    auto before = state_;
    state_.waveform = std::move(resampled);
    return remember(std::move(before));
}

bool EditorModel::undo() {
    if (stroke_) { cancelStroke(); return true; }
    if (undo_.empty()) return false;
    redo_.push_back(std::move(state_));
    state_ = std::move(undo_.back());
    undo_.pop_back();
    return true;
}

bool EditorModel::redo() {
    if (!canRedo()) return false;
    undo_.push_back(std::move(state_));
    state_ = std::move(redo_.back());
    redo_.pop_back();
    return true;
}
} // namespace wtd
