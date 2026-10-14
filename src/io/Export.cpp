#include "io/Export.h"
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace wtd {
namespace {
void validate(const Waveform& source, ExportOptions options) {
    if (source.size() != 1024 && source.size() != 2048)
        throw std::invalid_argument("Export requires a 1024- or 2048-sample cycle");
    if (options.profile != ExportProfile::sz404 && options.profile != ExportProfile::generic)
        throw std::invalid_argument("Unknown export profile");
    switch (options.format) {
        case ExportFormat::pcm16Wav: case ExportFormat::float32Wav:
        case ExportFormat::pcm16Raw: case ExportFormat::float32Raw: break;
        default: throw std::invalid_argument("Unknown export format");
    }
    if (options.profile == ExportProfile::sz404 && !isPcm16(options.format))
        throw std::invalid_argument("SZ404 requires PCM16 WAV or raw");
    if (options.sampleRate != 44100 && options.sampleRate != 48000)
        throw std::invalid_argument("WAV rate must be 44100 or 48000 Hz");
}
void le16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}
void le32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    le16(bytes, static_cast<std::uint16_t>(value));
    le16(bytes, static_cast<std::uint16_t>(value >> 16));
}
void tag(std::vector<std::uint8_t>& bytes, const char* text) {
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(text[i]));
}
}
bool isRaw(ExportFormat format) noexcept {
    return format == ExportFormat::pcm16Raw || format == ExportFormat::float32Raw;
}
bool isPcm16(ExportFormat format) noexcept {
    return format == ExportFormat::pcm16Wav || format == ExportFormat::pcm16Raw;
}
Waveform exportWaveform(const Waveform& source, ExportOptions options) {
    validate(source, options);
    return options.profile == ExportProfile::sz404 && source.size() != 1024
        ? source.resampled(1024) : source;
}
std::vector<std::uint8_t> encodeExport(const Waveform& source, ExportOptions options) {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    const auto wave = exportWaveform(source, options);
    const bool pcm = isPcm16(options.format);
    const auto frames = static_cast<std::uint32_t>(wave.size());
    const std::uint16_t stride = pcm ? 2 : 4;
    const auto dataSize = frames * stride;
    std::vector<std::uint8_t> bytes;
    bytes.reserve(dataSize + 56);
    if (!isRaw(options.format)) {
        // Float WAVE uses WAVE_FORMAT_IEEE_FLOAT plus a fact frame-count chunk.
        tag(bytes, "RIFF"); le32(bytes, dataSize + (pcm ? 36u : 48u)); tag(bytes, "WAVE");
        tag(bytes, "fmt "); le32(bytes, 16); le16(bytes, pcm ? 1 : 3); le16(bytes, 1);
        le32(bytes, static_cast<std::uint32_t>(options.sampleRate));
        le32(bytes, static_cast<std::uint32_t>(options.sampleRate) * stride);
        le16(bytes, stride); le16(bytes, pcm ? 16 : 32);
        if (!pcm) { tag(bytes, "fact"); le32(bytes, 4); le32(bytes, frames); }
        tag(bytes, "data"); le32(bytes, dataSize);
    }
    for (const float value : wave.samples()) {
        if (pcm) {
            // Symmetric full scale follows SZ404: -1 -> -32767, +1 -> 32767.
            const auto quantized = static_cast<std::int16_t>(std::lround(static_cast<double>(value) * 32767.0));
            le16(bytes, static_cast<std::uint16_t>(quantized));
        } else le32(bytes, std::bit_cast<std::uint32_t>(value));
    }
    return bytes;
}
} // namespace wtd
