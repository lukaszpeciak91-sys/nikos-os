#pragma once

#include <cstdint>

namespace nikos::countdown {

enum class State : std::uint8_t {
    Idle,
    Running,
    Paused,
    Expired,
};

class Service final {
public:
    using NowUsFunction = std::int64_t (*)();

    static constexpr std::uint32_t kMinimumDurationSeconds = 30;
    static constexpr std::uint32_t kDefaultDurationSeconds = 5 * 60;
    static constexpr std::uint32_t kMaximumDurationSeconds = 15 * 60;

    explicit Service(NowUsFunction now_us);

    void advance_configured_duration();

    bool start();
    void pause();
    void resume();
    void reset();
    void update();

    State state() const;
    std::uint32_t configured_duration_seconds() const;
    std::uint32_t remaining_seconds() const;
    bool expired_pending() const;

    void acknowledge_expiration();

private:
    static constexpr std::int64_t kMicrosecondsPerSecond = 1000000;

    static std::uint32_t next_duration_seconds(std::uint32_t current_seconds);

    std::int64_t now_us() const;
    std::int64_t running_remaining_us(std::int64_t now) const;
    static std::uint32_t ceil_seconds(std::int64_t remaining_us);

    NowUsFunction now_us_;
    State state_ = State::Idle;
    std::uint32_t configured_duration_seconds_ = kDefaultDurationSeconds;
    std::int64_t deadline_us_ = 0;
    std::int64_t paused_remaining_us_ = 0;
};

}  // namespace nikos::countdown
