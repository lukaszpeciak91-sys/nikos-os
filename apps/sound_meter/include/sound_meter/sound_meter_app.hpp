#pragma once
#include <cstdint>
#include "board/board.hpp"
#include "sound_meter/level_processor.hpp"

namespace nikos::sound_meter {
class SoundMeterApp final {
public:
    enum class Result : std::uint8_t { Running, ExitRequested };
    explicit SoundMeterApp(board::Board& board) : board_(board) {}
    void begin();
    void end();
    void redraw();
    Result update(const board::InputState& input, std::uint32_t now_ms);
private:
    void render(bool full);
    board::Board& board_;
    LevelProcessor levels_{};
    std::int16_t samples_[256]{};
    std::uint32_t last_capture_ms_ = 0;
    std::uint8_t shown_level_ = 255;
    std::uint8_t shown_peak_ = 255;
    bool active_ = false;
    bool microphone_ok_ = false;
    bool capture_pending_ = false;
};
}
