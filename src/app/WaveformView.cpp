#include "app/WaveformView.h"

namespace wtd {
WaveformView::WaveformView(EditorModel& editor) : editor_(editor) {
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

juce::Rectangle<float> WaveformView::plotBounds() const {
    return getLocalBounds().toFloat().reduced(29.0f, 25.0f);
}

void WaveformView::paint(juce::Graphics& g) {
    const auto theme = themeFor(dark_);
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(theme.panel);
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(theme.grid);
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    const auto plot = plotBounds();
    for (int division = 0; division <= 8; ++division) {
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(division) / 8.0f;
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
    }
    g.drawHorizontalLine(juce::roundToInt(plot.getCentreY()), plot.getX(), plot.getRight());

    juce::Path path;
    const auto samples = editor_.waveform().samples();
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(i)
            / static_cast<float>(samples.size());
        const float y = plot.getCentreY() - samples[i] * plot.getHeight() * 0.5f;
        if (i == 0) path.startNewSubPath(x, y);
        else path.lineTo(x, y);
    }
    g.setColour(theme.accent);
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

void WaveformView::changed() {
    repaint();
    if (onChanged) onChanged();
}

void WaveformView::drawPoint(juce::Point<float> position, bool begin) {
    const auto plot = plotBounds();
    const double phase = (position.x - plot.getX()) / plot.getWidth();
    const float amplitude = (plot.getCentreY() - position.y) * 2.0f / plot.getHeight();
    if (begin) editor_.beginStroke(phase, amplitude);
    else editor_.continueStroke(phase, amplitude);
    changed();
}

void WaveformView::mouseDown(const juce::MouseEvent& event) {
    if (!event.mods.isLeftButtonDown() || event.mods.isPopupMenu()
        || !plotBounds().contains(event.position)) return;
    if (isShowing()) grabKeyboardFocus();
    drawing_ = true;
    drawPoint(event.position, true);
}

void WaveformView::mouseDrag(const juce::MouseEvent& event) {
    if (drawing_ && editor_.strokeActive()) drawPoint(event.position, false);
}

void WaveformView::mouseUp(const juce::MouseEvent& event) {
    if (!drawing_) return;
    drawing_ = false;
    if (editor_.strokeActive()) {
        drawPoint(event.position, false);
        editor_.endStroke();
        changed();
    }
}

bool WaveformView::keyPressed(const juce::KeyPress& key) {
    if (key.getKeyCode() != juce::KeyPress::escapeKey || !editor_.strokeActive()) return false;
    drawing_ = false;
    editor_.cancelStroke();
    changed();
    return true;
}

void WaveformView::focusLost(FocusChangeType) {
    drawing_ = false;
    if (editor_.strokeActive()) { editor_.endStroke(); changed(); }
}
} // namespace wtd
