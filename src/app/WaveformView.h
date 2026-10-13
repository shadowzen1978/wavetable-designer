#pragma once
#include "app/Theme.h"
#include "core/EditorModel.h"
#include <functional>

namespace wtd {
class WaveformView final : public juce::Component {
public:
    explicit WaveformView(EditorModel& editor);
    std::function<void()> onChanged;
    void setDark(bool dark) { dark_ = dark; repaint(); }
    [[nodiscard]] juce::Rectangle<float> plotBounds() const;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusLost(FocusChangeType) override;
private:
    void drawPoint(juce::Point<float> position, bool begin);
    void changed();
    EditorModel& editor_;
    bool dark_ = true;
    bool drawing_ = false;
};
} // namespace wtd
