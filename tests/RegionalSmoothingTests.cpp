#include <doctest/doctest.h>
#include "core/EditorModel.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
wtd::Waveform unevenSection() {
    wtd::Waveform wave(1024, wtd::Shape::sine);
    for (std::size_t i = 256; i <= 512; ++i) {
        const double trend = -0.75 - 0.15 * static_cast<double>(i - 256) / 256.0;
        const double noise = i == 256 || i == 512 ? 0.0 : (i % 2 == 0 ? 0.015 : -0.015);
        wave.setSample(i, static_cast<float>(trend + noise));
    }
    return wave;
}
double residualEnergy(const wtd::Waveform& wave) {
    const double first = wave.samples()[256], last = wave.samples()[512];
    double sum = 0.0;
    for (std::size_t i = 256; i <= 512; ++i) {
        const double trend = std::lerp(first, last, static_cast<double>(i - 256) / 256.0);
        const double deviation = wave.samples()[i] - trend;
        sum += deviation * deviation;
    }
    return sum;
}
}

TEST_CASE("Regional smoothing removes unevenness while preserving outside samples and sharp edges") {
    const auto source = unevenSection();
    const auto result = source.smoothedRegion(1.0, { 256, 512 });
    REQUIRE(result.size() == source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (i <= 256 || i >= 512) CHECK(result.samples()[i] == source.samples()[i]);
        CHECK(std::isfinite(result.samples()[i]));
        CHECK(result.samples()[i] >= -1.0f);
        CHECK(result.samples()[i] <= 1.0f);
    }
    CHECK(residualEnergy(result) < residualEnergy(source) * 0.01);
    CHECK(result.samples()[256] - result.samples()[255] == source.samples()[256] - source.samples()[255]);
    CHECK(result.samples()[513] - result.samples()[512] == source.samples()[513] - source.samples()[512]);
}

TEST_CASE("Regional smoothing never reads across either selection boundary") {
    const auto a = unevenSection();
    auto b = a;
    for (std::size_t i = 0; i < b.size(); ++i) if (i < 256 || i > 512) b.setSample(i, -a.samples()[i]);
    const auto smoothA = a.smoothedRegion(0.75, { 256, 512 });
    const auto smoothB = b.smoothedRegion(0.75, { 256, 512 });
    for (std::size_t i = 256; i <= 512; ++i) CHECK(smoothA.samples()[i] == smoothB.samples()[i]);
}

TEST_CASE("Regional smoothing keeps straight slopes exact and tiny regions are no-ops") {
    wtd::Waveform line(1024, wtd::Shape::flat);
    line.drawSegment(256, -0.75f, 512, -0.9f);
    for (const auto strength : { 0.0, 0.25, 1.0 }) CHECK(line.smoothedRegion(strength, { 256, 512 }) == line);
    const auto noisy = unevenSection();
    CHECK(noisy.smoothedRegion(0.0, { 256, 512 }) == noisy);
    CHECK(noisy.smoothedRegion(1.0, { 256, 256 }) == noisy);
    CHECK(noisy.smoothedRegion(1.0, { 256, 257 }) == noisy);
    CHECK_THROWS_AS((void)noisy.smoothedRegion(1.0, { 512, 256 }), std::out_of_range);
    CHECK_THROWS_AS((void)noisy.smoothedRegion(1.0, { 256, 1024 }), std::out_of_range);
    CHECK_THROWS_AS((void)noisy.smoothedRegion(-1.0, { 256, 512 }), std::invalid_argument);
}

TEST_CASE("Selection drags are reversible, clamped and do not edit samples or add undo") {
    wtd::EditorModel editor;
    const auto original = editor.waveform();
    editor.selectRegion(0.5, 0.25);
    CHECK(editor.selection() == (wtd::SampleRange { 256, 512 }));
    CHECK(editor.waveform() == original);
    CHECK_FALSE(editor.canUndo());
    editor.selectRegion(-2.0, 3.0);
    CHECK(editor.selection() == (wtd::SampleRange { 0, 1023 }));
    editor.resizeCycle(2048);
    CHECK(editor.selection() == (wtd::SampleRange { 0, 2047 }));
    REQUIRE(editor.undo());
    CHECK(editor.selection() == (wtd::SampleRange { 0, 1023 }));
    editor.setSelection(std::nullopt);
    CHECK_FALSE(editor.selection());
    CHECK(editor.waveform() == original);
    CHECK_THROWS_AS(editor.selectRegion(std::numeric_limits<double>::quiet_NaN(), 0.5), std::invalid_argument);
    CHECK_THROWS_AS(editor.setSelection(wtd::SampleRange { 512, 256 }), std::out_of_range);
    CHECK_THROWS_AS(editor.setSelection(wtd::SampleRange { 0, 1024 }), std::out_of_range);
    CHECK_FALSE(editor.selection());
}

