#include <doctest/doctest.h>
#include "core/EditorModel.h"
#include <cmath>
#include <limits>
#include <stdexcept>

TEST_CASE("Sparse strokes fill skipped samples in either direction") {
    wtd::EditorModel forward(8), backward(8);
    const auto original = forward.waveform();
    forward.beginStroke(2.0 / 8, -0.75f);
    forward.continueStroke(6.0 / 8, 0.75f);
    REQUIRE(forward.endStroke());
    backward.beginStroke(6.0 / 8, 0.75f);
    backward.continueStroke(2.0 / 8, -0.75f);
    REQUIRE(backward.endStroke());
    CHECK(forward.waveform() == backward.waveform());
    CHECK(forward.waveform().samples()[2] == -0.75f);
    CHECK(forward.waveform().samples()[3] == doctest::Approx(-0.375f));
    CHECK(forward.waveform().samples()[4] == 0.0f);
    CHECK(forward.waveform().samples()[5] == doctest::Approx(0.375f));
    CHECK(forward.waveform().samples()[6] == 0.75f);
    for (const auto index : { 0u, 1u, 7u })
        CHECK(forward.waveform().samples()[index] == original.samples()[index]);
}

TEST_CASE("Repeated horizontal position overwrites one sample") {
    wtd::EditorModel editor(8);
    const auto original = editor.waveform();
    editor.beginStroke(0.25, -0.75f);
    editor.continueStroke(0.25, 0.75f);
    editor.endStroke();
    CHECK(editor.waveform().size() == 8);
    for (std::size_t i = 0; i < 8; ++i)
        CHECK(editor.waveform().samples()[i] == (i == 2 ? 0.75f : original.samples()[i]));
    REQUIRE(editor.undo());
    CHECK(editor.waveform() == original);
    CHECK_FALSE(editor.canUndo());
}

TEST_CASE("A multi-event stroke is exactly one undo action") {
    wtd::EditorModel editor;
    const auto original = editor.waveform();
    editor.beginStroke(0.1, 0.2f);
    editor.continueStroke(0.5, -0.7f);
    editor.continueStroke(0.9, 0.3f);
    REQUIRE(editor.endStroke());
    const auto drawn = editor.waveform();
    CHECK_FALSE(editor.preset().has_value());
    REQUIRE(editor.undo());
    CHECK(editor.waveform() == original);
    CHECK(editor.preset() == wtd::Shape::sine);
    CHECK_FALSE(editor.canUndo());
    REQUIRE(editor.redo());
    CHECK(editor.waveform() == drawn);
    CHECK_FALSE(editor.canRedo());
}

TEST_CASE("Outside drags clamp to endpoints without forcing a periodic seam") {
    wtd::EditorModel editor(8);
    editor.beginStroke(0.5, 0.0f);
    editor.continueStroke(3.0, 10.0f);
    editor.continueStroke(-2.0, -10.0f);
    editor.endStroke();
    CHECK(editor.waveform().samples().front() == -1.0f);
    CHECK(editor.waveform().samples().back() == 1.0f);
    CHECK(editor.waveform().size() == 8);
    for (const auto value : editor.waveform().samples()) {
        CHECK(std::isfinite(value));
        CHECK(value >= -1.0f);
        CHECK(value <= 1.0f);
    }
}

TEST_CASE("No-op strokes keep preset identity and create no history") {
    wtd::EditorModel editor;
    editor.beginStroke(0.0, 0.0f);
    CHECK_FALSE(editor.endStroke());
    CHECK(editor.preset() == wtd::Shape::sine);
    CHECK_FALSE(editor.canUndo());
    CHECK_FALSE(editor.canRedo());
}

TEST_CASE("Cancelling preserves redo; committed edits discard the redo branch") {
    wtd::EditorModel editor;
    editor.beginStroke(0.0, 0.5f);
    editor.endStroke();
    const auto drawn = editor.waveform();
    REQUIRE(editor.undo());
    const auto original = editor.waveform();
    editor.beginStroke(0.2, -0.5f);
    REQUIRE(editor.cancelStroke());
    CHECK(editor.waveform() == original);
    REQUIRE(editor.redo());
    CHECK(editor.waveform() == drawn);
    REQUIRE(editor.undo());
    editor.beginStroke(0.0, -0.5f);
    editor.endStroke();
    CHECK_FALSE(editor.canRedo());
}

