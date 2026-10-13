#pragma once
#include "app/Theme.h"
#include "core/Waveform.h"

namespace wtd {
class WaveformView final : public juce::Component {
public:
    explicit WaveformView(const Waveform& waveform) : waveform_(waveform) {}
    void setDark(bool dark) { dark_ = dark; repaint(); }
    void paint(juce::Graphics&) override;
private:
    const Waveform& waveform_;
    bool dark_ = true;
};
} // namespace wtd
