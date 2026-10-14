#pragma once
#include "core/Waveform.h"
#include <deque>
#include <optional>

namespace wtd {
enum class SmoothingScope { wholeCycle, selection };

// Editable cycle and bounded snapshot history, independent of GUI/event APIs.
class EditorModel {
public:
    static constexpr std::size_t historyLimit = 128;
    explicit EditorModel(std::size_t sampleCount = 1024);
    [[nodiscard]] const Waveform& waveform() const noexcept { return state_.waveform; }
    [[nodiscard]] std::optional<Shape> preset() const noexcept { return state_.preset; }
    [[nodiscard]] std::optional<SampleRange> selection() const noexcept { return state_.selection; }
    void selectRegion(double startPhase, double endPhase);
    void setSelection(std::optional<SampleRange> range);
    [[nodiscard]] SmoothingScope smoothingScope() const noexcept { return smoothingScope_; }
    [[nodiscard]] bool strokeActive() const noexcept { return stroke_.has_value(); }
    [[nodiscard]] bool smoothingActive() const noexcept { return smoothingBefore_.has_value(); }
    [[nodiscard]] const Waveform* smoothingSource() const noexcept {
        return smoothingBefore_ ? &smoothingBefore_->waveform : nullptr;
    }
    [[nodiscard]] bool canUndo() const noexcept { return smoothingActive() || strokeActive() || !undo_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !smoothingActive() && !strokeActive() && !redo_.empty(); }

    // Phase is a normalized canvas x position; clamp gestures at the cycle edges.
    void beginStroke(double phase, float amplitude);
    void continueStroke(double phase, float amplitude);
    bool endStroke();
    bool cancelStroke();
    void beginSmoothing();
    void previewSmoothing(double strength, SmoothingScope scope = SmoothingScope::wholeCycle,
                          SmoothingOptions options = {});
    bool applySmoothing();
    bool cancelSmoothing();
    bool applyPreset(Shape shape);
    bool resizeCycle(std::size_t sampleCount);
    bool undo();
    bool redo();
    // New/Open replace the document and clear history; validate before changing state.
    void loadDocument(Waveform waveform, std::optional<Shape> preset, std::optional<SampleRange> selection);

private:
    struct State { Waveform waveform; std::optional<Shape> preset; std::optional<SampleRange> selection; };
    struct Point { std::size_t index; float amplitude; };
    struct Stroke { State before; Point previous; };
    [[nodiscard]] Point pointFor(double phase, float amplitude) const;
    bool remember(State before);
    State state_;
    std::optional<Stroke> stroke_;
    std::optional<State> smoothingBefore_;
    SmoothingScope smoothingScope_ = SmoothingScope::wholeCycle;
    std::deque<State> undo_, redo_;
};
} // namespace wtd
