// Optional local compatibility harness: invokes the actual prebuilt SZ404 processor.
// No sister-repository files are modified and no audio device/editor is opened.
#include "app/PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "Expected native smoke fixture directory\n"; return 2; }
    const juce::ScopedJuceInitialiser_GUI gui;
    const juce::File directory(juce::String::fromUTF8(argv[1]));
    auto processor = std::make_unique<sz404::SZ404AudioProcessor>();
    int checked = 0;
    for (const int count : { 1024, 2048 }) {
        const auto project = juce::JSON::parse(directory.getChildFile(juce::String(count) + ".wtd"));
        const auto* source = project["samples"].getArray();
        if (!source || source->size() != count) return 3;
        for (const int rate : { 44100, 48000 }) for (const int format : { 1, 2, 3 }) {
            const int profile = format == 2 ? 2 : 1;
            const bool raw = format == 3;
            const auto name = juce::String(count) + "-" + juce::String(profile) + "-"
                + juce::String(format) + "-" + juce::String(rate) + (raw ? ".raw" : ".wav");
            juce::String error;
            if (!processor->loadWavetableFile(directory.getChildFile(name), error)) {
                std::cerr << name << ": " << error << "\n"; return 4;
            }
            for (int i = 0; i < 1024; ++i) {
                // Generic 2048 WAV tests the importer's intentional truncation.
                // SZ404 fixtures always carry the COMPLETE resampled 1024 cycle.
                const int index = profile == 1 ? i * count / 1024 : i;
                const auto value = static_cast<double>(source->getReference(index));
                const auto authoredPcm = std::lround(value * 32767.0);
                const auto expected = format == 1
                    ? std::lround(static_cast<double>(authoredPcm) / 32768.0 * 32767.0) : authoredPcm;
                if (processor->session.wavetable[static_cast<std::size_t>(i)] != expected) {
                    std::cerr << name << " mismatch at " << i << "\n"; return 5;
                }
            }
            ++checked;
        }
    }
    std::cout << "Actual SZ404 loadWavetableFile passed: " << checked << " WAV/raw imports.\n";
    return 0;
}
