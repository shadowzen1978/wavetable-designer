#include "app/WaveformView.h"
#include <cmath>

namespace wtd {
WaveformView::WaveformView(EditorModel& editor) : editor_(editor) {
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    scrollbar_.setRangeLimits(0.0, 1.0);
    scrollbar_.addListener(this);
    addChildComponent(scrollbar_);
}
void WaveformView::setSelectionMode(bool selected) {
    if (selectionMode_ == selected) return;
    selectionMode_ = selected;
    selecting_ = false;
    cursorSample_.reset();
    setMouseCursor(selected ? juce::MouseCursor::CrosshairCursor
        : editor_.smoothingActive() ? juce::MouseCursor::NormalCursor : juce::MouseCursor::CrosshairCursor);
    repaint();
}
void WaveformView::setDark(bool dark) {
    dark_ = dark;
    scrollbar_.setColour(juce::ScrollBar::thumbColourId, themeFor(dark_).muted);
    repaint();
}
juce::Rectangle<float> WaveformView::plotBounds() const {
    return getLocalBounds().toFloat().reduced(29.0f, 25.0f);
}
double WaveformView::phaseAtX(float x) const {
    const auto plot = plotBounds();
    return viewport_.phaseAt(std::clamp(static_cast<double>((x - plot.getX()) / plot.getWidth()), 0.0, 1.0));
}
float WaveformView::sampleX(std::size_t index) const {
    return plotBounds().getX() + plotBounds().getWidth() * static_cast<float>(
        viewport_.positionOf(static_cast<double>(index) / static_cast<double>(editor_.waveform().size())));
}
void WaveformView::syncScrollbar() {
    scrollbar_.setCurrentRange(viewport_.start(), viewport_.span(), juce::dontSendNotification);
    scrollbar_.setSingleStepSize(viewport_.span() * 0.1);
    scrollbar_.setVisible(viewport_.span() < 1.0);
    cursorSample_.reset();
    repaint();
}
void WaveformView::setZoom(double factor) {
    sampleZoom_ = false;
    double anchor = viewport_.phaseAt(0.5);
    if (viewport_.span() == 1.0 && editor_.selection()) {
        const auto range = *editor_.selection();
        anchor = static_cast<double>(range.first + range.last) / (2.0 * static_cast<double>(editor_.waveform().size()));
    }
    viewport_.zoom(factor, anchor);
    syncScrollbar();
}
void WaveformView::setSampleZoom() {
    setZoom(static_cast<double>(editor_.waveform().size())
        / std::max(2.0, std::floor(plotBounds().getWidth() / 12.0)));
    sampleZoom_ = true;
}
void WaveformView::panTo(double phase) { viewport_.pan(phase); syncScrollbar(); }
void WaveformView::refreshViewport() {
    if (displayedSampleCount_ == editor_.waveform().size()) return;
    displayedSampleCount_ = editor_.waveform().size();
    if (sampleZoom_) setSampleZoom();
    else { cursorSample_.reset(); repaint(); }
}
void WaveformView::scrollBarMoved(juce::ScrollBar*, double start) { panTo(start); }
void WaveformView::resized() {
    const auto plot = plotBounds();
    scrollbar_.setBounds(juce::roundToInt(plot.getX()), getHeight() - 19, juce::roundToInt(plot.getWidth()), 12);
    if (sampleZoom_) setSampleZoom();
}
void WaveformView::paint(juce::Graphics& g) {
    const auto theme = themeFor(dark_);
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(theme.panel);
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(theme.grid);
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);
    const auto plot = plotBounds();
    const auto selection = editor_.selection();
    const bool showSelection = selectionMode_ && selection;
    {
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(plot.toNearestInt());
        if (showSelection) {
            const float left = sampleX(selection->first);
            const float right = sampleX(selection->last);
            g.setColour(theme.accent.withAlpha(0.12f));
            g.fillRect(juce::Rectangle<float>(left, plot.getY(), right - left, plot.getHeight()));
        }
        g.setColour(theme.grid);
        const double samplesPerPixel = viewport_.span() * static_cast<double>(editor_.waveform().size()) / plot.getWidth();
        if (samplesPerPixel <= 0.25) {
            const auto begin = viewport_.sampleAt(0.0, editor_.waveform().size());
            const auto end = viewport_.sampleAt(1.0, editor_.waveform().size());
            for (auto i = begin; i <= end; ++i)
                g.drawVerticalLine(juce::roundToInt(sampleX(i)), plot.getY(), plot.getBottom());
        } else {
            for (int division = 0; division <= 8; ++division) {
                const float x = plot.getX() + plot.getWidth() * static_cast<float>(division) / 8.0f;
                g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
            }
        }
        g.drawHorizontalLine(juce::roundToInt(plot.getCentreY()), plot.getX(), plot.getRight());
        const auto drawWave = [&](const Waveform& wave, juce::Colour colour, float thickness, bool dots) {
            juce::Path path;
            const auto samples = wave.samples();
            const auto firstVisible = viewport_.sampleAt(0.0, samples.size());
            const auto first = firstVisible > 0 ? firstVisible - 1 : 0;
            const auto last = std::min(samples.size() - 1, viewport_.sampleAt(1.0, samples.size()) + 1);
            for (auto i = first; i <= last; ++i) {
                const float x = sampleX(i);
                const float y = plot.getCentreY() - samples[i] * plot.getHeight() * 0.5f;
                if (i == first) path.startNewSubPath(x, y); else path.lineTo(x, y);
                if (dots) { g.setColour(colour); g.fillEllipse(x - 2.0f, y - 2.0f, 4.0f, 4.0f); }
            }
            g.setColour(colour);
            g.strokePath(path, juce::PathStrokeType(thickness));
        };
        if (const auto* source = editor_.smoothingSource())
            drawWave(*source, theme.muted.withAlpha(0.4f), 1.0f, false);
        drawWave(editor_.waveform(), theme.accent, 2.0f, samplesPerPixel <= 0.125);
        if (showSelection) {
            for (const auto index : { selection->first, selection->last }) {
                const float x = sampleX(index);
                g.setColour(theme.accent);
                g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
                g.fillRoundedRectangle(x - 4.0f, plot.getY(), 8.0f, 16.0f, 3.0f);
                const float y = plot.getCentreY() - editor_.waveform().samples()[index] * plot.getHeight() * 0.5f;
                g.fillEllipse(x - 3.0f, y - 3.0f, 6.0f, 6.0f);
            }
        }
        if (selectionMode_ && cursorSample_) {
            g.setColour(theme.text.withAlpha(0.7f));
            g.drawVerticalLine(juce::roundToInt(sampleX(*cursorSample_)), plot.getY(), plot.getBottom());
        }
    }
    g.setColour(theme.muted);
    g.setFont(juce::FontOptions(12.0f));
    juce::String caption;
    if (selectionMode_ && cursorSample_) caption = "Sample " + juce::String(static_cast<int>(*cursorSample_));
    else if (viewport_.span() < 1.0) caption = "Samples "
        + juce::String(static_cast<int>(viewport_.sampleAt(0.0, editor_.waveform().size()))) + " – "
        + juce::String(static_cast<int>(viewport_.sampleAt(1.0, editor_.waveform().size())));
    g.drawText(caption, juce::roundToInt(plot.getX()), 3, juce::roundToInt(plot.getWidth()), 20, juce::Justification::left);
}
void WaveformView::changed() { repaint(); if (onChanged) onChanged(); }
void WaveformView::drawPoint(juce::Point<float> position, bool begin) {
    const auto plot = plotBounds();
    const float amplitude = (plot.getCentreY() - position.y) * 2.0f / plot.getHeight();
    if (begin) editor_.beginStroke(phaseAtX(position.x), amplitude);
    else editor_.continueStroke(phaseAtX(position.x), amplitude);
    changed();
}
void WaveformView::updateCursor(juce::Point<float> position) {
    if (!selectionMode_ || !plotBounds().contains(position)) { cursorSample_.reset(); repaint(); return; }
    const auto plot = plotBounds();
    cursorSample_ = viewport_.sampleAt((position.x - plot.getX()) / plot.getWidth(), editor_.waveform().size());
    bool nearBoundary = false;
    if (const auto range = editor_.selection())
        nearBoundary = std::min(std::abs(position.x - sampleX(range->first)), std::abs(position.x - sampleX(range->last))) <= 6.0f;
    setMouseCursor(nearBoundary ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::CrosshairCursor);
    repaint();
}
void WaveformView::mouseMove(const juce::MouseEvent& event) { updateCursor(event.position); }
void WaveformView::mouseExit(const juce::MouseEvent&) { cursorSample_.reset(); repaint(); }
void WaveformView::mouseDown(const juce::MouseEvent& event) {
    const auto plot = plotBounds();
    if (!isEnabled() || !event.mods.isLeftButtonDown() || event.mods.isPopupMenu()
        || event.position.x < plot.getX() || event.position.x > plot.getRight()
        || event.position.y < plot.getY() || event.position.y > plot.getBottom()) return;
    if (isShowing()) grabKeyboardFocus();
    if (selectionMode_ && editor_.smoothingActive()) {
        selecting_ = true;
        selectionBefore_ = editor_.selection();
        selectionAnchor_ = phaseAtX(event.position.x);
        boundary_ = Boundary::none;
        if (selectionBefore_ && selectionBefore_->first != selectionBefore_->last) {
            const float a = std::abs(event.position.x - sampleX(selectionBefore_->first));
            const float b = std::abs(event.position.x - sampleX(selectionBefore_->last));
            if (std::min(a, b) <= 6.0f) boundary_ = a <= b ? Boundary::first : Boundary::last;
        }
        if (boundary_ == Boundary::none) editor_.selectRegion(selectionAnchor_, selectionAnchor_);
        updateCursor(event.position);
        changed();
    } else if (!editor_.smoothingActive()) {
        drawing_ = true; drawPoint(event.position, true);
    }
}
void WaveformView::mouseDrag(const juce::MouseEvent& event) {
    if (selecting_) {
        const auto index = viewport_.sampleAt(std::clamp(static_cast<double>((event.position.x - plotBounds().getX()) / plotBounds().getWidth()), 0.0, 1.0),
                                             editor_.waveform().size());
        if (boundary_ == Boundary::first)
            editor_.setSelection(SampleRange { std::min(index, selectionBefore_->last), selectionBefore_->last });
        else if (boundary_ == Boundary::last)
            editor_.setSelection(SampleRange { selectionBefore_->first, std::max(index, selectionBefore_->first) });
        else editor_.selectRegion(selectionAnchor_, phaseAtX(event.position.x));
        updateCursor(event.position);
        changed();
    } else if (drawing_ && editor_.strokeActive()) drawPoint(event.position, false);
}
void WaveformView::mouseUp(const juce::MouseEvent& event) {
    if (selecting_) { mouseDrag(event); selecting_ = false; return; }
    if (!drawing_) return;
    drawing_ = false;
    if (editor_.strokeActive()) { drawPoint(event.position, false); editor_.endStroke(); changed(); }
}
bool WaveformView::cancelSelectionGesture() {
    if (!selecting_) return false;
    selecting_ = false;
    editor_.setSelection(selectionBefore_);
    changed();
    return true;
}
bool WaveformView::keyPressed(const juce::KeyPress& key) {
    if (key.getKeyCode() != juce::KeyPress::escapeKey) return false;
    if (cancelSelectionGesture()) return true;
    if (editor_.smoothingActive()) return false;
    if (!editor_.strokeActive()) return false;
    drawing_ = false;
    editor_.cancelStroke();
    changed();
    return true;
}
void WaveformView::focusLost(FocusChangeType) {
    drawing_ = false; selecting_ = false;
    if (editor_.strokeActive()) { editor_.endStroke(); changed(); }
}
} // namespace wtd
