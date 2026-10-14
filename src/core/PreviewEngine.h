#pragma once
#include "core/PreviewBank.h"
#include <atomic>
#include <cstdint>

namespace wtd {
// One producer publishes complete banks; one audio consumer renders. Four fixed
// slots cover two fading voices, one pending bank, and one producer write.
class PreviewEngine {
public:
    static constexpr float maxGain = 0.25f;
    PreviewEngine() = default;
    bool publish(const PreviewBank&) noexcept; // Single producer, off audio thread.
    void prepare(double sampleRate) noexcept; // Consumer quiescent; no producer-owned slot is touched.
    void render(float* const* output, int channels, int frames) noexcept;
    void setPlaying(bool value) noexcept { playing_.store(value, std::memory_order_relaxed); }
    void setNote(int midiNote);
    void setVolume(float value);
    [[nodiscard]] bool playing() const noexcept { return playing_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::uint64_t publications() const noexcept { return publications_.load(std::memory_order_acquire); }
private:
    enum class Owner : int { free, producer, ready, audio };
    struct Slot { PreviewBank bank; std::atomic<Owner> owner { Owner::free }; };
    struct Voice { int slot = -1, level = -1, note = 69; double frequency = 440.0, phase = 0.0; };
    void releasePrevious() noexcept;
    void beginUpdate(int slot, int note) noexcept;
    [[nodiscard]] float read(Voice&) noexcept;
    std::array<Slot, 4> slots_;
    std::atomic<int> mailbox_ { -1 }, note_ { 69 };
    std::atomic<bool> playing_ { false };
    std::atomic<float> volume_ { 0.2f };
    std::atomic<std::uint64_t> publications_ { 0 };
    Voice current_, previous_;
    double sampleRate_ = 48000.0;
    int fadeRemaining_ = 0, fadeLength_ = 480;
    float gain_ = 0.0f, targetGain_ = 0.0f, gainStep_ = 0.0f;
    int gainRemaining_ = 0, gainLength_ = 960;
};
static_assert(std::atomic<int>::is_always_lock_free && std::atomic<bool>::is_always_lock_free
    && std::atomic<float>::is_always_lock_free && std::atomic<std::uint64_t>::is_always_lock_free);
} // namespace wtd
