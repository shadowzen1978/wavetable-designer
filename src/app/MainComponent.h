#pragma once
#include "app/WaveformView.h"

namespace wtd {
class MainComponent final : public juce::Component {
public:
    MainComponent();
    ~MainComponent() override;
    void setDarkTheme(bool dark);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void applyTheme();
    void updateWaveform();
    juce::LookAndFeel_V4 lookAndFeel_;
    Waveform waveform_;
    WaveformView waveformView_ { waveform_ };
    juce::ComboBox shape_, length_;
    juce::TextButton themeButton_;
    bool dark_ = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
} // namespace wtd
