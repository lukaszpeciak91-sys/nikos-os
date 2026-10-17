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
inline void spawn(World& w) {
    // Deliberately conservative: only one live hull; avoid combining
    // a new hull with stray hostile shots from a destroyed shooter.
    for (const auto& e : w.enemies) if (e.active) return;
    for (const auto& s : w.hostile) if (s.active) return;
    for (auto& e : w.enemies) if (!e.active) {
        e = Enemy{};
        e.active = true;
        e.type = choose_type(w.rng, w.score);
        e.hp = hp_for(e.type);
        e.lane = static_cast<std::uint8_t>(w.rng.range(3));
        e.x_q8 = 244 * 256;
        e.next_shot_ms = w.time_ms + 1100;
        break;
    }
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