TEST_CASE("Scope and slider changes share one fixed source and applying preserves selection in history") {
    wtd::EditorModel editor;
    editor.beginStroke(0.25, -0.75f);
    editor.continueStroke(0.375, -0.95f);
    editor.continueStroke(0.5, -0.8f);
    editor.endStroke();
    editor.selectRegion(0.25, 0.5);
    const auto original = editor.waveform();
    const auto selection = editor.selection();
    editor.beginSmoothing();
    editor.previewSmoothing(0.75, wtd::SmoothingScope::selection);
    const auto local = editor.waveform();
    editor.previewSmoothing(0.75, wtd::SmoothingScope::wholeCycle);
    CHECK(editor.waveform() == original.smoothed(0.75));
    editor.previewSmoothing(0.25, wtd::SmoothingScope::selection);
    editor.previewSmoothing(0.75, wtd::SmoothingScope::selection);
    CHECK(editor.waveform() == local);
    editor.setSelection(wtd::SampleRange { 300, 450 });
    editor.previewSmoothing(0.75, wtd::SmoothingScope::selection);
    CHECK(editor.waveform() == original.smoothedRegion(0.75, { 300, 450 }));
    CHECK(*editor.smoothingSource() == original);
    editor.setSelection(selection);
    editor.previewSmoothing(0.75, wtd::SmoothingScope::selection);
    CHECK(editor.waveform() == local);
    REQUIRE(editor.applySmoothing());
    REQUIRE(editor.undo());
    CHECK(editor.waveform() == original);
    CHECK(editor.selection() == selection);
    REQUIRE(editor.redo());
    CHECK(editor.waveform() == local);
    CHECK(editor.selection() == selection);
    editor.beginSmoothing();
    editor.previewSmoothing(1.0, wtd::SmoothingScope::selection);
    editor.cancelSmoothing();
    CHECK(editor.waveform() == local);
    CHECK(editor.selection() == selection);
}

TEST_CASE("Missing selection is rejected instead of silently smoothing the whole cycle") {
    wtd::EditorModel editor;
    const auto original = editor.waveform();
    editor.beginSmoothing();
    CHECK_THROWS_AS(editor.previewSmoothing(0.5, wtd::SmoothingScope::selection), std::logic_error);
    CHECK(editor.waveform() == original);
    editor.cancelSmoothing();
}

TEST_CASE("Selection created or adjusted during smoothing uses the source and rolls back with the transaction") {
    wtd::EditorModel editor;
    editor.applyPreset(wtd::Shape::square);
    const auto original = editor.waveform();
    editor.beginSmoothing();
    editor.previewSmoothing(0.8);
    editor.selectRegion(0.4, 0.7);
    editor.previewSmoothing(0.8, wtd::SmoothingScope::selection);
    const auto range = *editor.selection();
    CHECK(editor.waveform() == original.smoothedRegion(0.8, range));
    editor.setSelection(wtd::SampleRange { range.first + 1, range.last - 1 });
    editor.previewSmoothing(1.0, wtd::SmoothingScope::selection);
    CHECK(editor.waveform() == original.smoothedRegion(1.0, *editor.selection()));
    editor.cancelSmoothing();
    CHECK(editor.waveform() == original);
    CHECK_FALSE(editor.selection());
    editor.beginSmoothing();
    editor.selectRegion(0.4, 0.7);
    editor.previewSmoothing(0.8, wtd::SmoothingScope::selection);
    const auto result = editor.waveform();
    REQUIRE(editor.applySmoothing());
    REQUIRE(editor.undo());
    CHECK(editor.waveform() == original);
    CHECK_FALSE(editor.selection());
    REQUIRE(editor.redo());
    CHECK(editor.waveform() == result);
    CHECK(editor.selection() == range);
}
