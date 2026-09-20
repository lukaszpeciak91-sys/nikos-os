#include "signal_sound/signal_sound_player.hpp"

#include <array>

#include "esp_timer.h"

namespace {

struct ToneStep {
    float frequency_hz;
    std::uint16_t duration_ms;
};

constexpr std::uint8_t kRepeatCount = 3;

constexpr std::array<ToneStep, 4> kGentleSteps = {{
    {2400.0F, 180},
    {0.0F, 180},
    {2800.0F, 180},
    {0.0F, 500},
}};

constexpr std::array<ToneStep, 6> kClassicSteps = {{
    {2800.0F, 160},
    {0.0F, 120},
    {3300.0F, 160},
    {0.0F, 120},
    {2800.0F, 160},
    {0.0F, 450},
}};

constexpr std::array<ToneStep, 4> kPagerSteps = {{
    {3000.0F, 220},
    {0.0F, 100},
    {3800.0F, 140},
    {0.0F, 320},
}};

std::uint32_t now_ms()
{
    return static_cast<std::uint32_t>(
        esp_timer_get_time() / 1000U);
}

const ToneStep& step_for(
    nikos::settings::SignalSound sound,
    std::uint8_t index)
{
    switch (sound) {
        case nikos::settings::SignalSound::Classic:
            return kClassicSteps[index];
        case nikos::settings::SignalSound::Pager:
            return kPagerSteps[index];
        case nikos::settings::SignalSound::Gentle:
        default:
            return kGentleSteps[index];
    }
}

std::uint8_t step_count_for(nikos::settings::SignalSound sound)
{
    switch (sound) {
        case nikos::settings::SignalSound::Classic:
            return static_cast<std::uint8_t>(kClassicSteps.size());
        case nikos::settings::SignalSound::Pager:
            return static_cast<std::uint8_t>(kPagerSteps.size());
        case nikos::settings::SignalSound::Gentle:
        default:
            return static_cast<std::uint8_t>(kGentleSteps.size());
    }
}

}  // namespace

namespace nikos::signal_sound {

Player::Player(
    board::Board& board,
    const settings::State& settings)
    : board_(board),
      settings_(settings)
{
}

void Player::play_selected()
{
    stop();

    playing_sound_ = settings_.signal_sound;
    playing_ = true;
    step_index_ = 0;
    repeat_index_ = 0;
    step_started_ms_ = now_ms();

    apply_current_step();
}

void Player::update()
{
    if (!playing_) {
        return;
    }

    const std::uint32_t now = now_ms();

    while (playing_
        && now - step_started_ms_ >= current_step_duration_ms()) {
        step_started_ms_ += current_step_duration_ms();
        advance_step();
    }
}

void Player::stop()
{
    board_.stop_tone();
    playing_ = false;
    step_index_ = 0;
    repeat_index_ = 0;
    step_started_ms_ = 0;
}

bool Player::playing() const
{
    return playing_;
}

std::uint8_t Player::step_count() const
{
    return step_count_for(playing_sound_);
}

std::uint16_t Player::current_step_duration_ms() const
{
    return step_for(playing_sound_, step_index_).duration_ms;
}

float Player::current_step_frequency_hz() const
{
    return step_for(playing_sound_, step_index_).frequency_hz;
}

void Player::apply_current_step()
{
    const float frequency_hz = current_step_frequency_hz();
    const std::uint16_t duration_ms = current_step_duration_ms();

    if (frequency_hz > 0.0F) {
        board_.tone(frequency_hz, duration_ms);
    } else {
        board_.stop_tone();
    }
}

void Player::advance_step()
{
    ++step_index_;
    if (step_index_ >= step_count()) {
        step_index_ = 0;
        ++repeat_index_;

        if (repeat_index_ >= kRepeatCount) {
            stop();
            return;
        }
    }

    apply_current_step();
}

}  // namespace nikos::signal_sound
