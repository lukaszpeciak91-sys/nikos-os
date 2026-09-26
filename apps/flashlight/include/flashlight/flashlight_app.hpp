#pragma once

#include <cstdint>

#include "board/board.hpp"
#include "power/display_lifecycle.hpp"

namespace nikos::flashlight {

class FlashlightApp final {
public:
    enum class UpdateResult : std::uint8_t {
        Running,
        ExitRequested,
    };

    FlashlightApp(
        board::Board& board,
        power::DisplayLifecycle& display_lifecycle);

    void begin();
    void end();
    void redraw();
    UpdateResult update(const board::InputState& input);

    void prepare_for_foreground_takeover();

private:
    enum class State : std::uint8_t {
        SelectBrightness,
        LightOn,
    };

    static std::uint8_t brightness_for_selection(
        std::uint8_t selection);

    void start_light();
    void stop_light(bool should_render_selector);
    void render_selector();

    board::Board& board_;
    power::DisplayLifecycle& display_lifecycle_;

    State state_ = State::SelectBrightness;
    bool active_ = false;
    std::uint8_t selection_ = 1;
};

}  // namespace nikos::flashlight
