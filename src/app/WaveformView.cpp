#include "app/WaveformView.h"

namespace wtd {
void WaveformView::paint(juce::Graphics& g) {
    const auto theme = themeFor(dark_);
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(theme.panel);
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(theme.grid);
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    const auto plot = bounds.reduced(28.0f, 24.0f);
    for (int division = 0; division <= 8; ++division) {
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(division) / 8.0f;
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
    }
    g.drawHorizontalLine(juce::roundToInt(plot.getCentreY()), plot.getX(), plot.getRight());

    juce::Path path;
    const auto samples = waveform_.samples();
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
} // namespace wtd
