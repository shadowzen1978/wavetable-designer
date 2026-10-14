#pragma once
#include "core/Waveform.h"
#include <array>
#include <span>

namespace wtd {
// Immutable, playback-only Fourier tables. Construction belongs off the audio thread.
struct PreviewBank {
    static constexpr std::size_t tableSize = 2048, levelCount = 10;
    std::array<std::array<float, tableSize>, levelCount> tables {};
    [[nodiscard]] static PreviewBank build(const Waveform&);
    [[nodiscard]] static constexpr unsigned harmonics(std::size_t level) noexcept { return 1023u >> level; }
    [[nodiscard]] static int levelFor(double frequency, double sampleRate) noexcept;
    [[nodiscard]] float sample(int level, double phase) const noexcept;
};
[[nodiscard]] double noteFrequency(int midiNote);
} // namespace wtd
