#pragma once
#include <cstdint>
namespace nikos::mini_racer::race {
constexpr std::uint32_t kStageLengthPx=7000U;
constexpr std::uint32_t kFinishPx=3U*kStageLengthPx;
constexpr std::uint32_t kDeadlineMs=600000U;
enum class Outcome : std::uint8_t { Racing, Collision, Victory, Timeout };
struct Round {
    std::uint32_t started_ms=0;
    std::uint32_t distance_q8=0;
    Outcome outcome=Outcome::Racing;
    void begin(std::uint32_t now) { started_ms=now; distance_q8=0; outcome=Outcome::Racing; }
    bool deadline(std::uint32_t now) const {
        return static_cast<std::uint32_t>(now-started_ms)>=kDeadlineMs;
    }
    void advance(std::uint32_t delta_q8,bool collided,std::uint32_t now) {
        if(outcome!=Outcome::Racing) return;
        // Guard is checked by the caller before any gameplay update.
        if(deadline(now)) { outcome=Outcome::Timeout; return; }
        if(collided) { outcome=Outcome::Collision; return; }
        const std::uint32_t limit=kFinishPx*256U;
        distance_q8=delta_q8>=limit-distance_q8?limit:distance_q8+delta_q8;
        if(distance_q8==limit) outcome=Outcome::Victory;
    }
    std::uint8_t stage() const {
        const auto pixels=distance_q8/256U;
        return static_cast<std::uint8_t>(pixels>=2*kStageLengthPx?3U
            :pixels>=kStageLengthPx?2U:1U);
    }
    std::uint8_t progress_percent() const {
        return static_cast<std::uint8_t>(
            static_cast<std::uint64_t>(distance_q8)*100U/(kFinishPx*256U));
    }
};
} // namespace nikos::mini_racer::race
