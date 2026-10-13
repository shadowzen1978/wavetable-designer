#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace wtd {
// View-only phase window. The underlying N samples stay at phase i/N.
class HorizontalViewport {
public:
    [[nodiscard]] double start() const noexcept { return start_; }
    [[nodiscard]] double span() const noexcept { return span_; }
    [[nodiscard]] double phaseAt(double position) const noexcept { return start_ + position * span_; }
    [[nodiscard]] double positionOf(double phase) const noexcept { return (phase - start_) / span_; }
    void zoom(double factor, double anchor) {
        if (!std::isfinite(factor) || !std::isfinite(anchor) || factor < 1.0)
            throw std::invalid_argument("Invalid viewport zoom");
        span_ = 1.0 / std::clamp(factor, 1.0, 65536.0);
        pan(anchor - span_ * 0.5);
    }
    void pan(double start) {
        if (!std::isfinite(start)) throw std::invalid_argument("Invalid viewport position");
        start_ = std::clamp(start, 0.0, 1.0 - span_);
    }
    [[nodiscard]] std::size_t sampleAt(double position, std::size_t count) const {
        if (!std::isfinite(position) || count < 2) throw std::invalid_argument("Invalid sample coordinate");
        return std::min(count - 1, static_cast<std::size_t>(
            std::floor(std::clamp(phaseAt(position), 0.0, 1.0) * static_cast<double>(count) + 0.5)));
    }
private:
    double start_ = 0.0, span_ = 1.0;
};
} // namespace wtd
