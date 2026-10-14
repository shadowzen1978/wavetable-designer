#pragma once
#include "app/WaveformView.h"
#include "audio/AudioPreview.h"

namespace wtd {
class MainComponent final : public juce::Component, private juce::Timer {
public:
    explicit MainComponent(bool enableAudioDevices = true);
    ~MainComponent() override;
    void setDarkTheme(bool dark);
    bool runDrawingSmokeCheck(const juce::File& directory);
    bool runSmoothingSmokeCheck(const juce::File& directory);
    bool runRegionalSmoothingSmokeCheck(const juce::File& directory);
    bool runEdgeSmoothingSmokeCheck(const juce::File& directory);
    bool runJitterSmokeCheck(const juce::File& directory);
    bool runAudioSmokeCheck(const juce::File& directory);
    void runAudioDeviceCheck(const juce::File& directory, std::function<void(bool)> finished);
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    void applyTheme();
    void timerCallback() override;
    void updateAudioControls();
    void showAudioSettings();
    void syncControls();
    void startSmoothing();
    void updateSmoothingPreview();
    juce::LookAndFeel_V4 lookAndFeel_;
    EditorModel editor_;
    WaveformView waveformView_ { editor_ };
    AudioPreview audioPreview_;
    std::optional<Waveform> submittedWaveform_;
    std::unique_ptr<juce::DialogWindow> audioSettings_;
    juce::String audioError_;
    juce::TextButton play_ { "Play" }, audioSettingsButton_ { "Audio settings" };
    juce::ComboBox note_;
    juce::Slider previewVolume_;
    juce::ComboBox shape_, length_, zoom_;
    juce::ToggleButton selectRegion_ { "Select region" }, preserveEdges_ { "Preserve edges" },
                       reduceJitter_ { "Reduce jitter" };
    juce::TextButton clearSelection_ { "Clear" };
    juce::TextButton undo_ { "Undo" }, redo_ { "Redo" }, clear_ { "Clear" }, themeButton_;
    juce::TextButton smooth_ { "Smooth..." }, applySmoothing_ { "Apply" }, cancelSmoothing_ { "Cancel" };
    juce::Slider smoothingStrength_, edgeAngle_;
    bool dark_ = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
} // namespace wtd
