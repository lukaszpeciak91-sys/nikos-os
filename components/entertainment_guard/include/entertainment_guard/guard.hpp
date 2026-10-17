#pragma once
#include <cstdint>
namespace nikos::entertainment_guard {
enum class Phase : std::uint8_t { Available, Window, Cooldown };
struct Snapshot { bool active=false; bool civil_valid=false; std::uint32_t civil_start=0; };
class Guard {
public:
    static constexpr std::uint32_t kWindowMs=20U*60U*1000U;
    static constexpr std::uint32_t kBreakMs=60U*60U*1000U;
    static constexpr std::uint32_t kCycleMs=kWindowMs+kBreakMs;
    void restore(Snapshot saved, std::uint32_t now_ms, bool clock_trusted,
                 std::uint32_t civil_now) {
        if (!saved.active) return;
        if (!saved.civil_valid || !clock_trusted || civil_now<saved.civil_start) {
            phase_=Phase::Cooldown; start_ms_=now_ms; fallback_=true;
            return;
        }
        const std::uint64_t age=static_cast<std::uint64_t>(civil_now-saved.civil_start)*60000U;
        if(age>=kCycleMs) { clear_pending_=true; return; }
        phase_=age>=kWindowMs?Phase::Cooldown:Phase::Window;
        start_ms_=now_ms-static_cast<std::uint32_t>(age);
        civil_start_=saved.civil_start;
        civil_valid_=true;
    }
    Phase phase(std::uint32_t now_ms) {
        if(phase_==Phase::Available)return phase_;
        const auto elapsed=now_ms-start_ms_;
        const auto expiry=fallback_?kBreakMs:kCycleMs;
        if(elapsed>=expiry) {
            phase_=Phase::Available; warning_shown_=false;
            fallback_=false; civil_valid_=false; clear_pending_=true;
        } else if(!fallback_ && elapsed>=kWindowMs)phase_=Phase::Cooldown;
        return phase_;
    }
    bool enter(std::uint32_t now_ms,bool clock_trusted,std::uint32_t civil_now) {
        if(phase(now_ms)==Phase::Cooldown)return false;
        if(phase_==Phase::Available) {
            phase_=Phase::Window;start_ms_=now_ms;warning_shown_=false;
            civil_valid_=clock_trusted; civil_start_=clock_trusted?civil_now:0;
            write_pending_=true;
        }
        return true;
    }
    bool warning_due(std::uint32_t now_ms) {
        return phase(now_ms)==Phase::Window && !warning_shown_
            && now_ms-start_ms_>=18U*60U*1000U;
    }
    void acknowledge_warning() { warning_shown_=true; }
    bool pending_write() const { return write_pending_; }
    bool pending_clear() const { return clear_pending_; }
    Snapshot snapshot() const { return {true,civil_valid_,civil_start_}; }
    void settled() { write_pending_=false;clear_pending_=false; }
private:
    Phase phase_=Phase::Available;
    std::uint32_t start_ms_=0,civil_start_=0;
    bool civil_valid_=false,fallback_=false,warning_shown_=false;
    bool write_pending_=false,clear_pending_=false;
};
}
