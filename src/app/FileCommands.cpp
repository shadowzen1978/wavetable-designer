#include "app/MainComponent.h"
#include <stdexcept>

namespace wtd {
bool MainComponent::documentDirty() const {
    return !(ProjectData { editor_.waveform(), editor_.preset(), editor_.selection() } == savedProject_);
}
ExportOptions MainComponent::exportOptions() const {
    return { exportProfile_.getSelectedId() == 1 ? ExportProfile::sz404 : ExportProfile::generic,
        static_cast<ExportFormat>(exportFormat_.getSelectedId() - 1), exportRate_.getSelectedId() };
}
juce::String MainComponent::exportSummary() const {
    const auto options = exportOptions();
    const auto count = options.profile == ExportProfile::sz404 ? 1024 : static_cast<int>(editor_.waveform().size());
    juce::String summary = options.profile == ExportProfile::sz404 ? "SZ404: " : "Generic: ";
    if (count != static_cast<int>(editor_.waveform().size()))
        summary += juce::String(static_cast<int>(editor_.waveform().size())) + " -> ";
    summary += juce::String(count) + " samples, mono, ";
    summary += isPcm16(options.format) ? "PCM16" : "float32";
    summary += isRaw(options.format) ? " LE raw (" + juce::String(count * (isPcm16(options.format) ? 2 : 4)) + " bytes)"
        : " WAV, " + juce::String(options.sampleRate) + " Hz";
    if (count != static_cast<int>(editor_.waveform().size())) summary += " (resampled copy)";
    return summary;
}
void MainComponent::updateFileControls() {
    const bool available = !fileActionPending_ && !editor_.smoothingActive() && !editor_.strokeActive();
    for (auto* button : { &new_, &open_, &save_, &saveAs_, &export_ }) button->setEnabled(available);
    exportProfile_.setEnabled(!fileActionPending_);
    exportFormat_.setEnabled(!fileActionPending_);
    exportRate_.setEnabled(!fileActionPending_ && !isRaw(exportOptions().format));
    const auto title = (projectFile_ == juce::File() ? juce::String("Untitled") : projectFile_.getFileName())
        + (documentDirty() ? " *" : "");
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        window->setName(title + " - Wavetable Designer");
    repaint();
}
void MainComponent::fileError(const juce::String& message) {
    fileStatus_ = message;
    fileStatusUntil_ = juce::Time::currentTimeMillis() + 4000;
    if (isShowing()) juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::WarningIcon, "File operation failed", message, "OK", this);
    repaint();
}
bool MainComponent::saveProjectFile(const juce::File& file) {
    try {
        const auto project = captureProject(editor_);
        const auto result = writeProject(file, project);
        if (result.failed()) { fileError(result.getErrorMessage()); return false; }
        savedProject_ = project;
        projectFile_ = file;
        fileStatus_ = "Saved " + file.getFileName();
        fileStatusUntil_ = juce::Time::currentTimeMillis() + 4000;
        syncControls(); return true;
    } catch (const std::exception& error) { fileError(error.what()); return false; }
}
bool MainComponent::loadProjectFile(const juce::File& file) {
    try {
        auto project = readProject(file); // Fully parse/validate before replacing anything.
        editor_.loadDocument(project.waveform, project.preset, project.selection);
        savedProject_ = std::move(project);
        projectFile_ = file;
        audioPreview_.setPlaying(false);
        zoom_.setSelectedId(1, juce::dontSendNotification);
        waveformView_.setZoom(1.0);
        fileStatus_ = "Opened " + file.getFileName();
        fileStatusUntil_ = juce::Time::currentTimeMillis() + 4000;
        syncControls(); return true;
    } catch (const std::exception& error) { fileError(error.what()); return false; }
}
bool MainComponent::exportToFile(const juce::File& file, const Waveform& wave, ExportOptions options) {
    try {
        const auto bytes = encodeExport(wave, options);
        const auto result = replaceFileSafely(file, bytes.data(), bytes.size());
        if (result.failed()) { fileError(result.getErrorMessage()); return false; }
        fileStatus_ = "Exported " + file.getFileName();
        fileStatusUntil_ = juce::Time::currentTimeMillis() + 4000;
        repaint(); return true;
    } catch (const std::exception& error) { fileError(error.what()); return false; }
}
void MainComponent::confirmOverwrite(const juce::File& file, std::function<void()> write) {
    if (!file.exists()) { write(); return; }
    const juce::Component::SafePointer<MainComponent> safe(this);
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Replace file?",
        file.getFullPathName() + " already exists.", "Replace", "Cancel", this,
        juce::ModalCallbackFunction::create([safe, write = std::move(write)](int answer) {
            if (!safe) return;
            if (answer == 1) write();
            else { safe->fileActionPending_ = false; safe->syncControls(); }
        }));
}
void MainComponent::confirmDiscard(std::function<void()> action) {
    if (fileActionPending_) return;
    editor_.endStroke();
    if (!documentDirty()) { action(); return; }
    fileActionPending_ = true; syncControls();
    const juce::Component::SafePointer<MainComponent> safe(this);
    juce::AlertWindow::showYesNoCancelBox(juce::MessageBoxIconType::QuestionIcon, "Unsaved project",
        "Save your changes before continuing?", "Save", "Discard", "Cancel", this,
        juce::ModalCallbackFunction::create([safe, action = std::move(action)](int answer) {
            if (!safe) return;
            safe->fileActionPending_ = false;
            safe->syncControls();
            if (answer == 1) safe->saveProject(false, action);
            else if (answer == 2) action();
        }));
}
void MainComponent::newProject() {
    if (editor_.smoothingActive()) return;
    confirmDiscard([this] {
        editor_.loadDocument(Waveform(), Shape::sine, std::nullopt);
        savedProject_ = captureProject(editor_);
        projectFile_ = {};
        audioPreview_.setPlaying(false);
        zoom_.setSelectedId(1, juce::dontSendNotification);
        waveformView_.setZoom(1.0);
        fileStatus_.clear();
        syncControls();
    });
}
void MainComponent::openProject() {
    if (editor_.smoothingActive()) return;
    confirmDiscard([this] {
        fileActionPending_ = true; syncControls();
        fileChooser_ = std::make_unique<juce::FileChooser>("Open waveform project", projectFile_, "*.wtd");
        const juce::Component::SafePointer<MainComponent> safe(this);
        fileChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& chooser) {
                if (!safe) return;
                const auto file = chooser.getResult();
                safe->fileActionPending_ = false;
                if (file != juce::File()) safe->loadProjectFile(file);
                safe->syncControls();
            });
    });
}
void MainComponent::saveProject(bool saveAs, std::function<void()> afterSave) {
    if (fileActionPending_ || editor_.smoothingActive()) return;
    editor_.endStroke();
    if (!saveAs && projectFile_ != juce::File()) {
        if (saveProjectFile(projectFile_) && afterSave) afterSave();
        return;
    }
    fileActionPending_ = true; syncControls();
    const auto initial = projectFile_ == juce::File()
        ? juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Untitled.wtd")
        : projectFile_;
    fileChooser_ = std::make_unique<juce::FileChooser>("Save waveform project", initial, "*.wtd");
    const juce::Component::SafePointer<MainComponent> safe(this);
    fileChooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, afterSave = std::move(afterSave)](const juce::FileChooser& chooser) {
            if (!safe) return;
            const auto chosen = chooser.getResult();
            if (chosen == juce::File()) { safe->fileActionPending_ = false; safe->syncControls(); return; }
            const auto file = chosen.withFileExtension("wtd");
            safe->confirmOverwrite(file, [safe, file, afterSave] {
                if (!safe) return;
                safe->fileActionPending_ = false;
                const bool saved = safe->saveProjectFile(file);
                safe->syncControls();
                if (saved && afterSave) afterSave();
            });
        });
}
void MainComponent::exportFile() {
    if (fileActionPending_ || editor_.smoothingActive()) return;
    editor_.endStroke();
    const auto wave = editor_.waveform();
    const auto options = exportOptions();
    const auto extension = isRaw(options.format) ? ".raw" : ".wav";
    const auto parent = projectFile_ == juce::File()
        ? juce::File::getSpecialLocation(juce::File::userDocumentsDirectory) : projectFile_.getParentDirectory();
    const auto name = projectFile_ == juce::File() ? juce::String("waveform") : projectFile_.getFileNameWithoutExtension();
    fileActionPending_ = true; syncControls();
    fileChooser_ = std::make_unique<juce::FileChooser>("Export " + exportSummary(),
        parent.getChildFile(name + extension), juce::String("*") + extension);
    const juce::Component::SafePointer<MainComponent> safe(this);
    fileChooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, wave, options, extension](const juce::FileChooser& chooser) {
            if (!safe) return;
            const auto chosen = chooser.getResult();
            if (chosen == juce::File()) { safe->fileActionPending_ = false; safe->syncControls(); return; }
            const auto file = chosen.withFileExtension(extension);
            safe->confirmOverwrite(file, [safe, file, wave, options] {
                if (!safe) return;
                safe->fileActionPending_ = false;
                safe->exportToFile(file, wave, options);
                safe->syncControls();
            });
        });
}
void MainComponent::requestQuit(std::function<void()> quit) {
    if (fileActionPending_) return;
    if (!editor_.smoothingActive()) { confirmDiscard(std::move(quit)); return; }
    fileActionPending_ = true; syncControls();
    const juce::Component::SafePointer<MainComponent> safe(this);
    juce::AlertWindow::showYesNoCancelBox(juce::MessageBoxIconType::QuestionIcon, "Smoothing preview",
        "Apply this preview before closing?", "Apply", "Discard preview", "Cancel", this,
        juce::ModalCallbackFunction::create([safe, quit = std::move(quit)](int answer) {
            if (!safe) return;
            safe->fileActionPending_ = false;
            if (answer == 1) safe->editor_.applySmoothing();
            else if (answer == 2) safe->editor_.cancelSmoothing();
            safe->syncControls();
            if (answer != 0) safe->confirmDiscard(quit);
        }));
}
} // namespace wtd
