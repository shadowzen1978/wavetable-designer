#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "core/Waveform.h"

#include <cmath>
#include <limits>
#include <stdexcept>

TEST_CASE("Cycle uses phase i/N without a duplicated endpoint") {
    for (const auto length : { 1024u, 2048u }) {
        const wtd::Waveform sine(length);
        CHECK(sine.size() == length);
        CHECK(sine.samples()[0] == 0.0f);
        CHECK(sine.samples()[length / 4] == doctest::Approx(1.0));
        CHECK(sine.samples()[3 * length / 4] == doctest::Approx(-1.0));
        CHECK(sine.samples().back() < 0.0f);
        const wtd::Waveform saw(length, wtd::Shape::saw);
        CHECK(saw.samples().front() == -1.0f);
        CHECK(saw.samples().back() == doctest::Approx(1.0 - 2.0 / length));
    }
}

TEST_CASE("All starting shapes retain finite bounded samples") {
    for (const auto shape : { wtd::Shape::sine, wtd::Shape::triangle,
                             wtd::Shape::saw, wtd::Shape::square }) {
        const wtd::Waveform wave(1024, shape);
        for (const auto value : wave.samples()) {
            CHECK(std::isfinite(value));
            CHECK(value >= -1.0f);
            CHECK(value <= 1.0f);
        }
    }
    const wtd::Waveform triangle(1024, wtd::Shape::triangle);
    CHECK(triangle.samples()[512] == 1.0f);
    const wtd::Waveform square(1024, wtd::Shape::square);
    CHECK(square.samples()[511] == 1.0f);
    CHECK(square.samples()[512] == -1.0f);
}

TEST_CASE("Invalid edits preserve the existing sample and valid edits clamp") {
    wtd::Waveform wave;
    wave.setSample(5, 0.25f);
    CHECK_THROWS_AS(wave.setSample(5, std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
    CHECK_THROWS_AS(wave.setSample(5, std::numeric_limits<float>::infinity()), std::invalid_argument);
    CHECK(wave.samples()[5] == 0.25f);
    CHECK_THROWS_AS(wave.setSample(wave.size(), 0.0f), std::out_of_range);
    wave.setSample(5, 4.0f);
    CHECK(wave.samples()[5] == 1.0f);
    wave.setSample(5, -4.0f);
    CHECK(wave.samples()[5] == -1.0f);
    CHECK(wave.size() == 1024);
}

TEST_CASE("Cycle length is validated before allocating") {
    CHECK_THROWS_AS(wtd::Waveform(0), std::invalid_argument);
    CHECK_THROWS_AS(wtd::Waveform(1), std::invalid_argument);
    CHECK_THROWS_AS(wtd::Waveform(wtd::Waveform::maxSampleCount + 1), std::invalid_argument);
}
