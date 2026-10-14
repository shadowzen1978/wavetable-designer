#include <doctest/doctest.h>
#include "core/PreviewEngine.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <new>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <malloc.h>
#endif

namespace {
thread_local bool countAudioMemory = false;
thread_local std::size_t audioAllocations = 0, audioDeallocations = 0;
}
void* operator new(std::size_t size) {
    if (countAudioMemory) ++audioAllocations;
    if (auto* memory = std::malloc(std::max(size, std::size_t { 1 }))) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { if (countAudioMemory && memory) ++audioDeallocations; std::free(memory); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete(void* memory, std::size_t) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }
void* operator new(std::size_t size, std::align_val_t alignment) {
    if (countAudioMemory) ++audioAllocations;
    const auto align = static_cast<std::size_t>(alignment);
#ifdef _WIN32
    void* memory = _aligned_malloc(std::max(size, std::size_t { 1 }), align);
#else
    const auto rounded = ((std::max(size, std::size_t { 1 }) + align - 1) / align) * align;
    void* memory = std::aligned_alloc(align, rounded);
#endif
    if (memory) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size, std::align_val_t alignment) { return ::operator new(size, alignment); }
void operator delete(void* memory, std::align_val_t) noexcept {
    if (countAudioMemory && memory) ++audioDeallocations;
#ifdef _WIN32
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}
void operator delete[](void* memory, std::align_val_t alignment) noexcept { ::operator delete(memory, alignment); }
void operator delete(void* memory, std::size_t, std::align_val_t alignment) noexcept { ::operator delete(memory, alignment); }
void operator delete[](void* memory, std::size_t, std::align_val_t alignment) noexcept { ::operator delete(memory, alignment); }

namespace {
void render(wtd::PreviewEngine& engine, std::span<float> samples) {
    float* channel = samples.data();
    engine.render(&channel, 1, static_cast<int>(samples.size()));
}
double coefficient(std::span<const float> samples, int harmonic) {
    double real = 0.0, imaginary = 0.0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double phase = 2.0 * std::numbers::pi * harmonic * static_cast<double>(i) / static_cast<double>(samples.size());
        real += samples[i] * std::cos(phase);
        imaginary -= samples[i] * std::sin(phase);
    }
    return 2.0 * std::hypot(real, imaginary) / static_cast<double>(samples.size());
}
}

TEST_CASE("Preview pitch uses musical note and device rate independently of editing length") {
    CHECK(wtd::noteFrequency(69) == 440.0);
    CHECK(wtd::noteFrequency(81) == 880.0);
    CHECK_THROWS_AS((void)wtd::noteFrequency(-1), std::out_of_range);
    CHECK_THROWS_AS((void)wtd::noteFrequency(128), std::out_of_range);
    for (const std::size_t length : { 1024u, 2048u }) {
        const auto bank = wtd::PreviewBank::build(wtd::Waveform(length));
        for (const double rate : { 44100.0, 48000.0, 96000.0 }) {
            wtd::PreviewEngine engine;
            engine.prepare(rate);
            REQUIRE(engine.publish(bank));
            engine.setVolume(1.0f);
            engine.setPlaying(true);
            std::vector<float> output(static_cast<std::size_t>(rate * 0.25));
            render(engine, output);
            double maxError = 0.0;
            for (std::size_t i = static_cast<std::size_t>(rate * 0.03); i < output.size(); ++i)
                maxError = std::max(maxError, std::abs(output[i]
                    - 0.25 * std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(i) / rate)));
            CHECK(maxError < 0.00001);
        }
    }
}

TEST_CASE("Preview banks leave authored data intact, remove monitor DC and limit harmonic content") {
    wtd::Waveform wave(1024, wtd::Shape::square);
    const auto original = wave;
    const auto bank = wtd::PreviewBank::build(wave);
    CHECK(wave == original);
    CHECK(coefficient(bank.tables[0], 1) > 0.5);
    CHECK(coefficient(bank.tables[0], 3) > 0.1);
    CHECK(coefficient(bank.tables[9], 1) > 0.5);
    CHECK(coefficient(bank.tables[9], 3) < 0.000001);
    for (std::size_t level = 0; level < wtd::PreviewBank::levelCount; ++level) {
        double mean = 0.0;
        for (const auto value : bank.tables[level]) {
            mean += value;
            CHECK(std::isfinite(value));
            CHECK(std::abs(value) <= 1.000001f);
        }
        CHECK(std::abs(mean / static_cast<double>(wtd::PreviewBank::tableSize)) < 0.000001);
        CHECK(coefficient(bank.tables[level], static_cast<int>(wtd::PreviewBank::harmonics(level)) + 2) < 0.000001);
    }
    wtd::Waveform constant(1024, wtd::Shape::flat);
    for (std::size_t i = 0; i < constant.size(); ++i) constant.setSample(i, 0.6f);
    const auto silent = wtd::PreviewBank::build(constant);
    for (const auto value : silent.tables[0]) CHECK(std::abs(value) < 0.000001f);
    CHECK(constant.samples()[0] == 0.6f);
    CHECK_THROWS_AS((void)wtd::PreviewBank::build(wtd::Waveform(1000)), std::invalid_argument);
}

