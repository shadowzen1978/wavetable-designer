#include "app/MainComponent.h"
#include <cmath>

namespace wtd {
// Exercise the actual canvas handlers, controls and keyboard commands in a hidden
// application instance. This never moves the user's pointer or sends OS input.
bool MainComponent::runDrawingSmokeCheck(const juce::File& directory) {
    struct Case { std::size_t length; int width, height; const char* image; };
    for (const auto test : { Case { 1024, 960, 640, "drawing-1024.png" },
                            Case { 2048, 960, 640, "drawing-2048.png" },
                            Case { 2048, 640, 480, "drawing-small.png" } }) {
        editor_ = EditorModel(test.length);
        setSize(test.width, test.height);
        syncControls();
        const auto original = editor_.waveform();
        const auto plot = waveformView_.plotBounds();
        const auto start = juce::Point<float>(plot.getX() + plot.getWidth() * 0.25f,
                                             plot.getCentreY() + plot.getHeight() * 0.25f);
        const auto finish = juce::Point<float>(plot.getX() + plot.getWidth() * 0.75f,
                                              plot.getCentreY() - plot.getHeight() * 0.25f);
        const auto event = [&](juce::Point<float> point, int modifiers, bool dragged) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
                juce::ModifierKeys(modifiers), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                &waveformView_, &waveformView_, juce::Time::getCurrentTime(), start,
                juce::Time::getCurrentTime(), 1, dragged);
        };
        // A right click and a click in the plot padding must not edit anything.
        waveformView_.mouseDown(event(start, juce::ModifierKeys::rightButtonModifier, false));
        waveformView_.mouseDown(event({ 1.0f, 1.0f }, juce::ModifierKeys::leftButtonModifier, false));
        if (!(editor_.waveform() == original) || editor_.strokeActive()) return false;
        waveformView_.mouseDown(event(start, juce::ModifierKeys::leftButtonModifier, false));
        waveformView_.mouseDrag(event(finish, juce::ModifierKeys::leftButtonModifier, true));
        waveformView_.mouseUp(event(finish, 0, true));
        const auto drawn = editor_.waveform();
        if (editor_.strokeActive() || !undo_.isEnabled() || redo_.isEnabled()
            || shape_.getSelectedId() != 6 || drawn.size() != test.length
            || std::abs(drawn.samples()[test.length / 4] + 0.5f) > 0.000001f
            || std::abs(drawn.samples()[test.length * 3 / 8] + 0.25f) > 0.000001f
            || std::abs(drawn.samples()[test.length * 3 / 4] - 0.5f) > 0.000001f)
            return false;
        if (!keyPressed(juce::KeyPress('z', juce::ModifierKeys(juce::ModifierKeys::commandModifier), 0))
            || !(editor_.waveform() == original) || undo_.isEnabled() || !redo_.isEnabled()) return false;
        if (!keyPressed(juce::KeyPress('z', juce::ModifierKeys(juce::ModifierKeys::commandModifier
                | juce::ModifierKeys::shiftModifier), 0)) || !(editor_.waveform() == drawn)) return false;
        waveformView_.mouseDown(event(start, juce::ModifierKeys::leftButtonModifier, false));
        waveformView_.mouseDrag(event({ plot.getX(), plot.getY() - 100.0f },
                                      juce::ModifierKeys::leftButtonModifier, true));
        if (!waveformView_.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey))
            || !(editor_.waveform() == drawn) || editor_.strokeActive()) return false;
        const int alternate = test.length == 1024 ? 2048 : 1024;
        length_.setSelectedId(alternate, juce::sendNotificationSync);
        if (editor_.waveform().size() != static_cast<std::size_t>(alternate)) return false;
        undo_.onClick();
        if (!(editor_.waveform() == drawn) || length_.getSelectedId() != static_cast<int>(test.length)) return false;
        clear_.onClick();
        if (shape_.getSelectedId() != 5) return false;
        for (const auto sample : editor_.waveform().samples()) if (sample != 0.0f) return false;
        undo_.onClick();
        if (!(editor_.waveform() == drawn)) return false;
        auto stream = directory.getChildFile(test.image).createOutputStream();
        if (!stream) return false;
        stream->setPosition(0);
        stream->truncate();
        if (!juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()), *stream)) return false;
    }
    editor_ = EditorModel();
    setSize(960, 640);
    syncControls();
    return true;
}
} // namespace wtd