TEST_CASE("Undo during a stroke cancels that gesture without consuming older history") {
    wtd::EditorModel editor;
    editor.applyPreset(wtd::Shape::square);
    const auto square = editor.waveform();
    editor.beginStroke(0.2, -0.5f);
    REQUIRE(editor.undo());
    CHECK(editor.waveform() == square);
    CHECK_FALSE(editor.strokeActive());
    REQUIRE(editor.undo());
    CHECK(editor.preset() == wtd::Shape::sine);
}

TEST_CASE("Clearing and replacing a preset can be undone exactly") {
    wtd::EditorModel editor;
    editor.beginStroke(0.25, -0.5f);
    editor.endStroke();
    const auto drawn = editor.waveform();
    REQUIRE(editor.applyPreset(wtd::Shape::flat));
    for (const auto value : editor.waveform().samples()) CHECK(value == 0.0f);
    REQUIRE(editor.undo());
    CHECK(editor.waveform() == drawn);
    REQUIRE(editor.applyPreset(wtd::Shape::triangle));
    REQUIRE(editor.undo());
    CHECK(editor.waveform() == drawn);
}

TEST_CASE("Length changes retain the edited curve and have exact undo") {
    wtd::EditorModel editor;
    editor.beginStroke(0.25, -0.5f);
    editor.continueStroke(0.5, 0.75f);
    editor.endStroke();
    const auto drawn = editor.waveform();
    REQUIRE(editor.resizeCycle(2048));
    CHECK(editor.waveform().size() == 2048);
    for (std::size_t i = 0; i < 1024; ++i)
        CHECK(editor.waveform().samples()[i * 2] == drawn.samples()[i]);
    REQUIRE(editor.undo());
    CHECK(editor.waveform() == drawn);
    REQUIRE(editor.redo());
    REQUIRE(editor.resizeCycle(1024));
    CHECK(editor.waveform() == drawn);
}

TEST_CASE("Periodic interpolation crosses the last/first sample without duplicating phase 1") {
    wtd::Waveform wave(4, wtd::Shape::flat);
    wave.setSample(0, -1.0f);
    wave.setSample(1, -0.5f);
    wave.setSample(2, 0.0f);
    wave.setSample(3, 0.5f);
    const auto doubled = wave.resampled(8);
    CHECK(doubled.size() == 8);
    CHECK(doubled.samples()[6] == 0.5f);
    CHECK(doubled.samples()[7] == -0.25f);
    CHECK(doubled.resampled(4) == wave);
}

TEST_CASE("Invalid coordinates cannot partially modify the cycle") {
    wtd::EditorModel editor;
    const auto original = editor.waveform();
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_AS(editor.beginStroke(nan, 0.0f), std::invalid_argument);
    CHECK_THROWS_AS(editor.beginStroke(0.0, nan), std::invalid_argument);
    CHECK_FALSE(editor.strokeActive());
    CHECK(editor.waveform() == original);
    editor.beginStroke(0.0, 0.5f);
    const auto before = editor.waveform();
    CHECK_THROWS_AS(editor.continueStroke(0.5, nan), std::invalid_argument);
    CHECK(editor.waveform() == before);
    editor.cancelStroke();
    CHECK_THROWS_AS(editor.resizeCycle(0), std::invalid_argument);
    CHECK(editor.waveform() == original);
    CHECK_FALSE(editor.canUndo());
    auto wave = original;
    CHECK_THROWS_AS(wave.drawSegment(0, 0.0f, wave.size(), 1.0f), std::out_of_range);
    CHECK_THROWS_AS(wave.drawSegment(0, 0.0f, 8, nan), std::invalid_argument);
    CHECK(wave == original);
}

TEST_CASE("Drawing history is bounded") {
    wtd::EditorModel editor(8);
    for (std::size_t i = 0; i < wtd::EditorModel::historyLimit + 10; ++i) {
        editor.beginStroke(0.0, i % 2 == 0 ? 0.5f : -0.5f);
        editor.endStroke();
    }
    std::size_t count = 0;
    while (editor.undo()) ++count;
    CHECK(count == wtd::EditorModel::historyLimit);
}
