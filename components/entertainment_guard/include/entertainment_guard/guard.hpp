#pragma once
#include <cstdint>

namespace nikos::entertainment_guard {

enum class Phase : std::uint8_t { Available, Window, Cooldown };

// ENT3 uses UTC seconds. ENT1/ENT2 retain civil_start (UTC/local minutes)
// solely for conservative on-load migration. An untrusted record fails closed.
struct Snapshot {
    bool active = false;
    bool civil_valid = false;
    std::uint32_t civil_start = 0;
    bool warning_shown = false;
    std::uint32_t used_ms = 0;
    std::uint64_t anchor_utc_s = 0;
    bool paused = false;
    bool cooldown = false;
    bool legacy = false;
};

class Guard {
public:
    static constexpr std::uint32_t kWindowMs = 20U * 60U * 1000U;
    static constexpr std::uint32_t kIdleResetMs = 15U * 60U * 1000U;
    static constexpr std::uint32_t kBreakMs = 60U * 60U * 1000U;
    static constexpr std::uint32_t kCheckpointMs = 60U * 1000U;
    static constexpr std::uint32_t kCycleMs = kWindowMs + kBreakMs;

    void restore(Snapshot saved, std::uint32_t now, bool trusted,
                 std::uint64_t utc_seconds) {
        last_ms_ = now;
        sampled_ = true;
        if (!saved.active) return;

        warning_shown_ = saved.warning_shown;
        if (saved.legacy) {
            const std::uint64_t start =
                static_cast<std::uint64_t>(saved.civil_start) * 60U;
            if (!saved.civil_valid || !trusted || utc_seconds < start) {
                begin_cooldown(now, false, 0);
                return;
            }
            const std::uint64_t age = utc_seconds - start;
            if (age >= kCycleMs / 1000U) {
                pending_clear_ = true;
                return;
            }
            if (age >= kWindowMs / 1000U) {
                // Preserve the ENT1/2 original mandatory cooldown deadline.
                phase_ = Phase::Cooldown;
                cooldown_started_ms_ = now;
                cooldown_remaining_ms_ =
                    kCycleMs - static_cast<std::uint32_t>(age * 1000U);
                civil_valid_ = true;
                anchor_utc_s_ = start + kWindowMs / 1000U;
                pending_write_ = true; // Upgrade to ENT3 on first boot.
                return;
            }
            // Conservative migration: count the whole old window age as use,
            // never give additional time for historical wall-clock gaps.
            phase_ = Phase::Window;
            used_ms_ = static_cast<std::uint32_t>(age * 1000U);
            last_active_ms_ = now; // Full 15-minute inactivity after reboot.
            civil_valid_ = true;
            anchor_utc_s_ = utc_seconds;
            paused_ = true;
            pending_write_ = true;
            return;
        }

        if (!saved.civil_valid || !trusted
            || utc_seconds < saved.anchor_utc_s
            || saved.used_ms > kWindowMs) {
            begin_cooldown(now, false, 0);
            return;
        }
        civil_valid_ = true;
        anchor_utc_s_ = saved.anchor_utc_s;
        used_ms_ = saved.used_ms;
        if (saved.cooldown || used_ms_ == kWindowMs) {
            phase_ = Phase::Cooldown;
            cooldown_started_ms_ = now;
            const std::uint64_t age_s = utc_seconds - saved.anchor_utc_s;
            if (saved.cooldown && age_s >= kBreakMs / 1000U) {
                complete_session();
            } else {
                cooldown_remaining_ms_ = saved.cooldown
                    ? kBreakMs - static_cast<std::uint32_t>(age_s * 1000U)
                    : kBreakMs; // Pre-reserved 20th minute, crash before lock.
                if (!saved.cooldown) {
                    civil_valid_ = false;
                    anchor_utc_s_ = 0;
                    pending_write_ = true;
                }
            }
            return;
        }

        phase_ = Phase::Window;
        paused_ = true;
        const std::uint64_t age_s = utc_seconds - saved.anchor_utc_s;
        if (saved.paused && age_s >= kIdleResetMs / 1000U) {
            complete_session();
            return;
        }
        const std::uint32_t inactive_ms = saved.paused
            ? static_cast<std::uint32_t>(age_s * 1000U) : 0U;
        last_active_ms_ = now - inactive_ms;
        reserved_until_ms_ = saved.used_ms;
        // An on-disk active marker has no trustworthy last-visible instant:
        // require an entire idle interval after reboot (never a free reset).
    }

