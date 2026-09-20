#pragma once

#include <cstdint>

#include "board/board.hpp"
#include "settings/settings.hpp"

namespace nikos::signal_sound {

class Player final {
public:
    Player(
        board::Board& board,
        const settings::State& settings);

    void play_selected();
    void update();
    void stop();
    bool playing() const;

private:
    std::uint8_t step_count() const;
    std::uint16_t current_step_duration_ms() const;
    float current_step_frequency_hz() const;
    void apply_current_step();
    void advance_step();

    board::Board& board_;
    const settings::State& settings_;

    settings::SignalSound playing_sound_ =
        settings::SignalSound::Gentle;
    bool playing_ = false;
    std::uint8_t step_index_ = 0;
    std::uint8_t repeat_index_ = 0;
    std::uint32_t step_started_ms_ = 0;
};

}  // namespace nikos::signal_sound
