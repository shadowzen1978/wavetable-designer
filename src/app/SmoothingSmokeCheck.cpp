#include "app/MainComponent.h"

namespace wtd {
bool MainComponent::runSmoothingSmokeCheck(const juce::File& directory) {
    struct Case { std::size_t length; int width, height; const char* name; };
    for (const auto test : { Case { 1024, 960, 640, "1024" },
                            Case { 2048, 960, 640, "2048" },
                            Case { 2048, 640, 480, "small" } }) {
        editor_ = EditorModel(test.length);
        editor_.applyPreset(Shape::square);
        setSize(test.width, test.height);
        smoothingStrength_.setValue(40.0, juce::dontSendNotification);
        syncControls();
        const auto original = editor_.waveform();
        smooth_.onClick();
        const auto preview = editor_.waveform();
        if (!editor_.smoothingActive() || !smoothingStrength_.isVisible()
            || !waveformView_.isEnabled() || waveformView_.selectionMode() || shape_.isEnabled() || length_.isEnabled()
            || clear_.isEnabled() || smooth_.isEnabled() || redo_.isEnabled()) return false;
        smoothingStrength_.setValue(80.0, juce::sendNotificationSync);
        smoothingStrength_.setValue(40.0, juce::sendNotificationSync);
        if (!(editor_.waveform() == preview) || !(*editor_.smoothingSource() == original)) return false;
        cancelSmoothing_.onClick();
        if (!(editor_.waveform() == original) || !waveformView_.isEnabled()
            || smoothingStrength_.isVisible() || shape_.getSelectedId() != 4) return false;
        smooth_.onClick();
        if (!keyPressed(juce::KeyPress(juce::KeyPress::escapeKey))
            || !(editor_.waveform() == original) || editor_.smoothingActive()) return false;
        smooth_.onClick();
        smoothingStrength_.setValue(100.0, juce::sendNotificationSync);
        const auto strong = editor_.waveform();
        for (const bool dark : { true, false }) {
            setDarkTheme(dark);
            const auto name = juce::String("smoothing-") + (dark ? "dark-" : "light-") + test.name + ".png";
            auto stream = directory.getChildFile(name).createOutputStream();
            if (!stream) return false;
            stream->setPosition(0);
            stream->truncate();
            if (!juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()), *stream)) return false;
        }
        applySmoothing_.onClick();
        if (editor_.smoothingActive() || !(editor_.waveform() == strong)
            || !waveformView_.isEnabled() || smoothingStrength_.isVisible()) return false;
        undo_.onClick();
        if (!(editor_.waveform() == original)) return false;
        redo_.onClick();
        if (!(editor_.waveform() == strong)) return false;
        smooth_.onClick();
        if (!keyPressed(juce::KeyPress('z', juce::ModifierKeys(juce::ModifierKeys::commandModifier), 0))
            || !(editor_.waveform() == strong) || editor_.smoothingActive()) return false;
        smooth_.onClick();
        smoothingStrength_.setValue(0.0, juce::sendNotificationSync);
        if (!keyPressed(juce::KeyPress(juce::KeyPress::returnKey))
            || !(editor_.waveform() == strong) || editor_.smoothingActive()) return false;
    }
    editor_ = EditorModel();
    smoothingStrength_.setValue(50.0, juce::dontSendNotification);
    setSize(960, 640);
    setDarkTheme(true);
    syncControls();
    return true;
}
} // namespace wtd
