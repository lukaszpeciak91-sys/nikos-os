#pragma once

#include <cstdint>

#include "board/board.hpp"

namespace nikos::battery_guard {

enum class AdvisoryLevel : std::uint8_t {
    None,
    Low,
    VeryLow,
};

struct UpdateResult {
    bool sampled = false;
    bool charging_detected = false;
    bool critical_confirmed = false;
    board::PowerStatus power_status{};
};

class BatteryGuard final {
public:
    static constexpr std::uint32_t kSampleIntervalMs = 10000;
    static constexpr std::int32_t kLowPercent = 20;
    static constexpr std::int32_t kVeryLowPercent = 10;
    static constexpr std::int32_t kLowRearmPercent = 25;
    static constexpr std::int32_t kVeryLowRearmPercent = 15;
    static constexpr std::int16_t kCriticalVoltageMv = 3300;
    static constexpr std::uint8_t kCriticalConfirmSamples = 2;

    explicit BatteryGuard(board::Board& board);

    UpdateResult update(std::uint32_t now_ms);

    AdvisoryLevel pending_advisory() const;
    void acknowledge_advisory();

private:
    void sample(std::uint32_t now_ms, UpdateResult& result);
    void update_advisory(const board::PowerStatus& status);
    bool update_critical(const board::PowerStatus& status);

    board::Board& board_;
    bool sample_valid_ = false;
    std::uint32_t last_sample_ms_ = 0;

    bool low_armed_ = true;
    bool very_low_armed_ = true;
    AdvisoryLevel pending_advisory_ = AdvisoryLevel::None;

    std::uint8_t critical_confirm_count_ = 0;
};

}  // namespace nikos::battery_guard
