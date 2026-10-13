#pragma once
#include "core/Waveform.h"
#include <deque>
#include <optional>

namespace wtd {
// Editable cycle and bounded snapshot history, independent of GUI/event APIs.
class EditorModel {
public:
    static constexpr std::size_t historyLimit = 128;
    explicit EditorModel(std::size_t sampleCount = 1024);
    [[nodiscard]] const Waveform& waveform() const noexcept { return state_.waveform; }
    [[nodiscard]] std::optional<Shape> preset() const noexcept { return state_.preset; }
    [[nodiscard]] bool strokeActive() const noexcept { return stroke_.has_value(); }
    [[nodiscard]] bool canUndo() const noexcept { return strokeActive() || !undo_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !strokeActive() && !redo_.empty(); }

    // Phase is a normalized canvas x position; clamp gestures at the cycle edges.
    void beginStroke(double phase, float amplitude);
    void continueStroke(double phase, float amplitude);
    bool endStroke();
    bool cancelStroke();
    bool applyPreset(Shape shape);
    bool resizeCycle(std::size_t sampleCount);
    bool undo();
    bool redo();

private:
    struct State { Waveform waveform; std::optional<Shape> preset; };
    struct Point { std::size_t index; float amplitude; };
    struct Stroke { State before; Point previous; };
    [[nodiscard]] Point pointFor(double phase, float amplitude) const;
    bool remember(State before);
    State state_;
    std::optional<Stroke> stroke_;
    std::deque<State> undo_, redo_;
};
} // namespace wtd
