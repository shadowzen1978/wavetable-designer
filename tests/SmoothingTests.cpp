#include <doctest/doctest.h>
#include "core/EditorModel.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
double mean(const wtd::Waveform& waveform) {
    double sum = 0.0;
    for (const auto sample : waveform.samples()) sum += sample;
    return sum / static_cast<double>(waveform.size());
}
double roughness(const wtd::Waveform& waveform) {
    double sum = 0.0;
    for (std::size_t i = 0; i < waveform.size(); ++i) {
        const double difference = waveform.samples()[i]
            - waveform.samples()[(i + waveform.size() - 1) % waveform.size()];
        sum += difference * difference;
    }
    return sum;
}
}

TEST_CASE("Zero smoothing is an exact identity and invalid strengths do not mutate") {
    const wtd::Waveform source(1024, wtd::Shape::saw);
    CHECK(source.smoothed(0.0) == source);
    CHECK_THROWS_AS((void)source.smoothed(-0.1), std::invalid_argument);
    CHECK_THROWS_AS((void)source.smoothed(1.1), std::invalid_argument);
    CHECK_THROWS_AS((void)source.smoothed(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    CHECK_THROWS_AS((void)source.smoothed(std::numeric_limits<double>::infinity()), std::invalid_argument);
}

TEST_CASE("Circular smoothing treats both sides of the seam as neighbors") {
    wtd::Waveform impulse(1024, wtd::Shape::flat);
    impulse.setSample(0, 1.0f);
    const auto smooth = impulse.smoothed(1.0);
    CHECK(smooth.size() == 1024);
    CHECK(smooth.samples()[1] > 0.0f);
    CHECK(smooth.samples()[1] == smooth.samples()[1023]);
    CHECK(smooth.samples()[2] == smooth.samples()[1022]);
    CHECK(smooth.samples()[0] > smooth.samples()[1]);
    CHECK(smooth.samples()[0] < 0.1f); // No silent peak normalization.
    CHECK(mean(smooth) == doctest::Approx(mean(impulse)).epsilon(0.00001));
}

TEST_CASE("Smoothing preserves constants, DC and amplitude bounds") {
    for (const auto length : { 2u, 1024u, 2048u }) {
        wtd::Waveform constant(length, wtd::Shape::flat);
        for (std::size_t i = 0; i < length; ++i) constant.setSample(i, 0.375f);
        CHECK(constant.smoothed(1.0) == constant);
    }
    wtd::Waveform source(1024, wtd::Shape::flat);
    for (std::size_t i = 0; i < source.size(); ++i) source.setSample(i, i < 384 ? 0.7f : -0.2f);
    for (const auto strength : { 0.01, 0.3, 0.75, 1.0 }) {
        const auto smooth = source.smoothed(strength);
        CHECK(mean(smooth) == doctest::Approx(mean(source)).epsilon(0.00001));
        for (const auto sample : smooth.samples()) {
            CHECK(std::isfinite(sample));
            CHECK(sample >= -0.2f);
            CHECK(sample <= 0.7f);
        }
    }
}

TEST_CASE("Increasing strength reduces rapid variation without changing cycle length") {
    wtd::Waveform source(1024, wtd::Shape::flat);
    for (std::size_t i = 0; i < source.size(); ++i) source.setSample(i, i % 2 == 0 ? 1.0f : -1.0f);
    const auto light = source.smoothed(0.2);
    const auto medium = source.smoothed(0.7);
    const auto strong = source.smoothed(1.0);
    CHECK(roughness(light) < roughness(source));
    CHECK(roughness(medium) < roughness(light));
    CHECK(roughness(strong) < roughness(medium));
    CHECK(roughness(strong) < 0.01);
    CHECK(strong.size() == source.size());
}

TEST_CASE("Smoothing commutes with cyclic phase shifts") {
    wtd::Waveform source(1024, wtd::Shape::flat), shifted(1024, wtd::Shape::flat);
    source.setSample(0, 0.8f);
    source.setSample(61, -0.5f);
    for (std::size_t i = 0; i < source.size(); ++i) shifted.setSample((i + 256) % source.size(), source.samples()[i]);
    const auto smooth = source.smoothed(0.75);
    const auto shiftedSmooth = shifted.smoothed(0.75);
    for (std::size_t i = 0; i < smooth.size(); ++i)
        CHECK(shiftedSmooth.samples()[(i + 256) % smooth.size()] == doctest::Approx(smooth.samples()[i]).epsilon(0.00001));
}

TEST_CASE("1024 and 2048 smoothing use the same phase width") {
    const auto shortWave = wtd::Waveform(1024, wtd::Shape::sine).smoothed(1.0);
    const auto longWave = wtd::Waveform(2048, wtd::Shape::sine).smoothed(1.0);
    for (std::size_t i = 0; i < shortWave.size(); ++i)
        CHECK(shortWave.samples()[i] == doctest::Approx(longWave.samples()[i * 2]).epsilon(0.00001));
}

TEST_CASE("Slider previews always recompute from the same source and apply one undo") {
    wtd::EditorModel editor;
    editor.applyPreset(wtd::Shape::saw);
    const auto original = editor.waveform();
    editor.beginSmoothing();
    editor.previewSmoothing(0.4);
    const auto first = editor.waveform();
    editor.previewSmoothing(0.9);
    editor.beginSmoothing(); // Re-entry must not replace the baseline with the preview.
    editor.previewSmoothing(0.4);
    CHECK(editor.waveform() == first);
    CHECK(*editor.smoothingSource() == original);
    REQUIRE(editor.applySmoothing());
    CHECK_FALSE(editor.smoothingActive());
    REQUIRE(editor.undo());
    CHECK(editor.waveform() == original);
    REQUIRE(editor.undo());
    CHECK(editor.preset() == wtd::Shape::sine);
    REQUIRE(editor.redo());
    REQUIRE(editor.redo());
    CHECK(editor.waveform() == first);
}

TEST_CASE("Cancel and no-op apply preserve redo and restore preset identity") {
    wtd::EditorModel editor;
    editor.applyPreset(wtd::Shape::square);
    REQUIRE(editor.undo());
    const auto original = editor.waveform();
    editor.beginSmoothing();
    editor.previewSmoothing(0.8);
    CHECK_FALSE(editor.canRedo());
    REQUIRE(editor.cancelSmoothing());
    CHECK(editor.waveform() == original);
    CHECK(editor.preset() == wtd::Shape::sine);
    CHECK(editor.canRedo());
    editor.beginSmoothing();
    editor.previewSmoothing(0.8);
    editor.previewSmoothing(0.0);
    CHECK_FALSE(editor.applySmoothing());
    CHECK(editor.waveform() == original);
    CHECK_FALSE(editor.canUndo());
    CHECK(editor.canRedo());
}

TEST_CASE("Pending smoothing cannot be mixed with drawing or length/preset changes") {
    wtd::EditorModel editor;
    editor.beginSmoothing();
    editor.previewSmoothing(0.5);
    const auto preview = editor.waveform();
    CHECK_THROWS_AS(editor.beginStroke(0.2, 0.5f), std::logic_error);
    CHECK_THROWS_AS(editor.resizeCycle(2048), std::logic_error);
    CHECK_THROWS_AS(editor.applyPreset(wtd::Shape::square), std::logic_error);
    CHECK_THROWS_AS(editor.previewSmoothing(-1.0), std::invalid_argument);
    CHECK(editor.waveform() == preview);
    CHECK(editor.smoothingActive());
    REQUIRE(editor.undo()); // Undo cancels the preview, leaving older history alone.
    CHECK(editor.preset() == wtd::Shape::sine);
    CHECK_FALSE(editor.smoothingActive());
    CHECK_FALSE(editor.canUndo());
    CHECK_THROWS_AS(editor.previewSmoothing(0.5), std::logic_error);
}