TEST_CASE("Table selection reserves Nyquist headroom and mutes unsupported high pitches") {
    for (const double rate : { 8000.0, 44100.0, 48000.0, 96000.0 })
        for (int note = 0; note <= 127; ++note) {
            const double frequency = wtd::noteFrequency(note);
            const int level = wtd::PreviewBank::levelFor(frequency, rate);
            if (level < 0) CHECK(frequency >= rate * 0.45);
            else CHECK(frequency * wtd::PreviewBank::harmonics(static_cast<std::size_t>(level)) <= rate * 0.45);
        }
    CHECK(wtd::PreviewBank::levelFor(0.0, 48000.0) == -1);
    CHECK(wtd::PreviewBank::levelFor(440.0, 0.0) == -1);
    wtd::PreviewEngine engine;
    engine.prepare(8000.0);
    engine.setNote(127);
    engine.setPlaying(true);
    REQUIRE(engine.publish(wtd::PreviewBank::build(wtd::Waveform())));
    std::array<float, 512> output;
    render(engine, output);
    for (const auto value : output) CHECK(value == 0.0f);
}

TEST_CASE("Transport and volume ramps are bounded, finish in silence and support null output channels") {
    wtd::PreviewEngine engine;
    engine.prepare(48000.0);
    REQUIRE(engine.publish(wtd::PreviewBank::build(wtd::Waveform(2048))));
    std::array<float, 2048> left, right;
    left.fill(123.0f); right.fill(123.0f);
    float* channels[] { left.data(), nullptr, right.data() };
    engine.render(channels, 3, 2048);
    for (const auto value : left) CHECK(value == 0.0f);
    engine.setPlaying(true);
    engine.setVolume(1.0f);
    engine.render(channels, 3, 2048);
    CHECK(left == right);
    CHECK(std::abs(left[1]) <= 0.25f * 2.0f / 960.0f + 0.000001f);
    for (const auto value : left) CHECK(std::abs(value) <= wtd::PreviewEngine::maxGain);
    engine.setPlaying(false);
    engine.render(channels, 3, 2048);
    for (std::size_t i = 960; i < left.size(); ++i) CHECK(left[i] == 0.0f);
    engine.setPlaying(true);
    engine.setVolume(0.0f);
    engine.render(channels, 3, 2048);
    for (const auto value : left) CHECK(value == 0.0f);
    CHECK_THROWS_AS(engine.setVolume(std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
    CHECK_THROWS_AS(engine.setNote(128), std::out_of_range);
}

TEST_CASE("Waveform and pitch swaps fade without abrupt jumps or resetting the new source") {
    const auto sine = wtd::PreviewBank::build(wtd::Waveform());
    auto inverted = sine;
    for (auto& table : inverted.tables) for (auto& value : table) value = -value;
    wtd::PreviewEngine engine;
    engine.prepare(48000.0);
    engine.setVolume(1.0f);
    engine.setPlaying(true);
    REQUIRE(engine.publish(sine));
    std::array<float, 2048> warmup; render(engine, warmup);
    const float previous = warmup.back();
    REQUIRE(engine.publish(inverted));
    std::array<float, 2048> changed; render(engine, changed);
    CHECK(std::abs(changed.front() - previous) < 0.02f);
    for (std::size_t i = 1; i < changed.size(); ++i) CHECK(std::abs(changed[i] - changed[i - 1]) < 0.025f);
    engine.setNote(81);
    render(engine, warmup);
    std::array<float, 4800> pitched; render(engine, pitched);
    int crossings = 0;
    for (std::size_t i = 1; i < pitched.size(); ++i) if (pitched[i - 1] <= 0.0f && pitched[i] > 0.0f) ++crossings;
    CHECK(crossings >= 87);
    CHECK(crossings <= 89);
    engine.prepare(44100.0);
    render(engine, pitched);
    for (const auto value : pitched) CHECK(std::isfinite(value));
}

TEST_CASE("Audio rendering allocates and frees no memory through updates and gain changes") {
    wtd::PreviewEngine engine;
    const auto sine = wtd::PreviewBank::build(wtd::Waveform());
    auto other = sine;
    for (auto& table : other.tables) for (auto& value : table) value *= -0.5f;
    engine.prepare(48000.0);
    REQUIRE(engine.publish(sine));
    engine.setPlaying(true);
    std::array<float, 512> output;
    render(engine, output);
    REQUIRE(engine.publish(other));
    audioAllocations = audioDeallocations = 0;
    countAudioMemory = true;
    for (int i = 0; i < 10; ++i) {
        engine.setNote(60 + i);
        engine.setVolume(static_cast<float>(i) / 10.0f);
        render(engine, output);
    }
    engine.setPlaying(false);
    render(engine, output);
    countAudioMemory = false;
    CHECK(audioAllocations == 0);
    CHECK(audioDeallocations == 0);
}

TEST_CASE("The bounded mailbox retains the newest pending bank while producer and audio overlap") {
    auto positive = wtd::PreviewBank {};
    auto negative = positive;
    for (auto& table : positive.tables) table.fill(0.8f);
    for (auto& table : negative.tables) table.fill(-0.8f);
    wtd::PreviewEngine engine;
    engine.prepare(48000.0);
    engine.setVolume(1.0f);
    engine.setPlaying(true);
    REQUIRE(engine.publish(positive));
    std::array<float, 512> output;
    render(engine, output);
    std::atomic<bool> finished { false }, failure { false };
    std::thread producer([&] {
        for (int i = 0; i < 2000; ++i) while (!engine.publish(i % 2 == 0 ? positive : negative)) std::this_thread::yield();
        while (!engine.publish(positive)) std::this_thread::yield();
        finished.store(true, std::memory_order_release);
    });
    while (!finished.load(std::memory_order_acquire)) {
        render(engine, output);
        for (const auto value : output) if (!std::isfinite(value) || std::abs(value) > 0.201f) failure.store(true);
    }
    producer.join();
    for (int i = 0; i < 8; ++i) render(engine, output);
    CHECK_FALSE(failure.load());
    CHECK(engine.publications() == 2002);
    CHECK(output.back() == doctest::Approx(0.2f).epsilon(0.00001));
}
