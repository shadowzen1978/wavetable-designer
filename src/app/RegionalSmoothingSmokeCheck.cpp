#include "app/MainComponent.h"

namespace wtd {
bool MainComponent::runRegionalSmoothingSmokeCheck(const juce::File& directory) {
    struct Case { std::size_t length; int width, height; const char* name; };
    for (const auto test : { Case { 1024, 960, 640, "1024" },
                            Case { 2048, 960, 640, "2048" },
                            Case { 2048, 640, 480, "small" } }) {
        editor_ = EditorModel(test.length);
        zoom_.setSelectedId(1, juce::sendNotificationSync);
        setSize(test.width, test.height);
        const auto first = test.length / 4, last = test.length / 2;
        editor_.beginStroke(0.25, -0.75f);
        for (auto i = first + 1; i <= last; ++i) {
            const float trend = -0.75f - 0.15f * static_cast<float>(i - first) / static_cast<float>(last - first);
            editor_.continueStroke(static_cast<double>(i) / static_cast<double>(test.length),
                                  trend + (i == last ? 0.0f : i % 2 == 0 ? 0.015f : -0.015f));
        }
        editor_.endStroke();
        syncControls();
        const auto original = editor_.waveform();
        if (selectRegion_.isVisible() || clearSelection_.isVisible() || waveformView_.selectionMode()) return false;
        smoothingStrength_.setValue(100.0, juce::dontSendNotification);
        smooth_.onClick();
        const auto point = [&](std::size_t index) {
            return juce::Point<float>(waveformView_.sampleX(index), waveformView_.plotBounds().getCentreY());
        };
        const auto event = [&](juce::Point<float> p, int mods = 0, bool dragged = false) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), p,
                juce::ModifierKeys(mods), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                &waveformView_, &waveformView_, juce::Time::getCurrentTime(), p,
                juce::Time::getCurrentTime(), 1, dragged);
        };
        const auto drag = [&](std::size_t from, std::size_t to) {
            waveformView_.mouseDown(event(point(from), juce::ModifierKeys::leftButtonModifier));
            waveformView_.mouseDrag(event(point(to), juce::ModifierKeys::leftButtonModifier, true));
            waveformView_.mouseUp(event(point(to), 0, true));
        };
        // Whole-cycle preview keeps the view interactive for zoom but blocks drawing.
        const auto whole = editor_.waveform();
        drag(first, last);
        if (!(editor_.waveform() == whole) || editor_.strokeActive() || editor_.selection()) return false;
        selectRegion_.setToggleState(true, juce::sendNotificationSync);
        if (!waveformView_.selectionMode() || applySmoothing_.isEnabled() || !(editor_.waveform() == original)) return false;
        keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
        if (!editor_.smoothingActive()) return false;
        waveformView_.mouseMove(event(point(first)));
        if (waveformView_.cursorSample() != first) return false;
        waveformView_.mouseExit(event(point(first)));
        if (waveformView_.cursorSample()) return false;
        drag(last, first); // reversed creation
        if (editor_.selection() != (SampleRange { first, last }) || !applySmoothing_.isEnabled()
            || waveformView_.cursorSample() != first) return false;
        const auto local = editor_.waveform();
        if (!(local == original.smoothedRegion(1.0, { first, last }))) return false;
        for (std::size_t i = 0; i < original.size(); ++i)
            if ((i <= first || i >= last) && local.samples()[i] != original.samples()[i]) return false;

        // Escape/Undo cancel a boundary drag while keeping the smoothing transaction.
        waveformView_.mouseDown(event(point(first), juce::ModifierKeys::leftButtonModifier));
        waveformView_.mouseDrag(event(point(first + test.length / 16), juce::ModifierKeys::leftButtonModifier, true));
        if (!keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) || !editor_.smoothingActive()
            || editor_.selection() != (SampleRange { first, last }) || !(editor_.waveform() == local)) return false;
        waveformView_.mouseDown(event(point(last), juce::ModifierKeys::leftButtonModifier));
        waveformView_.mouseDrag(event(point(last - test.length / 16), juce::ModifierKeys::leftButtonModifier, true));
        if (!keyPressed(juce::KeyPress('z', juce::ModifierKeys(juce::ModifierKeys::commandModifier), 0))
            || !editor_.smoothingActive() || editor_.selection() != (SampleRange { first, last })) return false;

        selectRegion_.setToggleState(false, juce::sendNotificationSync);
        if (!(editor_.waveform() == original.smoothed(1.0))) return false;
        selectRegion_.setToggleState(true, juce::sendNotificationSync);
        if (!(editor_.waveform() == local)) return false;
        for (const bool dark : { true, false }) {
            setDarkTheme(dark);
            auto stream = directory.getChildFile(juce::String("regional-smoothing-") + (dark ? "dark-" : "light-") + test.name + ".png").createOutputStream();
            if (!stream) return false;
            stream->setPosition(0); stream->truncate();
            if (!juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()), *stream)) return false;
        }
        // Boundary crossing clamps to the other endpoint instead of exchanging sides.
        waveformView_.mouseDown(event(point(first), juce::ModifierKeys::leftButtonModifier));
        waveformView_.mouseDrag(event(point(last + 10), juce::ModifierKeys::leftButtonModifier, true));
        if (editor_.selection() != (SampleRange { last, last }) || applySmoothing_.isEnabled()) return false;
        waveformView_.cancelSelectionGesture();
        if (editor_.selection() != (SampleRange { first, last })) return false;

        // Sample view and the real scrollbar listener support exact one-sample boundary adjustments.
        zoom_.setSelectedId(100, juce::sendNotificationSync);
        if (waveformView_.sampleX(first + 1) - waveformView_.sampleX(first) < 11.99f) return false;
        auto* scroll = dynamic_cast<juce::ScrollBar*>(waveformView_.getChildComponent(0));
        if (!scroll || !scroll->isVisible()) return false;
        const auto show = [&](std::size_t index) {
            scroll->setCurrentRangeStart(static_cast<double>(index) / static_cast<double>(test.length)
                - waveformView_.viewport().span() * 0.2, juce::sendNotificationSync);
        };
        show(first);
        waveformView_.mouseMove(event(point(first)));
        if (waveformView_.cursorSample() != first) return false;
        drag(first, first + 1);
        if (editor_.selection() != (SampleRange { first + 1, last })) return false;
        show(last);
        drag(last, last - 1);
        const auto adjusted = SampleRange { first + 1, last - 1 };
        if (editor_.selection() != adjusted || !(editor_.waveform() == original.smoothedRegion(1.0, adjusted))
            || !(*editor_.smoothingSource() == original)) return false;
        show(last - 1);
        waveformView_.mouseMove(event(point(last - 1)));
        for (const bool dark : { true, false }) {
            setDarkTheme(dark);
            auto stream = directory.getChildFile(juce::String("sample-selection-") + (dark ? "dark-" : "light-") + test.name + ".png").createOutputStream();
            if (!stream) return false;
            stream->setPosition(0); stream->truncate();
            if (!juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()), *stream)) return false;
        }
        const auto adjustedWave = editor_.waveform();
        zoom_.setSelectedId(1, juce::sendNotificationSync);
        if (!(editor_.waveform() == adjustedWave)) return false;
        clearSelection_.onClick();
        if (editor_.selection() || applySmoothing_.isEnabled() || !(editor_.waveform() == original)) return false;
        keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
        if (!editor_.smoothingActive()) return false;
        drag(last - 1, first + 1);
        applySmoothing_.onClick();
        if (editor_.smoothingActive() || selectRegion_.isVisible() || waveformView_.selectionMode()) return false;
        undo_.onClick();
        if (!(editor_.waveform() == original) || editor_.selection()) return false;
        redo_.onClick();
        if (!(editor_.waveform() == adjustedWave) || editor_.selection() != adjusted) return false;

        // Drawing uses the same zoom coordinates and changes exactly one sample on a click.
        zoom_.setSelectedId(100, juce::sendNotificationSync);
        show(last);
        const auto beforeClick = editor_.waveform();
        waveformView_.mouseDown(event(point(last), juce::ModifierKeys::leftButtonModifier));
        waveformView_.mouseUp(event(point(last)));
        for (std::size_t i = 0; i < test.length; ++i)
            if (editor_.waveform().samples()[i] != (i == last ? 0.0f : beforeClick.samples()[i])) return false;
        undo_.onClick();
        // Out-of-grid zoomed drags clamp to the visible edge, never edit unseen samples.
        const auto visibleLast = waveformView_.viewport().sampleAt(1.0, test.length);
        waveformView_.mouseDown(event(point(last), juce::ModifierKeys::leftButtonModifier));
        waveformView_.mouseDrag(event({ waveformView_.plotBounds().getRight() + 1000.0f,
            waveformView_.plotBounds().getCentreY() }, juce::ModifierKeys::leftButtonModifier, true));
        waveformView_.mouseUp(event({ waveformView_.plotBounds().getRight() + 1000.0f,
            waveformView_.plotBounds().getCentreY() }, 0, true));
        for (std::size_t i = 0; i < test.length; ++i)
            if (editor_.waveform().samples()[i] != (i >= last && i <= visibleLast ? 0.0f : beforeClick.samples()[i])) return false;
        undo_.onClick();
        const int alternate = test.length == 1024 ? 2048 : 1024;
        length_.setSelectedId(alternate, juce::sendNotificationSync);
        if (waveformView_.sampleX(101) - waveformView_.sampleX(100) < 11.99f) return false;
        undo_.onClick();
        if (!(editor_.waveform() == adjustedWave) || waveformView_.sampleX(101) - waveformView_.sampleX(100) < 11.99f) return false;
        zoom_.setSelectedId(1, juce::sendNotificationSync);
        // Cancel restores selection metadata along with source samples.
        smooth_.onClick();
        clearSelection_.onClick();
        cancelSmoothing_.onClick();
        if (editor_.selection() != adjusted || !(editor_.waveform() == adjustedWave)) return false;
    }
    editor_ = EditorModel();
    zoom_.setSelectedId(1, juce::sendNotificationSync);
    smoothingStrength_.setValue(50.0, juce::dontSendNotification);
    setSize(960, 640); setDarkTheme(true); syncControls();
    return true;
}
} // namespace wtd
