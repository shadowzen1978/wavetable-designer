#include "core/Waveform.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace wtd {
namespace {
std::size_t checkedLength(std::size_t length) {
    if (length < 2 || length > Waveform::maxSampleCount)
        throw std::invalid_argument("Cycle length must be between 2 and 65536 samples");
    return length;
}
} // namespace

Waveform::Waveform(std::size_t sampleCount, Shape shape)
    : samples_(checkedLength(sampleCount)) {
    fill(shape);
}

void Waveform::setSample(std::size_t index, float amplitude) {
    if (index >= samples_.size())
        throw std::out_of_range("Sample index is outside the cycle");
    if (!std::isfinite(amplitude))
        throw std::invalid_argument("Sample amplitude must be finite");
    samples_[index] = std::clamp(amplitude, -1.0f, 1.0f);
}

void Waveform::fill(Shape shape) noexcept {
    for (std::size_t i = 0; i < samples_.size(); ++i) {
        const double phase = static_cast<double>(i) / static_cast<double>(samples_.size());
        double amplitude = 0.0;
        switch (shape) {
            case Shape::sine: amplitude = std::sin(2.0 * std::numbers::pi * phase); break;
            case Shape::triangle: amplitude = 1.0 - 4.0 * std::abs(phase - 0.5); break;
            case Shape::saw: amplitude = 2.0 * phase - 1.0; break;
            case Shape::square: amplitude = phase < 0.5 ? 1.0 : -1.0; break;
            case Shape::flat: break;
        }
        samples_[i] = static_cast<float>(amplitude);
    }
}

void Waveform::drawSegment(std::size_t from, float fromAmplitude, std::size_t to, float toAmplitude) {
    if (from >= size() || to >= size()) throw std::out_of_range("Stroke is outside the cycle");
    if (!std::isfinite(fromAmplitude) || !std::isfinite(toAmplitude))
        throw std::invalid_argument("Stroke amplitudes must be finite");
    fromAmplitude = std::clamp(fromAmplitude, -1.0f, 1.0f);
    toAmplitude = std::clamp(toAmplitude, -1.0f, 1.0f);
    if (from == to) { samples_[to] = toAmplitude; return; }
    if (from > to) { std::swap(from, to); std::swap(fromAmplitude, toAmplitude); }
    for (auto i = from; i <= to; ++i) {
        const double fraction = static_cast<double>(i - from) / static_cast<double>(to - from);
        samples_[i] = static_cast<float>(std::lerp(static_cast<double>(fromAmplitude),
                                                 static_cast<double>(toAmplitude), fraction));
    }
}

Waveform Waveform::resampled(std::size_t sampleCount) const {
    Waveform result(sampleCount, Shape::flat);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const double position = static_cast<double>(i) * static_cast<double>(size())
            / static_cast<double>(sampleCount);
        const auto index = static_cast<std::size_t>(position);
        const double fraction = position - static_cast<double>(index);
        result.samples_[i] = static_cast<float>(std::lerp(static_cast<double>(samples_[index]),
            static_cast<double>(samples_[(index + 1) % size()]), fraction));
    }
    return result;
}
} // namespace wtd
