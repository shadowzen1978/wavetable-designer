#include "app/MainComponent.h"
#include <bit>
#include <limits>

namespace wtd {
bool MainComponent::runFileSmokeCheck(const juce::File& directory) {
    const auto fail = [&](int line) { directory.getChildFile("file-check-failure.txt").replaceWithText("line " + juce::String(line)); return false; };
    directory.getChildFile("file-check-failure.txt").deleteFile();
    const auto files = directory.getChildFile("files");
    if (files.createDirectory().failed()) return fail(__LINE__);
    for (const auto count : { 1024u, 2048u }) {
        Waveform wave(count, Shape::saw);
        // Broad float precision, tiny values, DC and asymmetric seam.
        juce::Random random(1337);
        for (std::size_t i = 0; i < count; ++i) wave.setSample(i, random.nextFloat() * 1.8f - 0.9f);
        wave.setSample(0, -1); wave.setSample(count - 1, 0.37f);
        wave.setSample(10, std::numeric_limits<float>::denorm_min());
        wave.setSample(11, std::numeric_limits<float>::min());
        wave.setSample(12, -0.0f);
        editor_.loadDocument(wave, std::nullopt, SampleRange { 24, 800 });
        projectFile_ = {}; savedProject_ = { Waveform(count), Shape::sine, std::nullopt };
        setSize(960, 640); syncControls();
        if (!documentDirty()) return fail(__LINE__);
        const auto file = files.getChildFile(juce::String(count) + ".wtd");
        if (!saveProjectFile(file) || documentDirty()) return fail(__LINE__);
        const auto saved = captureProject(editor_);
        const auto text = file.loadFileAsString();
        const auto restored = projectFromJson(text);
        if (!(saved == restored)) return fail(__LINE__);
        for (std::size_t i = 0; i < count; ++i)
            if (std::bit_cast<std::uint32_t>(saved.waveform.samples()[i])
                != std::bit_cast<std::uint32_t>(restored.waveform.samples()[i])) return fail(__LINE__);
        editor_.applyPreset(Shape::square); syncControls();
        if (!documentDirty()) return fail(__LINE__);
        editor_.undo(); syncControls(); if (documentDirty()) return fail(__LINE__);
        editor_.redo(); syncControls(); if (!documentDirty()) return fail(__LINE__);
        if (!loadProjectFile(file) || !(captureProject(editor_) == saved) || documentDirty()
            || editor_.canUndo() || editor_.canRedo()) return fail(__LINE__);
        // Malformed files must leave waveform, document identity, history and dirty state intact.
        editor_.applyPreset(Shape::triangle); syncControls();
        const auto before = captureProject(editor_);
        const auto bad = files.getChildFile("bad.wtd");
        for (const auto invalid : { juce::String("{"), text.replace("\"version\": 1", "\"version\": 2"), text + " {}", text + "garbage",
                text.replace("\"sampleCount\": " + juce::String(count), "\"sampleCount\": 8") }) {
            if (!bad.replaceWithText(invalid)) return fail(__LINE__);
            if (loadProjectFile(bad) || !(captureProject(editor_) == before)
                || projectFile_ != file || !documentDirty() || !editor_.canUndo()) return fail(__LINE__);
        }
        // Exercise validation with correctly formed JSON carrying invalid typed fields.
        for (int test = 0; test < 9; ++test) {
            auto json = juce::JSON::parse(text);
            auto* object = json.getDynamicObject();
            switch (test) {
                case 0: object->setProperty("version", "1"); break;
                case 1: object->setProperty("samples", juce::Array<juce::var>()); break;
                case 2: json["samples"].getArray()->getReference(0) = true; break;
                case 3: json["samples"].getArray()->getReference(0) = 1.01; break;
                case 4: json["samples"].getArray()->getReference(0) = juce::var(); break;
                case 5: object->setProperty("preset", "unknown"); break;
                case 6: object->setProperty("selection", 5); break;
                case 7: json["selection"].getDynamicObject()->setProperty("last", static_cast<int>(count)); break;
                case 8: object->removeProperty("samples"); break;
            }
            try { (void) projectFromJson(juce::JSON::toString(json)); return fail(__LINE__); }
            catch (const std::exception&) {}
        }
        try { (void) projectFromJson(juce::String::repeatedString("[", 30)); return fail(__LINE__); }
        catch (const std::exception&) {}
        try { (void) projectFromJson(juce::String::repeatedString(" ", 1024 * 1024 + 1)); return fail(__LINE__); }
        catch (const std::exception&) {}
        // Failed writes retain the saved checkpoint and do not damage a destination.
        if (saveProjectFile(files) || projectFile_ != file || !documentDirty()) return fail(__LINE__);
        if (file.loadFileAsString() != text) return fail(__LINE__);
       #if JUCE_WINDOWS
        {
            // An open output handle denies replacement: the original file/checkpoint must survive.
            auto locked = file.createOutputStream();
            if (!locked || saveProjectFile(file) || !documentDirty()) return fail(__LINE__);
            if (file.loadFileAsString() != text || projectFile_ != file) return fail(__LINE__);
        }
       #endif
        if (!loadProjectFile(file)) return fail(__LINE__);
        startSmoothing();
        if (save_.isEnabled() || open_.isEnabled() || export_.isEnabled()) return fail(__LINE__);
        try { (void) captureProject(editor_); return fail(__LINE__); } catch (const std::logic_error&) {}
        cancelSmoothing_.onClick();
        if (documentDirty() || !save_.isEnabled() || !export_.isEnabled()) return fail(__LINE__);
        // Every supported GUI profile/format/rate writes a real product export.
        for (const auto profile : { 1, 2 }) for (const auto format : { 1, 2, 3, 4 }) {
            if (profile == 1 && (format == 2 || format == 4)) continue;
            for (const auto rate : { 44100, 48000 }) {
                exportProfile_.setSelectedId(profile, juce::sendNotificationSync);
                exportFormat_.setSelectedId(format, juce::sendNotificationSync);
                exportRate_.setSelectedId(rate, juce::sendNotificationSync);
                const auto options = exportOptions();
                if (exportRate_.isEnabled() == isRaw(options.format)) return fail(__LINE__);
                const auto name = juce::String(count) + "-" + juce::String(profile) + "-"
                    + juce::String(format) + "-" + juce::String(rate) + (isRaw(options.format) ? ".raw" : ".wav");
                if (!exportToFile(files.getChildFile(name), editor_.waveform(), options)) return fail(__LINE__);
                if (!(captureProject(editor_) == saved) || documentDirty()) return fail(__LINE__);
            }
        }
        // Keyboard Save writes the associated file and resets the dirty checkpoint.
        editor_.applyPreset(Shape::square); syncControls();
        if (!keyPressed(juce::KeyPress('s', juce::ModifierKeys::commandModifier, 0)) || documentDirty()) return fail(__LINE__);
        // Restore the original authored fixture for independent numerical checks.
        editor_.loadDocument(saved.waveform, saved.preset, saved.selection);
        if (!saveProjectFile(file)) return fail(__LINE__);
        fileStatus_.clear();
        // Selecting SZ404 after float generic export switches to an allowed PCM format.
        exportProfile_.setSelectedId(2, juce::sendNotificationSync);
        exportFormat_.setSelectedId(2, juce::sendNotificationSync);
        exportProfile_.setSelectedId(1, juce::sendNotificationSync);
        if (exportFormat_.getSelectedId() != 1 || exportFormat_.isItemEnabled(2)) return fail(__LINE__);
        if (count == 2048 && !exportSummary().contains("resampled copy")) return fail(__LINE__);
        for (const auto small : { false, true }) for (const auto dark : { true, false }) {
            setSize(small ? 640 : 960, small ? 480 : 640); setDarkTheme(dark); syncControls();
            auto stream = directory.getChildFile(juce::String("files-") + (dark ? "dark-" : "light-")
                + juce::String(count) + (small ? "-small.png" : ".png")).createOutputStream();
            if (!stream) return fail(__LINE__);
            stream->setPosition(0); stream->truncate();
            if (!juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()), *stream)) return fail(__LINE__);
        }
    }
    // New replaces a clean document without a prompt, resetting history and preview.
    editor_.loadDocument(Waveform(), Shape::sine, std::nullopt);
    savedProject_ = captureProject(editor_); syncControls();
    if (!keyPressed(juce::KeyPress('n', juce::ModifierKeys::commandModifier, 0))) return fail(__LINE__);
    if (projectFile_ != juce::File() || documentDirty() || editor_.canUndo() || audioPreview_.playing()) return fail(__LINE__);
    return true;
}
} // namespace wtd
