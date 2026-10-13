#include "app/MainComponent.h"
#include <cmath>
#include <numbers>

namespace wtd {
bool MainComponent::runEdgeSmoothingSmokeCheck(const juce::File& directory) {
    struct Case { std::size_t length; int width, height; const char* name; };
    for (const auto test : { Case { 1024, 960, 640, "1024" }, Case { 2048, 960, 640, "2048" },
                            Case { 2048, 640, 480, "small" } }) {
        editor_ = EditorModel(test.length);
        setSize(test.width, test.height);
        const auto value = [&](std::size_t i) {
            return static_cast<float>((i < test.length / 2 ? 0.7 : -0.7)
                + 0.08 * std::sin(2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(test.length))
                + (i % 2 == 0 ? 0.001 : -0.001));
        };
        editor_.beginStroke(0.0, value(0));
        for (std::size_t i = 1; i < test.length; ++i)
            editor_.continueStroke(static_cast<double>(i) / static_cast<double>(test.length), value(i));
        editor_.endStroke();
        preserveEdges_.setToggleState(false, juce::dontSendNotification);
        edgeAngle_.setValue(30.0, juce::dontSendNotification);
        smoothingStrength_.setValue(100.0, juce::dontSendNotification);
        syncControls();
        const auto source = editor_.waveform();
        if (selectRegion_.isVisible() || preserveEdges_.isVisible() || edgeAngle_.isVisible()) return false;
        smooth_.onClick();
        if (!selectRegion_.isVisible() || selectRegion_.getToggleState() || !preserveEdges_.isVisible()
            || edgeAngle_.isVisible() || edgeAngle_.getValue() != 30.0) return false;
        const auto ordinary = editor_.waveform();
        const auto originalPlot = waveformView_.plotBounds();
        preserveEdges_.setToggleState(true, juce::sendNotificationSync);
        const auto preserved = editor_.waveform();
        if (!edgeAngle_.isVisible() || !(preserved == source.smoothed(1.0, { true, 30.0 }))
            || preserved == source || waveformView_.plotBounds() != originalPlot
            || preserved.samples()[test.length / 2] != source.samples()[test.length / 2]
            || preserved.samples()[test.length / 2 - 1] != source.samples()[test.length / 2 - 1]) return false;
        edgeAngle_.setValue(180.0, juce::sendNotificationSync);
        if (!(editor_.waveform() == ordinary)) return false;
        edgeAngle_.setValue(30.0, juce::sendNotificationSync);
        if (!(editor_.waveform() == preserved) || !(*editor_.smoothingSource() == source)) return false;
        for (const bool selection : { false, true }) {
            if (selection) editor_.setSelection(SampleRange { test.length / 4, test.length * 3 / 4 });
            selectRegion_.setToggleState(selection, juce::sendNotificationSync);
            if (selection && !(editor_.waveform() == source.smoothedRegion(1.0, *editor_.selection(), { true, 30.0 }))) return false;
            for (const bool dark : { true, false }) {
                setDarkTheme(dark);
                auto stream = directory.getChildFile(juce::String("edges-") + (selection ? "region-" : "whole-")
                    + (dark ? "dark-" : "light-") + test.name + ".png").createOutputStream();
                if (!stream) return false;
                stream->setPosition(0); stream->truncate();
                if (!juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()), *stream)) return false;
            }
        }
        selectRegion_.setToggleState(false, juce::sendNotificationSync);
        preserveEdges_.setToggleState(false, juce::sendNotificationSync);
        if (edgeAngle_.isVisible() || !(editor_.waveform() == ordinary) || waveformView_.selectionMode()) return false;
        preserveEdges_.setToggleState(true, juce::sendNotificationSync);
        applySmoothing_.onClick();
        if (edgeAngle_.isVisible() || preserveEdges_.isVisible() || selectRegion_.isVisible()
            || !(editor_.waveform() == preserved)) return false;
        undo_.onClick();
        if (!(editor_.waveform() == source)) return false;
        redo_.onClick();
        if (!(editor_.waveform() == preserved)) return false;
        smooth_.onClick();
        edgeAngle_.setValue(180.0, juce::sendNotificationSync);
        cancelSmoothing_.onClick();
        if (!(editor_.waveform() == preserved)) return false;
    }
    editor_ = EditorModel();
    preserveEdges_.setToggleState(false, juce::dontSendNotification);
    edgeAngle_.setValue(30.0, juce::dontSendNotification);
    smoothingStrength_.setValue(50.0, juce::dontSendNotification);
    setSize(960, 640); setDarkTheme(true); syncControls();
    return true;
}
} // namespace wtd