    // Called from the main loop with semantic foreground visibility, not
    // button activity. A visible in-app menu or pause is still entertainment.
    void tick(std::uint32_t now, bool visible, bool trusted,
              std::uint64_t utc_seconds) {
        const std::uint32_t delta = sampled_ ? now - last_ms_ : 0U;
        sampled_ = true;
        last_ms_ = now;
        phase(now);
        if (phase_ == Phase::Cooldown) return;
        if (phase_ != Phase::Window) return;

        if (civil_valid_ && (!trusted
            || (last_utc_sample_valid_
                && utc_seconds + 2U < last_utc_sample_s_))) {
            begin_cooldown(now, false, 0);
            return;
        }
        if (trusted) {
            last_utc_sample_valid_ = true;
            last_utc_sample_s_ = utc_seconds;
        }

        // Attribute elapsed time to the *previous* foreground state.
        // A single 20-ms iteration at a transition is the maximum skew.
        if (was_visible_) {
            if (delta >= kWindowMs - used_ms_) used_ms_ = kWindowMs;
            else used_ms_ += delta;
            if (used_ms_ == kWindowMs) {
                begin_cooldown(now, trusted, utc_seconds);
                return;
            }
        }

        if (visible != was_visible_) {
            was_visible_ = visible;
            paused_ = !visible;
            if (!visible) last_active_ms_ = now;
            if (trusted) {
                civil_valid_ = true;
                anchor_utc_s_ = utc_seconds;
            } else {
                civil_valid_ = false; // Restore will fail closed.
                anchor_utc_s_ = 0;
            }
            pending_write_ = true; // Persist every foreground transition.
        }
        if (!visible && now - last_active_ms_ >= kIdleResetMs) {
            complete_session();
            return;
        }
        if (visible && used_ms_ >= reserved_until_ms_)
            pending_write_ = true; // At most one checkpoint per minute.
    }

    Phase phase(std::uint32_t now) {
        if (phase_ == Phase::Cooldown
            && now - cooldown_started_ms_ >= cooldown_remaining_ms_) {
            complete_session();
        }
        return phase_;
    }

    bool enter(std::uint32_t now, bool trusted, std::uint64_t utc_seconds) {
        if (phase(now) == Phase::Cooldown || pending_clear_) return false;
        if (phase_ == Phase::Window
            && !was_visible_ && now - last_active_ms_ >= kIdleResetMs) {
            complete_session();
            return false; // Caller must durably clear before fresh admission.
        }
        if (phase_ == Phase::Available) {
            phase_ = Phase::Window;
            used_ms_ = 0;
            warning_shown_ = false;
            reserved_until_ms_ = 0;
            last_active_ms_ = now;
        }
        paused_ = false;
        reserve_for_launch_ = true;
        civil_valid_ = trusted;
        anchor_utc_s_ = trusted ? utc_seconds : 0;
        pending_write_ = true; // Must commit BEFORE app begin().
        return true;
    }

    bool warning_due(std::uint32_t now) {
        return phase(now) == Phase::Window && !warning_shown_
            && used_ms_ >= 18U * 60U * 1000U;
    }
    void acknowledge_warning() { warning_shown_ = true; pending_write_ = true; }

    // A failed NVS transaction makes the running OS deny entertainment.
    void storage_failure(std::uint32_t now) {
        begin_cooldown(now, false, 0);
        pending_write_ = false;
    }
    void abort_unpersisted_start() { // Kept for old callers; never unlock a dirty session.
        storage_failure(last_ms_);
    }

    bool pending_write() const { return pending_write_; }
    bool pending_clear() const { return pending_clear_; }
    std::uint32_t used_ms() const { return used_ms_; }
    Snapshot snapshot() const {
        const bool reserve = phase_ == Phase::Window
            && (was_visible_ || reserve_for_launch_);
        const auto persisted_used = reserve
            ? (used_ms_ >= kWindowMs - kCheckpointMs
                ? kWindowMs : used_ms_ + kCheckpointMs)
            : used_ms_;
        return {phase_ != Phase::Available, civil_valid_, 0,
                warning_shown_, persisted_used, anchor_utc_s_,
                phase_ == Phase::Window && !reserve,
                phase_ == Phase::Cooldown, false};
    }
    void settled() {
        if (phase_ == Phase::Window)
            reserved_until_ms_ = snapshot().used_ms;
        reserve_for_launch_ = false;
        pending_write_ = false;
        pending_clear_ = false;
    }

private:
    void complete_session() {
        phase_ = Phase::Available;
        used_ms_ = 0;
        was_visible_ = false;
        paused_ = false;
        reserve_for_launch_ = false;
        warning_shown_ = false;
        civil_valid_ = false;
        anchor_utc_s_ = 0;
        pending_write_ = false;
        pending_clear_ = true;
    }
    void begin_cooldown(std::uint32_t now, bool trusted,
                        std::uint64_t utc_seconds) {
        phase_ = Phase::Cooldown;
        used_ms_ = kWindowMs;
        cooldown_started_ms_ = now;
        cooldown_remaining_ms_ = kBreakMs;
        civil_valid_ = trusted;
        anchor_utc_s_ = trusted ? utc_seconds : 0;
        was_visible_ = false;
        reserve_for_launch_ = false;
        paused_ = false;
        pending_write_ = true;
    }

    Phase phase_ = Phase::Available;
    std::uint32_t used_ms_ = 0;
    std::uint32_t last_ms_ = 0;
    std::uint32_t last_active_ms_ = 0;
    std::uint32_t reserved_until_ms_ = 0;
    std::uint32_t cooldown_started_ms_ = 0;
    std::uint32_t cooldown_remaining_ms_ = kBreakMs;
    std::uint64_t anchor_utc_s_ = 0;
    std::uint64_t last_utc_sample_s_ = 0;
    bool sampled_ = false, was_visible_ = false, paused_ = false;
    bool civil_valid_ = false, last_utc_sample_valid_ = false;
    bool reserve_for_launch_ = false, warning_shown_ = false;
    bool pending_write_ = false, pending_clear_ = false;
};
} // namespace nikos::entertainment_guard
