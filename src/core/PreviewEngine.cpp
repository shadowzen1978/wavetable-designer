#include "core/PreviewEngine.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wtd {
bool PreviewEngine::publish(const PreviewBank& bank) noexcept {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        auto expected = Owner::free;
        if (!slots_[i].owner.compare_exchange_strong(expected, Owner::producer, std::memory_order_acquire)) continue;
        slots_[i].bank = bank;
        slots_[i].owner.store(Owner::ready, std::memory_order_release);
        const int replaced = mailbox_.exchange(static_cast<int>(i), std::memory_order_acq_rel);
        if (replaced >= 0) slots_[static_cast<std::size_t>(replaced)].owner.store(Owner::free, std::memory_order_release);
        publications_.fetch_add(1, std::memory_order_release);
        return true;
    }
    return false;
}
void PreviewEngine::setNote(int note) {
    (void)noteFrequency(note);
    note_.store(note, std::memory_order_relaxed);
}
void PreviewEngine::setVolume(float value) {
    if (!std::isfinite(value)) throw std::invalid_argument("Preview volume must be finite");
    volume_.store(std::clamp(value, 0.0f, 1.0f), std::memory_order_relaxed);
}
void PreviewEngine::releasePrevious() noexcept {
    if (previous_.slot >= 0 && previous_.slot != current_.slot)
        slots_[static_cast<std::size_t>(previous_.slot)].owner.store(Owner::free, std::memory_order_release);
    previous_.slot = -1;
    fadeRemaining_ = 0;
}
void PreviewEngine::prepare(double rate) noexcept {
    releasePrevious();
    sampleRate_ = std::isfinite(rate) && rate > 0.0 ? rate : 48000.0;
    fadeLength_ = std::max(1, static_cast<int>(sampleRate_ * 0.01));
    gainLength_ = std::max(1, static_cast<int>(sampleRate_ * 0.02));
    gain_ = targetGain_ = gainStep_ = 0.0f;
    gainRemaining_ = 0;
    current_.phase = 0.0;
    current_.level = PreviewBank::levelFor(current_.frequency, sampleRate_);
}
void PreviewEngine::beginUpdate(int slot, int note) noexcept {
    previous_ = current_;
    current_.slot = slot;
    current_.note = note;
    current_.frequency = 440.0 * std::exp2(static_cast<double>(note - 69) / 12.0);
    current_.level = PreviewBank::levelFor(current_.frequency, sampleRate_);
    if (previous_.slot >= 0) fadeRemaining_ = fadeLength_;
}
float PreviewEngine::read(Voice& voice) noexcept {
    const float sample = voice.slot < 0 ? 0.0f : slots_[static_cast<std::size_t>(voice.slot)].bank.sample(voice.level, voice.phase);
    voice.phase += voice.frequency / sampleRate_;
    voice.phase -= std::floor(voice.phase);
    return sample;
}
void PreviewEngine::render(float* const* output, int channels, int frames) noexcept {
    if (frames <= 0 || channels < 0) return;
    if (fadeRemaining_ == 0) {
        const int pending = mailbox_.exchange(-1, std::memory_order_acq_rel);
        const int note = note_.load(std::memory_order_relaxed);
        if (pending >= 0) {
            slots_[static_cast<std::size_t>(pending)].owner.store(Owner::audio, std::memory_order_release);
            beginUpdate(pending, note);
        } else if (current_.slot >= 0 && note != current_.note) beginUpdate(current_.slot, note);
    }
    const float target = playing_.load(std::memory_order_relaxed)
        ? volume_.load(std::memory_order_relaxed) * maxGain : 0.0f;
    if (target != targetGain_) {
        targetGain_ = target;
        gainRemaining_ = gainLength_;
        gainStep_ = (targetGain_ - gain_) / static_cast<float>(gainLength_);
    }
    for (int i = 0; i < frames; ++i) {
        float value = read(current_);
        if (fadeRemaining_ > 0) {
            const float progress = 1.0f - static_cast<float>(fadeRemaining_) / static_cast<float>(fadeLength_);
            value = std::lerp(read(previous_), value, progress);
            if (--fadeRemaining_ == 0) releasePrevious();
        }
        if (gainRemaining_ > 0 && --gainRemaining_ == 0) gain_ = targetGain_;
        else if (gainRemaining_ > 0) gain_ += gainStep_;
        value *= gain_;
        for (int channel = 0; channel < channels; ++channel) if (output && output[channel]) output[channel][i] = value;
    }
}
} // namespace wtd
