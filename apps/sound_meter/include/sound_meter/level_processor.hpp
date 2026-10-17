#pragma once
#include <cstddef>
#include <cstdint>

namespace nikos::sound_meter {

// Experimental signal-relative scale, NOT calibrated sound pressure or dB.
// All tunable constants are deliberately confined to this processor.
class LevelProcessor final {
public:
    void reset();
    void reset_peak();
    void consume(const std::int16_t* samples, std::size_t count, std::uint32_t now_ms);
    std::uint8_t level() const { return level_; }
    std::uint8_t peak() const { return peak_; }
    float last_rms() const { return last_rms_; }

private:
    float last_rms_ = 0.0F;
    std::uint8_t level_ = 0;
    std::uint8_t peak_ = 0;
    std::uint32_t peak_at_ms_ = 0;
};
}
