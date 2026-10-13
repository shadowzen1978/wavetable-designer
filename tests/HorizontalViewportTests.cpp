#include <doctest/doctest.h>
#include "core/HorizontalViewport.h"
#include <limits>

TEST_CASE("Full and zoomed sample coordinates round trip for both cycle lengths") {
    for (const std::size_t count : { 1024u, 2048u }) {
        wtd::HorizontalViewport view;
        CHECK(view.sampleAt(0.25, count) == count / 4);
        CHECK(view.sampleAt(-2.0, count) == 0);
        CHECK(view.sampleAt(2.0, count) == count - 1);
        for (const auto factor : { 1.0, 2.0, 16.0, 64.0, 128.0 }) {
            view.zoom(factor, 0.5);
            for (std::size_t i = 0; i < count; ++i) {
                const auto x = view.positionOf(static_cast<double>(i) / static_cast<double>(count));
                CHECK(view.sampleAt(x, count) == i);
            }
        }
    }
}
TEST_CASE("Zoom preserves the anchor and panning is clamped at both cycle edges") {
    wtd::HorizontalViewport view;
    view.zoom(8.0, 0.375);
    CHECK(view.phaseAt(0.5) == doctest::Approx(0.375));
    CHECK(view.span() == doctest::Approx(0.125));
    view.pan(-10.0);
    CHECK(view.start() == 0.0);
    view.pan(10.0);
    CHECK(view.phaseAt(1.0) == 1.0);
    CHECK(view.sampleAt(1.0, 2048) == 2047);
    view.zoom(1.0, 0.75);
    CHECK(view.start() == 0.0);
    CHECK(view.span() == 1.0);
    CHECK_THROWS_AS(view.zoom(0.0, 0.5), std::invalid_argument);
    CHECK_THROWS_AS(view.pan(std::numeric_limits<double>::infinity()), std::invalid_argument);
    CHECK_THROWS_AS((void)view.sampleAt(std::numeric_limits<double>::quiet_NaN(), 1024), std::invalid_argument);
}
