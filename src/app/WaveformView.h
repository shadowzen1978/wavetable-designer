#pragma once
#include "app/Theme.h"
#include "core/EditorModel.h"
#include "core/HorizontalViewport.h"
#include <functional>

namespace wtd {
class WaveformView final : public juce::Component, private juce::ScrollBar::Listener {
public:
    explicit WaveformView(EditorModel& editor);
    std::function<void()> onChanged;
    bool cancelSelectionGesture();
    void setSelectionMode(bool selected);
    [[nodiscard]] bool selectionMode() const noexcept { return selectionMode_; }
    void setDark(bool dark);
    void setZoom(double factor);
    void setSampleZoom();
    void panTo(double phase);
    void refreshViewport();
    [[nodiscard]] const HorizontalViewport& viewport() const noexcept { return viewport_; }
    [[nodiscard]] std::optional<std::size_t> cursorSample() const noexcept { return cursorSample_; }
    [[nodiscard]] double phaseAtX(float x) const;
    [[nodiscard]] float sampleX(std::size_t index) const;
    [[nodiscard]] juce::Rectangle<float> plotBounds() const;
    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusLost(FocusChangeType) override;
private:
    enum class Boundary { none, first, last };
    void drawPoint(juce::Point<float> position, bool begin);
    void changed();
    void updateCursor(juce::Point<float>);
    void syncScrollbar();
    void scrollBarMoved(juce::ScrollBar*, double start) override;
    EditorModel& editor_;
    HorizontalViewport viewport_;
    juce::ScrollBar scrollbar_ { false };
    bool sampleZoom_ = false;
    std::size_t displayedSampleCount_ = 0;
    bool dark_ = true, drawing_ = false;
    bool selectionMode_ = false, selecting_ = false;
    Boundary boundary_ = Boundary::none;
    double selectionAnchor_ = 0.0;
    std::optional<SampleRange> selectionBefore_;
    std::optional<std::size_t> cursorSample_;
};
} // namespace wtd
