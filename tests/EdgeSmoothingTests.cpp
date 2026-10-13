#include <doctest/doctest.h>
#include "core/EditorModel.h"
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
wtd::Waveform mixedWave(std::size_t count) {
    wtd::Waveform wave(count, wtd::Shape::flat);
    for (std::size_t i = 0; i < count; ++i) {
        const double phase = static_cast<double>(i) / static_cast<double>(count);
        wave.setSample(i, static_cast<float>((i < count / 2 ? 0.7 : -0.7)
            + 0.08 * std::sin(2.0 * std::numbers::pi * phase) + (i % 2 == 0 ? 0.001 : -0.001)));
    }
    return wave;
}
double localRoughness(const wtd::Waveform& wave) {
    double energy = 0.0;
    for (auto i = wave.size() / 8; i < wave.size() * 3 / 8; ++i) {
        const double change = wave.samples()[i - 1] - 2.0 * wave.samples()[i] + wave.samples()[i + 1];
        energy += change * change;
    }
    return energy;
}
}

TEST_CASE("Preserved edges keep both sides of jumps while still cleaning uneven curves") {
    for (const std::size_t count : { 1024u, 2048u }) {
        const auto source = mixedWave(count);
        const auto result = source.smoothed(1.0, { true, 30.0 });
        CHECK_FALSE(result == source);
        for (const auto i : { std::size_t { 0 }, count - 1, count / 2 - 1, count / 2 })
            CHECK(result.samples()[i] == source.samples()[i]);
        CHECK(localRoughness(result) < localRoughness(source) * 0.1);
        for (const auto value : result.samples()) {
            CHECK(std::isfinite(value));
            CHECK(value >= -1.0f);
            CHECK(value <= 1.0f);
        }
        CHECK(source.smoothed(0.0, { true, 30.0 }) == source);
        CHECK(source.smoothed(1.0, { true, 180.0 }) == source.smoothed(1.0));
        CHECK(source.smoothed(1.0, { false, 1.0 }) == source.smoothed(1.0));
    }
}

TEST_CASE("Corners of standard shapes survive while an angle above their turn allows rounding") {
    for (const std::size_t count : { 1024u, 2048u }) {
        for (const auto shape : { wtd::Shape::triangle, wtd::Shape::square, wtd::Shape::saw }) {
            const wtd::Waveform source(count, shape);
            CHECK(source.smoothed(1.0, { true, 30.0 }) == source);
        }
        const wtd::Waveform triangle(count, wtd::Shape::triangle);
        CHECK(triangle.smoothed(1.0, { true, 130.0 }) == triangle.smoothed(1.0));
        const wtd::Waveform sine(count, wtd::Shape::sine);
        CHECK(sine.smoothed(1.0, { true, 30.0 }) == sine.smoothed(1.0));
    }
}

TEST_CASE("Angle thresholds use direction change in normalized geometry at both lengths") {
    for (const std::size_t count : { 1024u, 2048u }) {
        wtd::Waveform source(count, wtd::Shape::flat);
        const double slope = 2.0 * std::tan(15.0 * std::numbers::pi / 180.0);
        for (std::size_t i = 0; i < count; ++i)
            source.setSample(i, static_cast<float>(0.2 + slope * std::abs(static_cast<double>(i) / static_cast<double>(count) - 0.5)));
        const auto range = wtd::SampleRange { count / 8, count * 7 / 8 };
        const auto protect = source.smoothedRegion(1.0, range, { true, 25.0 });
        const auto round = source.smoothedRegion(1.0, range, { true, 35.0 });
        CHECK(protect.samples()[count / 2] == source.samples()[count / 2]);
        CHECK(round.samples()[count / 2] > source.samples()[count / 2]);
        CHECK(round == source.smoothedRegion(1.0, range));
    }
}

