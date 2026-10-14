#include "io/ProjectFile.h"
#include <cmath>
#include <stdexcept>

namespace wtd {
namespace {
constexpr const char* names[] { "sine", "triangle", "saw", "square", "flat" };
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
const juce::var& field(const juce::var& object, const char* key) {
    if (!object.isObject() || !object.hasProperty(key)) invalid("Missing project field");
    return object[juce::Identifier(key)];
}
int integer(const juce::var& value) {
    if (!value.isInt() && !value.isInt64()) invalid("Expected an integer project field");
    const auto number = static_cast<juce::int64>(value);
    if (number < 0 || number > 65536) invalid("Project integer is out of range");
    return static_cast<int>(number);
}
void validate(const ProjectData& project) {
    if (project.waveform.size() != 1024 && project.waveform.size() != 2048)
        invalid("Project requires 1024 or 2048 samples");
    if (project.selection && (project.selection->first > project.selection->last
        || project.selection->last >= project.waveform.size())) invalid("Invalid selection range");
    if (project.preset && (static_cast<int>(*project.preset) < 0 || static_cast<int>(*project.preset) > 4))
        invalid("Invalid preset");
}
// Limit pathological nesting before entering JUCE's recursive JSON parser.
void checkNesting(const juce::String& text) {
    int depth = 0; bool quoted = false, escaped = false, started = false, finished = false;
    char stack[16] {};
    for (auto p = text.getCharPointer(); !p.isEmpty(); ++p) {
        const auto c = *p;
        if (finished && !juce::CharacterFunctions::isWhitespace(c)) invalid("Unexpected data after project JSON");
        if (quoted) {
            if (c < 0x20) invalid("Unescaped control character in project JSON");
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '[' || c == '{') {
            if (depth >= 16) invalid("Project JSON is too deeply nested");
            stack[depth++] = static_cast<char>(c); started = true;
        } else if (c == ']' || c == '}') {
            if (depth == 0 || stack[depth - 1] != (c == ']' ? '[' : '{')) invalid("Unbalanced project JSON");
            if (--depth == 0) finished = true;
        } else if (c == 0x27) invalid("Project JSON requires double-quoted strings");
        else if (!started && !juce::CharacterFunctions::isWhitespace(c)) invalid("Expected a project JSON object");
    }
}
}
ProjectData captureProject(const EditorModel& editor) {
    if (editor.smoothingActive() || editor.strokeActive())
        throw std::logic_error("Finish drawing or apply/cancel smoothing before saving");
    return { editor.waveform(), editor.preset(), editor.selection() };
}
juce::String projectToJson(const ProjectData& project) {
    validate(project);
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("format", "wavetable-designer");
    object->setProperty("version", 1);
    object->setProperty("sampleCount", static_cast<int>(project.waveform.size()));
    juce::Array<juce::var> samples;
    for (const auto value : project.waveform.samples()) samples.add(static_cast<double>(value));
    object->setProperty("samples", samples);
    object->setProperty("preset", project.preset ? juce::var(names[static_cast<int>(*project.preset)]) : juce::var());
    juce::var range;
    if (project.selection) {
        auto selection = std::make_unique<juce::DynamicObject>();
        selection->setProperty("first", static_cast<int>(project.selection->first));
        selection->setProperty("last", static_cast<int>(project.selection->last));
        range = juce::var(selection.release());
    }
    object->setProperty("selection", range);
    return juce::JSON::toString(juce::var(object.release()), false, 17) + "\n";
}
ProjectData projectFromJson(const juce::String& text) {
    if (text.getNumBytesAsUTF8() > 1024 * 1024) invalid("Project exceeds the 1 MiB size limit");
    checkNesting(text);
    juce::var root;
    const auto parsed = juce::JSON::parse(text, root);
    if (parsed.failed()) throw std::invalid_argument(parsed.getErrorMessage().toStdString());
    const auto& format = field(root, "format");
    if (!format.isString() || format.toString() != "wavetable-designer") invalid("Not a Wavetable Designer project");
    if (integer(field(root, "version")) != 1) invalid("Unsupported project version");
    const int count = integer(field(root, "sampleCount"));
    if (count != 1024 && count != 2048) invalid("Project requires 1024 or 2048 samples");
    const auto* samples = field(root, "samples").getArray();
    if (!samples || samples->size() != count) invalid("Sample array does not match sampleCount");
    ProjectData project { Waveform(static_cast<std::size_t>(count)), std::nullopt, std::nullopt };
    for (int i = 0; i < count; ++i) {
        const auto& sample = samples->getReference(i);
        if (!sample.isInt() && !sample.isInt64() && !sample.isDouble()) invalid("Samples must be numbers");
        const auto value = static_cast<double>(sample);
        if (!std::isfinite(value) || value < -1.0 || value > 1.0) invalid("Samples must be finite and within [-1, 1]");
        project.waveform.setSample(static_cast<std::size_t>(i), static_cast<float>(value));
    }
    const auto& preset = field(root, "preset");
    if (!preset.isVoid()) {
        if (!preset.isString()) invalid("Preset must be a name or null");
        bool found = false;
        for (int i = 0; i < 5; ++i) if (preset.toString() == names[i]) {
            project.preset = static_cast<Shape>(i); found = true;
        }
        if (!found) invalid("Unknown preset");
    }
    const auto& selection = field(root, "selection");
    if (!selection.isVoid()) project.selection = SampleRange {
        static_cast<std::size_t>(integer(field(selection, "first"))),
        static_cast<std::size_t>(integer(field(selection, "last"))) };
    validate(project);
    return project;
}
ProjectData readProject(const juce::File& file) {
    if (!file.existsAsFile()) invalid("Project file does not exist");
    if (file.getSize() > 1024 * 1024) invalid("Project exceeds the 1 MiB size limit");
    auto stream = file.createInputStream();
    if (!stream || !stream->openedOk()) invalid("Cannot read project file");
    return projectFromJson(stream->readEntireStreamAsString());
}
juce::Result replaceFileSafely(const juce::File& file, const void* data, std::size_t size) {
    if (file.isDirectory()) return juce::Result::fail("Choose a file, not a folder");
    juce::TemporaryFile temporary(file);
    {
        juce::FileOutputStream stream(temporary.getFile());
        if (!stream.openedOk()) return stream.getStatus();
        if (!stream.write(data, size)) return juce::Result::fail("Could not write the complete file");
        stream.flush();
        if (stream.getStatus().failed()) return stream.getStatus();
    }
    if (!temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not replace the destination file");
    return juce::Result::ok();
}
juce::Result writeProject(const juce::File& file, const ProjectData& project) {
    const auto json = projectToJson(project);
    return replaceFileSafely(file, json.toRawUTF8(), json.getNumBytesAsUTF8());
}
} // namespace wtd
