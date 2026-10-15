#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nikos::mini_racer::rules {

constexpr std::size_t kEnemyCount = 3;
constexpr std::int32_t kSpacingPx = 110;
constexpr std::int32_t kEnemyWidth = 26;
constexpr std::int32_t kRoadLeft = 5;
constexpr std::int32_t kPlayerLeft = 33;
constexpr std::int32_t kPlayerRight = 59;

struct Enemy {
    std::int32_t x_q8 = 0;
    std::uint8_t lane = 0;
};
using Traffic = std::array<Enemy, kEnemyCount>;

inline void initialize(Traffic& traffic, std::int32_t spawn_x,
                       std::uint8_t first_lane) {
    for (std::size_t i = 0; i < traffic.size(); ++i) {
        traffic[i].x_q8 = (spawn_x + static_cast<std::int32_t>(i) * kSpacingPx) * 256;
        traffic[i].lane = static_cast<std::uint8_t>((first_lane ^ (i & 1U)) & 1U);
    }
}

// Swept collisions and fixed 110 px recycling. Opposite lane stays clear
// beside each obstacle; at 70 px/s the following car arrives >=1.57 s later.
inline bool advance(Traffic& traffic, std::uint32_t speed_q8,
                    std::uint32_t dt_ms, std::uint8_t player_lane) {
    for (auto& enemy : traffic) {
        const std::int32_t old_x = enemy.x_q8 / 256;
        enemy.x_q8 -= static_cast<std::int32_t>((speed_q8 * dt_ms) / 1000U);
        const std::int32_t new_x = enemy.x_q8 / 256;
        if (enemy.lane == player_lane
            && old_x + kEnemyWidth >= kPlayerLeft
            && new_x <= kPlayerRight) {
            return true;
        }
    }
    for (auto& enemy : traffic) {
        if (enemy.x_q8 / 256 + kEnemyWidth >= kRoadLeft) continue;
        std::int32_t rightmost = enemy.x_q8;
        std::uint8_t rightmost_lane = enemy.lane;
        for (const auto& other : traffic) {
            if (&other != &enemy && other.x_q8 > rightmost) {
                rightmost = other.x_q8;
                rightmost_lane = other.lane;
            }
        }
        enemy.x_q8 = rightmost + kSpacingPx * 256;
        enemy.lane = static_cast<std::uint8_t>(rightmost_lane ^ 1U);
    }
    return false;
}

}  // namespace nikos::mini_racer::rules
