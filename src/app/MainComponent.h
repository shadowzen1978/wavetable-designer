#pragma once
#include "app/WaveformView.h"
#include "audio/AudioPreview.h"
#include "io/ProjectFile.h"
#include "io/Export.h"

namespace wtd {
class MainComponent final : public juce::Component, private juce::Timer {
public:
    explicit MainComponent(bool enableAudioDevices = true);
    ~MainComponent() override;
    void setDarkTheme(bool dark);
    void requestQuit(std::function<void()> quit);
    bool runFileSmokeCheck(const juce::File& directory);
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
    void newProject();
    void openProject();
    void saveProject(bool saveAs, std::function<void()> afterSave = {});
    void exportFile();
    void confirmDiscard(std::function<void()> action);
    void confirmOverwrite(const juce::File& file, std::function<void()> write);
    bool loadProjectFile(const juce::File& file);
    bool saveProjectFile(const juce::File& file);
    bool exportToFile(const juce::File& file, const Waveform& wave, ExportOptions options);
    void fileError(const juce::String& message);
    void updateFileControls();
    [[nodiscard]] bool documentDirty() const;
    [[nodiscard]] ExportOptions exportOptions() const;
    [[nodiscard]] juce::String exportSummary() const;
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
    juce::TextButton new_ { "New" }, open_ { "Open..." }, save_ { "Save" }, saveAs_ { "Save as..." },
                     export_ { "Export..." };
    juce::ComboBox exportProfile_, exportFormat_, exportRate_;
    juce::File projectFile_;
    ProjectData savedProject_ { Waveform(), Shape::sine, std::nullopt };
    std::unique_ptr<juce::FileChooser> fileChooser_;
    juce::String fileStatus_;
    juce::int64 fileStatusUntil_ = 0;
    bool fileActionPending_ = false;
    bool dark_ = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
} // namespace wtd
