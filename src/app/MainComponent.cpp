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
    waveformView_.onChanged = [this] { syncControls(); };
    themeButton_.onClick = [this] { dark_ = !dark_; applyTheme(); };
    for (auto* component : { static_cast<juce::Component*>(&shape_),
                              static_cast<juce::Component*>(&length_),
                              static_cast<juce::Component*>(&undo_),
                              static_cast<juce::Component*>(&redo_),
                              static_cast<juce::Component*>(&clear_),
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
    undo_.setEnabled(editor_.canUndo());
    redo_.setEnabled(editor_.canRedo());
    waveformView_.repaint();
    repaint();
}

bool MainComponent::keyPressed(const juce::KeyPress& key) {
    const auto mods = key.getModifiers();
    if (!mods.isCommandDown() || mods.isAltDown()) return false;
    const int code = key.getKeyCode();
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
    g.drawText("Cycle length", getWidth() - 212, getHeight() - 92, 180, 20,
               juce::Justification::left);
    g.drawText("One cycle  /  " + juce::String(static_cast<int>(editor_.waveform().size())) + " samples",
               32, getHeight() - 54, getWidth() - 276, 24, juce::Justification::left);
}

void MainComponent::resized() {
    themeButton_.setBounds(getWidth() - 160, 28, 128, 34);
    shape_.setBounds(32, 104, 172, 36);
    undo_.setBounds(228, 104, 80, 36);
    redo_.setBounds(320, 104, 80, 36);
    clear_.setBounds(412, 104, 80, 36);
    // Editing and persistence live above the canvas; transport and export below.
    length_.setBounds(getWidth() - 212, getHeight() - 64, 180, 36);
    waveformView_.setBounds(32, 164, getWidth() - 64, getHeight() - 276);
}
} // namespace wtd
