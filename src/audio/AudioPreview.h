#pragma once
#include "core/PreviewEngine.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <mutex>
#include <optional>

namespace wtd {
class AudioPreview final : private juce::AudioIODeviceCallback, private juce::Thread {
public:
    explicit AudioPreview(bool enableDevices = true);
    ~AudioPreview() override;
    std::uint64_t requestWaveform(const Waveform&); // Message-thread snapshot; newest job wins.
    juce::String ensureDevice();
    juce::String setPlaying(bool);
    void setNote(int note) { engine_.setNote(note); }
    void setVolume(float volume) { engine_.setVolume(volume); }
    [[nodiscard]] bool playing() const noexcept { return engine_.playing(); }
    [[nodiscard]] juce::String status() const;
    [[nodiscard]] juce::AudioDeviceManager& deviceManager() noexcept { return manager_; }
    [[nodiscard]] bool devicesEnabled() const noexcept { return enableDevices_; }
    [[nodiscard]] std::uint64_t publishedRevision() const noexcept { return publishedRevision_.load(std::memory_order_acquire); }
    [[nodiscard]] std::uint64_t publications() const noexcept { return engine_.publications(); }
    [[nodiscard]] std::uint64_t framesRendered() const noexcept { return frames_.load(std::memory_order_relaxed); }
    [[nodiscard]] float outputPeak() const noexcept { return peak_.load(std::memory_order_relaxed); }
    void renderOffline(std::span<float> output); // Hidden validation mode only; one consumer.
private:
    void run() override;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                         const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String&) override;
    juce::AudioDeviceManager manager_;
    PreviewEngine engine_;
    std::mutex pendingMutex_;
    struct Job { Waveform waveform; std::uint64_t revision; };
    std::optional<Job> pending_;
    std::uint64_t requestedRevision_ = 0;
    std::atomic<std::uint64_t> publishedRevision_ { 0 };
    juce::WaitableEvent wake_;
    std::atomic<bool> deviceFailed_ { false }, workerFailed_ { false };
    std::atomic<std::uint64_t> frames_ { 0 };
    std::atomic<float> peak_ { 0.0f };
    bool enableDevices_ = true, callbackAdded_ = false;
};
} // namespace wtd
