#include "app/MainComponent.h"
#include <algorithm>

namespace wtd {
bool MainComponent::runJitterSmokeCheck(const juce::File& directory) {
    struct Case { std::size_t length; int width, height; const char* name; };
    for (const auto test : { Case { 1024, 960, 640, "1024" }, Case { 2048, 960, 640, "2048" },
                            Case { 2048, 640, 480, "small" } }) {
        editor_ = EditorModel(test.length);
        setSize(test.width, test.height);
        const auto value = [&](std::size_t i) {
            const double t = static_cast<double>(i) / static_cast<double>(test.length - 1);
            return static_cast<float>(std::lerp(-0.4, 0.1, t) + 0.2 * t * (1.0 - t)
                + (i == 0 || i + 1 == test.length ? 0.0
                    : 0.016 * (static_cast<double>((i * 17) % 23) / 22.0 - 0.5)));
        };
        editor_.beginStroke(0.0, value(0));
        for (std::size_t i = 1; i < test.length; ++i)
            editor_.continueStroke(static_cast<double>(i) / static_cast<double>(test.length), value(i));
        editor_.endStroke();
        reduceJitter_.setToggleState(false, juce::dontSendNotification);
        preserveEdges_.setToggleState(false, juce::dontSendNotification);
        smoothingStrength_.setValue(100.0, juce::dontSendNotification);
        edgeAngle_.setValue(30.0, juce::dontSendNotification);
        syncControls();
        const auto source = editor_.waveform();
        smooth_.onClick();
        if (!reduceJitter_.isVisible() || clearSelection_.isVisible() || selectRegion_.getToggleState()) return false;
        reduceJitter_.setToggleState(true, juce::sendNotificationSync);
        const auto standalone = editor_.waveform();
        if (!(standalone == source.smoothed(1.0, { false, 30.0, true })) || standalone == source || edgeAngle_.isVisible()) return false;
        edgeAngle_.setValue(150.0, juce::sendNotificationSync);
        if (!(editor_.waveform() == standalone)) return false;
        edgeAngle_.setValue(30.0, juce::sendNotificationSync);
        for (const bool region : { false, true }) {
            if (region) editor_.setSelection(SampleRange { test.length / 4, test.length * 3 / 4 });
            selectRegion_.setToggleState(region, juce::sendNotificationSync);
            if (region && !(editor_.waveform() == source.smoothedRegion(1.0, *editor_.selection(), { false, 30.0, true }))) return false;
            for (const bool dark : { true, false }) {
                setDarkTheme(dark);
                auto stream = directory.getChildFile(juce::String("jitter-independent-") + (region ? "region-" : "whole-")
                    + (dark ? "dark-" : "light-") + test.name + ".png").createOutputStream();
                if (!stream) return false;
                stream->setPosition(0); stream->truncate();
                if (!juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()), *stream)) return false;
            }
        }
        selectRegion_.setToggleState(false, juce::sendNotificationSync);
        reduceJitter_.setToggleState(false, juce::sendNotificationSync);
        preserveEdges_.setToggleState(true, juce::sendNotificationSync);
        if (!reduceJitter_.isVisible() || reduceJitter_.getToggleState()) return false;
        const auto normal = editor_.waveform();
        const auto originalPlot = waveformView_.plotBounds();
        reduceJitter_.setToggleState(true, juce::sendNotificationSync);
        const auto cleaned = editor_.waveform();
        if (!(cleaned == source.smoothed(1.0, { true, 30.0, true })) || cleaned == source
            || waveformView_.plotBounds() != originalPlot) return false;
        smoothingStrength_.setValue(50.0, juce::sendNotificationSync);
        smoothingStrength_.setValue(100.0, juce::sendNotificationSync);
        if (!(editor_.waveform() == cleaned) || !(*editor_.smoothingSource() == source)) return false;
        reduceJitter_.setToggleState(false, juce::sendNotificationSync);
        if (!(editor_.waveform() == normal)) return false;
        reduceJitter_.setToggleState(true, juce::sendNotificationSync);
        preserveEdges_.setToggleState(false, juce::sendNotificationSync);
        if (!reduceJitter_.isVisible() || edgeAngle_.isVisible() || !(editor_.waveform() == standalone)) return false;
        preserveEdges_.setToggleState(true, juce::sendNotificationSync);
        if (!(editor_.waveform() == cleaned)) return false;
        for (const bool region : { false, true }) {
            if (region) editor_.setSelection(SampleRange { test.length / 4, test.length * 3 / 4 });
            selectRegion_.setToggleState(region, juce::sendNotificationSync);
            if (region && (!clearSelection_.isVisible()
                || !(editor_.waveform() == source.smoothedRegion(1.0, *editor_.selection(), { true, 30.0, true })))) return false;
            if (!region && clearSelection_.isVisible()) return false;
            for (const bool dark : { true, false }) {
                setDarkTheme(dark);
                auto stream = directory.getChildFile(juce::String("jitter-") + (region ? "region-" : "whole-")
                    + (dark ? "dark-" : "light-") + test.name + ".png").createOutputStream();
                if (!stream) return false;
                stream->setPosition(0); stream->truncate();
                if (!juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()), *stream)) return false;
            }
        }
        selectRegion_.setToggleState(false, juce::sendNotificationSync);
        // Unchecking a retained range blanks the area beneath Select region.
        if (clearSelection_.isVisible() || waveformView_.selectionMode() || !(editor_.waveform() == cleaned)) return false;
        zoom_.setSelectedId(100, juce::sendNotificationSync);
        auto* scroll = dynamic_cast<juce::ScrollBar*>(waveformView_.getChildComponent(0));
        if (!scroll) return false;
        scroll->setCurrentRangeStart(0.3, juce::sendNotificationSync);
        for (const bool dark : { true, false }) {
            setDarkTheme(dark);
            auto stream = directory.getChildFile(juce::String("jitter-samples-") + (dark ? "dark-" : "light-") + test.name + ".png").createOutputStream();
            if (!stream) return false;
            stream->setPosition(0); stream->truncate();
            if (!juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()), *stream)) return false;
        }
        if (!(editor_.waveform() == cleaned)) return false;
        applySmoothing_.onClick();
        if (reduceJitter_.isVisible() || !(editor_.waveform() == cleaned)) return false;
        undo_.onClick();
        if (!(editor_.waveform() == source)) return false;
        redo_.onClick();
        if (!(editor_.waveform() == cleaned)) return false;
        smooth_.onClick();
        reduceJitter_.setToggleState(false, juce::sendNotificationSync);
        cancelSmoothing_.onClick();
        if (!(editor_.waveform() == cleaned)) return false;
        zoom_.setSelectedId(1, juce::sendNotificationSync);
    }
    editor_ = EditorModel();
    reduceJitter_.setToggleState(false, juce::dontSendNotification);
    preserveEdges_.setToggleState(false, juce::dontSendNotification);
    edgeAngle_.setValue(30.0, juce::dontSendNotification);
    smoothingStrength_.setValue(50.0, juce::dontSendNotification);
    setSize(960, 640); setDarkTheme(true); syncControls();
    return true;
}
} // namespace wtd
