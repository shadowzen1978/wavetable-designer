#include "app/MainComponent.h"

namespace wtd {
MainComponent::MainComponent() {
    setLookAndFeel(&lookAndFeel_);
    shape_.addItemList({ "Sine", "Triangle", "Saw", "Square" }, 1);
    shape_.setSelectedId(1, juce::dontSendNotification);
    shape_.setTooltip("Choose a starting waveform");
    shape_.onChange = [this] { updateWaveform(); };
    length_.addItem("1024 samples", 1024);
    length_.addItem("2048 samples", 2048);
    length_.setSelectedId(1024, juce::dontSendNotification);
    length_.setTooltip("Choose the number of samples in one cycle");
    length_.onChange = [this] { updateWaveform(); };
    themeButton_.onClick = [this] { dark_ = !dark_; applyTheme(); };
    for (auto* component : { static_cast<juce::Component*>(&shape_),
                              static_cast<juce::Component*>(&length_),
                              static_cast<juce::Component*>(&themeButton_),
                              static_cast<juce::Component*>(&waveformView_) })
        addAndMakeVisible(component);
    setSize(960, 640);
    applyTheme();
}

MainComponent::~MainComponent() { setLookAndFeel(nullptr); }

void MainComponent::setDarkTheme(bool dark) {
    dark_ = dark;
    applyTheme();
}

void MainComponent::updateWaveform() {
    const Shape shapes[] { Shape::sine, Shape::triangle, Shape::saw, Shape::square };
    waveform_ = Waveform(static_cast<std::size_t>(length_.getSelectedId()),
                         shapes[shape_.getSelectedId() - 1]);
    waveformView_.repaint();
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
    g.drawText("Waveform", 32, 78, 120, 20, juce::Justification::left);
    g.drawText("Cycle length", getWidth() - 212, getHeight() - 92, 180, 20,
               juce::Justification::left);
    g.drawText("One cycle  /  " + juce::String(static_cast<int>(waveform_.size())) + " samples",
               32, getHeight() - 54, getWidth() - 276, 24, juce::Justification::left);
}

void MainComponent::resized() {
    themeButton_.setBounds(getWidth() - 160, 28, 128, 34);
    shape_.setBounds(32, 104, 172, 36);
    // Editing and persistence live above the canvas; transport and export below.
    length_.setBounds(getWidth() - 212, getHeight() - 64, 180, 36);
    waveformView_.setBounds(32, 164, getWidth() - 64, getHeight() - 276);
}
} // namespace wtd
