#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace nikos::mini_racer::rules {
constexpr std::size_t kEnemyCount = 3;
constexpr std::int32_t kEnemyWidth = 26;
constexpr std::int32_t kRoadLeft = 5;
constexpr std::int32_t kPlayerLeft = 33;
constexpr std::int32_t kPlayerRight = 59;
constexpr std::int32_t kMinGapPx = 120;
constexpr std::int32_t kMaxGapPx = 180;
constexpr std::uint32_t kInitialSpeedQ8 = 36U * 256U;
constexpr std::uint32_t kSpeedStepQ8 = 5U * 256U;
constexpr std::uint32_t kMaxSpeedQ8 = 75U * 256U;
enum class State : std::uint8_t { Playing, Paused, GameOver, TimeLimit, FinishCrossing, Victory };
inline State pause(State state) { return state == State::Playing ? State::Paused : state; }
inline State resume(State state) { return state == State::Paused ? State::Playing : state; }
inline State hit(State state) { return state == State::Playing ? State::GameOver : state; }

inline std::uint32_t speed_for_score(std::uint32_t score) {
    const std::uint32_t steps = score / 100U;
    if (steps >= 8U) return kMaxSpeedQ8;
    const std::uint32_t computed = kInitialSpeedQ8 + steps * kSpeedStepQ8;
    return computed < kMaxSpeedQ8 ? computed : kMaxSpeedQ8;
}
struct Motion {
    std::uint32_t remainder = 0;
    // Integer Q8 pixels, with thousandths of Q8 retained across frames.
    std::uint32_t advance(std::uint32_t speed_q8, std::uint32_t dt_ms) {
        const std::uint64_t total =
            static_cast<std::uint64_t>(speed_q8) * dt_ms + remainder;
        remainder = static_cast<std::uint32_t>(total % 1000U);
        return static_cast<std::uint32_t>(total / 1000U);
    }
};
struct Random {
    std::uint32_t state = 1;
    explicit Random(std::uint32_t seed = 1) : state(seed ? seed : 1) {}
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    std::uint32_t bounded(std::uint32_t count) { return next() % count; }
};
inline std::int32_t random_gap(Random& rng) {
    return kMinGapPx + static_cast<std::int32_t>(
        rng.bounded(static_cast<std::uint32_t>(kMaxGapPx - kMinGapPx + 1)));
}
struct Enemy {
    std::int32_t x_q8 = 0;
    std::uint8_t lane = 0;
};
using Traffic = std::array<Enemy, kEnemyCount>;

// The minimum 120px leading-edge gap leaves 94px of unobstructed
// longitudinal separation between 26px cars, or >=1.25s at 75px/s.
// Each obstacle occupies only one lane and lane switches are instantaneous.
inline void initialize(Traffic& traffic, std::int32_t spawn_x, Random& rng) {
    std::int32_t x = spawn_x;
    for (auto& enemy : traffic) {
        enemy.x_q8 = x * 256;
        enemy.lane = static_cast<std::uint8_t>(rng.bounded(2));
        x += random_gap(rng);
    }
}
inline bool advance(Traffic& traffic, std::uint32_t displacement_q8,
                    std::uint8_t player_lane, Random& rng) {
    // Sweep all vehicles first; collision must not be skipped on delayed updates.
    for (auto& enemy : traffic) {
        const std::int32_t old_x = enemy.x_q8 / 256;
        enemy.x_q8 -= static_cast<std::int32_t>(displacement_q8);
        const std::int32_t new_x = enemy.x_q8 / 256;
        if (enemy.lane == player_lane
            && old_x + kEnemyWidth >= kPlayerLeft
            && new_x <= kPlayerRight) return true;
    }
    // Recycle in front-to-back exit order, always after current rightmost.
    for (auto& enemy : traffic) {
        if (enemy.x_q8 / 256 + kEnemyWidth >= kRoadLeft) continue;
        std::int32_t rightmost = enemy.x_q8;
        for (const auto& other : traffic)
            if (&other != &enemy && other.x_q8 > rightmost) rightmost = other.x_q8;
        enemy.x_q8 = rightmost + random_gap(rng) * 256;
        enemy.lane = static_cast<std::uint8_t>(rng.bounded(2));
    }
    return false;
}
} // namespace nikos::mini_racer::rules
