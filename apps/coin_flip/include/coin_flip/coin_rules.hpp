#pragma once
#include <cstdint>
namespace nikos::coin_flip {
enum class Phase : std::uint8_t { Idle, Flipping, Result };
enum class Face : std::uint8_t { Eagle, Tails };
constexpr std::uint32_t kFlipMs = 900;
constexpr std::uint32_t kFrameMs = 75;
// A single uniform hardware random bit makes the two outcomes equiprobable.
constexpr Face from_random(std::uint32_t word) {
    return (word & 1U) ? Face::Tails : Face::Eagle;
}
struct State {
    Phase phase = Phase::Idle;
    Face result = Face::Eagle;
    std::uint32_t elapsed_ms = 0;
    std::uint32_t last_ms = 0;
    std::uint8_t frame = 0;
    bool suspended = false;
    void begin(std::uint32_t now) { *this = {}; last_ms = now; }
    bool toss(std::uint32_t now, std::uint32_t word) {
        if (phase == Phase::Flipping || suspended) return false;
        result = from_random(word);
        phase = Phase::Flipping;
        elapsed_ms = 0;
        frame = 0;
        last_ms = now;
        return true;
    }
    bool tick(std::uint32_t now) {
        if (phase != Phase::Flipping || suspended) return false;
        const auto dt = static_cast<std::uint32_t>(now - last_ms);
        last_ms = now;
        elapsed_ms = dt >= kFlipMs - elapsed_ms ? kFlipMs : elapsed_ms + dt;
        if (elapsed_ms == kFlipMs) {
            phase = Phase::Result;
            return true;
        }
        const auto next = static_cast<std::uint8_t>(elapsed_ms / kFrameMs);
        if (next == frame) return false;
        frame = next;
        return true;
    }
    void suspend() { suspended = true; }
    void resume(std::uint32_t now) { last_ms = now; suspended = false; }
};
}
