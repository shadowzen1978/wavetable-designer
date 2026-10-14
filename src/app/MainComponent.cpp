#include "app/MainComponent.h"

namespace wtd {
namespace {
constexpr Shape shapes[] { Shape::sine, Shape::triangle, Shape::saw, Shape::square, Shape::flat };
class AudioSettingsWindow final : public juce::DialogWindow {
public:
    AudioSettingsWindow(juce::AudioDeviceManager& manager, juce::LookAndFeel& look, juce::Colour background)
        : DialogWindow("Audio settings", background, true) {
        setLookAndFeel(&look);
        setUsingNativeTitleBar(true);
        auto* selector = new juce::AudioDeviceSelectorComponent(manager, 0, 0, 0, 2, false, false, true, true);
        selector->setSize(560, 380);
        setContentOwned(selector, true);
        setResizable(true, false);
        setResizeLimits(480, 320, 900, 650);
        centreWithSize(560, 400);
    }
    ~AudioSettingsWindow() override { setLookAndFeel(nullptr); }
    void closeButtonPressed() override { setVisible(false); }
};
}

MainComponent::MainComponent(bool enableAudioDevices) : audioPreview_(enableAudioDevices) {
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
    play_.onClick = [this] {
        audioError_ = audioPreview_.setPlaying(!audioPreview_.playing());
        updateAudioControls();
    };
    play_.setTooltip("Play or stop the current waveform (Space)");
    static const char* noteNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int midi = 24; midi <= 108; ++midi)
        note_.addItem(juce::String(noteNames[midi % 12]) + juce::String(midi / 12 - 1) + "  "
            + juce::String(noteFrequency(midi), 0) + " Hz", midi + 1);
    note_.setSelectedId(70, juce::dontSendNotification);
    note_.setTitle("Preview note");
    note_.onChange = [this] { audioPreview_.setNote(note_.getSelectedId() - 1); };
    previewVolume_.setSliderStyle(juce::Slider::LinearHorizontal);
    previewVolume_.setTextBoxStyle(juce::Slider::TextBoxRight, true, 42, 26);
    previewVolume_.setRange(0.0, 100.0, 1.0);
    previewVolume_.setValue(20.0, juce::dontSendNotification);
    previewVolume_.setTextValueSuffix("%");
    previewVolume_.setName("Preview volume");
    previewVolume_.setTooltip("Listening volume only; authored and exported samples retain their amplitudes");
    previewVolume_.onValueChange = [this] { audioPreview_.setVolume(static_cast<float>(previewVolume_.getValue() / 100.0)); };
    audioSettingsButton_.onClick = [this] { showAudioSettings(); };
    themeButton_.onClick = [this] { dark_ = !dark_; applyTheme(); };
    for (auto* component : { static_cast<juce::Component*>(&shape_),
                              static_cast<juce::Component*>(&length_),
                              static_cast<juce::Component*>(&undo_),
                              static_cast<juce::Component*>(&redo_),
                              static_cast<juce::Component*>(&clear_),
                              static_cast<juce::Component*>(&smooth_),
                              static_cast<juce::Component*>(&zoom_),
                              static_cast<juce::Component*>(&themeButton_),
                              static_cast<juce::Component*>(&waveformView_),
                              static_cast<juce::Component*>(&play_),
                              static_cast<juce::Component*>(&note_),
                              static_cast<juce::Component*>(&previewVolume_),
                              static_cast<juce::Component*>(&audioSettingsButton_) })
        addAndMakeVisible(component);
    new_.onClick = [this] { newProject(); };
    open_.onClick = [this] { openProject(); };
    save_.onClick = [this] { saveProject(false); };
    saveAs_.onClick = [this] { saveProject(true); };
    export_.onClick = [this] { exportFile(); };
    new_.setTooltip("New project (Ctrl/Cmd+N)");
    open_.setTooltip("Open project (Ctrl/Cmd+O)");
    save_.setTooltip("Save project (Ctrl/Cmd+S). Apply or cancel smoothing first.");
    saveAs_.setTooltip("Save project under a new name (Ctrl/Cmd+Shift+S)");
    export_.setTooltip("Export one authored cycle (Ctrl/Cmd+E). Preview gain and filtering are excluded.");
    exportProfile_.addItem("SZ404", 1);
    exportProfile_.addItem("Generic", 2);
    exportProfile_.setSelectedId(1, juce::dontSendNotification);
    exportProfile_.setTitle("Export profile");
    exportProfile_.setTooltip("SZ404 exports a complete 1024-sample PCM16 copy. Generic retains the editing length.");
    exportFormat_.addItemList({ "PCM16 WAV", "Float32 WAV", "PCM16 raw", "Float32 raw" }, 1);
    exportFormat_.setSelectedId(1, juce::dontSendNotification);
    exportFormat_.setTitle("Export encoding and container");
    exportRate_.addItem("44.1 kHz", 44100);
    exportRate_.addItem("48 kHz", 48000);
    exportRate_.setSelectedId(44100, juce::dontSendNotification);
    exportRate_.setTitle("WAV header rate");
    exportRate_.setTooltip("WAV metadata only; independent of note pitch and audio device rate. Raw has no rate.");
    exportProfile_.onChange = [this] {
        const bool sz404 = exportProfile_.getSelectedId() == 1;
        exportFormat_.setItemEnabled(2, !sz404);
        exportFormat_.setItemEnabled(4, !sz404);
        if (sz404 && !isPcm16(exportOptions().format))
            exportFormat_.setSelectedId(1, juce::dontSendNotification);
        fileStatus_.clear(); updateFileControls();
    };
    exportFormat_.onChange = [this] { fileStatus_.clear(); updateFileControls(); };
    exportRate_.onChange = [this] { fileStatus_.clear(); updateFileControls(); };
    exportFormat_.setItemEnabled(2, false);
    exportFormat_.setItemEnabled(4, false);
    for (auto* component : { static_cast<juce::Component*>(&new_), static_cast<juce::Component*>(&open_),
        static_cast<juce::Component*>(&save_), static_cast<juce::Component*>(&saveAs_),
        static_cast<juce::Component*>(&export_), static_cast<juce::Component*>(&exportProfile_),
        static_cast<juce::Component*>(&exportFormat_), static_cast<juce::Component*>(&exportRate_) })
        addAndMakeVisible(component);
    setSize(960, 640);
    syncControls();
    applyTheme();
    startTimerHz(10);
}

