#pragma once
#include "core/EditorModel.h"
#include <juce_core/juce_core.h>

namespace wtd {
struct ProjectData {
    Waveform waveform;
    std::optional<Shape> preset;
    std::optional<SampleRange> selection;
    bool operator==(const ProjectData&) const = default;
};
[[nodiscard]] ProjectData captureProject(const EditorModel& editor);
[[nodiscard]] juce::String projectToJson(const ProjectData& project);
// Decode fully before modifying the editor. Invalid/future projects throw.
[[nodiscard]] ProjectData projectFromJson(const juce::String& text);
[[nodiscard]] ProjectData readProject(const juce::File& file);
[[nodiscard]] juce::Result writeProject(const juce::File& file, const ProjectData& project);
[[nodiscard]] juce::Result replaceFileSafely(const juce::File& file, const void* data, std::size_t size);
} // namespace wtd
