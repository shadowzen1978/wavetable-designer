#pragma once
#include "app/WaveformView.h"

namespace wtd {
class MainComponent final : public juce::Component {
public:
    MainComponent();
    ~MainComponent() override;
    void setDarkTheme(bool dark);
    bool runDrawingSmokeCheck(const juce::File& directory);
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    void applyTheme();
    void syncControls();
    juce::LookAndFeel_V4 lookAndFeel_;
    EditorModel editor_;
    WaveformView waveformView_ { editor_ };
    juce::ComboBox shape_, length_;
    juce::TextButton undo_ { "Undo" }, redo_ { "Redo" }, clear_ { "Clear" }, themeButton_;
    bool dark_ = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
} // namespace wtd
