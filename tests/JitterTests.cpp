#include <doctest/doctest.h>
#include "core/EditorModel.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <limits>

namespace {
wtd::Waveform noisyGentle(std::size_t count, double curvature) {
    wtd::Waveform wave(count, wtd::Shape::flat);
    for (std::size_t i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(count - 1);
        const double noise = i == 0 || i + 1 == count ? 0.0
            : 0.016 * (static_cast<double>((i * 17) % 23) / 22.0 - 0.5);
        wave.setSample(i, static_cast<float>(std::lerp(-0.4, 0.1, t) + curvature * t * (1.0 - t) + noise));
    }
    return wave;
}
}
TEST_CASE("Jitter cleanup makes a mostly straight ramp straight while fixing both sides of its seam") {
    for (const std::size_t count : { 1024u, 2048u }) {
        const auto source = noisyGentle(count, 0.0);
        const auto clean = source.smoothed(1.0, { true, 30.0, true });
        CHECK_FALSE(clean == source);
        CHECK(clean.samples().front() == source.samples().front());
        CHECK(clean.samples().back() == source.samples().back());
        for (std::size_t i = 0; i < count; ++i)
            CHECK(clean.samples()[i] == doctest::Approx(std::lerp(-0.4, 0.1,
                static_cast<double>(i) / static_cast<double>(count - 1))).epsilon(0.000001));
        CHECK(source.smoothed(0.0, { true, 30.0, true }) == source);
        const auto half = source.smoothed(0.5, { true, 30.0, true });
        for (std::size_t i = 0; i < count; ++i)
            CHECK(half.samples()[i] == doctest::Approx(std::lerp(source.samples()[i], clean.samples()[i], 0.5f)).epsilon(0.000001));
    }
}
TEST_CASE("Jitter cleanup retains shallow curvature instead of flattening it") {
    for (const std::size_t count : { 1024u, 2048u }) {
        const auto source = noisyGentle(count, 0.2);
        const auto clean = source.smoothed(1.0, { true, 30.0, true });
        double before = 0.0, after = 0.0;
        for (std::size_t i = 0; i < count; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(count - 1);
            const double expected = std::lerp(-0.4, 0.1, t) + 0.2 * t * (1.0 - t);
            before += std::pow(source.samples()[i] - expected, 2);
            after += std::pow(clean.samples()[i] - expected, 2);
            CHECK(std::abs(clean.samples()[i] - expected) < 0.001);
        }
        CHECK(after < before * 0.01);
        // A clean quadratic has nearly constant second differences, with no sample-sized zigzags.
        for (std::size_t i = 2; i + 1 < count; ++i) {
            const double a = clean.samples()[i - 2] - 2.0 * clean.samples()[i - 1] + clean.samples()[i];
            const double b = clean.samples()[i - 1] - 2.0 * clean.samples()[i] + clean.samples()[i + 1];
            CHECK(std::abs(a - b) < 0.000001);
        }
    }
}
TEST_CASE("Already clean lines and quadratics remain no-ops and large complex curves use ordinary smoothing") {
    wtd::Waveform line(1024, wtd::Shape::flat), curve(1024, wtd::Shape::flat), complex(1024, wtd::Shape::flat);
    for (std::size_t i = 0; i < line.size(); ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(line.size() - 1);
        line.setSample(i, static_cast<float>(std::lerp(-0.4, 0.1, t)));
        curve.setSample(i, static_cast<float>(std::lerp(-0.4, 0.1, t) + 0.2 * t * (1.0 - t)));
        complex.setSample(i, static_cast<float>(0.5 * std::sin(4.0 * std::numbers::pi * t)));
    }
    CHECK(line.smoothedRegion(1.0, { 0, 1023 }, { true, 30.0, true }) == line);
    CHECK(curve.smoothedRegion(1.0, { 0, 1023 }, { true, 30.0, true }) == curve);
    CHECK(complex.smoothedRegion(1.0, { 0, 1023 }, { true, 180.0, true })
        == complex.smoothedRegion(1.0, { 0, 1023 }));
    CHECK(complex.smoothed(1.0, { false, 30.0, true }) == complex.smoothed(1.0));
    CHECK(complex.smoothedRegion(1.0, { 0, 1023 }, { false, 30.0, true })
        == complex.smoothedRegion(1.0, { 0, 1023 }));
}
TEST_CASE("Selected jitter cleanup is isolated and preserves sharp standard shapes") {
    const auto source = noisyGentle(1024, 0.0);
    auto differentOutside = source;
    const wtd::SampleRange range { 256, 768 };
    for (std::size_t i = 0; i < source.size(); ++i)
        if (i < range.first || i > range.last) differentOutside.setSample(i, -source.samples()[i]);
    const auto a = source.smoothedRegion(1.0, range, { true, 30.0, true });
    const auto b = differentOutside.smoothedRegion(1.0, range, { true, 30.0, true });
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (i <= range.first || i >= range.last) CHECK(a.samples()[i] == source.samples()[i]);
        if (i >= range.first && i <= range.last) CHECK(a.samples()[i] == b.samples()[i]);
    }
    for (const auto shape : { wtd::Shape::triangle, wtd::Shape::square, wtd::Shape::saw }) {
        const wtd::Waveform wave(1024, shape);
        CHECK(wave.smoothed(1.0, { true, 30.0, true }) == wave);
    }
    for (const std::size_t count : { 2u, 3u, 4u }) {
        const wtd::Waveform tiny(count, wtd::Shape::square);
        const auto clean = tiny.smoothed(1.0, { true, 30.0, true });
        CHECK(clean.size() == count);
        for (const auto v : clean.samples()) { CHECK(std::isfinite(v)); CHECK(v >= -1.0f); CHECK(v <= 1.0f); }
    }
}
TEST_CASE("Jitter fitting is independent of phase origin and leaves step samples untouched") {
    const auto source = noisyGentle(1024, 0.2);
    wtd::Waveform shifted(1024, wtd::Shape::flat);
    for (std::size_t i = 0; i < source.size(); ++i) shifted.setSample((i + 263) % source.size(), source.samples()[i]);
    const auto a = source.smoothed(1.0, { true, 30.0, true });
    const auto b = shifted.smoothed(1.0, { true, 30.0, true });
    for (std::size_t i = 0; i < source.size(); ++i)
        CHECK(a.samples()[i] == doctest::Approx(b.samples()[(i + 263) % source.size()]).epsilon(0.000001));
    CHECK(b.samples()[263] == shifted.samples()[263]);
    CHECK(b.samples()[262] == shifted.samples()[262]);
}
TEST_CASE("Jitter toggle previews never accumulate and retain exact cancel and history") {
    wtd::EditorModel editor;
    const auto input = noisyGentle(1024, 0.2);
    editor.beginStroke(0.0, input.samples()[0]);
    for (std::size_t i = 1; i < input.size(); ++i)
        editor.continueStroke(static_cast<double>(i) / static_cast<double>(input.size()), input.samples()[i]);
    editor.endStroke();
    const auto source = editor.waveform();
    editor.beginSmoothing();
    editor.previewSmoothing(1.0, wtd::SmoothingScope::wholeCycle, { true, 30.0, true });
    const auto clean = editor.waveform();
    editor.previewSmoothing(1.0, wtd::SmoothingScope::wholeCycle, { true, 30.0, false });
    editor.previewSmoothing(1.0, wtd::SmoothingScope::wholeCycle, { true, 30.0, true });
    CHECK(editor.waveform() == clean);
    CHECK(*editor.smoothingSource() == source);
    REQUIRE(editor.applySmoothing());
    REQUIRE(editor.undo());
    CHECK(editor.waveform() == source);
    REQUIRE(editor.redo());
    CHECK(editor.waveform() == clean);
    editor.beginSmoothing();
    editor.previewSmoothing(1.0);
    editor.cancelSmoothing();
    CHECK(editor.waveform() == clean);
}

