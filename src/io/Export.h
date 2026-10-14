#pragma once
#include "core/Waveform.h"
#include <cstdint>
#include <vector>

namespace wtd {
enum class ExportProfile { sz404, generic };
enum class ExportFormat { pcm16Wav, float32Wav, pcm16Raw, float32Raw };
struct ExportOptions {
    ExportProfile profile = ExportProfile::sz404;
    ExportFormat format = ExportFormat::pcm16Wav;
    int sampleRate = 44100; // WAV metadata only; never changes phase or sample count.
};
[[nodiscard]] bool isRaw(ExportFormat format) noexcept;
[[nodiscard]] bool isPcm16(ExportFormat format) noexcept;
[[nodiscard]] Waveform exportWaveform(const Waveform& source, ExportOptions options);
[[nodiscard]] std::vector<std::uint8_t> encodeExport(const Waveform& source, ExportOptions options);
} // namespace wtd