MainComponent::~MainComponent() {
    stopTimer();
    audioSettings_.reset();
    fileChooser_.reset();
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
    shape_.setEnabled(!smoothing && !fileActionPending_);
    length_.setEnabled(!smoothing && !fileActionPending_);
    clear_.setEnabled(!smoothing && !fileActionPending_);
    smooth_.setEnabled(!smoothing && !fileActionPending_);
    waveformView_.setEnabled(!fileActionPending_);
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
    undo_.setEnabled(!fileActionPending_ && editor_.canUndo());
    redo_.setEnabled(!fileActionPending_ && editor_.canRedo());
    updateFileControls();
    resized();
    if (!submittedWaveform_ || *submittedWaveform_ != editor_.waveform()) {
        submittedWaveform_ = editor_.waveform();
        audioPreview_.requestWaveform(*submittedWaveform_);
    }
    waveformView_.repaint();
    repaint();
}

void MainComponent::timerCallback() { updateAudioControls(); }
void MainComponent::updateAudioControls() {
    play_.setButtonText(audioPreview_.playing() ? "Stop" : "Play");
    play_.setTooltip("Play/stop (Space). " + audioPreview_.status());
    repaint(0, getHeight() - 66, getWidth(), 66);
}
void MainComponent::showAudioSettings() {
    audioError_ = audioPreview_.ensureDevice();
    if (!audioSettings_) audioSettings_ = std::make_unique<AudioSettingsWindow>(
        audioPreview_.deviceManager(), lookAndFeel_, themeFor(dark_).panel);
    audioSettings_->setVisible(true);
    audioSettings_->toFront(true);
    updateAudioControls();
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
    if (fileActionPending_) return true;
    if (key.getKeyCode() == juce::KeyPress::spaceKey && !key.getModifiers().isAnyModifierKeyDown()) {
        play_.onClick(); return true;
    }
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
    if (!editor_.smoothingActive()) {
        if (code == 'n' || code == 'N') { newProject(); return true; }
        if (code == 'o' || code == 'O') { openProject(); return true; }
        if (code == 's' || code == 'S') { saveProject(mods.isShiftDown()); return true; }
        if (code == 'e' || code == 'E') { exportFile(); return true; }
    }
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
    lookAndFeel_.setColour(juce::Label::textColourId, theme.text);
    lookAndFeel_.setColour(juce::ListBox::backgroundColourId, theme.panel);
    lookAndFeel_.setColour(juce::ListBox::textColourId, theme.text);
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
    if (audioSettings_) { audioSettings_->setBackgroundColour(theme.panel); audioSettings_->sendLookAndFeelChange(); }
    sendLookAndFeelChange();
    repaint();
}