TEST_CASE("Standalone jitter cleanup works independently of hidden edge angle for either scope") {
    for (const std::size_t count : { 1024u, 2048u }) {
        for (const double curvature : { 0.0, 0.2 }) {
            const auto source = noisyGentle(count, curvature);
            const auto clean = source.smoothed(1.0, { false, 30.0, true });
            CHECK_FALSE(clean == source);
            CHECK(clean == source.smoothed(1.0, { false, 1.0, true }));
            CHECK(clean == source.smoothed(1.0, { false, 180.0, true }));
            CHECK(clean == source.smoothed(1.0, { false, std::numeric_limits<double>::quiet_NaN(), true }));
            CHECK(source.smoothed(0.0, { false, 30.0, true }) == source);
            for (std::size_t i = 0; i < count; ++i) {
                const double t = static_cast<double>(i) / static_cast<double>(count - 1);
                const double expected = std::lerp(-0.4, 0.1, t) + curvature * t * (1.0 - t);
                CHECK(std::abs(clean.samples()[i] - expected) < 0.001);
            }
            const wtd::SampleRange range { count / 4, count * 3 / 4 };
            const auto selected = source.smoothedRegion(1.0, range, { false, 30.0, true });
            CHECK_FALSE(selected == source);
            CHECK(selected == source.smoothedRegion(1.0, range, { false, 1.0, true }));
            CHECK(selected == source.smoothedRegion(1.0, range, { false, 180.0, true }));
            for (std::size_t i = 0; i < count; ++i)
                if (i <= range.first || i >= range.last) CHECK(selected.samples()[i] == source.samples()[i]);
        }
        const wtd::Waveform triangle(count, wtd::Shape::triangle);
        CHECK(triangle.smoothed(1.0, { false, 30.0, true }) == triangle.smoothed(1.0));
        CHECK(triangle.smoothed(1.0, { true, 30.0, true }) == triangle);
    }
}
TEST_CASE("Independent jitter toggles and edge protection replay from the source in one transaction") {
    wtd::EditorModel editor;
    const auto input = noisyGentle(1024, 0.2);
    editor.beginStroke(0.0, input.samples()[0]);
    for (std::size_t i = 1; i < input.size(); ++i)
        editor.continueStroke(static_cast<double>(i) / static_cast<double>(input.size()), input.samples()[i]);
    editor.endStroke();
    const auto source = editor.waveform();
    editor.beginSmoothing();
    editor.previewSmoothing(1.0, wtd::SmoothingScope::wholeCycle, { false, 30.0, true });
    const auto standalone = editor.waveform();
    editor.previewSmoothing(1.0, wtd::SmoothingScope::wholeCycle, { true, 30.0, false });
    editor.previewSmoothing(1.0, wtd::SmoothingScope::wholeCycle, { true, 30.0, true });
    editor.previewSmoothing(1.0, wtd::SmoothingScope::wholeCycle, { false, 150.0, true });
    CHECK(editor.waveform() == standalone);
    CHECK(*editor.smoothingSource() == source);
    REQUIRE(editor.applySmoothing());
    REQUIRE(editor.undo());
    CHECK(editor.waveform() == source);
    REQUIRE(editor.redo());
    CHECK(editor.waveform() == standalone);
    editor.beginSmoothing();
    editor.previewSmoothing(1.0);
    editor.cancelSmoothing();
    CHECK(editor.waveform() == standalone);
}
