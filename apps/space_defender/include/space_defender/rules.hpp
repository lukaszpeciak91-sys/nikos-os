#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

namespace nikos::space_defender::rules {
constexpr std::size_t kMaxEnemies = 4;
constexpr std::size_t kMaxShots = 8;
constexpr std::size_t kMaxHostileShots = 4;
constexpr std::uint32_t kFireIntervalMs = 350;
constexpr std::uint32_t kInvulnerableMs = 1500;
constexpr std::uint32_t kSessionMs = 600000;
constexpr std::uint32_t kMaxElapsedMs = 120;
constexpr std::uint32_t kStepMs = 20;
constexpr int kPlayerX = 24;
constexpr int kLaneY[3] = {44, 70, 96};
enum class Type : std::uint8_t { Scout, Armored, Shooter };
enum class Phase : std::uint8_t { Playing, Paused, GameOver, TimeLimit };
struct Random {
    std::uint32_t value = 1;
    explicit Random(std::uint32_t seed = 1) : value(seed ? seed : 1) {}
    std::uint32_t next() {
        value ^= value << 13; value ^= value >> 17; value ^= value << 5;
        return value;
    }
    std::uint32_t range(std::uint32_t n) { return next() % n; }
};
struct Enemy {
    bool active = false;
    Type type = Type::Scout;
    std::uint8_t lane = 0, hp = 1;
    std::int32_t x_q8 = 0;
    std::uint32_t next_shot_ms = 0;
    std::uint32_t warning_ms = 0;
    std::uint32_t motion_remainder = 0;
};
struct Shot {
    bool active = false;
    std::uint8_t lane = 0;
    std::int32_t x_q8 = 0;
};
struct World {
    Phase phase = Phase::Playing;
    std::array<Enemy, kMaxEnemies> enemies{};
    std::array<Shot, kMaxShots> shots{};
    std::array<Shot, kMaxHostileShots> hostile{};
    Random rng{};
    std::uint32_t time_ms = 0, last_fire_ms = 0, next_spawn_ms = 0;
    std::uint32_t invulnerable_until_ms = 0;
    std::uint32_t score = 0, record = 0;
    std::uint8_t lane = 1, lives = 3;
    std::uint32_t motion_remainder[3]{}; // player shot, enemy, hostile fractional Q8
};
inline std::uint8_t hp_for(Type t) { return t == Type::Armored ? 3 : (t == Type::Shooter ? 2 : 1); }
inline std::uint32_t points_for(Type t) { return t == Type::Armored ? 30U : (t == Type::Shooter ? 25U : 10U); }
inline std::uint32_t speed_for(Type t, std::uint32_t score) {
    return (t == Type::Scout ? 38U : (t == Type::Armored ? 26U : 31U)) +
        (score >= 500U ? 6U : 0U);
}
inline Type choose_type(Random& rng, std::uint32_t score) {
    if (score < 100) return Type::Scout;
    if (score < 250) return rng.range(3) ? Type::Scout : Type::Armored;
    const auto r = rng.range(5);
    return r < 2 ? Type::Scout : (r < 4 ? Type::Armored : Type::Shooter);
}
inline void reset(World& w, std::uint32_t seed, std::uint32_t record) {
    w = World{};
    w.rng = Random(seed);
    w.record = record;
    w.next_spawn_ms = 800;
}
inline void cycle_lane(World& w) { if (w.phase == Phase::Playing) w.lane = (w.lane + 1U) % 3U; }
inline void damage(World& w) {
    if (w.phase != Phase::Playing || w.lives == 0
        || (w.invulnerable_until_ms && w.time_ms < w.invulnerable_until_ms)) return;
    --w.lives;
    w.invulnerable_until_ms = w.time_ms + kInvulnerableMs;
    if (w.lives == 0) w.phase = Phase::GameOver;
}
inline std::uint32_t move_q8(std::uint32_t px_per_sec, std::uint32_t ms, std::uint32_t& rem) {
    const std::uint64_t n = static_cast<std::uint64_t>(px_per_sec) * 256U * ms + rem;
    rem = n % 1000U;
    return static_cast<std::uint32_t>(n / 1000U);
}
inline void fire(World& w) {
    for (auto& s : w.shots) if (!s.active) {
        s = Shot{true, w.lane, 42 * 256};
        break;
    }
}
// Conservative one-button reachability budget: the worst possible lane
// transition needs two M5 presses. Require >=1100ms between arrivals of
// different hazards; even at the fastest 44px/s, this is >48px separation.
inline std::uint8_t concurrent_limit(std::uint32_t score) {
    return score < 100 ? 2U : (score < 250 ? 3U : 4U);
}
inline std::uint32_t active_enemies(const World& w) {
    std::uint32_t count = 0;
    for (const auto& e : w.enemies) if (e.active) ++count;
    return count;
}
// Bound collision arrival under an arbitrary future 500-point speed step.
// Future change timing is unknown, so every possible arrival lies in this
// interval (earliest at boosted speed, latest at current speed).
struct ArrivalWindow {
    std::uint32_t first;
    std::uint32_t last;
};
inline std::uint32_t arrival_ms(std::int32_t x_q8, std::uint32_t speed) {
    const std::int32_t remaining = x_q8 - (kPlayerX + 16) * 256;
    return remaining > 0
        ? static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(remaining) * 1000U / (speed * 256U))
        : 0;
}
inline ArrivalWindow hull_window(const World& w, Type type, std::int32_t x_q8) {
    const std::uint32_t current = speed_for(type, w.score);
    const std::uint32_t maximum = current + (w.score < 500U ? 6U : 0U);
    return {arrival_ms(x_q8, maximum), arrival_ms(x_q8, current)};
}
inline bool separated(ArrivalWindow a, ArrivalWindow b) {
    constexpr std::uint32_t kTwoPressBudgetMs = 1100;
    return (a.last < b.first && b.first - a.last >= kTwoPressBudgetMs)
        || (b.last < a.first && a.first - b.last >= kTwoPressBudgetMs);
}
inline bool safe_spawn(const World& w, std::uint8_t lane, Type type,
                       std::int32_t x_q8) {
    const auto candidate = hull_window(w, type, x_q8);
    // Shooter future projectiles are not yet represented in the hostile
    // pool. Never overlap a shooter with another hull: its warning-to-fire
    // interval and subsequent slower projectile are independent hazards.
    for (const auto& e : w.enemies) if (e.active) {
        if (e.type == Type::Shooter || type == Type::Shooter) return false;
        if (!separated(candidate, hull_window(w, e.type, e.x_q8))) return false;
    }
    // Bullets from already destroyed shooters remain hazards; their speed
    // does not change on difficulty transition.
    for (const auto& shot : w.hostile) if (shot.active) {
        const auto arrival = arrival_ms(shot.x_q8, 27U);
        if (!separated(candidate, {arrival, arrival})) return false;
    }
    (void)lane; // timing separation is required even for different lanes.
    return true;
}
inline void spawn(World& w) {
    if (active_enemies(w) >= concurrent_limit(w.score)) return;
    Enemy* slot = nullptr;
    for (auto& e : w.enemies) if (!e.active) { slot = &e; break; }
    if (!slot) return;
    // One sampled type/lane. Fixed bounded spatial offsets (no reroll loop).
    const Type type = choose_type(w.rng, w.score);
    const auto lane = static_cast<std::uint8_t>(w.rng.range(3));
    std::int32_t x = 244 * 256;
    for (const auto& e : w.enemies) if (e.active && e.x_q8 >= x - 80 * 256)
        x = e.x_q8 + (80 + static_cast<std::int32_t>(w.rng.range(61))) * 256;
    if (x > 560 * 256 || !safe_spawn(w, lane, type, x)) return;
    *slot = Enemy{};
    slot->active = true;
    slot->type = type;
    slot->hp = hp_for(type);
    slot->lane = lane;
    slot->x_q8 = x;
    slot->next_shot_ms = w.time_ms + 1100;
}
inline void substep(World& w, std::uint32_t dt) {
    if (w.phase != Phase::Playing || dt == 0) return;
    w.time_ms += dt;
    if (w.time_ms - w.last_fire_ms >= kFireIntervalMs) {
        w.last_fire_ms += kFireIntervalMs;
        fire(w);
    }
    if (w.time_ms >= w.next_spawn_ms) {
        spawn(w);
        w.next_spawn_ms = w.time_ms + (w.score >= 500 ? 800 : 1100) + w.rng.range(501);
    }
    // Fractional displacement is shared per velocity class, never rounded
    // separately for each object (otherwise motion diverges with pool size).
    const auto friendly_dx = move_q8(112, dt, w.motion_remainder[0]);
    const auto hostile_dx = move_q8(27, dt, w.motion_remainder[2]);
    for (auto& s : w.shots) if (s.active) {
        const auto old = s.x_q8;
        s.x_q8 += friendly_dx;
        if (s.x_q8 >= 240 * 256) s.active = false;
        for (auto& e : w.enemies) if (s.active && e.active && s.lane == e.lane
                && old <= e.x_q8 + 18 * 256 && s.x_q8 >= e.x_q8 - 4 * 256) {
            s.active = false;
            if (--e.hp == 0) {
                e.active = false;
                w.score += points_for(e.type);
                if (w.score > w.record) w.record = w.score;
            }
        }
    }
    for (auto& e : w.enemies) if (e.active) {
        const auto old = e.x_q8;
        // Enemy type speed differs; calculate once per substep in high
        // precision and keep each object's remainder separately later.
        const auto dx = static_cast<std::int32_t>(
            move_q8(speed_for(e.type,w.score), dt, e.motion_remainder));
        e.x_q8 -= dx;
        if (e.type == Type::Shooter) {
            if (w.time_ms >= e.next_shot_ms && e.x_q8 > 105 * 256) {
                e.warning_ms = w.time_ms + 320;
                e.next_shot_ms = w.time_ms + 2100;
            }
            if (e.warning_ms && w.time_ms >= e.warning_ms) {
                e.warning_ms = 0;
                for (auto& shot : w.hostile) if (!shot.active) {
                    shot = Shot{true,e.lane,e.x_q8 - 5 * 256};
                    break;
                }
            }
        }
        if (e.lane == w.lane && old + 18 * 256 >= (kPlayerX - 7) * 256
            && e.x_q8 <= (kPlayerX + 16) * 256) damage(w);
        if (e.x_q8 + 18 * 256 < 0) e.active = false;
    }
    for (auto& s : w.hostile) if (s.active) {
        const auto old = s.x_q8;
        s.x_q8 -= hostile_dx;
        if (s.lane == w.lane && old >= (kPlayerX - 4) * 256
            && s.x_q8 <= (kPlayerX + 15) * 256) {
            s.active = false; damage(w);
        }
        if (s.x_q8 < 0) s.active = false;
    }
}
inline void advance(World& w, std::uint32_t elapsed_ms) {
    if (w.phase != Phase::Playing) return;
    std::uint32_t remaining = elapsed_ms < kMaxElapsedMs ? elapsed_ms : kMaxElapsedMs;
    while (remaining) {
        const auto dt = remaining < kStepMs ? remaining : kStepMs;
        substep(w,dt);
        remaining -= dt;
        if (w.phase != Phase::Playing) break;
    }
}
} // namespace nikos::space_defender::rules
