#include "core/Waveform.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <limits>
#include <stdexcept>
#include <utility>

namespace wtd {
namespace {
std::size_t checkedLength(std::size_t length) {
    if (length < 2 || length > Waveform::maxSampleCount)
        throw std::invalid_argument("Cycle length must be between 2 and 65536 samples");
    return length;
}
struct GaussianKernel { int radius; std::vector<double> weights; };
void checkStrength(double strength) {
    if (!std::isfinite(strength) || strength < 0.0 || strength > 1.0)
        throw std::invalid_argument("Smoothing strength must be within [0, 1]");
}
GaussianKernel gaussianKernel(std::size_t length, double strength) {
    const double maxSigma = std::clamp(static_cast<double>(length) / 32.0, 0.5, 64.0);
    const double sigma = 0.5 + strength * strength * (maxSigma - 0.5);
    const int radius = static_cast<int>(std::ceil(4.0 * sigma));
    std::vector<double> weights(static_cast<std::size_t>(radius * 2 + 1));
    double total = 0.0;
    for (int offset = -radius; offset <= radius; ++offset) {
        const double distance = static_cast<double>(offset) / sigma;
        const double weight = std::exp(-0.5 * distance * distance);
        weights[static_cast<std::size_t>(offset + radius)] = weight;
        total += weight;
    }
    for (auto& weight : weights) weight /= total;
    return { radius, std::move(weights) };
}

void checkOptions(SmoothingOptions options) {
    if (options.preserveEdges && (!std::isfinite(options.edgeAngleDegrees)
        || options.edgeAngleDegrees < 0.0 || options.edgeAngleDegrees > 180.0))
        throw std::invalid_argument("Edge angle must be within [0, 180] degrees");
}

// Fit the incoming/outgoing tangent over 1/128 of a cycle. Phase and half-amplitude
// are normalized coordinates; neither the view scale nor sample density sets angles.
double fittedSlope(std::span<const float> source, std::size_t index, int direction, std::size_t width) {
    const double centre = static_cast<double>(width) * 0.5;
    double covariance = 0.0;
    for (std::size_t j = 0; j <= width; ++j) {
        const auto neighbor = static_cast<std::size_t>((static_cast<long long>(index)
            + direction * static_cast<long long>(j) + static_cast<long long>(source.size()))
            % static_cast<long long>(source.size()));
        covariance += (static_cast<double>(j) - centre) * source[neighbor];
    }
    const double n = static_cast<double>(width + 1);
    return direction * covariance / (n * (n * n - 1.0) / 12.0);
}
double signedTurn(std::span<const float> source, std::size_t i, SampleRange range,
                  bool periodic, std::size_t window) {
    const auto before = periodic ? window : std::min(window, i - range.first);
    const auto after = periodic ? window : std::min(window, range.last - i);
    const double scale = static_cast<double>(source.size()) * 0.5;
    return (std::atan(fittedSlope(source, i, 1, after) * scale)
        - std::atan(fittedSlope(source, i, -1, before) * scale)) * 180.0 / std::numbers::pi;
}
std::vector<std::size_t> edgeAnchors(std::span<const float> source, SampleRange range, bool periodic,
                                     double angle, bool reduceJitter) {
    std::vector<std::size_t> anchors;
    const auto window = std::min(source.size() - 1, std::max(std::size_t { 2 }, source.size() / 128));
    if (!reduceJitter) {
        for (auto i = range.first; i <= range.last; ++i) {
            if (!periodic && (i == range.first || i == range.last)) { anchors.push_back(i); continue; }
            if (std::abs(signedTurn(source, i, range, periodic, window)) > angle) anchors.push_back(i);
        }
        return anchors;
    }
    // A jitter sample can produce a large angle at one scale. Require persistent
    // direction change at both scales, then retain peaks instead of a frozen band.
    const auto coarseWindow = std::min(source.size() - 1, window * 2);
    std::vector<double> turns(source.size());
    std::vector<bool> candidate(source.size()), fixed(source.size());
    if (!periodic) fixed[range.first] = fixed[range.last] = true;
    for (auto i = range.first; i <= range.last; ++i) {
        if (!periodic && (i == range.first || i == range.last)) continue;
        const double fine = signedTurn(source, i, range, periodic, window);
        turns[i] = signedTurn(source, i, range, periodic, coarseWindow);
        candidate[i] = std::abs(fine) > angle && std::abs(turns[i]) > angle && fine * turns[i] > 0.0;
    }
    // Keep both original sides of an isolated hard step, including the cycle seam.
    const auto delta = [&](std::size_t i) { return static_cast<double>(source[(i + 1) % source.size()]) - source[i]; };
    for (auto i = range.first; i <= range.last; ++i) {
        if (!periodic && i == range.last) break;
        const double previous = !periodic && i == range.first ? 0.0 : std::abs(delta((i + source.size() - 1) % source.size()));
        const double next = !periodic && i + 1 == range.last ? 0.0 : std::abs(delta((i + 1) % source.size()));
        const double jump = std::abs(delta(i));
        if (jump > 0.04 && jump > 8.0 * std::max({ previous, next, 0.000001 })) {
            // A hard step must also exceed the user's angle threshold.
            if (std::abs(turns[i]) > angle) fixed[i] = true;
            const auto j = (i + 1) % source.size();
            if (std::abs(turns[j]) > angle) fixed[j] = true;
        }
    }
    for (auto i = range.first; i <= range.last; ++i) {
        if (fixed[i]) { anchors.push_back(i); continue; }
        if (!candidate[i]) continue;
        bool peak = true;
        for (int offset = -static_cast<int>(window); offset <= static_cast<int>(window); ++offset) {
            const auto raw = static_cast<long long>(i) + offset;
            if (!periodic && (raw < static_cast<long long>(range.first) || raw > static_cast<long long>(range.last))) continue;
            const auto j = static_cast<std::size_t>((raw + static_cast<long long>(source.size())) % static_cast<long long>(source.size()));
            if ((fixed[j] && turns[i] * turns[j] > 0.0)
                || (candidate[j] && turns[i] * turns[j] > 0.0 && std::abs(turns[j]) > std::abs(turns[i]) + 1.0e-9)) {
                peak = false; break;
            }
        }
        if (peak) anchors.push_back(i);
    }
    return anchors;
}

// A constrained quadratic t*(1-t) keeps both endpoints exact. Accept the fit
// only when it is close to the drawing and its total turn is below the threshold.
bool fitGentleInterval(std::span<const float> source, std::span<float> output,
                       double strength, double angle, std::size_t cycleCount) {
    if (source.size() < 5) return false;
    const double first = source.front(), last = source.back();
    const double denominator = static_cast<double>(source.size() - 1);
    double cross = 0.0, energy = 0.0;
    for (std::size_t i = 1; i + 1 < source.size(); ++i) {
        const double t = static_cast<double>(i) / denominator, basis = t * (1.0 - t);
        cross += basis * (source[i] - std::lerp(first, last, t));
        energy += basis * basis;
    }
    double curvature = cross / energy;
    const auto errorFor = [&](double coefficient) {
        double sum = 0.0, max = 0.0;
        for (std::size_t i = 1; i + 1 < source.size(); ++i) {
            const double t = static_cast<double>(i) / denominator;
            const double difference = source[i] - (std::lerp(first, last, t) + coefficient * t * (1.0 - t));
            sum += difference * difference;
            max = std::max(max, std::abs(difference));
        }
        return std::pair { std::sqrt(sum / static_cast<double>(source.size() - 2)), max };
    };
    auto error = errorFor(curvature);
    // Curvature comparable to the noise does not justify bending a straight line.
    if (std::abs(curvature) <= 4.0 * error.first) { curvature = 0.0; error = errorFor(curvature); }
    if (error.first > 0.02 || error.second > 0.04) return false;
    const double scale = static_cast<double>(cycleCount) / (2.0 * denominator);
    const double turn = std::abs(std::atan((last - first + curvature) * scale)
        - std::atan((last - first - curvature) * scale)) * 180.0 / std::numbers::pi;
    if (turn > angle) return false;
    std::vector<float> fitted(source.size());
    double maxChange = 0.0;
    for (std::size_t i = 1; i + 1 < source.size(); ++i) {
        const double t = static_cast<double>(i) / denominator;
        fitted[i] = std::clamp(static_cast<float>(std::lerp(first, last, t) + curvature * t * (1.0 - t)), -1.0f, 1.0f);
        maxChange = std::max(maxChange, std::abs(static_cast<double>(fitted[i]) - source[i]));
    }
    if (maxChange <= 2.0 * std::numeric_limits<float>::epsilon()) return true;
    for (std::size_t i = 1; i + 1 < source.size(); ++i)
        output[i] = std::clamp(static_cast<float>(std::lerp(static_cast<double>(source[i]),
            static_cast<double>(fitted[i]), strength)), -1.0f, 1.0f);
    return true;
}
// Smooth only the interior of an interval, anchoring its two original endpoints.
// Input/output may alias: residuals are captured before writing any result.
void smoothInterval(std::span<const float> source, std::span<float> output, double strength,
                    bool reduceJitter, double angle, std::size_t cycleCount) {
    const auto length = source.size();
    if (strength == 0.0 || length < 3) return;
    if (reduceJitter && fitGentleInterval(source, output, strength, angle, cycleCount)) return;
    const double first = source.front(), last = source.back();
    const auto trend = [&](std::size_t i) {
        return std::lerp(first, last, static_cast<double>(i) / static_cast<double>(length - 1));
    };
    std::vector<double> residual(length);
    double maxResidual = 0.0;
    for (std::size_t i = 0; i < length; ++i) {
        residual[i] = source[i] - trend(i);
        maxResidual = std::max(maxResidual, std::abs(residual[i]));
    }
    if (maxResidual <= 2.0 * std::numeric_limits<float>::epsilon()) return;
    const auto kernel = gaussianKernel(length, strength);
    for (std::size_t i = 1; i + 1 < length; ++i) {
        double sum = 0.0;
        for (int offset = -kernel.radius; offset <= kernel.radius; ++offset) {
            const auto neighbor = static_cast<long long>(i) + offset;
            if (neighbor >= 0 && neighbor < static_cast<long long>(length))
                sum += residual[static_cast<std::size_t>(neighbor)]
                    * kernel.weights[static_cast<std::size_t>(offset + kernel.radius)];
        }
        output[i] = std::clamp(static_cast<float>(trend(i) + std::lerp(residual[i], sum, strength)), -1.0f, 1.0f);
    }
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

Waveform Waveform::smoothed(double strength, SmoothingOptions options) const {
    checkStrength(strength);
    checkOptions(options);
    if (strength == 0.0) return *this;
    if (options.preserveEdges) {
        const auto anchors = edgeAnchors(samples_, { 0, size() - 1 }, true, options.edgeAngleDegrees, options.reduceJitter);
        if (!anchors.empty()) {
            auto result = *this;
            for (std::size_t a = 0; a < anchors.size(); ++a) {
                const auto first = anchors[a];
                const auto last = a + 1 < anchors.size() ? anchors[a + 1] : anchors.front() + size();
                std::vector<float> interval(last - first + 1);
                for (std::size_t j = 0; j < interval.size(); ++j) interval[j] = samples_[(first + j) % size()];
                smoothInterval(interval, interval, strength, options.reduceJitter, options.edgeAngleDegrees, size());
                for (std::size_t j = 1; j + 1 < interval.size(); ++j) result.samples_[(first + j) % size()] = interval[j];
            }
            return result;
        }
    }
    if (options.reduceJitter) {
        auto result = *this;
        const double fitAngle = options.preserveEdges ? options.edgeAngleDegrees : 30.0;
        if (fitGentleInterval(samples_, result.samples_, strength, fitAngle, size())) return result;
    }
    const auto kernel = gaussianKernel(size(), strength);
    Waveform result(size(), Shape::flat);
    for (std::size_t i = 0; i < size(); ++i) {
        double sum = 0.0;
        for (int offset = -kernel.radius; offset <= kernel.radius; ++offset) {
            const auto index = static_cast<std::size_t>((static_cast<long long>(i)
                + offset + static_cast<long long>(size())) % static_cast<long long>(size()));
            sum += samples_[index] * kernel.weights[static_cast<std::size_t>(offset + kernel.radius)];
        }
        result.samples_[i] = std::clamp(static_cast<float>(std::lerp(
            static_cast<double>(samples_[i]), sum, strength)), -1.0f, 1.0f);
    }
    return result;
}

Waveform Waveform::smoothedRegion(double strength, SampleRange range, SmoothingOptions options) const {
    checkStrength(strength);
    checkOptions(options);
    if (range.first > range.last || range.last >= size())
        throw std::out_of_range("Smoothing region is outside the cycle");
    if (strength == 0.0 || range.last - range.first < 2) return *this;
    const auto anchors = options.preserveEdges
        ? edgeAnchors(samples_, range, false, options.edgeAngleDegrees, options.reduceJitter)
        : std::vector<std::size_t> { range.first, range.last };
    auto result = *this;
    for (std::size_t a = 0; a + 1 < anchors.size(); ++a) {
        const auto first = anchors[a], length = anchors[a + 1] - first + 1;
        smoothInterval(std::span<const float>(samples_).subspan(first, length),
                       std::span<float>(result.samples_).subspan(first, length), strength,
                       options.reduceJitter, options.preserveEdges ? options.edgeAngleDegrees : 30.0, size());
    }
    return result;
}
} // namespace wtd