void MainComponent::paint(juce::Graphics& g) {
    const auto theme = themeFor(dark_);
    g.fillAll(theme.background);
    g.setColour(theme.text);
    g.setFont(juce::FontOptions(getWidth() < 800 ? 22.0f : 26.0f));
    g.drawText("Wavetable Designer", 32, 24, getWidth() - 396, 36, juce::Justification::left);
    g.setColour(theme.muted);
    g.setFont(juce::FontOptions(15.0f));
    const auto name = (projectFile_ == juce::File() ? juce::String("Untitled") : projectFile_.getFileName())
        + (documentDirty() ? " *" : "");
    g.drawText(name, 356, 76, getWidth() - 388, 24, juce::Justification::left);
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
    g.drawText("Volume", 284, getHeight() - 99, 52, 26, juce::Justification::left);
    g.setFont(juce::FontOptions(12.0f));
    const auto audioStatus = audioPreview_.status();
    const auto summary = fileStatus_.isNotEmpty() && juce::Time::currentTimeMillis() < fileStatusUntil_
        ? fileStatus_ + " | " + exportSummary() : exportSummary();
    const bool audioFailed = audioStatus.startsWith("Unable") || audioStatus.startsWith("Audio output stopped");
    g.drawText(audioError_.isNotEmpty() ? audioError_ : audioFailed ? audioStatus : summary,
        32, getHeight() - 26, getWidth() - 64, 22, juce::Justification::left);
}

void MainComponent::resized() {
    themeButton_.setBounds(getWidth() - 160, 28, 128, 34);
    shape_.setBounds(32, 104, 172, 36);
    zoom_.setBounds(getWidth() - 348, 28, 172, 34);
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
    new_.setBounds(32, 72, 60, 28);
    open_.setBounds(100, 72, 72, 28);
    save_.setBounds(180, 72, 64, 28);
    saveAs_.setBounds(252, 72, 88, 28);
    exportProfile_.setBounds(32, getHeight() - 64, 100, 32);
    exportFormat_.setBounds(140, getHeight() - 64, 134, 32);
    exportRate_.setBounds(282, getHeight() - 64, 86, 32);
    length_.setBounds(getWidth() - 264, getHeight() - 64, 136, 32);
    export_.setBounds(getWidth() - 120, getHeight() - 64, 88, 32);
    play_.setBounds(32, getHeight() - 102, 80, 32);
    note_.setBounds(128, getHeight() - 102, 140, 32);
    previewVolume_.setBounds(342, getHeight() - 102, getWidth() - 530, 32);
    audioSettingsButton_.setBounds(getWidth() - 164, getHeight() - 102, 132, 32);
    const int top = editor_.smoothingActive() ? 252 : 164;
    waveformView_.setBounds(32, top, getWidth() - 64, getHeight() - top - 112);
}
} // namespace wtd
