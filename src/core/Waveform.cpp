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
        }
        samples_[i] = static_cast<float>(amplitude);
    }
}
} // namespace wtd
