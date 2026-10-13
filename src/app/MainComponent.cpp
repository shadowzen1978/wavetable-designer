#include "app/MainComponent.h"

namespace wtd {
namespace {
constexpr Shape shapes[] { Shape::sine, Shape::triangle, Shape::saw, Shape::square, Shape::flat };
}

MainComponent::MainComponent() {
    setLookAndFeel(&lookAndFeel_);
    setWantsKeyboardFocus(true);
    shape_.addItemList({ "Sine", "Triangle", "Saw", "Square", "Flat", "Custom" }, 1);
    shape_.setItemEnabled(6, false);
    shape_.setTooltip("Replace the cycle with a starting waveform (undoable)");
    shape_.onChange = [this] {
        const int index = shape_.getSelectedId() - 1;
        if (index >= 0 && index < 5) editor_.applyPreset(shapes[index]);
        syncControls();
    };
    length_.addItem("1024 samples", 1024);
    length_.addItem("2048 samples", 2048);
    length_.setTooltip("Resample the current cycle to this length (undoable)");
    length_.onChange = [this] {
        editor_.resizeCycle(static_cast<std::size_t>(length_.getSelectedId()));
        syncControls();
    };
    undo_.onClick = [this] { editor_.undo(); syncControls(); };
    redo_.onClick = [this] { editor_.redo(); syncControls(); };
    clear_.onClick = [this] { editor_.applyPreset(Shape::flat); syncControls(); };
    undo_.setTooltip("Undo (Ctrl/Cmd+Z)");
    redo_.setTooltip("Redo (Ctrl/Cmd+Shift+Z or Ctrl+Y)");
    clear_.setTooltip("Clear the cycle to silence (undoable)");
    waveformView_.onChanged = [this] {
        if (editor_.smoothingActive()) updateSmoothingPreview(); else syncControls();
    };
    clearSelection_.onClick = [this] { editor_.setSelection(std::nullopt); updateSmoothingPreview(); };
    zoom_.addItem("1x (whole cycle)", 1);
    for (int factor = 2; factor <= 64; factor *= 2) zoom_.addItem(juce::String(factor) + "x zoom", factor);
    zoom_.addItem("Sample view", 100);
    zoom_.setSelectedId(1, juce::dontSendNotification);
    zoom_.setTitle("Horizontal zoom");
    zoom_.setTooltip("Horizontal magnification; Sample view shows individual sample points. Scroll below the grid to navigate.");
    zoom_.onChange = [this] {
        if (zoom_.getSelectedId() == 100) waveformView_.setSampleZoom();
        else waveformView_.setZoom(static_cast<double>(zoom_.getSelectedId()));
    };
    selectRegion_.onClick = [this] { updateSmoothingPreview(); };
    selectRegion_.setTooltip("Checked: smooth a selected range. Unchecked: smooth the whole cycle.");
    preserveEdges_.onClick = [this] { updateSmoothingPreview(); };
    reduceJitter_.onClick = [this] { updateSmoothingPreview(); };
    reduceJitter_.setTooltip("Clean nearly straight lines and shallow curves independently. Smoothness controls the blend; combine with Preserve edges to protect interior corners.");
    clearSelection_.setTooltip("Clear the selected smoothing range");
    preserveEdges_.setTooltip("Keep corners above the angle threshold unchanged and smooth between them.");
    edgeAngle_.setSliderStyle(juce::Slider::LinearHorizontal);
    edgeAngle_.setTextBoxStyle(juce::Slider::TextBoxRight, true, 52, 28);
    edgeAngle_.setName("Edge angle");
    edgeAngle_.setRange(1.0, 180.0, 1.0);
    edgeAngle_.setTextValueSuffix(juce::String::charToString(0x00b0));
    edgeAngle_.setValue(30.0, juce::dontSendNotification);
    edgeAngle_.setTooltip("Protect turns greater than this angle. Lower values retain more corners. Angles use nearby fitted slopes and do not depend on zoom.");
    edgeAngle_.onValueChange = [this] { updateSmoothingPreview(); };
    addChildComponent(&selectRegion_);
    addChildComponent(&preserveEdges_);
    addChildComponent(&reduceJitter_);
    addChildComponent(&edgeAngle_);
    addChildComponent(&clearSelection_);
    smooth_.onClick = [this] { startSmoothing(); };
    smooth_.setTooltip("Preview periodic smoothing, then apply or cancel");
    smoothingStrength_.setSliderStyle(juce::Slider::LinearHorizontal);
    smoothingStrength_.setTextBoxStyle(juce::Slider::TextBoxRight, true, 52, 28);
    smoothingStrength_.setRange(0.0, 100.0, 1.0);
    smoothingStrength_.setTextValueSuffix("%");
    smoothingStrength_.setValue(50.0, juce::dontSendNotification);
    smoothingStrength_.onValueChange = [this] { updateSmoothingPreview(); };
    applySmoothing_.onClick = [this] { editor_.applySmoothing(); syncControls(); };
    cancelSmoothing_.onClick = [this] { editor_.cancelSmoothing(); syncControls(); };
    for (auto* component : { static_cast<juce::Component*>(&smoothingStrength_),
                              static_cast<juce::Component*>(&applySmoothing_),
                              static_cast<juce::Component*>(&cancelSmoothing_) })
        addChildComponent(component);
    themeButton_.onClick = [this] { dark_ = !dark_; applyTheme(); };
    for (auto* component : { static_cast<juce::Component*>(&shape_),
                              static_cast<juce::Component*>(&length_),
                              static_cast<juce::Component*>(&undo_),
                              static_cast<juce::Component*>(&redo_),
                              static_cast<juce::Component*>(&clear_),
                              static_cast<juce::Component*>(&smooth_),
                              static_cast<juce::Component*>(&zoom_),
                              static_cast<juce::Component*>(&themeButton_),
                              static_cast<juce::Component*>(&waveformView_) })
        addAndMakeVisible(component);
    setSize(960, 640);
    syncControls();
    applyTheme();
}

MainComponent::~MainComponent() {
    waveformView_.onChanged = {};
    setLookAndFeel(nullptr);
}

void MainComponent::setDarkTheme(bool dark) { dark_ = dark; applyTheme(); }

void MainComponent::syncControls() {
    int presetId = 6;
    if (const auto preset = editor_.preset()) {
        for (int i = 0; i < 5; ++i) if (shapes[i] == *preset) presetId = i + 1;
    }
    shape_.setSelectedId(presetId, juce::dontSendNotification);
    length_.setSelectedId(static_cast<int>(editor_.waveform().size()), juce::dontSendNotification);
    const bool smoothing = editor_.smoothingActive();
    shape_.setEnabled(!smoothing);
    length_.setEnabled(!smoothing);
    clear_.setEnabled(!smoothing);
    smooth_.setEnabled(!smoothing);
    waveformView_.setEnabled(true);
    waveformView_.refreshViewport();
    waveformView_.setSelectionMode(smoothing && selectRegion_.getToggleState());
    waveformView_.setMouseCursor(waveformView_.selectionMode() ? juce::MouseCursor::CrosshairCursor
        : smoothing ? juce::MouseCursor::NormalCursor : juce::MouseCursor::CrosshairCursor);
    clearSelection_.setVisible(smoothing && selectRegion_.getToggleState() && editor_.selection().has_value());
    selectRegion_.setVisible(smoothing);
    preserveEdges_.setVisible(smoothing);
    reduceJitter_.setVisible(smoothing);
    edgeAngle_.setVisible(smoothing && preserveEdges_.getToggleState());
    clear_.setVisible(!smoothing);
    smooth_.setVisible(!smoothing);
    const auto range = editor_.selection();
    applySmoothing_.setEnabled(!selectRegion_.getToggleState() || (range && range->last - range->first >= 2));
    smoothingStrength_.setVisible(smoothing);
    applySmoothing_.setVisible(smoothing);
    cancelSmoothing_.setVisible(smoothing);
    undo_.setEnabled(editor_.canUndo());
    redo_.setEnabled(editor_.canRedo());
    resized();
    waveformView_.repaint();
    repaint();
}

void MainComponent::startSmoothing() {
    selectRegion_.setToggleState(editor_.selection().has_value(), juce::dontSendNotification);
    editor_.beginSmoothing();
    updateSmoothingPreview();
}

void MainComponent::updateSmoothingPreview() {
    if (!editor_.smoothingActive()) return;
    const auto scope = selectRegion_.getToggleState()
        ? SmoothingScope::selection : SmoothingScope::wholeCycle;
    const SmoothingOptions options { preserveEdges_.getToggleState(), edgeAngle_.getValue(), reduceJitter_.getToggleState() };
    // An empty region shows the source while waiting for a drag; never smooth the whole cycle by accident.
    if (scope == SmoothingScope::selection && !editor_.selection())
        editor_.previewSmoothing(0.0, SmoothingScope::wholeCycle);
    else editor_.previewSmoothing(smoothingStrength_.getValue() / 100.0, scope, options);
    syncControls();
}

bool MainComponent::keyPressed(const juce::KeyPress& key) {
    if (editor_.smoothingActive() && key.getKeyCode() == juce::KeyPress::escapeKey) {
        if (!waveformView_.cancelSelectionGesture()) { editor_.cancelSmoothing(); syncControls(); }
        return true;
    }
    if (editor_.smoothingActive() && key.getKeyCode() == juce::KeyPress::returnKey) {
        if (applySmoothing_.isEnabled()) { editor_.applySmoothing(); syncControls(); }
        return true;
    }
    const auto mods = key.getModifiers();
    if (!mods.isCommandDown() || mods.isAltDown()) return false;
    const int code = key.getKeyCode();
    if ((code == 'z' || code == 'Z' || code == 'y' || code == 'Y')
        && waveformView_.cancelSelectionGesture()) return true;
    if (code == 'z' || code == 'Z') {
        if (mods.isShiftDown()) editor_.redo();
        else editor_.undo();
    } else if (code == 'y' || code == 'Y') {
        editor_.redo();
    } else return false;
    syncControls();
    return true;
}

void MainComponent::applyTheme() {
    const auto theme = themeFor(dark_);
    lookAndFeel_.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    lookAndFeel_.setColour(juce::ComboBox::textColourId, theme.text);
    lookAndFeel_.setColour(juce::ComboBox::outlineColourId, theme.grid);
    lookAndFeel_.setColour(juce::ComboBox::arrowColourId, theme.muted);
    lookAndFeel_.setColour(juce::PopupMenu::backgroundColourId, theme.panel);
    lookAndFeel_.setColour(juce::PopupMenu::textColourId, theme.text);
    lookAndFeel_.setColour(juce::PopupMenu::highlightedBackgroundColourId, theme.grid);
    lookAndFeel_.setColour(juce::PopupMenu::highlightedTextColourId, theme.text);
    lookAndFeel_.setColour(juce::TextButton::buttonColourId, theme.panel);
    lookAndFeel_.setColour(juce::TextButton::textColourOffId, theme.text);
    lookAndFeel_.setColour(juce::ToggleButton::textColourId, theme.text);
    lookAndFeel_.setColour(juce::ToggleButton::tickColourId, theme.accent);
    lookAndFeel_.setColour(juce::ToggleButton::tickDisabledColourId, theme.muted);
    lookAndFeel_.setColour(juce::Slider::thumbColourId, theme.accent);
    lookAndFeel_.setColour(juce::Slider::trackColourId, theme.accent);
    lookAndFeel_.setColour(juce::Slider::backgroundColourId, theme.grid);
    lookAndFeel_.setColour(juce::Slider::textBoxTextColourId, theme.text);
    lookAndFeel_.setColour(juce::Slider::textBoxBackgroundColourId, theme.panel);
    lookAndFeel_.setColour(juce::Slider::textBoxOutlineColourId, theme.grid);
    themeButton_.setButtonText(dark_ ? "Light theme" : "Dark theme");
    waveformView_.setDark(dark_);
    sendLookAndFeelChange();
    repaint();
}

void MainComponent::paint(juce::Graphics& g) {
    const auto theme = themeFor(dark_);
    g.fillAll(theme.background);
    g.setColour(theme.text);
    g.setFont(juce::FontOptions(26.0f));
    g.drawText("Wavetable Designer", 32, 24, getWidth() - 220, 36, juce::Justification::left);
    g.setColour(theme.muted);
    g.setFont(juce::FontOptions(15.0f));
    g.drawText("Waveform", 32, 78, 172, 20, juce::Justification::left);
    g.drawText("Zoom", getWidth() - 264, 78, 48, 20, juce::Justification::left);
    if (editor_.smoothingActive()) {
        g.drawText("Smoothness", 228, 183, 88, 24, juce::Justification::left);
        if (selectRegion_.getToggleState()) {
            const auto range = editor_.selection();
            const auto label = range ? juce::String(static_cast<int>(range->first)) + " – "
                + juce::String(static_cast<int>(range->last))
                : juce::String("Drag a range");
            g.drawText(label, 36, 183, 108, 24, juce::Justification::left);
        }
    }
    g.drawText("Cycle length", getWidth() - 212, getHeight() - 92, 180, 20,
               juce::Justification::left);
    const auto status = waveformView_.selectionMode()
        ? juce::String("Drag a range; drag either boundary to adjust")
        : juce::String("One cycle  /  ") + juce::String(static_cast<int>(editor_.waveform().size())) + " samples";
    g.drawText(status,
               32, getHeight() - 54, getWidth() - 276, 24, juce::Justification::left);
}

void MainComponent::resized() {
    themeButton_.setBounds(getWidth() - 160, 28, 128, 34);
    shape_.setBounds(32, 104, 172, 36);
    zoom_.setBounds(getWidth() - 212, 76, 180, 24);
    selectRegion_.setBounds(32, 148, 132, 28);
    preserveEdges_.setBounds(228, 148, 144, 28);
    clearSelection_.setBounds(152, 180, 52, 28);
    reduceJitter_.setBounds(228, 212, 152, 28);
    undo_.setBounds(228, 104, 80, 36);
    redo_.setBounds(320, 104, 80, 36);
    clear_.setBounds(412, 104, 80, 36);
    smooth_.setBounds(504, 104, 104, 36);
    smoothingStrength_.setBounds(320, 178, getWidth() - 352, 32);
    edgeAngle_.setBounds(384, 146, getWidth() - 416, 32);
    applySmoothing_.setBounds(412, 104, 80, 36);
    cancelSmoothing_.setBounds(504, 104, 104, 36);
    // Editing and persistence live above the canvas; transport and export below.
    length_.setBounds(getWidth() - 212, getHeight() - 64, 180, 36);
    const int top = editor_.smoothingActive() ? 252 : 164;
    waveformView_.setBounds(32, top, getWidth() - 64, getHeight() - top - 112);
}
} // namespace wtd
