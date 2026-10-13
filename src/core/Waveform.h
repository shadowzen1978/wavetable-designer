#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace wtd {

enum class Shape { sine, triangle, saw, square };

// N values at phase i/N. No duplicated endpoint; no framework dependencies.
class Waveform {
public:
    static constexpr std::size_t maxSampleCount = 65536;
    explicit Waveform(std::size_t sampleCount = 1024, Shape shape = Shape::sine);

    [[nodiscard]] std::span<const float> samples() const noexcept { return samples_; }
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    void setSample(std::size_t index, float amplitude);
    void fill(Shape shape) noexcept;

private:
    std::vector<float> samples_;
};

} // namespace wtd
