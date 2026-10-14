#include <doctest/doctest.h>
#include "io/Export.h"
#include "core/EditorModel.h"
#include <bit>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {
std::uint32_t u32(const std::vector<std::uint8_t>& b, std::size_t i) {
    return std::uint32_t(b[i]) | (std::uint32_t(b[i+1]) << 8)
        | (std::uint32_t(b[i+2]) << 16) | (std::uint32_t(b[i+3]) << 24);
}
std::int16_t i16(const std::vector<std::uint8_t>& b, std::size_t i) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(b[i] | (b[i+1] << 8)));
}
}
TEST_CASE("PCM16 raw golden values use symmetric quantization and little endian") {
    wtd::Waveform wave(1024, wtd::Shape::flat);
    const float values[] { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 1.0f / 32767, -1.0f / 32767 };
    for (std::size_t i = 0; i < 7; ++i) wave.setSample(i, values[i]);
    const auto bytes = wtd::encodeExport(wave, { wtd::ExportProfile::sz404, wtd::ExportFormat::pcm16Raw });
    REQUIRE(bytes.size() == 2048);
    const std::uint8_t golden[] { 1,128, 0,192, 0,0, 0,64, 255,127, 1,0, 255,255 };
    for (std::size_t i = 0; i < 14; ++i) CHECK(bytes[i] == golden[i]);
    CHECK(i16(bytes, 0) == -32767);
}
TEST_CASE("WAV exports have one complete mono cycle with header-only rate changes") {
    for (const auto count : { 1024u, 2048u }) for (const auto rate : { 44100, 48000 })
        for (const auto format : { wtd::ExportFormat::pcm16Wav, wtd::ExportFormat::float32Wav }) {
            wtd::Waveform wave(count, wtd::Shape::saw);
            wave.setSample(count - 1, 0.1234567f);
            const auto original = wave;
            const bool pcm = format == wtd::ExportFormat::pcm16Wav;
            const auto bytes = wtd::encodeExport(wave, { wtd::ExportProfile::generic, format, rate });
            const std::size_t offset = pcm ? 44 : 56;
            REQUIRE(bytes.size() == offset + count * (pcm ? 2 : 4));
            CHECK(std::string(bytes.begin(), bytes.begin() + 4) == "RIFF");
            CHECK(u32(bytes, 4) + 8 == bytes.size());
            CHECK(std::string(bytes.begin() + 8, bytes.begin() + 12) == "WAVE");
            CHECK(bytes[20] == (pcm ? 1 : 3)); CHECK(bytes[22] == 1);
            CHECK(u32(bytes, 24) == rate);
            CHECK(u32(bytes, 28) == rate * (pcm ? 2 : 4));
            CHECK(bytes[32] == (pcm ? 2 : 4)); CHECK(bytes[34] == (pcm ? 16 : 32));
            if (!pcm) {
                CHECK(std::string(bytes.begin() + 36, bytes.begin() + 40) == "fact");
                CHECK(u32(bytes, 44) == count);
            }
            CHECK(std::string(bytes.begin() + static_cast<std::ptrdiff_t>(offset - 8),
                bytes.begin() + static_cast<std::ptrdiff_t>(offset - 4)) == "data");
            CHECK(u32(bytes, offset - 4) == count * (pcm ? 2 : 4));
            for (std::size_t i = 0; i < count; ++i) {
                if (pcm) CHECK(i16(bytes, offset + i * 2) == std::lround(static_cast<double>(wave.samples()[i]) * 32767));
                else CHECK(u32(bytes, offset + i * 4) == std::bit_cast<std::uint32_t>(wave.samples()[i]));
            }
            CHECK(wave == original);
        }
}
TEST_CASE("Float raw is exact IEEE754 little endian including quiet samples and DC") {
    wtd::Waveform wave(2048, wtd::Shape::flat);
    wave.setSample(0, 1); wave.setSample(1, -1); wave.setSample(2, 0.000001f); wave.setSample(2047, 0.2f);
    const auto bytes = wtd::encodeExport(wave, { wtd::ExportProfile::generic, wtd::ExportFormat::float32Raw });
    REQUIRE(bytes.size() == 8192);
    CHECK(bytes[0] == 0); CHECK(bytes[1] == 0); CHECK(bytes[2] == 128); CHECK(bytes[3] == 63);
    CHECK(bytes[7] == 191);
    for (std::size_t i = 0; i < wave.size(); ++i)
        CHECK(u32(bytes, i * 4) == std::bit_cast<std::uint32_t>(wave.samples()[i]));
}
TEST_CASE("SZ404 resamples a complete cycle copy instead of truncating or modifying it") {
    wtd::Waveform wave(2048, wtd::Shape::flat);
    for (std::size_t i = 0; i < wave.size(); ++i)
        wave.setSample(i, static_cast<float>(i) / 2048.0f - 0.5f);
    wave.setSample(2000, 0.9f); // A second-half feature must survive.
    const auto original = wave;
    const auto bytes = wtd::encodeExport(wave, { wtd::ExportProfile::sz404, wtd::ExportFormat::pcm16Raw });
    REQUIRE(bytes.size() == 2048);
    for (std::size_t i = 0; i < 1024; ++i)
        CHECK(i16(bytes, i * 2) == std::lround(static_cast<double>(wave.samples()[i * 2]) * 32767));
    CHECK(i16(bytes, 2000) > 29000);
    CHECK(wave == original);
    CHECK(wtd::exportWaveform(wave, {}).size() == 1024);
}
TEST_CASE("Unsupported export combinations are rejected") {
    CHECK_THROWS_AS((void) wtd::encodeExport(wtd::Waveform(8), {}), std::invalid_argument);
    CHECK_THROWS_AS((void) wtd::encodeExport(wtd::Waveform(), { wtd::ExportProfile::sz404, wtd::ExportFormat::float32Wav }), std::invalid_argument);
    CHECK_THROWS_AS((void) wtd::encodeExport(wtd::Waveform(), { wtd::ExportProfile::sz404, wtd::ExportFormat::float32Raw }), std::invalid_argument);
    CHECK_THROWS_AS((void) wtd::encodeExport(wtd::Waveform(), { wtd::ExportProfile::generic, wtd::ExportFormat::pcm16Wav, 96000 }), std::invalid_argument);
    CHECK_THROWS_AS((void) wtd::encodeExport(wtd::Waveform(), { static_cast<wtd::ExportProfile>(55) }), std::invalid_argument);
    CHECK_THROWS_AS((void) wtd::encodeExport(wtd::Waveform(), { wtd::ExportProfile::generic, static_cast<wtd::ExportFormat>(55) }), std::invalid_argument);
}
TEST_CASE("Document replacement clears all pending edits and history and validates first") {
    wtd::EditorModel editor;
    editor.applyPreset(wtd::Shape::square);
    editor.beginSmoothing(); editor.previewSmoothing(1);
    const auto before = editor.waveform();
    CHECK_THROWS_AS(editor.loadDocument(wtd::Waveform(), std::nullopt, wtd::SampleRange { 99, 9 }), std::out_of_range);
    CHECK(editor.waveform() == before); CHECK(editor.smoothingActive());
    wtd::Waveform loaded(2048, wtd::Shape::saw);
    loaded.setSample(123, 0.37f);
    editor.loadDocument(loaded, std::nullopt, wtd::SampleRange { 30, 400 });
    CHECK(editor.waveform() == loaded); CHECK(editor.selection() == wtd::SampleRange { 30, 400 });
    CHECK_FALSE(editor.preset()); CHECK_FALSE(editor.smoothingActive()); CHECK_FALSE(editor.strokeActive());
    CHECK_FALSE(editor.canUndo()); CHECK_FALSE(editor.canRedo());
    editor.beginStroke(0.2, -0.4f);
    editor.loadDocument(wtd::Waveform(), wtd::Shape::sine, std::nullopt);
    CHECK_FALSE(editor.strokeActive()); CHECK_FALSE(editor.canUndo());
}
