#include "audio/AudioPreview.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wtd {
AudioPreview::AudioPreview(bool enableDevices) : juce::Thread("Preview table builder"), enableDevices_(enableDevices) {
    if (!enableDevices_) engine_.prepare(48000.0);
    startThread();
}
AudioPreview::~AudioPreview() {
    engine_.setPlaying(false);
    if (callbackAdded_) manager_.removeAudioCallback(this);
    manager_.closeAudioDevice();
    signalThreadShouldExit();
    wake_.signal();
    stopThread(-1);
}
std::uint64_t AudioPreview::requestWaveform(const Waveform& waveform) {
    const auto revision = ++requestedRevision_;
    { const std::lock_guard lock(pendingMutex_); pending_ = Job { waveform, revision }; }
    wake_.signal();
    return revision;
}
void AudioPreview::run() {
    while (!threadShouldExit()) {
        std::optional<Job> job;
        { const std::lock_guard lock(pendingMutex_); job.swap(pending_); }
        if (!job) { wake_.wait(1000); continue; }
        try {
            const auto bank = PreviewBank::build(job->waveform);
            while (!threadShouldExit() && !engine_.publish(bank)) wait(2);
            if (!threadShouldExit()) publishedRevision_.store(job->revision, std::memory_order_release);
            workerFailed_.store(false, std::memory_order_relaxed);
        } catch (...) { workerFailed_.store(true, std::memory_order_relaxed); }
    }
}
juce::String AudioPreview::ensureDevice() {
    if (!enableDevices_) return {};
    if (!callbackAdded_) { manager_.addAudioCallback(this); callbackAdded_ = true; }
    if (auto* device = manager_.getCurrentAudioDevice(); device && device->isOpen() && device->isPlaying()) return {};
    deviceFailed_.store(false, std::memory_order_relaxed);
    const auto error = manager_.initialiseWithDefaultDevices(0, 2);
    if (error.isNotEmpty()) return error;
    auto* device = manager_.getCurrentAudioDevice();
    if (!device || !device->isOpen()) return "No audio output is available. Choose an output in Audio settings.";
    if (!device->isPlaying()) return "The selected audio output could not start.";
    return {};
}
juce::String AudioPreview::setPlaying(bool value) {
    if (value) {
        const auto error = ensureDevice();
        if (error.isNotEmpty()) { engine_.setPlaying(false); return error; }
    }
    engine_.setPlaying(value);
    return {};
}
juce::String AudioPreview::status() const {
    if (workerFailed_.load(std::memory_order_relaxed)) return "Unable to prepare the waveform for preview.";
    if (deviceFailed_.load(std::memory_order_relaxed)) return "Audio output stopped. Check Audio settings.";
    if (enableDevices_) {
        if (auto* device = manager_.getCurrentAudioDevice())
            return device->getName() + "  /  " + juce::String(device->getCurrentSampleRate() / 1000.0, 1) + " kHz";
    }
    return playing() ? "Preview playing" : "Preview stopped";
}
void AudioPreview::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    engine_.prepare(device->getCurrentSampleRate());
    deviceFailed_.store(false, std::memory_order_relaxed);
}
void AudioPreview::audioDeviceStopped() { engine_.setPlaying(false); }
void AudioPreview::audioDeviceError(const juce::String&) {
    deviceFailed_.store(true, std::memory_order_relaxed);
    engine_.setPlaying(false);
}
void AudioPreview::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* output,
    int channels, int frames, const juce::AudioIODeviceCallbackContext&) {
    engine_.render(output, channels, frames);
    float blockPeak = 0.0f;
    for (int channel = 0; channel < channels; ++channel)
        if (output[channel]) for (int i = 0; i < frames; ++i) blockPeak = std::max(blockPeak, std::abs(output[channel][i]));
    peak_.store(std::max(peak_.load(std::memory_order_relaxed), blockPeak), std::memory_order_relaxed);
    frames_.fetch_add(static_cast<std::uint64_t>(frames), std::memory_order_relaxed);
}
void AudioPreview::renderOffline(std::span<float> output) {
    if (enableDevices_) throw std::logic_error("Offline rendering cannot share a running device consumer");
    float* channel = output.data();
    engine_.render(&channel, 1, static_cast<int>(output.size()));
}
} // namespace wtd
