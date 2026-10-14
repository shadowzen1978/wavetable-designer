#include "app/MainComponent.h"
#include <memory>

namespace wtd {
class Application final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise(const juce::String& commandLine) override {
        const bool smoke = commandLine.trim() == "--smoke-test";
        const bool audioCheck = commandLine.trim() == "--audio-device-check";
        window_ = std::make_unique<Window>(getApplicationName(), !smoke && !audioCheck, !smoke);
        if (audioCheck) {
            juce::Timer::callAfterDelay(150, [this] {
                window_->checkAudio(juce::File::getCurrentWorkingDirectory().getChildFile("build/audio-check"),
                    [this](bool passed) { setApplicationReturnValue(passed ? 0 : 1); systemRequestedQuit(); });
            });
        }
        if (smoke) {
            juce::Timer::callAfterDelay(150, [this] {
                const auto directory = juce::File::getCurrentWorkingDirectory().getChildFile("build/gui-smoke");
                setApplicationReturnValue(window_->writeSnapshots(directory) ? 0 : 1);
                systemRequestedQuit();
            });
        }
    }
    void shutdown() override { window_.reset(); }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override {}
private:
    class Window final : public juce::DocumentWindow {
    public:
        Window(const juce::String& name, bool visible, bool enableAudioDevices)
            : DocumentWindow(name, themeFor(true).background, allButtons) {
            setUsingNativeTitleBar(true);
            content_ = new MainComponent(enableAudioDevices);
            setContentOwned(content_, true);
            setResizable(true, false);
            setResizeLimits(640, 480, 2560, 1600);
            centreWithSize(getWidth(), getHeight());
            setVisible(visible);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
        void checkAudio(const juce::File& directory, std::function<void(bool)> finished) {
            content_->runAudioDeviceCheck(directory, std::move(finished));
        }
        bool writeSnapshots(const juce::File& directory) {
            if (directory.createDirectory().failed()) return false;
            for (const bool dark : { true, false }) {
                content_->setDarkTheme(dark);
                const auto image = content_->createComponentSnapshot(content_->getLocalBounds());
                const auto file = directory.getChildFile(dark ? "dark.png" : "light.png");
                auto stream = file.createOutputStream();
                if (stream == nullptr) return false;
                stream->setPosition(0);
                stream->truncate();
                if (!juce::PNGImageFormat().writeImageToStream(image, *stream)) return false;
            }
            return content_->runDrawingSmokeCheck(directory) && content_->runSmoothingSmokeCheck(directory)
                && content_->runRegionalSmoothingSmokeCheck(directory) && content_->runEdgeSmoothingSmokeCheck(directory) && content_->runJitterSmokeCheck(directory) && content_->runAudioSmokeCheck(directory);
        }
    private:
        MainComponent* content_ = nullptr; // Owned by DocumentWindow.

    };
    std::unique_ptr<Window> window_;
};
} // namespace wtd

START_JUCE_APPLICATION(wtd::Application)
