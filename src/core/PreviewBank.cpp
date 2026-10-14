#include "core/PreviewBank.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace wtd {
namespace {
void fft(std::vector<std::complex<double>>& data, bool inverse) {
    const auto n = data.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        auto bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }
    for (std::size_t length = 2; length <= n; length <<= 1) {
        const double angle = (inverse ? 2.0 : -2.0) * std::numbers::pi / static_cast<double>(length);
        const auto step = std::polar(1.0, angle);
        for (std::size_t base = 0; base < n; base += length) {
            std::complex<double> weight { 1.0, 0.0 };
            for (std::size_t j = 0; j < length / 2; ++j) {
                const auto even = data[base + j], odd = data[base + j + length / 2] * weight;
                data[base + j] = even + odd;
                data[base + j + length / 2] = even - odd;
                weight *= step;
            }
        }
    }
    if (inverse) for (auto& sample : data) sample /= static_cast<double>(n);
}
}
double noteFrequency(int note) {
    if (note < 0 || note > 127) throw std::out_of_range("MIDI note must be within [0, 127]");
    return 440.0 * std::exp2(static_cast<double>(note - 69) / 12.0);
}
PreviewBank PreviewBank::build(const Waveform& wave) {
    if ((wave.size() & (wave.size() - 1)) != 0)
        throw std::invalid_argument("Preview requires a power-of-two cycle length");
    std::vector<std::complex<double>> spectrum(wave.size());
    for (std::size_t i = 0; i < wave.size(); ++i) spectrum[i] = wave.samples()[i];
    fft(spectrum, false);
    PreviewBank result;
    double peak = 1.0;
    const double scale = static_cast<double>(tableSize) / static_cast<double>(wave.size());
    for (std::size_t level = 0; level < levelCount; ++level) {
        std::vector<std::complex<double>> filtered(tableSize);
        const auto last = std::min(static_cast<std::size_t>(harmonics(level)), wave.size() / 2);
        for (std::size_t k = 1; k <= last; ++k) {
            auto coefficient = spectrum[k] * scale;
            if (k == wave.size() / 2) coefficient *= 0.5; // Source Nyquist is a single real bin.
            filtered[k] = coefficient;
            filtered[tableSize - k] = std::conj(coefficient);
        }
        // No DC in the monitor copy. Authored samples retain their original offset.
        fft(filtered, true);
        for (std::size_t i = 0; i < tableSize; ++i) {
            result.tables[level][i] = static_cast<float>(filtered[i].real());
            peak = std::max(peak, std::abs(filtered[i].real()));
        }
    }
    // Suppress reconstruction overshoot only; never boost a quiet authored waveform.
    if (peak > 1.0) for (auto& table : result.tables)
        for (auto& value : table) value = static_cast<float>(value / peak);
    return result;
}
int PreviewBank::levelFor(double frequency, double sampleRate) noexcept {
    if (!std::isfinite(frequency) || !std::isfinite(sampleRate) || frequency <= 0.0 || sampleRate <= 0.0
        || frequency >= sampleRate * 0.45) return -1;
    const double limit = sampleRate * 0.45 / frequency;
    for (std::size_t level = 0; level < levelCount; ++level)
        if (static_cast<double>(harmonics(level)) <= limit) return static_cast<int>(level);
    return -1;
}
float PreviewBank::sample(int level, double phase) const noexcept {
    if (level < 0) return 0.0f;
    const double position = phase * static_cast<double>(tableSize);
    const auto first = static_cast<std::size_t>(position) % tableSize;
    const auto next = (first + 1) % tableSize;
    return std::lerp(tables[static_cast<std::size_t>(level)][first],
                     tables[static_cast<std::size_t>(level)][next], static_cast<float>(position - std::floor(position)));
}
} // namespace wtd
