#include "sound_meter/level_processor.hpp"
#include <cmath>
#include <cstdint>

namespace nikos::sound_meter {
namespace {
constexpr float kNoiseFloorRms = 180.0F;
constexpr float kFullScaleRms = 9000.0F;
constexpr float kRise = 0.80F;
constexpr float kFall = 0.20F;
constexpr std::uint32_t kPeakHoldMs = 1000;
constexpr std::uint8_t kPeakDecayPerUpdate = 1;

std::uint8_t clamp_level(float x)
{
    if (!(x > 0.0F)) return 0;
    if (x >= 100.0F) return 100;
    return static_cast<std::uint8_t>(x + 0.5F);
}
}

void LevelProcessor::reset()
{
    level_ = 0;
    reset_peak();
}

void LevelProcessor::reset_peak()
{
    peak_ = level_;
    peak_at_ms_ = 0;
}

void LevelProcessor::consume(const std::int16_t* samples, std::size_t count,
                             std::uint32_t now_ms)
{
    if (!samples || count == 0) return;

    // Remove DC offset from PDM-to-PCM capture before measuring AC energy.
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < count; ++i) sum += samples[i];
    const double mean = static_cast<double>(sum) / static_cast<double>(count);
    double energy = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const double centered = static_cast<double>(samples[i]) - mean;
        energy += centered * centered;
    }
    const float rms = static_cast<float>(std::sqrt(energy / count));
    // Logarithmic compressor is chosen only to make the relative indicator
    // usable over a wide amplitude range. This is NOT dB SPL or dB(A).
    float relative = 0.0F;
    if (rms > kNoiseFloorRms) {
        const float span = std::log1p(kFullScaleRms - kNoiseFloorRms);
        relative = 100.0F * std::log1p(rms - kNoiseFloorRms) / span;
    }
    const float current = static_cast<float>(level_);
    const float alpha = relative > current ? kRise : kFall;
    const std::uint8_t next = clamp_level(current + alpha * (relative - current));
    level_ = next;

    if (next >= peak_) {
        peak_ = next;
        peak_at_ms_ = now_ms;
    } else if (now_ms - peak_at_ms_ >= kPeakHoldMs && peak_ > next) {
        const std::uint8_t decayed =
            peak_ > kPeakDecayPerUpdate ? peak_ - kPeakDecayPerUpdate : 0;
        peak_ = decayed > next ? decayed : next;
    }
}
}
