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
    bool light_on() const;

private:
    enum class State : std::uint8_t {
        SelectBrightness,
        LightOn,
    };

    static constexpr std::uint8_t kBrightnessLevels[3] = {
        128,
        192,
        255,
    };

    void start_light();
    void stop_light(bool render_selector);
    void render_selector();

    board::Board& board_;
    power::DisplayLifecycle& display_lifecycle_;

    State state_ = State::SelectBrightness;
    bool active_ = false;
    std::uint8_t selection_ = 1;
};

}  // namespace nikos::flashlight