TEST_CASE("Regional edge detection and smoothing cannot read across selection boundaries") {
    const auto source = mixedWave(1024);
    auto differentOutside = source;
    const wtd::SampleRange range { 256, 768 };
    for (std::size_t i = 0; i < source.size(); ++i)
        if (i < range.first || i > range.last) differentOutside.setSample(i, -source.samples()[i]);
    const auto a = source.smoothedRegion(0.8, range, { true, 30.0 });
    const auto b = differentOutside.smoothedRegion(0.8, range, { true, 30.0 });
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (i <= range.first || i >= range.last) CHECK(a.samples()[i] == source.samples()[i]);
        if (i >= range.first && i <= range.last) CHECK(a.samples()[i] == b.samples()[i]);
    }
    CHECK(a.samples()[511] == source.samples()[511]);
    CHECK(a.samples()[512] == source.samples()[512]);
}

TEST_CASE("Edge-preserving whole-cycle smoothing respects cyclic shifts and polarity") {
    const auto source = mixedWave(1024);
    wtd::Waveform shifted(1024, wtd::Shape::flat), inverted(1024, wtd::Shape::flat);
    for (std::size_t i = 0; i < source.size(); ++i) {
        shifted.setSample((i + 263) % source.size(), source.samples()[i]);
        inverted.setSample(i, -source.samples()[i]);
    }
    const auto a = source.smoothed(0.8, { true, 30.0 });
    const auto b = shifted.smoothed(0.8, { true, 30.0 });
    const auto c = inverted.smoothed(0.8, { true, 30.0 });
    for (std::size_t i = 0; i < source.size(); ++i) {
        CHECK(a.samples()[i] == doctest::Approx(b.samples()[(i + 263) % source.size()]).epsilon(0.000001));
        CHECK(a.samples()[i] == doctest::Approx(-c.samples()[i]).epsilon(0.000001));
    }
}

TEST_CASE("Invalid angles and tiny cycles are handled without corrupting preview state") {
    const auto source = mixedWave(1024);
    for (const auto angle : { -1.0, 181.0, std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::infinity() }) {
        CHECK_THROWS_AS((void)source.smoothed(1.0, { true, angle }), std::invalid_argument);
        CHECK_THROWS_AS((void)source.smoothedRegion(1.0, { 256, 768 }, { true, angle }), std::invalid_argument);
    }
    for (const std::size_t count : { 2u, 3u, 4u }) {
        const wtd::Waveform tiny(count, wtd::Shape::square);
        const auto result = tiny.smoothed(1.0, { true, 30.0 });
        CHECK(result.size() == count);
        for (const auto value : result.samples()) CHECK(std::isfinite(value));
        CHECK(tiny.smoothedRegion(1.0, { 0, 1 }, { true, 30.0 }) == tiny);
    }
}

TEST_CASE("Edge checkbox and angle previews use one source and retain exact apply/cancel history") {
    wtd::EditorModel editor;
    const auto input = mixedWave(1024);
    editor.beginStroke(0.0, input.samples()[0]);
    for (std::size_t i = 1; i < input.size(); ++i)
        editor.continueStroke(static_cast<double>(i) / static_cast<double>(input.size()), input.samples()[i]);
    editor.endStroke();
    const auto source = editor.waveform();
    editor.beginSmoothing();
    editor.previewSmoothing(0.8, wtd::SmoothingScope::wholeCycle, { true, 30.0 });
    const auto protectedPreview = editor.waveform();
    editor.previewSmoothing(0.8, wtd::SmoothingScope::wholeCycle, { true, 180.0 });
    CHECK(editor.waveform() == source.smoothed(0.8));
    editor.previewSmoothing(0.8, wtd::SmoothingScope::wholeCycle, { true, 30.0 });
    CHECK(editor.waveform() == protectedPreview);
    CHECK(*editor.smoothingSource() == source);
    CHECK_THROWS_AS(editor.previewSmoothing(0.8, wtd::SmoothingScope::wholeCycle, { true, -1.0 }), std::invalid_argument);
    CHECK(editor.waveform() == protectedPreview);
    REQUIRE(editor.applySmoothing());
    REQUIRE(editor.undo());
    CHECK(editor.waveform() == source);
    REQUIRE(editor.redo());
    CHECK(editor.waveform() == protectedPreview);
    editor.beginSmoothing();
    editor.previewSmoothing(1.0, wtd::SmoothingScope::wholeCycle, { false, 30.0 });
    editor.cancelSmoothing();
    CHECK(editor.waveform() == protectedPreview);
}
