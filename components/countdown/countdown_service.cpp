#include "countdown/countdown_service.hpp"

#include <algorithm>

namespace nikos::countdown {

Service::Service(NowUsFunction now_us)
    : now_us_(now_us)
{
}

void Service::advance_configured_duration()
{
    if (state_ != State::Idle) {
        return;
    }

    configured_duration_seconds_ =
        next_duration_seconds(configured_duration_seconds_);
}

bool Service::start()
{
    if (now_us_ == nullptr
        || configured_duration_seconds_ < kMinimumDurationSeconds
        || configured_duration_seconds_ > kMaximumDurationSeconds) {
        return false;
    }

    const std::int64_t duration_us =
        static_cast<std::int64_t>(configured_duration_seconds_)
        * kMicrosecondsPerSecond;

    deadline_us_ = now_us() + duration_us;
    paused_remaining_us_ = 0;
    state_ = State::Running;
    return true;
}

void Service::pause()
{
    if (state_ != State::Running || now_us_ == nullptr) {
        return;
    }

    const std::int64_t remaining_us =
        running_remaining_us(now_us());
    if (remaining_us <= 0) {
        deadline_us_ = 0;
        paused_remaining_us_ = 0;
        state_ = State::Expired;
        return;
    }

    paused_remaining_us_ = remaining_us;
    deadline_us_ = 0;
    state_ = State::Paused;
}

void Service::resume()
{
    if (state_ != State::Paused || now_us_ == nullptr) {
        return;
    }

    if (paused_remaining_us_ <= 0) {
        paused_remaining_us_ = 0;
        state_ = State::Expired;
        return;
    }

    deadline_us_ = now_us() + paused_remaining_us_;
    paused_remaining_us_ = 0;
    state_ = State::Running;
}

void Service::reset()
{
    state_ = State::Idle;
    deadline_us_ = 0;
    paused_remaining_us_ = 0;
}

void Service::update()
{
    if (state_ != State::Running || now_us_ == nullptr) {
        return;
    }

    if (now_us() < deadline_us_) {
        return;
    }

    deadline_us_ = 0;
    paused_remaining_us_ = 0;
    state_ = State::Expired;
}

State Service::state() const
{
    return state_;
}

std::uint32_t Service::configured_duration_seconds() const
{
    return configured_duration_seconds_;
}

std::uint32_t Service::remaining_seconds() const
{
    if (state_ == State::Running && now_us_ != nullptr) {
        return ceil_seconds(running_remaining_us(now_us()));
    }

    if (state_ == State::Paused) {
        return ceil_seconds(paused_remaining_us_);
    }

    return 0;
}

bool Service::expired_pending() const
{
    return state_ == State::Expired;
}

void Service::acknowledge_expiration()
{
    if (state_ == State::Expired) {
        reset();
    }
}

std::uint32_t Service::next_duration_seconds(
    std::uint32_t current_seconds)
{
    if (current_seconds < kMinimumDurationSeconds
        || current_seconds >= kMaximumDurationSeconds) {
        return kMinimumDurationSeconds;
    }

    if (current_seconds < 5U * 60U) {
        return current_seconds + 30U;
    }

    return current_seconds + 60U;
}

std::int64_t Service::now_us() const
{
    return now_us_();
}

std::int64_t Service::running_remaining_us(
    std::int64_t now) const
{
    return std::max<std::int64_t>(deadline_us_ - now, 0);
}

std::uint32_t Service::ceil_seconds(
    std::int64_t remaining_us)
{
    if (remaining_us <= 0) {
        return 0;
    }

    return static_cast<std::uint32_t>(
        (remaining_us + kMicrosecondsPerSecond - 1)
        / kMicrosecondsPerSecond);
}

}  // namespace nikos::countdown
