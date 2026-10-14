#include "app/MainComponent.h"
#include <algorithm>
#include <cmath>

namespace wtd {
bool MainComponent::runAudioSmokeCheck(const juce::File& directory) {
    if (audioPreview_.devicesEnabled()) return false;
    struct Case { std::size_t length; int width, height; const char* name; };
    for (const auto test : { Case { 1024, 960, 640, "1024" }, Case { 2048, 960, 640, "2048" },
                            Case { 2048, 640, 480, "small" } }) {
        editor_ = EditorModel(test.length);
        setSize(test.width, test.height);
        note_.setSelectedId(70, juce::sendNotificationSync);
        previewVolume_.setValue(40.0, juce::sendNotificationSync);
        syncControls();
        const auto waitForBank = [&](const Waveform& wave) {
            const auto revision = audioPreview_.requestWaveform(wave);
            const auto deadline = juce::Time::getMillisecondCounter() + 3000;
            while (audioPreview_.publishedRevision() < revision) {
                if (juce::Time::getMillisecondCounter() >= deadline) return false;
                juce::Thread::sleep(2);
            }
            return true;
        };
        if (!waitForBank(editor_.waveform())) return false;
        if (audioPreview_.playing() || audioSettings_ || play_.getButtonText() != "Play") return false;
        play_.onClick();
        if (!audioPreview_.playing() || play_.getButtonText() != "Stop" || audioError_.isNotEmpty()) return false;
        std::array<float, 4096> output;
        audioPreview_.renderOffline(output);
        audioPreview_.renderOffline(output);
        const float peak = *std::max_element(output.begin(), output.end());
        if (peak < 0.09f || peak > 0.101f) return false;
        note_.setSelectedId(82, juce::sendNotificationSync);
        audioPreview_.renderOffline(output); // Complete the pitch crossfade.
        audioPreview_.renderOffline(output);
        int crossings = 0;
        for (std::size_t i = 1; i < output.size(); ++i) if (output[i - 1] <= 0.0f && output[i] > 0.0f) ++crossings;
        if (crossings < 74 || crossings > 76) return false;
        previewVolume_.setValue(0.0, juce::sendNotificationSync);
        audioPreview_.renderOffline(output);
        for (std::size_t i = 960; i < output.size(); ++i) if (output[i] != 0.0f) return false;
        previewVolume_.setValue(40.0, juce::sendNotificationSync);
        // Actual editor controls publish the display/preview, including cancel.
        shape_.setSelectedId(4, juce::sendNotificationSync);
        const auto square = editor_.waveform();
        if (!waitForBank(square)) return false;
        audioPreview_.renderOffline(output);
        smoothingStrength_.setValue(100.0, juce::dontSendNotification);
        smooth_.onClick();
        if (editor_.waveform() == square || !audioPreview_.playing()) return false;
        if (!waitForBank(editor_.waveform())) return false;
        audioPreview_.renderOffline(output);
        cancelSmoothing_.onClick();
        if (!(editor_.waveform() == square) || !waitForBank(editor_.waveform())) return false;
        audioPreview_.renderOffline(output);
        clear_.onClick();
        if (!waitForBank(editor_.waveform())) return false;
        audioPreview_.renderOffline(output);
        audioPreview_.renderOffline(output);
        for (const auto value : output) if (std::abs(value) > 0.000001f) return false;
        undo_.onClick();
        if (!(editor_.waveform() == square) || !waitForBank(editor_.waveform())) return false;
        audioPreview_.renderOffline(output);
        for (const bool dark : { true, false }) {
            setDarkTheme(dark);
            auto stream = directory.getChildFile(juce::String("audio-") + (dark ? "dark-" : "light-") + test.name + ".png").createOutputStream();
            if (!stream) return false;
            stream->setPosition(0); stream->truncate();
            if (!juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()), *stream)) return false;
        }
        if (!keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)) || audioPreview_.playing()) return false;
        audioPreview_.renderOffline(output);
        for (std::size_t i = 960; i < output.size(); ++i) if (output[i] != 0.0f) return false;
    }
    if (audioPreview_.deviceManager().getCurrentAudioDevice()) return false;
    // Render the actual JUCE output selector without showing a window or opening audio.
    juce::AudioDeviceSelectorComponent selector(audioPreview_.deviceManager(), 0, 0, 0, 2, false, false, true, true);
    selector.setLookAndFeel(&lookAndFeel_);
    selector.setSize(560, 380);
    for (const bool dark : { true, false }) {
        setDarkTheme(dark);
        selector.sendLookAndFeelChange();
        auto stream = directory.getChildFile(dark ? "audio-settings-dark.png" : "audio-settings-light.png").createOutputStream();
        if (!stream) { selector.setLookAndFeel(nullptr); return false; }
        stream->setPosition(0); stream->truncate();
        const auto snapshot = selector.createComponentSnapshot(selector.getLocalBounds());
        juce::Image panel(juce::Image::ARGB, snapshot.getWidth(), snapshot.getHeight(), true);
        {
            juce::Graphics graphics(panel);
            graphics.fillAll(themeFor(dark).panel);
            graphics.drawImageAt(snapshot, 0, 0);
        }
        if (!juce::PNGImageFormat().writeImageToStream(panel, *stream)) {
            selector.setLookAndFeel(nullptr); return false;
        }
    }
    selector.setLookAndFeel(nullptr);
    if (audioPreview_.deviceManager().getCurrentAudioDevice()) return false;
    editor_ = EditorModel();
    note_.setSelectedId(70, juce::sendNotificationSync);
    previewVolume_.setValue(20.0, juce::sendNotificationSync);
    setSize(960, 640); setDarkTheme(true); syncControls();
    return true;
}
void MainComponent::runAudioDeviceCheck(const juce::File& directory, std::function<void(bool)> finished) {
    editor_ = EditorModel();
    syncControls();
    note_.setSelectedId(70, juce::sendNotificationSync);
    previewVolume_.setValue(8.0, juce::sendNotificationSync);
    const auto error = audioPreview_.setPlaying(true);
    const juce::Component::SafePointer<MainComponent> safe(this);
    juce::Timer::callAfterDelay(1500, [safe, directory, error, finished = std::move(finished)] {
        if (!safe) { finished(false); return; }
        auto* object = new juce::DynamicObject();
        object->setProperty("initializationError", error);
        object->setProperty("output", safe->audioPreview_.status());
        object->setProperty("framesRendered", static_cast<juce::int64>(safe->audioPreview_.framesRendered()));
        object->setProperty("outputPeak", safe->audioPreview_.outputPeak());
        object->setProperty("note", "A4");
        object->setProperty("frequencyHz", 440.0);
        object->setProperty("inputsRequested", 0);
        auto* device = safe->audioPreview_.deviceManager().getCurrentAudioDevice();
        object->setProperty("sampleRate", device ? device->getCurrentSampleRate() : 0.0);
        object->setProperty("bufferFrames", device ? device->getCurrentBufferSizeSamples() : 0);
        const bool ok = error.isEmpty() && safe->audioPreview_.framesRendered() > 0
            && safe->audioPreview_.outputPeak() > 0.001f && safe->audioPreview_.outputPeak() <= 0.021f;
        object->setProperty("passed", ok);
        const auto report = juce::JSON::toString(juce::var(object), false);
        safe->audioPreview_.setPlaying(false);
        const bool written = directory.createDirectory().wasOk()
            && directory.getChildFile("device-check.json").replaceWithText(report);
        juce::Timer::callAfterDelay(100, [finished, ok, written] { finished(ok && written); });
    });
}
} // namespace wtd
